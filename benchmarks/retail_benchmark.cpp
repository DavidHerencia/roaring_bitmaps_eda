#include "retail_index.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using clock_type = std::chrono::steady_clock;
using bitmap = roaring::roaring_bitmap;
using posting_map = std::map<std::string, bitmap>;

struct settings {
    std::string data_path;
    std::filesystem::path output_path;
    unsigned repetitions = 15;
    unsigned warmup = 3;
    unsigned build_repetitions = 3;
    unsigned seed = 20261115;
    bool overwrite = false;
};

struct selected_query {
    std::string id;
    retail::query request;
    std::uint64_t first_count;
    std::uint64_t second_count;
};

struct key_count {
    std::string key;
    std::uint64_t count;
};

struct space_totals {
    std::uint64_t postings = 0;
    std::uint64_t cardinality = 0;
    std::uint64_t blocks = 0;
    std::uint64_t key_bytes = 0;
    std::uint64_t payload_bytes = 0;
    std::uint64_t metadata_bytes = 0;
    std::uint64_t allocated_bytes = 0;
    std::uint64_t flat_bytes = 0;
    std::uint64_t full_bitmap_bytes = 0;
};

std::uint64_t elapsed_ns(clock_type::time_point start) {
    auto duration = clock_type::now() - start;
    return std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
}

unsigned parse_number(const std::string& value) {
    if (value.empty() || value.find_first_not_of("0123456789") != std::string::npos) {
        throw std::invalid_argument("expected a nonnegative integer: " + value);
    }
    return std::stoul(value);
}

settings read_settings(int argc, char** argv) {
    settings result;
    for (int position = 1; position < argc; ++position) {
        std::string option = argv[position];
        if (option == "--overwrite") {
            result.overwrite = true;
            continue;
        }
        if (position + 1 >= argc) {
            throw std::invalid_argument("missing value for " + option);
        }
        std::string value = argv[position + 1];
        ++position;
        if (option == "--data") {
            result.data_path = value;
        } else if (option == "--output") {
            result.output_path = value;
        } else if (option == "--repetitions") {
            result.repetitions = parse_number(value);
        } else if (option == "--warmup") {
            result.warmup = parse_number(value);
        } else if (option == "--build-repetitions") {
            result.build_repetitions = parse_number(value);
        } else if (option == "--seed") {
            result.seed = parse_number(value);
        } else {
            throw std::invalid_argument("unknown option: " + option);
        }
    }
    if (result.data_path.empty() || result.output_path.empty() ||
        result.repetitions == 0 || result.build_repetitions == 0) {
        throw std::invalid_argument("required: --data CSV --output DIR; repetitions > 0");
    }
    bool has_files = std::filesystem::exists(result.output_path) &&
                     !std::filesystem::is_empty(result.output_path);
    if (has_files && !result.overwrite) {
        throw std::invalid_argument("output is not empty; choose a new directory or use --overwrite");
    }
    std::filesystem::create_directories(result.output_path);
    return result;
}

std::ofstream output_file(const settings& options, const std::string& name) {
    std::ofstream stream(options.output_path / name);
    if (!stream) {
        throw std::runtime_error("cannot write " + name);
    }
    stream << std::setprecision(17);
    return stream;
}

// escribe un texto como campo csv entre comillas
void field(std::ostream& output, const std::string& value) {
    output << '"';
    for (char character : value) {
        if (character == '"') {
            output << '"';
        }
        output << character;
    }
    output << '"';
}

std::string kind_name(retail::query_kind kind) {
    if (kind == retail::query_kind::term) {
        return "term";
    }
    if (kind == retail::query_kind::country) {
        return "country";
    }
    if (kind == retail::query_kind::term_and_country) {
        return "and";
    }
    return "or";
}

bool fewer_ids_first(const key_count& left, const key_count& right) {
    if (left.count != right.count) {
        return left.count < right.count;
    }
    return left.key < right.key;
}

// claves ordenadas de menor a mayor cantidad de filas
std::vector<key_count> rank_postings(const posting_map& postings) {
    std::vector<key_count> ranked;
    for (const auto& entry : postings) {
        ranked.push_back({entry.first, entry.second.cardinality()});
    }
    std::sort(ranked.begin(), ranked.end(), fewer_ids_first);
    return ranked;
}

void add_query(std::vector<selected_query>& queries, const std::string& id, retail::query_kind kind,
               const key_count& first, const key_count& second) {
    selected_query item;
    item.id = id;
    item.request = {kind, first.key, second.key};
    item.first_count = first.count;
    item.second_count = second.count;
    queries.push_back(item);
}

// elige terminos raro/mediano/frecuente y paises raro/frecuente, y los combina
std::vector<selected_query> select_queries(const retail::retail_index& index) {
    std::vector<key_count> terms = rank_postings(index.term_postings());
    std::vector<key_count> countries = rank_postings(index.country_postings());
    if (terms.empty() || countries.empty()) {
        throw std::invalid_argument("benchmark needs at least one term and one country");
    }
    std::vector<key_count> chosen = {terms.front(), terms[terms.size() / 2], terms.back()};
    std::vector<std::string> labels = {"rare", "median", "frequent"};
    key_count rare_country = countries.front();
    key_count frequent_country = countries.back();
    key_count nothing = {"", 0};

    std::vector<selected_query> queries;
    for (std::size_t i = 0; i < chosen.size(); ++i) {
        add_query(queries, "term_" + labels[i], retail::query_kind::term, chosen[i], nothing);
    }
    add_query(queries, "country_rare", retail::query_kind::country, rare_country, nothing);
    add_query(queries, "country_frequent", retail::query_kind::country, frequent_country, nothing);
    for (std::size_t i = 0; i < chosen.size(); ++i) {
        add_query(queries, "and_" + labels[i], retail::query_kind::term_and_country,
                  chosen[i], frequent_country);
    }
    for (std::size_t i = 0; i < chosen.size(); ++i) {
        for (std::size_t j = i + 1; j < chosen.size(); ++j) {
            add_query(queries, "or_" + labels[i] + "_" + labels[j], retail::query_kind::term_or_term,
                      chosen[i], chosen[j]);
        }
    }
    std::string missing = "zzzzmissingterm";
    while (index.term_postings().count(missing) != 0) {
        missing += 'z';
    }
    add_query(queries, "term_missing", retail::query_kind::term, {missing, 0}, nothing);
    return queries;
}

// escribe una fila por posting y una por contenedor, y devuelve los totales
space_totals write_postings(std::ostream& postings, std::ostream& containers, const std::string& name,
                            const posting_map& source, std::size_t row_count) {
    space_totals total;
    for (const auto& entry : source) {
        const bitmap& posting = entry.second;
        std::vector<roaring::container_stats> blocks = posting.statistics();
        roaring::memory_stats memory = posting.memory_usage();
        std::uint64_t count = posting.cardinality();
        std::uint64_t flat = 4 * count;
        std::uint64_t full = (row_count + 7) / 8;

        for (const roaring::container_stats& block : blocks) {
            field(containers, name);
            containers << ',';
            field(containers, entry.first);
            containers << ',' << block.key << ',' << block.cardinality << ','
                       << block.cardinality / 65536.0 << ','
                       << (block.is_bitmap ? "bitmap" : "array") << ',' << block.payload_bytes << '\n';
        }

        field(postings, name);
        postings << ',';
        field(postings, entry.first);
        postings << ',' << count << ',' << blocks.size() << ',' << memory.key_bytes << ','
                 << memory.container_payload_bytes << ',' << memory.metadata_bytes << ','
                 << memory.allocated_bytes << ',' << flat << ',' << full << '\n';

        total.postings += 1;
        total.cardinality += count;
        total.blocks += blocks.size();
        total.key_bytes += memory.key_bytes;
        total.payload_bytes += memory.container_payload_bytes;
        total.metadata_bytes += memory.metadata_bytes;
        total.allocated_bytes += memory.allocated_bytes;
        total.flat_bytes += flat;
        total.full_bitmap_bytes += full;
    }
    return total;
}

void write_total(std::ostream& space, const std::string& name, const space_totals& total) {
    space << name << ',' << total.postings << ',' << total.cardinality << ',' << total.blocks << ','
          << total.key_bytes << ',' << total.payload_bytes << ',' << total.metadata_bytes << ','
          << total.allocated_bytes << ',' << total.flat_bytes << ',' << total.full_bitmap_bytes << '\n';
}

void write_space(const settings& options, const retail::retail_index& index, std::size_t row_count) {
    std::ofstream postings = output_file(options, "postings.csv");
    std::ofstream containers = output_file(options, "containers.csv");
    std::ofstream space = output_file(options, "space.csv");
    postings << "namespace,key,cardinality,blocks,key_bytes,payload_bytes,metadata_bytes,"
                "allocated_bytes,flat32_bytes,full_bitmap_bytes\n";
    containers << "namespace,key,high_bits,cardinality,density,type,payload_bytes\n";
    space << "namespace,postings,total_cardinality,blocks,key_bytes,payload_bytes,metadata_bytes,"
             "allocated_bytes,flat32_bytes,full_bitmap_bytes\n";
    space_totals terms = write_postings(postings, containers, "term", index.term_postings(), row_count);
    space_totals countries = write_postings(postings, containers, "country", index.country_postings(), row_count);
    write_total(space, "term", terms);
    write_total(space, "country", countries);
}

// compara cada consulta con el scan y guarda sus ids correctos para las mediciones
std::vector<std::vector<std::uint32_t>> validate_queries(
    const settings& options, const std::vector<retail::normalized_row>& rows,
    const retail::retail_index& index, const std::vector<selected_query>& queries) {
    std::ofstream output = output_file(options, "queries.csv");
    output << "query_id,type,first,second,first_cardinality,second_cardinality,"
              "result_cardinality,result_blocks,array_blocks,bitmap_blocks,ids_equal\n";
    std::vector<std::vector<std::uint32_t>> expected;
    for (const selected_query& item : queries) {
        std::vector<std::uint32_t> scanned = retail::scan_rows(rows, item.request);
        bitmap indexed = index.execute(item.request);
        if (scanned != indexed.to_vector()) {
            throw std::runtime_error("full ID mismatch: " + item.id);
        }
        std::size_t array_blocks = 0;
        std::size_t bitmap_blocks = 0;
        for (const roaring::container_stats& block : indexed.statistics()) {
            if (block.is_bitmap) {
                ++bitmap_blocks;
            } else {
                ++array_blocks;
            }
        }
        field(output, item.id);
        output << ',' << kind_name(item.request.kind) << ',';
        field(output, item.request.first);
        output << ',';
        field(output, item.request.second);
        output << ',' << item.first_count << ',' << item.second_count << ',' << scanned.size() << ','
               << array_blocks + bitmap_blocks << ',' << array_blocks << ',' << bitmap_blocks << ",true\n";
        expected.push_back(scanned);
    }
    return expected;
}

// mide cada consulta con roaring y con scan, alternando cual va primero
void query_samples(const settings& options, const std::vector<retail::normalized_row>& rows,
                   const retail::retail_index& index, const std::vector<selected_query>& queries,
                   const std::vector<std::vector<std::uint32_t>>& expected) {
    std::ofstream output = output_file(options, "query_samples.csv");
    output << "query_id,method,repetition,order,elapsed_ns,cardinality,ids_equal\n";
    for (std::size_t q = 0; q < queries.size(); ++q) {
        for (unsigned iteration = 0; iteration < options.warmup + options.repetitions; ++iteration) {
            for (unsigned order = 0; order < 2; ++order) {
                bool use_index = (iteration + order + q) % 2 == 0;
                std::vector<std::uint32_t> ids;
                auto start = clock_type::now();
                if (use_index) {
                    ids = index.execute(queries[q].request).to_vector();
                } else {
                    ids = retail::scan_rows(rows, queries[q].request);
                }
                std::uint64_t duration = elapsed_ns(start);
                if (ids != expected[q]) {
                    throw std::runtime_error("query sample ID mismatch: " + queries[q].id);
                }
                // las primeras repeticiones son de calentamiento y no se guardan
                if (iteration < options.warmup) {
                    continue;
                }
                field(output, queries[q].id);
                output << ',' << (use_index ? "roaring" : "scan") << ',' << iteration - options.warmup
                       << ',' << order << ',' << duration << ',' << ids.size() << ",true\n";
            }
        }
    }
}

bitmap from_values(const std::vector<std::uint32_t>& values) {
    bitmap result;
    for (std::uint32_t value : values) {
        result.add(value);
    }
    return result;
}

std::vector<std::uint32_t> range_values(std::uint32_t first, std::uint32_t count, std::uint32_t step = 1) {
    std::vector<std::uint32_t> result;
    for (std::uint32_t i = 0; i < count; ++i) {
        result.push_back(first + i * step);
    }
    return result;
}

// resultado esperado calculado con la biblioteca estandar, sin roaring
std::vector<std::uint32_t> expected_result(const std::vector<std::uint32_t>& left,
                                           const std::vector<std::uint32_t>& right, bool intersect) {
    std::vector<std::uint32_t> result;
    if (intersect) {
        std::set_intersection(left.begin(), left.end(), right.begin(), right.end(), std::back_inserter(result));
    } else {
        std::set_union(left.begin(), left.end(), right.begin(), right.end(), std::back_inserter(result));
    }
    return result;
}

void record_synthetic(std::ostream& output, const std::string& scenario, const std::string& operation,
                      unsigned repetition, std::uint64_t duration, const bitmap& result,
                      const std::vector<std::uint32_t>& expected) {
    if (result.to_vector() != expected) {
        throw std::runtime_error("synthetic ID mismatch: " + scenario + "/" + operation);
    }
    std::vector<roaring::container_stats> blocks = result.statistics();
    std::size_t arrays = 0;
    std::size_t bitmaps = 0;
    for (const roaring::container_stats& block : blocks) {
        if (block.is_bitmap) {
            ++bitmaps;
        } else {
            ++arrays;
        }
    }
    field(output, scenario);
    output << ',' << operation << ',' << repetition << ',' << duration << ',' << result.cardinality()
           << ',' << blocks.size() << ',' << arrays << ',' << bitmaps << ','
           << result.memory_usage().container_payload_bytes << ",true\n";
}

struct pair_case {
    std::string name;
    std::vector<std::uint32_t> left;
    std::vector<std::uint32_t> right;
};

// casos controlados: umbral 4096, dispersion entre bloques y los pares de contenedores
void synthetic_samples(const settings& options) {
    std::ofstream output = output_file(options, "synthetic.csv");
    output << "scenario,operation,repetition,elapsed_ns,cardinality,blocks,array_blocks,"
              "bitmap_blocks,payload_bytes,ids_equal\n";
    std::mt19937 random(options.seed);

    std::vector<std::uint32_t> sizes = {1, 16, 256, 4095, 4096, 4097, 8192, 65536};
    for (std::uint32_t count : sizes) {
        std::vector<std::uint32_t> values = range_values(0, count);
        std::vector<std::uint32_t> shuffled = values;
        std::shuffle(shuffled.begin(), shuffled.end(), random);
        for (unsigned repetition = 0; repetition < options.repetitions; ++repetition) {
            auto start = clock_type::now();
            bitmap result = from_values(shuffled);
            std::uint64_t duration = elapsed_ns(start);
            record_synthetic(output, "single_block_" + std::to_string(count), "build",
                             repetition, duration, result, values);
        }
    }

    // 8192 ids, uno en cada bloque distinto
    std::vector<std::uint32_t> dispersed = range_values(0, 8192, 65537);
    std::vector<std::uint32_t> shuffled = dispersed;
    std::shuffle(shuffled.begin(), shuffled.end(), random);
    for (unsigned repetition = 0; repetition < options.repetitions; ++repetition) {
        auto start = clock_type::now();
        bitmap result = from_values(shuffled);
        std::uint64_t duration = elapsed_ns(start);
        record_synthetic(output, "dispersed_8192_blocks", "build", repetition, duration, result, dispersed);
    }

    // la insercion numero 4097 convierte el array en bitmap
    bitmap before = from_values(range_values(0, 4096));
    std::vector<std::uint32_t> after = range_values(0, 4097);
    for (unsigned repetition = 0; repetition < options.repetitions; ++repetition) {
        bitmap result = before;
        auto start = clock_type::now();
        result.add(4096);
        std::uint64_t duration = elapsed_ns(start);
        record_synthetic(output, "threshold_4096_to_4097", "insert", repetition, duration, result, after);
    }

    std::vector<pair_case> cases = {
        {"array_array_overlap", range_values(0, 3000), range_values(2000, 3000)},
        {"array_bitmap_overlap", range_values(3000, 3000), range_values(0, 5000)},
        {"bitmap_array_overlap", range_values(0, 5000), range_values(3000, 3000)},
        {"bitmap_bitmap_to_array", range_values(0, 5000), range_values(4000, 5000)},
        {"bitmap_bitmap_dense", range_values(0, 8192), range_values(1024, 8192)},
        {"bitmap_bitmap_empty", range_values(0, 5000), range_values(10000, 5000)},
        {"different_high_keys", range_values(0, 256), range_values(65536, 256)}
    };
    for (const pair_case& item : cases) {
        bitmap left = from_values(item.left);
        bitmap right = from_values(item.right);
        std::vector<std::uint32_t> expected_and = expected_result(item.left, item.right, true);
        std::vector<std::uint32_t> expected_or = expected_result(item.left, item.right, false);
        for (unsigned repetition = 0; repetition < options.repetitions; ++repetition) {
            auto start = clock_type::now();
            bitmap result = left.intersection(right);
            std::uint64_t duration = elapsed_ns(start);
            record_synthetic(output, item.name, "and", repetition, duration, result, expected_and);
        }
        for (unsigned repetition = 0; repetition < options.repetitions; ++repetition) {
            auto start = clock_type::now();
            bitmap result = left.union_with(right);
            std::uint64_t duration = elapsed_ns(start);
            record_synthetic(output, item.name, "or", repetition, duration, result, expected_or);
        }
    }
}

}

int main(int argc, char** argv) {
    try {
        settings options = read_settings(argc, argv);
        std::vector<retail::retail_row> rows = retail::load_csv(options.data_path);
        std::vector<retail::normalized_row> normalized = retail::normalize_rows(rows);
        if (rows.empty()) {
            throw std::invalid_argument("benchmark dataset is empty");
        }

        // la construccion se mide aparte de la lectura y normalizacion del csv
        std::ofstream builds = output_file(options, "build_samples.csv");
        builds << "repetition,elapsed_ns,rows,term_postings,country_postings\n";
        for (unsigned repetition = 0; repetition < options.build_repetitions; ++repetition) {
            auto start = clock_type::now();
            retail::retail_index built(normalized);
            std::uint64_t duration = elapsed_ns(start);
            builds << repetition << ',' << duration << ',' << rows.size() << ','
                   << built.term_postings().size() << ',' << built.country_postings().size() << '\n';
        }

        retail::retail_index index(normalized);
        std::vector<selected_query> queries = select_queries(index);
        std::vector<std::vector<std::uint32_t>> expected = validate_queries(options, normalized, index, queries);
        write_space(options, index, rows.size());
        query_samples(options, normalized, index, queries, expected);
        synthetic_samples(options);
        std::cout << "benchmark passed: rows=" << rows.size() << "; queries=" << queries.size()
                  << "; repetitions=" << options.repetitions << "; output=" << options.output_path << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "retail_benchmark: " << error.what() << '\n';
        return 1;
    }
}

#include "retail_index.hpp"

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

// escribe un campo entre comillas, duplicando las comillas internas
void write_field(const std::string& value) {
    std::cout << '"';
    for (char character : value) {
        if (character == '"') {
            std::cout << '"';
        }
        std::cout << character;
    }
    std::cout << '"';
}

std::size_t parse_limit(const std::string& value) {
    if (value.empty() || value.find_first_not_of("0123456789") != std::string::npos) {
        throw std::invalid_argument("limit must be a nonnegative integer");
    }
    return std::stoul(value);
}

retail::query_kind parse_kind(const std::string& value) {
    if (value == "term") {
        return retail::query_kind::term;
    }
    if (value == "country") {
        return retail::query_kind::country;
    }
    if (value == "and") {
        return retail::query_kind::term_and_country;
    }
    if (value == "or") {
        return retail::query_kind::term_or_term;
    }
    throw std::invalid_argument("type must be term, country, and or or");
}

}

int main(int argc, char** argv) {
    try {
        std::string data_path;
        std::string type;
        std::string first;
        std::string second;
        bool has_first = false;
        bool has_second = false;
        bool verify = false;
        std::size_t limit = 10;
        for (int position = 1; position < argc; ++position) {
            std::string option = argv[position];
            if (option == "--help") {
                std::cout << "retail_cli --data CSV --type term|country|and|or --first VALUE "
                             "[--second VALUE] [--limit N] [--verify]\n";
                return 0;
            }
            if (option == "--verify") {
                verify = true;
                continue;
            }
            if (position + 1 >= argc) {
                throw std::invalid_argument("missing value for " + option);
            }
            std::string value = argv[position + 1];
            ++position;
            if (option == "--data") {
                data_path = value;
            } else if (option == "--type") {
                type = value;
            } else if (option == "--first") {
                first = value;
                has_first = true;
            } else if (option == "--second") {
                second = value;
                has_second = true;
            } else if (option == "--limit") {
                limit = parse_limit(value);
            } else {
                throw std::invalid_argument("unknown option: " + option);
            }
        }
        if (data_path.empty() || type.empty() || !has_first) {
            throw std::invalid_argument("required: --data, --type, --first; use --help");
        }
        retail::query_kind kind = parse_kind(type);
        bool needs_second = kind == retail::query_kind::term_and_country ||
                            kind == retail::query_kind::term_or_term;
        if (needs_second != has_second) {
            throw std::invalid_argument("--second is required only for and/or");
        }

        std::vector<retail::retail_row> rows = retail::load_csv(data_path);
        std::vector<retail::normalized_row> normalized = retail::normalize_rows(rows);
        retail::retail_index index(normalized);
        retail::query request{kind, first, second};
        std::vector<std::uint32_t> ids = index.execute(request).to_vector();

        // --verify compara todos los ids con el recorrido completo
        if (verify && ids != retail::scan_rows(normalized, request)) {
            throw std::runtime_error("index/scan ID mismatch");
        }

        std::cout << "row_id,invoice_no,description,country\n";
        std::size_t shown = std::min(limit, ids.size());
        for (std::size_t position = 0; position < shown; ++position) {
            const retail::retail_row& row = rows[ids[position]];
            std::cout << row.row_id << ',';
            write_field(row.invoice_no);
            std::cout << ',';
            write_field(row.description);
            std::cout << ',';
            write_field(row.country);
            std::cout << '\n';
        }
        std::cerr << "matches=" << ids.size() << "; showing=" << shown
                  << "; verified_scan=" << (verify ? "true" : "false") << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "retail_cli: " << error.what() << '\n';
        return 1;
    }
}

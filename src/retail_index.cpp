#include "retail_index.hpp"

#include <algorithm>
#include <fstream>
#include <stdexcept>
// #include <iostream>  // nota: descomentar para usar los prints 

namespace retail {
namespace {

bool ascii_space(char value) {
    return value == ' ' || value == '\t' || value == '\r' || value == '\n' ||
           value == '\f' || value == '\v';
}

char ascii_lower(char value) {
    if (value >= 'A' && value <= 'Z') {
        return static_cast<char>(value + ('a' - 'A'));
    }
    return value;
}

bool token_character(char value) {
    return (value >= 'a' && value <= 'z') || (value >= '0' && value <= '9');
}

// lee un registro del csv; dentro de comillas se permiten comas y saltos de linea
bool read_record(std::istream& input, std::vector<std::string>& fields) {
    fields.clear();
    if (input.peek() == std::istream::traits_type::eof()) {
        return false;
    }
    std::string field;
    bool quoted = false;
    char value;
    while (input.get(value)) {
        if (quoted) {
            if (value == '"' && input.peek() == '"') {
                input.get();
                field += '"';
            } else if (value == '"') {
                quoted = false;
            } else {
                field += value;
            }
        } else if (value == '"') {
            quoted = true;
        } else if (value == ',') {
            fields.push_back(field);
            field.clear();
        } else if (value == '\n') {
            break;
        } else if (value != '\r') {
            field += value;
        }
    }
    fields.push_back(field);
    return true;
}

std::string normalize_term(const std::string& value) {
    std::vector<std::string> terms = description_terms(value);
    if (terms.size() != 1) {
        throw std::invalid_argument("term query must contain exactly one ASCII token");
    }
    return terms[0];
}

query normalize_query(const query& request) {
    query result = request;
    if (request.kind == query_kind::country) {
        result.first = normalize_country(request.first);
    } else {
        result.first = normalize_term(request.first);
    }
    if (request.kind == query_kind::term_and_country) {
        result.second = normalize_country(request.second);
    } else if (request.kind == query_kind::term_or_term) {
        result.second = normalize_term(request.second);
    } else {
        result.second = "";
    }
    return result;
}

// si la clave no existe se devuelve un bitmap vacio
const roaring::roaring_bitmap& find_posting(
    const std::map<std::string, roaring::roaring_bitmap>& postings, const std::string& key) {
    static const roaring::roaring_bitmap empty;
    auto found = postings.find(key);
    if (found == postings.end()) {
        return empty;
    }
    return found->second;
}

bool has_term(const normalized_row& row, const std::string& term) {
    return std::binary_search(row.terms.begin(), row.terms.end(), term);
}

}

std::vector<retail_row> load_csv(std::istream& input) {
    std::vector<std::string> fields;
    std::vector<std::string> header{"row_id", "invoice_no", "description", "country"};
    if (!read_record(input, fields) || fields != header) {
        throw std::runtime_error("csv: expected header row_id,invoice_no,description,country");
    }
    std::vector<retail_row> rows;
    while (read_record(input, fields)) {
        // el id de cada fila debe ser su posicion en el archivo
        if (fields.size() != 4 || std::stoul(fields[0]) != rows.size()) {
            throw std::runtime_error("csv: invalid record " + std::to_string(rows.size() + 2));
        }
        std::uint32_t row_id = rows.size();
        rows.push_back({row_id, fields[1], fields[2], fields[3]});
    }
    return rows;
}

std::vector<retail_row> load_csv(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("cannot open csv: " + path);
    }
    return load_csv(input);
}

// separa la descripcion en palabras [a-z0-9]+, ordenadas y sin repetir
std::vector<std::string> description_terms(const std::string& description) {
    std::vector<std::string> terms;
    std::string term;
    for (char value : description) {
        value = ascii_lower(value);
        if (token_character(value)) {
            term += value;
        } else if (!term.empty()) {
            terms.push_back(term);
            term.clear();
        }
    }
    if (!term.empty()) {
        terms.push_back(term);
    }
    std::sort(terms.begin(), terms.end());
    terms.erase(std::unique(terms.begin(), terms.end()), terms.end());
    return terms;
}

// minusculas y un solo espacio entre palabras
std::string normalize_country(const std::string& country) {
    std::string result;
    bool pending_space = false;
    for (char value : country) {
        if (ascii_space(value)) {
            pending_space = !result.empty();
        } else {
            if (pending_space) {
                result += ' ';
            }
            result += ascii_lower(value);
            pending_space = false;
        }
    }
    return result;
}

std::vector<normalized_row> normalize_rows(const std::vector<retail_row>& rows) {
    std::vector<normalized_row> normalized;
    for (const retail_row& row : rows) {
        normalized.push_back({row.row_id, description_terms(row.description), normalize_country(row.country)});
    }
    return normalized;
}

retail_index::retail_index(const std::vector<normalized_row>& rows) {
    for (const normalized_row& row : rows) {
        // cada termino y cada pais guardan en su bitmap el id de la fila
        for (const std::string& term : row.terms) {
            term_postings_[term].add(row.row_id);
        }
        country_postings_[row.country].add(row.row_id);
    }
}

// and = interseccion de postings, or = union de postings
roaring::roaring_bitmap retail_index::execute(const query& request) const {
    query normalized = normalize_query(request);
    // std::cout << "consulta: " << normalized.first << " " << normalized.second << '\n';
    if (normalized.kind == query_kind::term) {
        return find_posting(term_postings_, normalized.first);
    }
    if (normalized.kind == query_kind::country) {
        return find_posting(country_postings_, normalized.first);
    }
    const roaring::roaring_bitmap& first = find_posting(term_postings_, normalized.first);
    if (normalized.kind == query_kind::term_and_country) {
        return first.intersection(find_posting(country_postings_, normalized.second));
    }
    return first.union_with(find_posting(term_postings_, normalized.second));
}

const std::map<std::string, roaring::roaring_bitmap>& retail_index::term_postings() const {
    return term_postings_;
}

const std::map<std::string, roaring::roaring_bitmap>& retail_index::country_postings() const {
    return country_postings_;
}

// referencia sin indice: revisa todas las filas una por una
std::vector<std::uint32_t> scan_rows(const std::vector<normalized_row>& rows, const query& request) {
    query normalized = normalize_query(request);
    std::vector<std::uint32_t> result;
    for (const normalized_row& row : rows) {
        bool matches = false;
        if (normalized.kind == query_kind::term) {
            matches = has_term(row, normalized.first);
        } else if (normalized.kind == query_kind::country) {
            matches = row.country == normalized.first;
        } else if (normalized.kind == query_kind::term_and_country) {
            matches = has_term(row, normalized.first) && row.country == normalized.second;
        } else {
            matches = has_term(row, normalized.first) || has_term(row, normalized.second);
        }
        if (matches) {
            result.push_back(row.row_id);
        }
    }
    return result;
}

}

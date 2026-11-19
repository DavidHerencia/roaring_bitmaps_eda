#ifndef RETAIL_INDEX_HPP
#define RETAIL_INDEX_HPP

#include "roaring_bitmap.hpp"

#include <cstdint>
#include <istream>
#include <map>
#include <string>
#include <vector>

namespace retail {

struct retail_row {
    std::uint32_t row_id;
    std::string invoice_no;
    std::string description;
    std::string country;
};

struct normalized_row {
    std::uint32_t row_id;
    std::vector<std::string> terms;
    std::string country;
};

std::vector<normalized_row> normalize_rows(const std::vector<retail_row>& rows);
std::vector<retail_row> load_csv(std::istream& input);
std::vector<retail_row> load_csv(const std::string& path);
std::vector<std::string> description_terms(const std::string& description);
std::string normalize_country(const std::string& country);

enum class query_kind { term, country, term_and_country, term_or_term };

struct query {
    query_kind kind;
    std::string first;
    std::string second;
};

class retail_index {
public:
    explicit retail_index(const std::vector<normalized_row>& rows);
    roaring::roaring_bitmap execute(const query& request) const;
    const std::map<std::string, roaring::roaring_bitmap>& term_postings() const;
    const std::map<std::string, roaring::roaring_bitmap>& country_postings() const;

private:
    std::map<std::string, roaring::roaring_bitmap> term_postings_;
    std::map<std::string, roaring::roaring_bitmap> country_postings_;
};

std::vector<std::uint32_t> scan_rows(const std::vector<normalized_row>& rows,
                                     const query& request);

}

#endif

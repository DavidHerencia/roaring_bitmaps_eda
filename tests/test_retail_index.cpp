#include "retail_index.hpp"

#include <filesystem>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

template <typename function_type>
void expect_error(function_type action, const std::string& message) {
    bool caught = false;
    try {
        action();
    } catch (const std::exception&) {
        caught = true;
    }
    require(caught, message);
}

void check_query(const std::vector<retail::normalized_row>& rows,
                 const retail::retail_index& index, const retail::query& request,
                 const std::vector<std::uint32_t>& expected) {
    auto indexed = index.execute(request).to_vector();
    auto scanned = retail::scan_rows(rows, request);
    require(indexed == expected, "indexed IDs differ from expected IDs");
    require(scanned == expected, "scanned IDs differ from expected IDs");
}

void test_csv() {
    std::istringstream input("row_id,invoice_no,description,country\r\n0,C42,\"a,\"\"b\"\"\nline\",UK\r\n1,42,,");
    auto rows = retail::load_csv(input);
    require(rows.size() == 2, "CSV must preserve both rows");
    require(rows[0].description == "a,\"b\"\nline", "CSV quoting or multiline field lost");
    require(rows[0].invoice_no == "C42", "cancelled invoice must be preserved");
    require(rows[1].description.empty() && rows[1].country.empty(), "empty values must be preserved");
    const std::string header = "row_id,invoice_no,description,country\n";
    for (const auto& record : {"0,a,b", "1,a,b,c", "0,a,b,c\n0,d,e,f", "-1,a,b,c",
                               "4294967296,a,b,c", ",a,b,c", "x,a,b,c", "0,a,\"open,c"}) {
        expect_error([&] { std::istringstream invalid(header + record); retail::load_csv(invalid); },
                     "malformed CSV must fail");
    }
    expect_error([] { std::istringstream invalid(""); retail::load_csv(invalid); }, "missing header must fail");
    expect_error([] { std::istringstream invalid("a,b,c,d\n"); retail::load_csv(invalid); }, "wrong header must fail");
    expect_error([] { retail::load_csv("file_that_does_not_exist.csv"); }, "missing file must fail");
    std::istringstream empty(header);
    require(retail::load_csv(empty).empty(), "header-only CSV should be allowed");
}

void test_index(const std::string& fixture_path) {
    auto raw_rows = retail::load_csv(fixture_path);
    require(raw_rows.size() == 8, "fixture must contain eight rows including duplicates");
    require(raw_rows[0].description == raw_rows[6].description, "fixture duplicate missing");
    require(raw_rows[3].description == "white\nlight", "multiline fixture missing");
    require(retail::description_terms("WHITE,white a1 B-2 caf\xc3\xa9") ==
            std::vector<std::string>({"2", "a1", "b", "caf", "white"}), "ASCII tokenization mismatch");
    require(retail::normalize_country(" \tUNITED  Kingdom\r\n") == "united kingdom", "country normalization mismatch");
    auto rows = retail::normalize_rows(raw_rows);
    retail::retail_index index(rows);
    using retail::query_kind;
    check_query(rows, index, {query_kind::term, "WHITE", ""}, {0, 3, 6});
    check_query(rows, index, {query_kind::country, "  United\tKingdom ", ""}, {0, 1, 4, 6});
    check_query(rows, index, {query_kind::term_and_country, "heart", "United Kingdom"}, {0, 1, 6});
    check_query(rows, index, {query_kind::term_or_term, "white", "RED"}, {0, 1, 3, 4, 6});
    check_query(rows, index, {query_kind::term_or_term, "heart", "heart"}, {0, 1, 6});
    check_query(rows, index, {query_kind::term_and_country, "heart", "Germany"}, {});
    check_query(rows, index, {query_kind::term, "absent", ""}, {});
    check_query(rows, index, {query_kind::country, "absent", ""}, {});
    check_query(rows, index, {query_kind::term_or_term, "absent", "nonexistent"}, {});
    check_query(rows, index, {query_kind::country, "", ""}, {5});
    check_query(rows, index, {query_kind::country, "france", ""}, {2, 3});
    check_query(rows, index, {query_kind::term, "france", ""}, {4});
    require(index.term_postings().at("white").cardinality() == 3, "repeated token must add row only once");
    require(index.country_postings().at("france").cardinality() == 2, "missing description must not remove row");
    for (const auto& term : {"", "!!!", "white heart"}) {
        expect_error([&] { index.execute({query_kind::term, term, ""}); }, "invalid term must fail index");
        expect_error([&] { retail::scan_rows(rows, {query_kind::term, term, ""}); }, "invalid term must fail scan");
    }
    std::vector<retail::normalized_row> empty;
    retail::retail_index empty_index(empty);
    check_query(empty, empty_index, {query_kind::term, "white", ""}, {});
}

void test_real_data(const std::string& path) {
    auto raw_rows = retail::load_csv(path);
    require(raw_rows.size() == 541909, "official dataset must contain 541909 rows");
    std::size_t missing_descriptions = 0;
    std::size_t cancellations = 0;
    for (const auto& row : raw_rows) {
        missing_descriptions += row.description.empty();
        cancellations += !row.invoice_no.empty() && (row.invoice_no.front() == 'C' || row.invoice_no.front() == 'c');
    }
    require(missing_descriptions == 1454, "missing descriptions must be preserved");
    require(cancellations == 9288, "cancellations must be preserved");
    auto rows = retail::normalize_rows(raw_rows);
    retail::retail_index index(rows);
    using retail::query_kind;
    const std::vector<retail::query> requests = {
        {query_kind::term, "white", ""}, {query_kind::term, "heart", ""},
        {query_kind::country, "United Kingdom", ""}, {query_kind::country, "France", ""},
        {query_kind::term_and_country, "white", "United Kingdom"},
        {query_kind::term_and_country, "heart", "France"},
        {query_kind::term_or_term, "white", "red"},
        {query_kind::term_or_term, "heart", "heart"},
        {query_kind::term, "termnotpresent123456", ""},
        {query_kind::term_and_country, "white", "countrynotpresent123456"},
        {query_kind::term_or_term, "termnotpresent123456", "otherabsent123456"}
    };
    for (const auto& request : requests) {
        auto expected = retail::scan_rows(rows, request);
        check_query(rows, index, request, expected);
    }
    std::cout << "real dataset: " << rows.size() << " rows, " << missing_descriptions
              << " empty descriptions, " << cancellations << " cancellations, "
              << index.term_postings().size() << " terms, " << index.country_postings().size()
              << " countries; 11 complete ID-set comparisons passed\n";
}

}

int main(int argc, char** argv) {
    try {
        auto fixture_path = std::filesystem::path(__FILE__).parent_path() / "fixtures/retail.csv";
        test_csv();
        test_index(fixture_path.string());
        std::cout << "retail CSV, normalization, index and independent scan tests passed\n";
        if (argc == 2) {
            test_real_data(argv[1]);
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "retail test failure: " << error.what() << '\n';
        return 1;
    }
}

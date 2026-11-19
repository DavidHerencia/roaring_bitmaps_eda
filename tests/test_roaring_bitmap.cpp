#include "roaring_bitmap.hpp"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <iterator>
#include <limits>
#include <random>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using roaring::roaring_bitmap;
using value_set = std::set<std::uint32_t>;
std::size_t checks = 0;

void require(bool condition, const std::string& message) {
    ++checks;
    if (!condition) {
        throw std::runtime_error(message);
    }
}

roaring_bitmap make_bitmap(const value_set& values) {
    roaring_bitmap result;
    for (std::uint32_t value : values) {
        result.add(value);
    }
    return result;
}

value_set range(std::uint32_t first, std::uint32_t end, std::uint32_t prefix = 0) {
    value_set result;
    for (std::uint32_t value = first; value < end; ++value) {
        result.insert(prefix | value);
    }
    return result;
}

void check_bitmap(const roaring_bitmap& bitmap, const value_set& expected) {
    std::vector<std::uint32_t> expected_vector(expected.begin(), expected.end());
    require(bitmap.to_vector() == expected_vector, "sorted values differ");
    require(bitmap.cardinality() == expected.size(), "cardinality differs");
    for (std::uint32_t value : expected) {
        require(bitmap.contains(value), "stored value not found");
    }
    auto statistics = bitmap.statistics();
    std::uint64_t count = 0;
    std::size_t payload = 0;
    for (std::size_t i = 0; i < statistics.size(); ++i) {
        const auto& entry = statistics[i];
        require(entry.cardinality > 0 && entry.cardinality <= 65536, "invalid container count");
        require(entry.is_bitmap == (entry.cardinality > 4096), "container type differs");
        require(entry.payload_bytes == (entry.is_bitmap ? 8192 : entry.cardinality * 2),
            "container payload differs");
        if (i > 0) {
            require(statistics[i - 1].key < entry.key, "keys are not strictly ordered");
        }
        count += entry.cardinality;
        payload += entry.payload_bytes;
    }
    require(count == expected.size(), "statistics cardinality differs");
    auto memory = bitmap.memory_usage();
    require(memory.key_bytes == statistics.size() * sizeof(std::uint16_t), "key memory differs");
    require(memory.container_payload_bytes == payload, "payload memory differs");
    require(memory.metadata_bytes >= sizeof(roaring_bitmap), "metadata omits object");
    require(memory.allocated_bytes >= memory.key_bytes + memory.container_payload_bytes
        + memory.metadata_bytes, "allocated memory is smaller than logical memory");
}

void check_operations(const value_set& left_values, const value_set& right_values) {
    roaring_bitmap left = make_bitmap(left_values);
    roaring_bitmap right = make_bitmap(right_values);
    value_set expected_intersection;
    value_set expected_union;
    std::set_intersection(left_values.begin(), left_values.end(), right_values.begin(),
        right_values.end(), std::inserter(expected_intersection, expected_intersection.end()));
    std::set_union(left_values.begin(), left_values.end(), right_values.begin(),
        right_values.end(), std::inserter(expected_union, expected_union.end()));
    roaring_bitmap intersection = left.intersection(right);
    roaring_bitmap united = left.union_with(right);
    check_bitmap(intersection, expected_intersection);
    check_bitmap(united, expected_union);
    check_bitmap(right.intersection(left), expected_intersection);
    check_bitmap(right.union_with(left), expected_union);
    check_bitmap(left.intersection(left), left_values);
    check_bitmap(left.union_with(left), left_values);
    check_bitmap(left, left_values);
    check_bitmap(right, right_values);
    intersection.add(123456789);
    united.add(987654321);
    check_bitmap(left, left_values);
    check_bitmap(right, right_values);
}

void test_empty_and_boundaries() {
    roaring_bitmap empty;
    check_bitmap(empty, {});
    require(!empty.contains(0), "empty contains zero");
    require(!empty.contains(std::numeric_limits<std::uint32_t>::max()), "empty contains max");
    check_operations({}, {});
    value_set values = {0, 1, 63, 64, 65, 65535, 65536, 65537, 131071, 131072,
        std::numeric_limits<std::uint32_t>::max()};
    roaring_bitmap bitmap;
    for (auto it = values.rbegin(); it != values.rend(); ++it) {
        bitmap.add(*it);
        bitmap.add(*it);
    }
    check_bitmap(bitmap, values);
    require(!bitmap.contains(2), "unexpected array value");
    require(!bitmap.contains(196608), "unexpected high key");
    check_operations({}, values);
    check_operations({0, 65536, 131072}, {65535, 131071, 196607});
    roaring_bitmap copy = bitmap;
    copy.add(42);
    check_bitmap(bitmap, values);
    values.insert(42);
    check_bitmap(copy, values);
}

void test_threshold_and_dense() {
    roaring_bitmap bitmap;
    value_set expected;
    for (std::uint32_t value = 0; value < 65536; ++value) {
        bitmap.add(value);
        expected.insert(value);
        if (value == 4094 || value == 4095 || value == 4096 || value == 65535) {
            check_bitmap(bitmap, expected);
            auto entry = bitmap.statistics().front();
            require(entry.is_bitmap == (value >= 4096), "threshold transition differs");
        }
    }
    bitmap.add(0);
    bitmap.add(65535);
    check_bitmap(bitmap, expected);
    auto full_high = range(0, 65536, 0xffff0000U);
    roaring_bitmap high_bitmap = make_bitmap(full_high);
    check_bitmap(high_bitmap, full_high);
    require(high_bitmap.contains(std::numeric_limits<std::uint32_t>::max()), "dense max missing");
    require(!high_bitmap.contains(0), "dense block leaked into another key");
    roaring_bitmap partial = make_bitmap(range(1, 5001));
    require(!partial.contains(0) && !partial.contains(5001), "bitmap contains missing bit");
}

void test_container_pairs() {
    check_operations(range(0, 20), range(10, 30));
    check_operations(range(0, 4096), range(0, 4096));
    check_operations(range(0, 2048), range(2048, 4096));
    check_operations(range(0, 2048), range(2048, 4097));
    check_operations(range(0, 4096), range(4096, 8192));
    check_operations(range(1000, 3000), range(2000, 9000));
    check_operations(range(0, 2000), range(5000, 12000));
    check_operations(range(0, 6000), range(1904, 7904));
    check_operations(range(0, 6000), range(1903, 7903));
    check_operations(range(0, 6000), range(1905, 7905));
    check_operations(range(0, 6000), range(10000, 16000));
    check_operations(range(0, 6000), range(1000, 7000));
    check_operations(range(0, 65536), range(0, 65536));
    value_set mixed_left = range(0, 5000);
    value_set mixed_right = range(2000, 8000);
    auto sparse = range(0, 100, 0x00010000U);
    mixed_left.insert(sparse.begin(), sparse.end());
    sparse = range(50, 150, 0x00010000U);
    mixed_right.insert(sparse.begin(), sparse.end());
    sparse = range(0, 5000, 0x00030000U);
    mixed_left.insert(sparse.begin(), sparse.end());
    sparse = range(0, 20, 0xffff0000U);
    mixed_right.insert(sparse.begin(), sparse.end());
    check_operations(mixed_left, mixed_right);
}

void test_randomized() {
    std::mt19937 generator(20261115);
    for (int trial = 0; trial < 24; ++trial) {
        value_set left_values;
        value_set right_values;
        roaring_bitmap inserted;
        for (int i = 0; i < 12000; ++i) {
            std::uint32_t value = generator();
            if (trial % 3 == 0) {
                value %= 18000;
            } else if (trial % 3 == 1) {
                value = (generator() % 4) * 65536 + value % 9000;
            }
            if (i % 2 == 0) {
                inserted.add(value);
                left_values.insert(value);
            } else {
                right_values.insert(value);
            }
        }
        check_bitmap(inserted, left_values);
        for (int i = 0; i < 500; ++i) {
            std::uint32_t value = generator();
            require(inserted.contains(value) == (left_values.count(value) != 0),
                "random membership differs");
        }
        check_operations(left_values, right_values);
    }
    for (std::uint32_t seed : {7U, 42U, 123456U}) {
        std::mt19937 dense_generator(seed);
        value_set left;
        value_set right;
        for (int i = 0; i < 15000; ++i) {
            left.insert(dense_generator() % 65536);
            right.insert(dense_generator() % 65536);
        }
        check_operations(left, right);
    }
}

}

int main() {
    try {
        test_empty_and_boundaries();
        test_threshold_and_dense();
        test_container_pairs();
        test_randomized();
        std::cout << "roaring bitmap tests passed: " << checks << " checks\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "test failure: " << error.what() << '\n';
        return 1;
    }
}

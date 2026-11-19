#ifndef ROARING_BITMAP_HPP
#define ROARING_BITMAP_HPP

#include <cstddef>
#include <cstdint>
#include <vector>

namespace roaring {

struct container_stats {
    std::uint16_t key;
    std::uint32_t cardinality;
    bool is_bitmap;
    std::size_t payload_bytes;
};

struct memory_stats {
    std::size_t key_bytes;
    std::size_t container_payload_bytes;
    std::size_t metadata_bytes;
    std::size_t allocated_bytes;
};

class roaring_bitmap {
public:
    void add(std::uint32_t value);
    bool contains(std::uint32_t value) const;
    std::uint64_t cardinality() const;
    std::vector<std::uint32_t> to_vector() const;
    roaring_bitmap intersection(const roaring_bitmap& other) const;
    roaring_bitmap union_with(const roaring_bitmap& other) const;
    std::vector<container_stats> statistics() const;
    memory_stats memory_usage() const;

private:
    static constexpr std::uint32_t array_limit = 4096;
    static constexpr std::size_t bitmap_words = 1024;

    struct container {
        std::uint16_t key = 0;
        std::uint32_t count = 0;
        std::vector<std::uint16_t> values;
        std::vector<std::uint64_t> words;

        bool is_bitmap() const;
        std::size_t payload_bytes() const;
        bool contains(std::uint16_t value) const;
        void add(std::uint16_t value);
        void normalize();
        void make_bitmap();
        std::vector<std::uint16_t> to_array() const;
    };

    std::size_t find_position(std::uint16_t key) const;
    static container intersect_containers(const container& left, const container& right);
    static container unite_containers(const container& left, const container& right);
    std::vector<container> containers;
};

}

#endif

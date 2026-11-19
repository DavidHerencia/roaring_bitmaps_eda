#include "roaring_bitmap.hpp"

#include <algorithm>
#include <iterator>
#include <utility>
// #include <iostream>  // nota: descomentar solo para usar los prints 

namespace roaring {

bool roaring_bitmap::container::is_bitmap() const {
    return !words.empty();
}

std::size_t roaring_bitmap::container::payload_bytes() const {
    if (is_bitmap()) {
        return bitmap_words * sizeof(std::uint64_t);
    }
    return values.size() * sizeof(std::uint16_t);
}

bool roaring_bitmap::container::contains(std::uint16_t value) const {
    // bitmap: lectura directa del bit; array: busqueda binaria
    if (is_bitmap()) {
        return (words[value / 64] & (std::uint64_t{1} << (value % 64))) != 0;
    }
    return std::binary_search(values.begin(), values.end(), value);
}

// convierte un array en bitmap encendiendo el bit de cada valor
void roaring_bitmap::container::make_bitmap() {
    // std::cout << "array -> bitmap, clave " << key << ", valores " << count << '\n';
    std::vector<std::uint64_t> new_words(bitmap_words, 0);
    for (std::uint16_t value : values) {
        new_words[value / 64] |= std::uint64_t{1} << (value % 64);
    }
    words.swap(new_words);
    std::vector<std::uint16_t>().swap(values);
}

void roaring_bitmap::container::add(std::uint16_t value) {
    if (is_bitmap()) {
        std::uint64_t mask = std::uint64_t{1} << (value % 64);
        if ((words[value / 64] & mask) == 0) {
            words[value / 64] |= mask;
            ++count;
        }
        return;
    }
    auto position = std::lower_bound(values.begin(), values.end(), value);
    if (position == values.end() || *position != value) {
        values.insert(position, value);
        ++count;
        // al pasar de 4096 valores el bitmap ocupa menos que el array
        if (count > array_limit) {
            make_bitmap();
        }
    }
}

std::vector<std::uint16_t> roaring_bitmap::container::to_array() const {
    if (!is_bitmap()) {
        return values;
    }
    std::vector<std::uint16_t> result;
    result.reserve(count);
    for (std::size_t i = 0; i < bitmap_words; ++i) {
        std::uint64_t word = words[i];
        while (word != 0) {
            // posicion del bit encendido mas bajo de la palabra
            int bit = __builtin_ctzll(word);
            result.push_back(static_cast<std::uint16_t>(i * 64 + bit));
            word &= word - 1;
        }
    }
    return result;
}

// mantiene la invariante: c <= 4096 es array y c > 4096 es bitmap
void roaring_bitmap::container::normalize() {
    if (is_bitmap() && count <= array_limit) {
        values = to_array();
        std::vector<std::uint64_t>().swap(words);
    } else if (!is_bitmap() && count > array_limit) {
        make_bitmap();
    }
}

// busqueda binaria: posicion del primer contenedor con clave >= key
std::size_t roaring_bitmap::find_position(std::uint16_t key) const {
    std::size_t low = 0;
    std::size_t high = containers.size();
    while (low < high) {
        std::size_t middle = (low + high) / 2;
        if (containers[middle].key < key) {
            low = middle + 1;
        } else {
            high = middle;
        }
    }
    return low;
}

void roaring_bitmap::add(std::uint32_t value) {
    // los 16 bits altos eligen el contenedor y los 16 bajos van dentro
    auto key = static_cast<std::uint16_t>(value >> 16);
    auto low = static_cast<std::uint16_t>(value & 0xFFFF);
    std::size_t position = find_position(key);
    if (position == containers.size() || containers[position].key != key) {
        // std::cout << "nuevo contenedor con clave " << key << '\n';
        container entry;
        entry.key = key;
        containers.insert(containers.begin() + position, std::move(entry));
    }
    containers[position].add(low);
}

bool roaring_bitmap::contains(std::uint32_t value) const {
    auto key = static_cast<std::uint16_t>(value >> 16);
    auto low = static_cast<std::uint16_t>(value & 0xFFFF);
    std::size_t position = find_position(key);
    if (position == containers.size() || containers[position].key != key) {
        return false;
    }
    return containers[position].contains(low);
}

std::uint64_t roaring_bitmap::cardinality() const {
    std::uint64_t result = 0;
    for (const container& entry : containers) {
        result += entry.count;
    }
    return result;
}

std::vector<std::uint32_t> roaring_bitmap::to_vector() const {
    std::vector<std::uint32_t> result;
    result.reserve(cardinality());
    for (const container& entry : containers) {
        // reconstruye cada id como (clave << 16) | valor bajo
        std::uint32_t prefix = static_cast<std::uint32_t>(entry.key) << 16;
        for (std::uint16_t value : entry.to_array()) {
            result.push_back(prefix | value);
        }
    }
    return result;
}

roaring_bitmap::container roaring_bitmap::intersect_containers(
    const container& left, const container& right) {
    container result;
    result.key = left.key;
    if (!left.is_bitmap() && !right.is_bitmap()) {
        // array and array: interseccion de dos secuencias ordenadas
        std::set_intersection(left.values.begin(), left.values.end(),
            right.values.begin(), right.values.end(), std::back_inserter(result.values));
        result.count = result.values.size();
    
    } else if (left.is_bitmap() && right.is_bitmap()) {
        // bitmap and bitmap: and palabra a palabra y conteo de bits
        result.words.resize(bitmap_words);
        for (std::size_t i = 0; i < bitmap_words; ++i) {
            result.words[i] = left.words[i] & right.words[i];
            result.count += __builtin_popcountll(result.words[i]);
        }
        // si quedan 4096 valores o menos vuelve a array
        result.normalize();
    
    } else {
        // array and bitmap: se prueba cada valor del array en el bitmap
        const container& array = left.is_bitmap() ? right : left;
        const container& bitmap = left.is_bitmap() ? left : right;
        for (std::uint16_t value : array.values) {
            if (bitmap.contains(value)) {
                result.values.push_back(value);
            }
        }
        result.count = result.values.size();
    }
    return result;
}

roaring_bitmap::container roaring_bitmap::unite_containers(
    const container& left, const container& right) {
    container result;
    result.key = left.key;
    if (!left.is_bitmap() && !right.is_bitmap()) {
        // array or array: union ordenada; si pasa de 4096 se vuelve bitmap
        std::set_union(left.values.begin(), left.values.end(),
            right.values.begin(), right.values.end(), std::back_inserter(result.values));
        result.count = result.values.size();
        result.normalize();
    
    } else if (left.is_bitmap() && right.is_bitmap()) {
        // bitmap or bitmap: or palabra a palabra y conteo de bits
        result.words.resize(bitmap_words);
        for (std::size_t i = 0; i < bitmap_words; ++i) {
            result.words[i] = left.words[i] | right.words[i];
            result.count += __builtin_popcountll(result.words[i]);
        }
    
    } else {
        // array or bitmap: se copia el bitmap y se encienden los bits del array
        const container& array = left.is_bitmap() ? right : left;
        const container& bitmap = left.is_bitmap() ? left : right;
        result = bitmap;
        for (std::uint16_t value : array.values) {
            result.add(value);
        }
    }
    return result;
}

// recorre las claves de ambos y solo combina las comunes
roaring_bitmap roaring_bitmap::intersection(const roaring_bitmap& other) const {
    roaring_bitmap result;
    std::size_t left = 0;
    std::size_t right = 0;
    while (left < containers.size() && right < other.containers.size()) {
        std::uint16_t left_key = containers[left].key;
        std::uint16_t right_key = other.containers[right].key;
        if (left_key < right_key) {
            ++left;
        } else if (right_key < left_key) {
            ++right;
        } else {
            container entry = intersect_containers(containers[left], other.containers[right]);
            // std::cout << "and clave " << entry.key << ": " << entry.count << " valores\n";
            if (entry.count != 0) {
                result.containers.push_back(std::move(entry));
            }
            ++left;
            ++right;
        }
    }
    return result;
}

// las claves de un solo op se copian; las comunes se unen
roaring_bitmap roaring_bitmap::union_with(const roaring_bitmap& other) const {
    roaring_bitmap result;
    std::size_t left = 0;
    std::size_t right = 0;
    while (left < containers.size() && right < other.containers.size()) {
        std::uint16_t left_key = containers[left].key;
        std::uint16_t right_key = other.containers[right].key;
        if (left_key < right_key) {
            result.containers.push_back(containers[left]);
            ++left;
        } else if (right_key < left_key) {
            result.containers.push_back(other.containers[right]);
            ++right;
        } else {
            result.containers.push_back(unite_containers(containers[left], other.containers[right]));
            ++left;
            ++right;
        }
    }
    // lo que sobra de cualquiera de los dos se copia tal cual
    while (left < containers.size()) {
        result.containers.push_back(containers[left]);
        ++left;
    }
    while (right < other.containers.size()) {
        result.containers.push_back(other.containers[right]);
        ++right;
    }
    return result;
}

std::vector<container_stats> roaring_bitmap::statistics() const {
    std::vector<container_stats> result;
    for (const container& entry : containers) {
        result.push_back({entry.key, entry.count, entry.is_bitmap(), entry.payload_bytes()});
    }
    return result;
}

memory_stats roaring_bitmap::memory_usage() const {
    memory_stats result{};
    result.key_bytes = containers.size() * sizeof(std::uint16_t);
    result.metadata_bytes = sizeof(*this) + containers.size() * (sizeof(container) - sizeof(std::uint16_t));
    // capacidad reservada por los vectores; no incluye allocator ni rss
    result.allocated_bytes = sizeof(*this) + containers.capacity() * sizeof(container);
    for (const container& entry : containers) {
        result.container_payload_bytes += entry.payload_bytes();
        result.allocated_bytes += entry.values.capacity() * sizeof(std::uint16_t);
        result.allocated_bytes += entry.words.capacity() * sizeof(std::uint64_t);
    }
    return result;
}

}

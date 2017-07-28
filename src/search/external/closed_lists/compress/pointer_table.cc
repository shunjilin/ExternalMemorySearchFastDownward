#include "pointer_table.h"

#include <vector>
#include <utility>
#include <cstddef>
#include <limits>
#include <cmath>
#include <climits>
#include <iostream>

using namespace std;

constexpr size_t size_t_bits = sizeof(size_t) * CHAR_BIT;

// Note: bools in vectors take up 1 bit each
// (max_entries * ptr_bits) == bits.size()

PointerTable::PointerTable(size_t ptr_table_size_in_bytes) :
    ptr_size_in_bits(get_ptr_size_in_bits(ptr_table_size_in_bytes)),
    max_entries((ptr_table_size_in_bytes * 8) / ptr_size_in_bits), // round down
    bit_vector(ptr_size_in_bits * max_entries, true),
    invalid_ptr(numeric_limits<size_t>::max() >> (size_t_bits - ptr_size_in_bits))
    {}

// find takes an index and returns pointer value at that index
size_t PointerTable::find(std::size_t index) const {
    size_t pointer = 0;
    size_t bit_index = index * ptr_size_in_bits; // actual offset
    for (std::size_t i = 0; i < ptr_size_in_bits; ++i) {
	pointer <<= 1;
	if (bit_vector[bit_index]) pointer |= 1;
	++bit_index;
    }
    return pointer;
}

bool PointerTable::ptr_is_invalid(size_t ptr) const {
    return ptr == invalid_ptr;
}
    
// insert takes pointer and index and inserts pointer into the table
void PointerTable::insert(size_t pointer, size_t index) {
    // go to last bit of the entry at index
    std::size_t bit_index = ((index + 1) * ptr_size_in_bits) - 1;
    // insert from last bit to first bit
    for (std::size_t i = 0; i < ptr_size_in_bits; ++i) {
	if (!(pointer & 1)) bit_vector[bit_index] = false;
	pointer >>= 1;
	--bit_index;
    }
    ++n_entries;
}

// single hash -> change to dbl hash for better performance?
void PointerTable::hash_insert(std::size_t pointer, std::size_t hash_value) {
    std::size_t hash_index = hash_value % max_entries;
    while (!ptr_is_invalid(find(hash_index))) {
	++hash_index;
	if (hash_index == max_entries) hash_index = 0; //reset
    }
    insert(pointer, hash_index);
}

// single hash
// return ptr
size_t PointerTable::hash_find(size_t hash_value,
                               bool first_probe) const {
    if (first_probe) {
	current_probe_index = hash_value % max_entries;
    } else {
	++current_probe_index;
	if (current_probe_index == max_entries) {
	    current_probe_index = 0;
	}
    }
    return find(current_probe_index);
}

size_t PointerTable::get_n_entries() const {
    return n_entries;
}

size_t PointerTable::get_max_entries() const {
    return max_entries;
}

size_t PointerTable::get_max_size_in_bytes() const {
    return bit_vector.size() / 8;
}

// get the size of a pointer (in bits)  given the size of
// the pointer table in bytes
// conservative estimate, rounds down
size_t PointerTable::get_ptr_size_in_bits(size_t ptr_table_size_in_bytes) const {
    size_t ptr_size_in_bits = 0;
    auto max_ptr_bits = size_t_bits;
    for (std::size_t ptr_sz = 8; ptr_sz < max_ptr_bits; ++ptr_sz) {
	std::size_t total_ptr_table_bits = ptr_sz * pow(2, ptr_sz);
	if (total_ptr_table_bits >= (ptr_table_size_in_bytes * CHAR_BIT)) {
	    ptr_size_in_bits = ptr_sz;
	    break;
	}
    }
    if (ptr_size_in_bits < 8) {
        cerr << "pointer table too small" << endl;
	//throw std::runtime_error(std::string("Not enough memory for pointer table."));
    }
    return ptr_size_in_bits;
}

size_t PointerTable::get_ptr_size_in_bits() const {
    return ptr_size_in_bits;
}



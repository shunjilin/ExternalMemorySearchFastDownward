#include "pointer_table.h"

#include <vector>
#include <utility>
#include <cstddef>
#include <limits>
#include <cmath>
#include <climits>
#include <iostream>

// for primality testing
#include <boost/multiprecision/miller_rabin.hpp>

using namespace std;
using namespace boost::multiprecision;

constexpr size_t size_t_bits = sizeof(size_t) * CHAR_BIT;

#define PRIME // perhaps set this as option parameter instead of macro

// Note: bools in vectors take up 1 bit each
// (max_entries * ptr_bits) == bits.size()

PointerTable::PointerTable(size_t ptr_table_size_in_bytes) //:
//ptr_size_in_bits(get_ptr_size_in_bits(ptr_table_size_in_bytes)),
//max_entries((ptr_table_size_in_bytes * 8) / ptr_size_in_bits), // round down
    //bit_vector(ptr_size_in_bits * max_entries, true),
    //invalid_ptr(numeric_limits<size_t>::max() >> (size_t_bits - ptr_size_in_bits))
{
    size_t big_ptr_size_in_bits = get_ptr_size_in_bits(ptr_table_size_in_bytes);
    
    // need to check if size is optimal
    size_t small_ptr_size_in_bits = 0;
    
    if (ptr_size_in_bits > 1) // guard against edge case where ptr is 1 bit
        small_ptr_size_in_bits = big_ptr_size_in_bits - 1;
    
    size_t big_ptr_entries = ptr_table_size_in_bytes * 8 / big_ptr_size_in_bits;
    size_t small_ptr_entries = pow(2, small_ptr_size_in_bits);
#ifdef PRIME
    for (; big_ptr_entries > 0; --big_ptr_entries) {
        if (miller_rabin_test(big_ptr_entries, 25)) break;
    }
    for (; small_ptr_entries > 0; --small_ptr_entries) {
        if (miller_rabin_test(small_ptr_entries, 25)) break;
    }
#endif
    if (big_ptr_entries > small_ptr_entries) {
        ptr_size_in_bits = big_ptr_size_in_bits;
        max_entries = big_ptr_entries;
    } else {
        ptr_size_in_bits = small_ptr_size_in_bits;
        max_entries = small_ptr_entries;
    }
#ifndef PRIME
    // On the off chance that max_entries is exactly 2^ptr_size_in_bits, we
    // need to reduce the table size so that the arbitrarily defined invalid
    // pointer (all bools set to true) does not point to an actual entry.
    // If max_entry is a prime, we do not need to worry about this edge case.
    if (max_entries == pow(2, ptr_size_in_bits))
        --max_entries;
#endif
    
    bit_vector.resize(ptr_size_in_bits * max_entries, true);

    // all true within size of ptr
    invalid_ptr = numeric_limits<size_t>::max() >> (size_t_bits - ptr_size_in_bits);
}

    

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


//TODO IMPORTANT!: Need to optimize this. Make sure that increasing pointer size
// leads to ability to access bigger hash table, as sometimes the space constraint
// leads to smaller access sizes! Assume user input is Hard Limit

// Get the size of a pointer (in bits)  given the size of
// the pointer table in bytes.
// Returns the biggest pointer size, may not be optimal in terms of size of
// pointer table.
size_t PointerTable::get_ptr_size_in_bits(size_t ptr_table_size_in_bytes) const {
    size_t ptr_size_in_bits = 0;
    auto max_ptr_bits = size_t_bits;
    for (std::size_t ptr_sz = 0; ptr_sz < max_ptr_bits; ++ptr_sz) {
	std::size_t total_ptr_table_bits = ptr_sz * pow(2, ptr_sz);
	if (total_ptr_table_bits >= (ptr_table_size_in_bytes * CHAR_BIT)) {
	    ptr_size_in_bits = ptr_sz;
	    break;
	}
    }
    return ptr_size_in_bits;
}

size_t PointerTable::get_ptr_size_in_bits() const {
    return ptr_size_in_bits;
}

double PointerTable::get_load_factor() const {
    return static_cast<double>(get_n_entries()) /
        static_cast<double>(get_max_entries());
}



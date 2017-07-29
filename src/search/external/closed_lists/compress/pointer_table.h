#ifndef POINTER_TABLE_H
#define POINTER_TABLE_H

#include <vector>
#include <cstddef>

/************************************************************************//**
 * PointerTable packs compactly arbitrary sized pointers into a table of
 * pointers
 ****************************************************************************/
using namespace std;

class PointerTable {
    size_t ptr_size_in_bits;
    size_t n_entries = 0; // current # of pointers in table
    size_t max_entries; // max # of pointers in table
    vector<bool> bit_vector;
    const size_t invalid_ptr; // value of unset pointer (bools all set to true)
    mutable size_t current_probe_index; // for hashing

    size_t get_ptr_size_in_bits(size_t ptr_table_size_in_bytes) const;
        
public:
    PointerTable(std::size_t ptr_table_size_in_bytes);

    size_t find(size_t index) const; // change to [] operator?

    bool ptr_is_invalid(size_t ptr) const;

    void insert(size_t ptr, size_t index);

    void hash_insert(size_t pointer, size_t hash_value);

    size_t hash_find(size_t hash_value, bool first_probe=true) const;

    size_t get_n_entries() const;

    size_t get_max_entries() const; // change to size?

    size_t get_max_size_in_bytes() const;

    size_t get_ptr_size_in_bits() const;

    double get_load_factor() const;
};

#endif

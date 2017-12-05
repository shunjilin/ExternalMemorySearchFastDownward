#ifndef POINTER_TABLE_H
#define POINTER_TABLE_H

#include <vector>
#include <cstddef>

/*                                                                      \
| PointerTable provides a hash table for pointers.                      |
|                                                                       |
| PointerTable packs compactly arbitrary sized pointers into a table of |
| pointers.                                                             |
\======================================================================*/

using namespace std;

class PointerTable {
    size_t ptr_size_in_bits;
    size_t n_entries = 0;
    vector<bool> bit_vector;
    size_t invalid_ptr; // representation of invalid (unset) pointer
    mutable size_t current_probe_index = 0;

    size_t get_ptr_size_in_bits(size_t ptr_table_size_limit_in_bytes) const;
    size_t get_ptr_at_index(size_t index) const;
    void insert_ptr_at_index(size_t ptr, size_t index);
        
public:
    PointerTable(std::size_t ptr_table_size_limit_in_bytes);

    bool ptr_is_invalid(size_t ptr) const;

    // Default probe value of 1 for linear probing.
    void insert_ptr_with_hash(size_t pointer, size_t hash_value, size_t probe_value=1);

    // If first_probe=false, triggers probe sequence.  Default probe value of 1
    // for linear probing.
    size_t get_ptr_with_hash(size_t hash_value, size_t probe_value=1, bool first_probe=true) const;

    size_t get_n_entries() const;

    size_t get_max_entries() const;

    size_t get_max_size_in_bytes() const;

    size_t get_ptr_size_in_bits() const;

    double get_load_factor() const;
};

#endif

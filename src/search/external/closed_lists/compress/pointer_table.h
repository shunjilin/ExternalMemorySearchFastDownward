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
    size_t n_entries = 0; // current # of pointers in table
    vector<bool> bit_vector;
    size_t invalid_ptr; // value of unset pointer (bools all set to true)
    mutable size_t current_probe_index = 0; // for hashing

    size_t get_ptr_size_in_bits(size_t ptr_table_size_in_bytes) const;
    size_t find(size_t index) const;
    void insert(size_t ptr, size_t index);
        
public:
    // ptr_table_size_in_bytes is the hard limit for the pointer table size
    PointerTable(std::size_t ptr_table_size_in_bytes);

    // Returns true if entry is null (empty).
    bool ptr_is_invalid(size_t ptr) const;

    // Insert pointer into table given hash value.
    // Default probe value of 1 for linear probing.
    void hash_insert(size_t pointer, size_t hash_value, size_t probe_value=1);

    // Returns pointer given hash value, if first_probe=false, triggers probe
    // sequence.
    // Default probe value of 1 for linear probing.
    size_t hash_find(size_t hash_value, size_t probe_value=1, bool first_probe=true) const;

    // Return current number of (valid) pointers in the table.
    size_t get_n_entries() const;

    // Return maximum capacity in pointers of the table.
    size_t get_max_entries() const;

    // Return maximum size in bytes table can hold in terms of pointers.
    size_t get_max_size_in_bytes() const;

    // Returns size of pointer in bits (fixed on initialization).
    size_t get_ptr_size_in_bits() const;

    // Returns load factor, i.e. fraction of table filled with valid pointers.
    double get_load_factor() const;
};

#endif

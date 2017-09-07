#ifndef TRANSPOSITION_TABLE_H
#define TRANSPOSITION_TABLE_H

#include <vector>
#include <cstddef>
#include <iostream>

using namespace std;

template<class Entry>
class TranspositionTable {
    vector<Entry> table;
    size_t max_entries = 0;
    size_t size_in_bytes;
public:
    TranspositionTable(size_t size_in_bytes);
    void initialize();
    void clear();
    bool find_insert(Entry entry);
};

template<class Entry>
TranspositionTable<Entry>::TranspositionTable(size_t size_in_bytes) :
    size_in_bytes(size_in_bytes) {}

template<class Entry>
void TranspositionTable<Entry>::initialize() {
    // lazy initialization
    if (max_entries == 0) max_entries =
                              size_in_bytes / Entry::get_size_in_bytes();
    table.resize(max_entries);
}

template<class Entry>
void TranspositionTable<Entry>::clear() {
    table = vector<Entry>();
}

// return true if lower cost duplicate found in table according to hash value
// if not found, insert new node, and evict existing node
template<class Entry>
bool TranspositionTable<Entry>::find_insert(Entry entry) {
    auto index = entry.get_hash_value() % max_entries;
    if (entry == table[index]) {
        if (entry.get_g() < table[index].get_g()) {
            table[index] = entry;
            return false;
        }   
        return true;
    }
    table[index] = entry;
    return false;
}

#endif

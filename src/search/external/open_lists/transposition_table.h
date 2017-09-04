#ifndef TRANSPOSITION_TABLE_H
#define TRANSPOSITION_TABLE_H

#include <vector>
#include <cstddef>

using namespace std;

template<class Entry>
class TranspositionTable {
    vector<Entry> table;
    size_t max_entries;
public:
    TranspositionTable(size_t size_in_bytes);
    void initialize();
    void clear();
    bool find_insert(Entry entry);
};

template<class Entry>
TranspositionTable<Entry>::TranspositionTable(size_t size_in_bytes) :
    max_entries(size_in_bytes / Entry::get_size_in_bytes()) {}

template<class Entry>
void TranspositionTable<Entry>::initialize() {
    table.resize(max_entries);
}

template<class Entry>
void TranspositionTable<Entry>::clear() {
    table = vector<Entry>();
}

template<class Entry>
bool TranspositionTable<Entry>::find_insert(Entry entry) {
    auto index = entry.get_hash_value() % max_entries;
    if (entry == table[index]) {
        return true;
    }
    table[index] = entry;
    return false;
}

#endif

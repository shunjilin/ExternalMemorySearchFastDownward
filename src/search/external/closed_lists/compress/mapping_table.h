#ifndef MAPPING_TABLE_H
#define MAPPING_TABLE_H

#include <vector>
#include <iostream>
#include <cassert>

/*                                                                           \
| Table to store abstraction values of portions of the nodes in the external |
| hash table. Since nodes are flushed in batches by their abstraction value  |
| (for example h-value), the mapping table provides an intermediary cache -  |
| before looking into the external hash table for a particular node, one can |
| look into the mapping table to check first if the abstraction value of the |
| state is equivalent the one hashed; if not, this saves a disk lookup.      |
\===========================================================================*/

class MappingTable {
    std::vector<unsigned> table;
    std::size_t flush_size; // size of each portion
public:
    MappingTable(std::size_t flush_size) :
	flush_size(flush_size) {}

    void insert_value(unsigned value) {
	table.push_back(value);
    }

    unsigned find_by_ptr(std::size_t ptr) const {
	auto map_index = ptr / flush_size;
	assert(map_index < table.size());
	return table[map_index];
    }
};



#endif

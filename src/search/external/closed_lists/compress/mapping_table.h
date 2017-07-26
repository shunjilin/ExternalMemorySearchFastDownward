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
    std::size_t nodes_per_map; // size of each mapping
public:
    MappingTable(std::size_t nodes_per_map) :
        nodes_per_map(nodes_per_map) {}

    void insert_map_value(unsigned map_value) {
	table.push_back(map_value);
    }

    unsigned get_value_from_ptr(std::size_t ptr) const {
	auto map_index = ptr / nodes_per_map;
	assert(map_index < table.size());
	return table[map_index];
    }
};



#endif

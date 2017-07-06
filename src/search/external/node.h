#ifndef EXTERNAL_NODE_H
#define EXTERNAL_NODE_H

#include <vector>
#include <cstddef>

#include "../globals.h"
#include "../algorithms/int_packer.h"
#include "state_id.h"
#include <cassert>
#include <vector>

using PackedStateBin = int_packer::IntPacker::Bin;
using namespace std;


class Node {
 private:
    std::vector<PackedStateBin> packedState;
    
    // Modified from "../search_node_info.h"
    // Do you need status for external search? Maybe delete
    enum NodeStatus {NEW = 0, OPEN = 1, CLOSED = 2, DEAD_END = 3};

    unsigned int status : 2;
    int g : 30;
    int f;
    
    StateID state_id;
    StateID parent_state_id;
    int creating_operator; // TODO: initialize this to -1
    std::size_t parent_hash_value; // For closed list lookup,
                                   // is this worth additional memory?

 public:
    // Copy and assignment constructors?
    // Constructors

    int operator[](int var) const;

    // bool operator==(const Node &other) const;
    
    int get_g() const;
    
    int get_f() const;

    const StateID get_parent_state_id() const;
    
    int get_creating_operator() const;

    //TODO: CHANGE FROM GLOBAL TO NUM VARS INITIALIZED FROM SEARCH SPACE?
    // INSTEAD OF SEARCH REGISTRY
    vector<int> get_values() const;

    // Are these necessary in external search?
    bool is_new() const;
    bool is_open() const;
    bool is_closed() const;
    void close();
    bool is_dead_end() const;
    void mark_as_dead_end();
    
    
    
};

std::size_t get_bytes_per_state(); // Put this somewhere else


#endif

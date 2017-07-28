#ifndef GLOBAL_STATE_H
#define GLOBAL_STATE_H

#ifdef EXTERNAL_SEARCH

#include "algorithms/int_packer.h"
#include "state_id.h"
#include "external/hash_functions/state_hash.h"
#include <vector>
#include <fstream>
#include <memory>

using PackedStateBin = int_packer::IntPacker::Bin;
using namespace statehash;

// GlobalState IS a node for external search purposes

class GlobalState {
    std::vector<PackedStateBin> packedState;
    StateID state_id;
    StateID parent_state_id = StateID::no_state;
    int creating_operator = -1;
    int g = 0;

    void initialize_state_info();
    
    // Primary hash function to prevent unnecessary creation of hash function
    // resources (bitstrings in the case of zobrist hash)
    // Initialization delegated to class that needs it, e.g. closed list
    static std::unique_ptr<StateHash<GlobalState> > hasher;
    GlobalState(StateID state_id);
 public:
    GlobalState();
    GlobalState(const std::vector<PackedStateBin> &packedState);
    GlobalState(const std::vector<PackedStateBin> &packedState,
                StateID parent_state_id,
                int creating_operator,
                int g);
    std::vector<int> get_values() const;
    int operator[](int var) const;
    bool operator==(const GlobalState &other) const;
    const std::vector<PackedStateBin> &get_packed_vec() const;

    StateID get_state_id() const;
    StateID get_parent_state_id() const;
    int get_creating_operator() const;
    int get_g() const;

    bool write(std::fstream& file) const; // serialize Globalstate
    void write(char* ptr) const;
    bool read(std::fstream& file); // deserialize Globalstate
    void read(char* ptr); 
   
    size_t get_hash_value() const;
    
    static size_t packedState_bytes;
    static size_t bytes_per_state;

    static void initialize_hash_function(std::unique_ptr<StateHash<GlobalState> > hash_function);
};


// Specialize hash for hash tables, e.g. std::unordered_map 
namespace std {
    template<> struct hash<GlobalState> {
        // TODO: check if hash function is initialized
        size_t operator()(const GlobalState &state) const {
            return state.get_hash_value();
        }
    };
}

#else // ifndef EXTERNAL_SEARCH

#include "state_id.h"

#include "algorithms/int_packer.h"

#include <cstddef>
#include <iostream>
#include <vector>

class GlobalOperator;
class StateRegistry;

using PackedStateBin = int_packer::IntPacker::Bin;

// For documentation on classes relevant to storing and working with registered
// states see the file state_registry.h.
class GlobalState {
    friend class StateRegistry;
    template<typename Entry>
    friend class PerStateInformation;

    // Values for vars are maintained in a packed state and accessed on demand.
    const PackedStateBin *buffer;

    // registry isn't a reference because we want to support operator=
    const StateRegistry *registry;
    StateID id;

    // Only used by the state registry.
    GlobalState(
        const PackedStateBin *buffer, const StateRegistry &registry, StateID id);

    const PackedStateBin *get_packed_buffer() const {
        return buffer;
    }

    const StateRegistry &get_registry() const {
        return *registry;
    }
public:
    ~GlobalState() = default;

    StateID get_id() const {
        return id;
    }

    int operator[](int var) const;

    std::vector<int> get_values() const;

    void dump_pddl() const;
    void dump_fdr() const;
};

#endif // ifdef EXTERNAL_SEARCH

#endif // GLOBAL_STATE_H

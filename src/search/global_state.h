#ifndef GLOBAL_STATE_H
#define GLOBAL_STATE_H

#ifdef EXTERNAL_SEARCH

#include "algorithms/int_packer.h"
#include "search_node_info.h"
#include <vector>

using PackedStateBin = int_packer::IntPacker::Bin;

// GlobalState IS a node for external search purposes
class GlobalState {
    std::vector<PackedStateBin> packedState;
    SearchNodeInfo info;
 public:
    GlobalState() = default;
    GlobalState(const std::vector<PackedStateBin> &packedState);
    std::vector<int> get_values() const;
    int operator[](int var) const;
    bool operator==(const GlobalState &other) const;
    const std::vector<PackedStateBin> &get_packed_vec() const;

    int get_g() const;

};

namespace std {
    template<> struct hash<GlobalState> {
        size_t operator()(const GlobalState &state) const {
            auto &vec = state.get_packed_vec();
            std::size_t seed = vec.size();
            for (auto& i : vec ) {
                seed ^= i + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            }
            return seed;
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

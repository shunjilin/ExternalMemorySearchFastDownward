#ifdef EXTERNAL_SEARCH
#include "global_state.h"
#include "globals.h"
#include "state_id.h"
#include "algorithms/int_packer.h"

#include <vector>
#include <fstream>
#include <cassert>
#include <cstring>

using PackedStateBin = int_packer::IntPacker::Bin;
const GlobalState GlobalState::dummy = GlobalState(StateID::no_state);

// true value initialized on initial node generation
std::size_t GlobalState::bytes_per_state = 0;
std::size_t GlobalState::packedState_bytes = 0;
    
GlobalState::GlobalState(const std::vector<PackedStateBin> &packedState) :
    packedState(packedState)
{
    if (!packedState_bytes) {
        packedState_bytes = sizeof(packedState.front()) * packedState.size();
    }
    if (!bytes_per_state) {
        bytes_per_state =  sizeof(packedState.front()) * packedState.size() +
            sizeof(state_id) +
            sizeof(parent_state_id) +
            sizeof(creating_operator) +
            sizeof(g);
    }
}

// for dummy state that does not increment state id
GlobalState::GlobalState(const StateID state_id) : state_id(state_id) {}

GlobalState::GlobalState(const std::vector<PackedStateBin> &packedState,
                         StateID parent_state_id,
                         int creating_operator,
                         int g)
    : packedState(packedState),
      parent_state_id(parent_state_id),
      creating_operator(creating_operator),
      g(g) {}
    
std::vector<int> GlobalState::get_values() const {
    int num_variables = g_initial_state_data.size();
    std::vector<int> values(num_variables);
    for (int var = 0; var < num_variables; ++var)
        values[var] = (*this)[var];
    return values;
}

int GlobalState::operator[](int var) const {
    assert(var >= 0);
    assert(var < g_initial_state_data.size());
    return g_state_packer->get(&(packedState[0]), var);
}

bool GlobalState::operator==(const GlobalState& other) const {
    return packedState == other.get_packed_vec();
}

const std::vector<PackedStateBin> &GlobalState::get_packed_vec() const {
    return packedState;
}

StateID GlobalState::get_state_id() const {
    return state_id;    
}

StateID GlobalState::get_parent_state_id() const {
    return parent_state_id;
}

int GlobalState::get_creating_operator() const {
    return creating_operator;
}

int GlobalState::get_g() const {
    return g;
}

bool GlobalState::write(std::fstream& file) const {
    file.write(reinterpret_cast<const char *>
               (&packedState.front()), packedState_bytes);
    
    file.write(reinterpret_cast<const char *>(&state_id), sizeof(state_id));
    file.write(reinterpret_cast<const char *>(&parent_state_id), sizeof(parent_state_id));
    file.write(reinterpret_cast<const char *>(&creating_operator), sizeof(creating_operator));
    file.write(reinterpret_cast<const char *>(&g), sizeof(g));
    return !file.fail();
}

void GlobalState::write(char* ptr) const {
    memcpy(ptr, &packedState.front(), packedState_bytes);
    ptr += packedState_bytes;
    memcpy(ptr, &state_id, sizeof(state_id));
    ptr += sizeof(state_id);
    memcpy(ptr, &parent_state_id, sizeof(parent_state_id));
    ptr += sizeof(parent_state_id);
    memcpy(ptr, &creating_operator, sizeof(creating_operator));
    ptr += sizeof(creating_operator);
    memcpy(ptr, &g, sizeof(g));
}

bool GlobalState::read(std::fstream& file) {
    packedState.resize(packedState_bytes / sizeof(PackedStateBin));
    file.read(reinterpret_cast<char *>
              (&packedState.front()), packedState_bytes);
    file.read(reinterpret_cast<char *>(&state_id), sizeof(state_id));
    file.read(reinterpret_cast<char *>(&parent_state_id), sizeof(parent_state_id));
    file.read(reinterpret_cast<char *>(&creating_operator), sizeof(creating_operator));
    file.read(reinterpret_cast<char *>(&g), sizeof(g));

    return !file.fail();
}

void GlobalState::read(char* ptr) {
    packedState.resize(packedState_bytes / sizeof(PackedStateBin));
    memcpy(&packedState.front(), ptr, packedState_bytes);
    ptr += packedState_bytes;
    memcpy(&state_id, ptr, sizeof(state_id));
    ptr += sizeof(state_id);
    memcpy(&parent_state_id, ptr, sizeof(parent_state_id));
    ptr += sizeof(parent_state_id);
    memcpy(&creating_operator, ptr, sizeof(creating_operator));
    ptr += sizeof(creating_operator);
    memcpy(&g, ptr, sizeof(g));
}

#else // ifndef EXTERNAL_SEARCH

#include "global_state.h"

#include "globals.h"
#include "state_registry.h"
#include "task_proxy.h"

#include <algorithm>
#include <iostream>
#include <cassert>
using namespace std;


GlobalState::GlobalState(
    const PackedStateBin *buffer, const StateRegistry &registry, StateID id)
    : buffer(buffer),
      registry(&registry),
      id(id) {
    assert(buffer);
    assert(id != StateID::no_state);
}

int GlobalState::operator[](int var) const {
    assert(var >= 0);
    assert(var < registry->get_num_variables());
    return registry->get_state_value(buffer, var);
}

vector<int> GlobalState::get_values() const {
    int num_variables = registry->get_num_variables();
    vector<int> values(num_variables);
    for (int var = 0; var < num_variables; ++var)
        values[var] = (*this)[var];
    return values;
}

void GlobalState::dump_pddl() const {
    State state(registry->get_task(), get_values());
    state.dump_pddl();
}

void GlobalState::dump_fdr() const {
    State state(registry->get_task(), get_values());
    state.dump_fdr();
}

#endif

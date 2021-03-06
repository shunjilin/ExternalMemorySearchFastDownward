#ifdef EXTERNAL_SEARCH
#include "global_state.h"
#include "globals.h"
#include "state_id.h"
#include "algorithms/int_packer.h"
#include "utils/memory.h"
#include "external/hash_functions/state_hash.h"

#include <vector>
#include <fstream>
#include <cassert>
#include <cstring>
#include <memory>

std::unique_ptr<StateHash<GlobalState> > GlobalState::hasher = nullptr;

// These will be initialized on the generation of the first node (lazily).
// This limitation is due to the fact that information on state variables
// is only available at runtime.
std::size_t GlobalState::packedState_bytes = 0;
std::size_t GlobalState::size_in_bytes = 0;

std::size_t GlobalState::get_packedState_bytes() { return packedState_bytes; }
std::size_t GlobalState::get_size_in_bytes() { return size_in_bytes; }


// Initialize memory information of states, as well as primary hash function used.
// Should only be called after states have been packed by int_packer.
void GlobalState::initialize_state_info() {
    packedState_bytes = g_state_packer->get_num_bins() * sizeof(PackedStateBin);
    size_in_bytes =
        packedState_bytes +
        sizeof(state_id) +
        sizeof(parent_state_id) +
        sizeof(creating_operator) +
        sizeof(g) +
        sizeof(parent_hash_value);
}

GlobalState::GlobalState(const StateID state_id) : state_id(state_id) {}


// 'Empty' states that can be used for copying.
// This prevents unecessary increments and assigning of StateIDs to non-states.
GlobalState::GlobalState() : GlobalState(StateID::no_state) {}


// Assumes that this is used on the creation of initial state - i.e. is done
// before any other GlobalState constructor is used - and therefore is in charge
// of the initialization of static variables. Otherwise we would have to put
// initialize_state_info() in other constructors as well.
GlobalState::GlobalState(const std::vector<PackedStateBin> &packedState) :
    packedState(packedState)
{
    if (!packedState_bytes || !size_in_bytes || !hasher) initialize_state_info();
}

GlobalState::GlobalState(const std::vector<PackedStateBin> &packedState,
                         StateID parent_state_id,
                         int creating_operator,
                         int g,
                         size_t parent_hash_value)
    : packedState(packedState),
      parent_state_id(parent_state_id),
      creating_operator(creating_operator),
      g(g),
      parent_hash_value(parent_hash_value)
{}
    
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

bool GlobalState::operator<(const GlobalState& other) const {
    return packedState < other.get_packed_vec();
}

bool GlobalState::operator>(const GlobalState& other) const {
    return other < *this;
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
    file.write(reinterpret_cast<const char *>(&parent_hash_value), sizeof(parent_hash_value));
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
    ptr += sizeof(g);
    memcpy(ptr, &parent_hash_value, sizeof(parent_hash_value));
    
}

bool GlobalState::read(std::fstream& file) {
    packedState.resize(packedState_bytes / sizeof(PackedStateBin));
    file.read(reinterpret_cast<char *>
              (&packedState.front()), packedState_bytes);
    file.read(reinterpret_cast<char *>(&state_id), sizeof(state_id));
    file.read(reinterpret_cast<char *>(&parent_state_id), sizeof(parent_state_id));
    file.read(reinterpret_cast<char *>(&creating_operator), sizeof(creating_operator));
    file.read(reinterpret_cast<char *>(&g), sizeof(g));
    file.read(reinterpret_cast<char *>(&parent_hash_value), sizeof(parent_hash_value));
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
    ptr+= sizeof(g);
    memcpy(&parent_hash_value, ptr, sizeof(parent_hash_value));
}

size_t GlobalState::get_hash_value() const {
    if (hasher != nullptr) {
        return (*hasher)(*this);
    }
    return 0;
}

size_t GlobalState::get_parent_hash_value() const {
    return parent_hash_value;
}

void GlobalState::
initialize_hash_function(std::unique_ptr<StateHash<GlobalState> > hash_function) {
    hasher = std::move(hash_function);
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

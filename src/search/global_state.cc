#ifdef EXTERNAL_SEARCH
#include "global_state.h"
#include "globals.h"
#include "algorithms/int_packer.h"

#include <vector>
#include <cassert>

using PackedStateBin = int_packer::IntPacker::Bin;

GlobalState::GlobalState(const std::vector<PackedStateBin> &packedState)
    : packedState(packedState) {
}
    
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

int GlobalState::get_g() const {
    return info.g;
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

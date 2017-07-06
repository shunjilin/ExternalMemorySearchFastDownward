#include "node.h"

using PackedStateBin = int_packer::IntPacker::Bin;

int Node::operator[](int var) const {
    assert(var >= 0);
    assert(var < g_initial_state_data.size());
    return g_state_packer->get(&(packedState[0]), var);
}

//bool Node::operator==(const Node &other) const;

int Node::get_g() const {
    return g;
}
int Node::get_f() const {
    return f;
}

const StateID Node::get_parent_state_id() const {
    return parent_state_id;
}
    

int Node::get_creating_operator() const {
    return creating_operator;
}

//TODO: CHANGE FROM GLOBAL TO NUM VARS INITIALIZED FROM SEARCH SPACE?
// INSTEAD OF SEARCH REGISTRY
vector<int> Node::get_values() const {
    int num_variables = g_initial_state_data.size();
    vector<int> values(num_variables);
    for (int var = 0; var < num_variables; ++var)
        values[var] = (*this)[var];
    return values;
}

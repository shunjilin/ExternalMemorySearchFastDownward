#ifdef EXTERNAL_SEARCH

#include "state_registry.h"
#include "abstract_task.h"
#include "axioms.h"
#include "algorithms/int_packer.h"
#include "global_operator.h"
#include "global_state.h"
#include "operator_cost.h"

#include <vector>
#include <algorithm>

using namespace std;

StateRegistry::StateRegistry(
    const AbstractTask &task, const int_packer::IntPacker &state_packer,
    AxiomEvaluator &axiom_evaluator, const std::vector<int> &initial_state_data,
                             OperatorCost cost_type)
    : task(task),
      state_packer(state_packer),
      axiom_evaluator(axiom_evaluator),
      initial_state_data(initial_state_data),
      num_variables(initial_state_data.size()),
      cost_type(cost_type) {}


int StateRegistry::get_bins_per_state() const {
    return state_packer.get_num_bins();
}

const AbstractTask &StateRegistry::get_task() const {
    return task;
}

int StateRegistry::get_num_variables() const {
    return num_variables;
}

int StateRegistry::get_state_value(const PackedStateBin *buffer, int var) const {
    return state_packer.get(buffer, var);
}

const GlobalState StateRegistry::get_initial_state() {
    std::vector<PackedStateBin> buffer(get_bins_per_state(), 0);
    // Avoid garbage values in half-full bins.
    fill_n(&buffer[0], get_bins_per_state(), 0);
    for (size_t i = 0; i < initial_state_data.size(); ++i) {
        state_packer.set(&buffer[0], i, initial_state_data[i]);
    }
    axiom_evaluator.evaluate(&buffer[0], state_packer);
    // seems costly to copy buffer each time in constructor
    // TODO:: use move semantics for buffer
    return GlobalState(buffer);
}
    
    
GlobalState StateRegistry::
get_successor_state(const GlobalState &predecessor, const GlobalOperator *op) {
    assert(!op.is_axiom());
    std::vector<PackedStateBin> buffer(predecessor.get_packed_vec());
    for (size_t i = 0; i < op->get_effects().size(); ++i) {
        const GlobalEffect &effect = op->get_effects()[i];
        if (effect.does_fire(predecessor))
            state_packer.set(&buffer[0], effect.var, effect.val);
    }
    axiom_evaluator.evaluate(&buffer[0], state_packer);
    // seems costly to copy buffer each time in constructor
    // TODO:: use move semantics for buffer
    return GlobalState(buffer,
                       predecessor.get_state_id(),
                       get_op_index_hacked(op),
                       predecessor.get_g() +
                       get_adjusted_action_cost(*op, cost_type));
}

int StateRegistry::get_state_size_in_bytes() const {
    return get_bins_per_state() * sizeof(PackedStateBin);
}

#else // ifndef EXTERNAL_SEARCH

#include "state_registry.h"

#include "global_operator.h"
#include "per_state_information.h"

using namespace std;

StateRegistry::StateRegistry(
    const AbstractTask &task, const int_packer::IntPacker &state_packer,
    AxiomEvaluator &axiom_evaluator, const vector<int> &initial_state_data)
    : task(task),
      state_packer(state_packer),
      axiom_evaluator(axiom_evaluator),
      initial_state_data(initial_state_data),
      num_variables(initial_state_data.size()),
      state_data_pool(get_bins_per_state()),
      registered_states(
          0,
          StateIDSemanticHash(state_data_pool, get_bins_per_state()),
          StateIDSemanticEqual(state_data_pool, get_bins_per_state())),
      cached_initial_state(0) {
}


StateRegistry::~StateRegistry() {
    for (set<PerStateInformationBase *>::iterator it = subscribers.begin();
         it != subscribers.end(); ++it) {
        (*it)->remove_state_registry(this);
    }
    delete cached_initial_state;
}

StateID StateRegistry::insert_id_or_pop_state() {
    /*
      Attempt to insert a StateID for the last state of state_data_pool
      if none is present yet. If this fails (another entry for this state
      is present), we have to remove the duplicate entry from the
      state data pool.
    */
    StateID id(state_data_pool.size() - 1);
    pair<StateIDSet::iterator, bool> result = registered_states.insert(id);
    bool is_new_entry = result.second;
    if (!is_new_entry) {
        state_data_pool.pop_back();
    }
    assert(registered_states.size() == state_data_pool.size());
    return *result.first;
}

GlobalState StateRegistry::lookup_state(StateID id) const {
    return GlobalState(state_data_pool[id.value], *this, id);
}

const GlobalState &StateRegistry::get_initial_state() {
    if (cached_initial_state == 0) {
        PackedStateBin *buffer = new PackedStateBin[get_bins_per_state()];
        // Avoid garbage values in half-full bins.
        fill_n(buffer, get_bins_per_state(), 0);
        for (size_t i = 0; i < initial_state_data.size(); ++i) {
            state_packer.set(buffer, i, initial_state_data[i]);
        }
        axiom_evaluator.evaluate(buffer, state_packer);
        state_data_pool.push_back(buffer);
        // buffer is copied by push_back
        delete[] buffer;
        StateID id = insert_id_or_pop_state();
        cached_initial_state = new GlobalState(lookup_state(id));
    }
    return *cached_initial_state;
}

//TODO it would be nice to move the actual state creation (and operator application)
//     out of the StateRegistry. This could for example be done by global functions
//     operating on state buffers (PackedStateBin *).
GlobalState StateRegistry::get_successor_state(const GlobalState &predecessor, const GlobalOperator &op) {
    assert(!op.is_axiom());
    state_data_pool.push_back(predecessor.get_packed_buffer());
    PackedStateBin *buffer = state_data_pool[state_data_pool.size() - 1];
    for (size_t i = 0; i < op.get_effects().size(); ++i) {
        const GlobalEffect &effect = op.get_effects()[i];
        if (effect.does_fire(predecessor))
            state_packer.set(buffer, effect.var, effect.val);
    }
    axiom_evaluator.evaluate(buffer, state_packer);
    StateID id = insert_id_or_pop_state();
    return lookup_state(id);
}

int StateRegistry::get_bins_per_state() const {
    return state_packer.get_num_bins();
}

int StateRegistry::get_state_size_in_bytes() const {
    return get_bins_per_state() * sizeof(PackedStateBin);
}

void StateRegistry::subscribe(PerStateInformationBase *psi) const {
    subscribers.insert(psi);
}

void StateRegistry::unsubscribe(PerStateInformationBase *const psi) const {
    subscribers.erase(psi);
}

#endif // ifdef EXTERNAL_SEARCH

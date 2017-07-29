#include "lazy_search.h"

#include "../../evaluation_context.h"
#include "../../globals.h"
#include "../../heuristic.h"
#include "../../open_list_factory.h"
#include "../closed_list_factory.h"
#include "../../option_parser.h"
#include "../../pruning_method.h"


#include "../../algorithms/ordered_set.h" // what is this for>
#include "../../task_utils/successor_generator.h"

#include <cassert>
#include <cstdlib>
#include <memory>
#include <set>

using namespace std;

namespace lazy_search {
    LazySearch::LazySearch(const Options &opts)
        : SearchEngine(opts),
          reopen_closed_nodes(opts.get<bool>("reopen_closed")), // not needed? taken cared of in open list
          open_list(opts.get<shared_ptr<OpenListFactory> >("open")->
                    create_state_open_list()),
          closed_list(opts.get<shared_ptr<ClosedListFactory> >("closed")->
                      create_state_closed_list()),
          f_evaluator(opts.get<Evaluator *>("f_eval", nullptr)),
          preferred_operator_heuristics(opts.get_list<Heuristic *>("preferred")),
          pruning_method(opts.get<shared_ptr<PruningMethod> >("pruning")) {
    }

    void LazySearch::initialize() {
        cout << "Conducting best first search"
             << (reopen_closed_nodes ? " with" : " without")
             << " reopening closed nodes, (real) bound = " << bound
             << endl;
        assert(open_list);
        assert(closed_list);

        set<Heuristic *> hset;
        open_list->get_involved_heuristics(hset);

        // Add heuristics that are used for preferred operators (in case they are
        // not also used in the open list).
        hset.insert(preferred_operator_heuristics.begin(),
                    preferred_operator_heuristics.end());

        // Add heuristics that are used in the f_evaluator. They are usually also
        // used in the open list and are hence already included, but we want to be
        // sure.
        if (f_evaluator) {
            f_evaluator->get_involved_heuristics(hset);
        }

        heuristics.assign(hset.begin(), hset.end());
        assert(!heuristics.empty());

        const GlobalState &initial_state = state_registry.get_initial_state();
        
        for (Heuristic *heuristic : heuristics) {
            heuristic->notify_initial_state(initial_state);
        }

        // Note: we consider the initial state as reached by a preferred
        // operator.
        EvaluationContext eval_context(initial_state, 0, true, &statistics);

        statistics.inc_evaluated_states();

        if (open_list->is_dead_end(eval_context)) {
            cout << "Initial state is a dead end." << endl;
        } else {
            start_f_value_statistics(eval_context);

            open_list->insert(eval_context, initial_state);
        }

    }

    void LazySearch::print_checkpoint_line(int g) const {
        cout << "[g=" << g << ", ";
        statistics.print_basic_statistics();
        cout << "]" << endl;
    }
    
    void LazySearch::print_statistics() const {
        statistics.print_detailed_statistics();
        closed_list->print_statistics();
        //search_space.print_statistics();
    }

    SearchStatus LazySearch::step() {
        pair<GlobalState, bool> n = fetch_next_node();
        if (!n.second) {
            return FAILED;
        }

        GlobalState s = n.first;

        if (check_goal_and_set_plan(s)) {
            open_list->clear();
            return SOLVED;
        }
        bool found, reopened;
        std::tie(found, reopened) = closed_list->find_insert(s);
        // SHUNJI TO CONSIDER: (currently reopening handled by closed node)
        // If we do not reopen closed nodes, we just update the parent pointers.
        // Note that this could cause an incompatibility between
        // the g-value and the actual path that is traced back.
        // succ_node.update_parent(node, op);

        if (found && !reopened) return IN_PROGRESS; // in closed node

        if (reopened) statistics.inc_reopened();
        
        vector<OperatorID> applicable_ops;
        g_successor_generator->generate_applicable_ops(s, applicable_ops);

        /*
          TODO: When preferred operators are in use, a preferred operator will be
          considered by the preferred operator queues even when it is pruned.
        */
        pruning_method->prune_operators(s, applicable_ops);

        // This evaluates the expanded state (again) to get preferred ops
        EvaluationContext eval_context(s, s.get_g(), false, &statistics, true);
        ordered_set::OrderedSet<OperatorID> preferred_operators =
            collect_preferred_operators(eval_context, preferred_operator_heuristics);

        for (OperatorID op_id : applicable_ops) {
            const GlobalOperator *op = &g_operators[op_id.get_index()];
            /*if ((node.get_real_g() + op->get_cost()) >= bound)
              continue;*/ // no need for bound?

            GlobalState succ_state = state_registry.get_successor_state(s, op);
            statistics.inc_generated();
            bool is_preferred = preferred_operators.contains(op_id);

            /*
              Note: we must call notify_state_transition for each heuristic, so
              don't break out of the for loop early.
            */
            // is this necessary?
            //for (Heuristic *heuristic : heuristics) {
            //  heuristic->notify_state_transition(s, *op, succ_state);
            //}
            // We have not seen this state before.
            // Evaluate and create a new node.

            // Careful: succ_node.get_g() is not available here yet,
            // hence the stupid computation of succ_g.
            // TODO: Make this less fragile.
            int succ_g = succ_state.get_g() + get_adjusted_cost(*op);

            EvaluationContext eval_context(
                                           succ_state, succ_g, is_preferred, &statistics);
            statistics.inc_evaluated_states();

            if (open_list->is_dead_end(eval_context)) {
                statistics.inc_dead_ends();
                continue;
            }
            //succ_node.open(node, op);

            open_list->insert(eval_context, succ_state);
            
        }
        
        return IN_PROGRESS;
    }

        
    pair<GlobalState, bool> LazySearch::fetch_next_node() {
        while (true) {
            if (open_list->empty()) {
                cout << "Completely explored state space -- no solution!" << endl;
                return make_pair(GlobalState(), false);
            }

            GlobalState state = open_list->remove_min();
            update_f_value_statistics(state);
            return make_pair(state, true);
        }
    }


    bool LazySearch::check_goal_and_set_plan(const GlobalState &state) {
        if (test_goal(state)) {
            cout << "Solution found!" << endl;
            set_plan(closed_list->trace_path(state));
            return true;
        }
            return false;
    }
        
    

    /*void LazySearch::dump_search_space() const {
        search_space.dump();
        }*/

    void LazySearch::start_f_value_statistics(EvaluationContext &eval_context) {
        if (f_evaluator) {
            int f_value = eval_context.get_heuristic_value(f_evaluator);
            statistics.report_f_value_progress(f_value);
        }
    }

    /* TODO: HACK! This is very inefficient for simply looking up an h value.
       Also, if h values are not saved it would recompute h for each and every state. */
    void LazySearch::update_f_value_statistics(const GlobalState &state) {
        if (f_evaluator) {
            /*
              TODO: This code doesn't fit the idea of supporting
              an arbitrary f evaluator.
            */
            EvaluationContext eval_context(state, state.get_g(), false, &statistics);
            int f_value = eval_context.get_heuristic_value(f_evaluator);
            statistics.report_f_value_progress(f_value);
        }
    }
}

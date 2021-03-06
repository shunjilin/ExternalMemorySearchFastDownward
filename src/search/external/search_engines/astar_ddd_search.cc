#include "astar_ddd_search.h"

#include "../../evaluation_context.h"
#include "../../globals.h"
#include "../../heuristic.h"
#include "../../open_list_factory.h"
#include "../../option_parser.h"
#include "../../pruning_method.h"
#include "../utils/errors.h"

#include "../../algorithms/ordered_set.h"
#include "../../task_utils/successor_generator.h"

#include <cassert>
#include <cstdlib>
#include <memory>

using namespace std;

namespace astar_ddd_search {
    AStarDDDSearch::AStarDDDSearch(const Options &opts)
        : SearchEngine(opts),
          reopen_closed_nodes(opts.get<bool>("reopen_closed")),
          open_list(opts.get<shared_ptr<OpenListFactory> >("open")->
                    create_state_open_list()),
          f_evaluator(opts.get<Evaluator *>("f_eval", nullptr)),
          preferred_operator_heuristics(opts.get_list<Heuristic *>("preferred")),
          pruning_method(opts.get<shared_ptr<PruningMethod> >("pruning")) {
    }

    void AStarDDDSearch::initialize() {
        cout << "Conducting best first search."
             << (reopen_closed_nodes ? " with" : " without")
             << " reopening closed nodes"
             << endl;
        assert(open_list);

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
        EvaluationContext eval_context(initial_state, true, &statistics);

        statistics.inc_evaluated_states();

        start_f_value_statistics(eval_context);

        open_list->insert(eval_context, initial_state);

    }

    void AStarDDDSearch::print_checkpoint_line(int g) const {
        cout << "[g=" << g << ", ";
        statistics.print_basic_statistics();
        cout << "]" << endl;
    }
    
    void AStarDDDSearch::print_statistics() const {
        statistics.print_detailed_statistics();
        // Find better place to put the following logging
        cout << "Size of a node: " << GlobalState::get_size_in_bytes()
             << " bytes" << endl;
    }

    SearchStatus AStarDDDSearch::step() {
        pair<GlobalState, bool> n = fetch_next_node();
        if (!n.second) {
            return FAILED;
        }

        GlobalState s = n.first;
        
        if (check_goal_and_set_plan(s)) {
            open_list->clear();
            return SOLVED;
        }

        expand(s);
        
        
        return IN_PROGRESS;
    }

    void AStarDDDSearch::expand(const GlobalState& s) {

        vector<OperatorID> applicable_ops;
        g_successor_generator->generate_applicable_ops(s, applicable_ops);
        
        /*
          TODO: When preferred operators are in use, a preferred operator will be
          considered by the preferred operator queues even when it is pruned.
        */
        pruning_method->prune_operators(s, applicable_ops);

        // This evaluates the expanded state (again) to get preferred ops
        EvaluationContext eval_context(s, false, &statistics, true);
        ordered_set::OrderedSet<OperatorID> preferred_operators =
            collect_preferred_operators(eval_context, preferred_operator_heuristics);

        statistics.inc_expanded();
        for (OperatorID op_id : applicable_ops) {
            const GlobalOperator *op = &g_operators[op_id.get_index()];

            GlobalState succ_state = state_registry.get_successor_state(s, op);
            statistics.inc_generated();
            bool is_preferred = preferred_operators.contains(op_id);

            EvaluationContext eval_context(
                                           succ_state, is_preferred, &statistics);
            
            statistics.inc_evaluated_states();

            if (open_list->is_dead_end(eval_context)) {
                statistics.inc_dead_ends();
                continue;
            }

            open_list->insert(eval_context, succ_state);
            
        };
    }

        
    pair<GlobalState, bool> AStarDDDSearch::fetch_next_node() {
        while (true) {
            try {
                GlobalState state = open_list->remove_min();
                update_f_value_statistics(state);
                return make_pair(state, true);
            } catch (OpenListEmpty& e) {
                cout << "Completely explored state space -- no solution!" << endl;
                return make_pair(GlobalState(), false);
            } catch (...) {
                throw;
            }
        }
    }


    bool AStarDDDSearch::check_goal_and_set_plan(const GlobalState &state) {
        if (test_goal(state)) {
            cout << "Solution found!" << endl;
            set_plan(open_list->trace_path(state));
            return true;
        }
            return false;
    }

    void AStarDDDSearch::start_f_value_statistics(EvaluationContext &eval_context) {
        if (f_evaluator) {
            int f_value = eval_context.get_heuristic_value(f_evaluator);
            statistics.report_f_value_progress(f_value);
        }
    }

    /* TODO: HACK! This is very inefficient for simply looking up an h value.
       Also, if h values are not saved it would recompute h for each and every state. */
    void AStarDDDSearch::update_f_value_statistics(const GlobalState &state) {
        if (f_evaluator) {
            /*
              TODO: This code doesn't fit the idea of supporting
              an arbitrary f evaluator.
            */
            EvaluationContext eval_context(state, false, &statistics);
            int f_value = eval_context.get_heuristic_value(f_evaluator);
            statistics.report_f_value_progress(f_value);
        }
    }
}

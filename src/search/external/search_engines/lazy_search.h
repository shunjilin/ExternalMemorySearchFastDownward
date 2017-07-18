#ifndef EXTERNAL_SEARCH_ENGINES_LAZY_SEARCH_H
#define EXTERNAL_SEARCH_ENGINES_LAZY_SEARCH_H

#include "../../open_list.h"
#include "../closed_list.h"
#include "../../search_engine.h"

#include <memory>
#include <vector>

class Evaluator;
class GlobalOperator;
class Heuristic;
class PruningMethod;

namespace options {
    class Options;
}
// adapted from eager search, which is used in astar for original FD
namespace lazy_search {
    class LazySearch : public SearchEngine {
        const bool reopen_closed_nodes;

        std::unique_ptr<StateOpenList> open_list;
        std::unique_ptr<StateClosedList> closed_list;
        Evaluator *f_evaluator;
        
        std::vector<Heuristic *> heuristics;
        std::vector<Heuristic *> preferred_operator_heuristics;
        std::shared_ptr<PruningMethod> pruning_method;

        std::pair<GlobalState, bool> fetch_next_node(); // should i use node here?
        bool check_goal_and_set_plan(const GlobalState &state);

        void start_f_value_statistics(EvaluationContext &eval_context);
        void update_f_value_statistics(const GlobalState &node);
        void reward_progress();
        void print_checkpoint_line(int g) const;

    protected:
        virtual void initialize() override;
        virtual SearchStatus step() override;
    public:
        explicit LazySearch(const options::Options &opts);
        virtual ~LazySearch() = default;

        virtual void print_statistics() const override;

        //void dump_search_space() const;
    };
}

#endif
        

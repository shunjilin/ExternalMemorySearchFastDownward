#include "regular_tiebreaking_open_list.h"

#include "../../open_list.h"
#include "../../option_parser.h"
#include "../../plugin.h"

#include "../../utils/memory.h"

#include "../utils/errors.h"

#include <utility>
#include <vector>
#include <unordered_set>

using namespace std;

namespace regular_tiebreaking_open_list {
    template<class Entry>
    class RegularTieBreakingOpenList : public OpenList<Entry> {

        map<int, map<int, vector<Entry> > > fg_buckets;
        
        
        int size;

        vector<Evaluator *> evaluators; // f, h

    protected:
        virtual void do_insertion(EvaluationContext &eval_context,
                                  const Entry &entry) override;

    public:
        explicit RegularTieBreakingOpenList(const Options &opts);
        virtual ~RegularTieBreakingOpenList() override = default;

        virtual Entry remove_min() override;
        virtual void clear() override;
        virtual bool empty() const override;
        virtual void get_involved_heuristics(set<Heuristic *> &hset) override;
        virtual bool is_dead_end(EvaluationContext &eval_context) const override;
        virtual bool is_reliable_dead_end(EvaluationContext &eval_context) const override;
    };

    
    template<class Entry>
    RegularTieBreakingOpenList<Entry>::RegularTieBreakingOpenList(const Options &opts)
        : OpenList<Entry>(false), //opts.get<bool>("pref_only")),
        size(0), evaluators(opts.get_list<Evaluator *>("evals")) {
    }

    template<class Entry>
    void RegularTieBreakingOpenList<Entry>::
    do_insertion(EvaluationContext &eval_context, const Entry &entry) {
        assert(evaluators.size() == 1);
        auto f = eval_context.get_heuristic_value_or_infinity(evaluators[0]);
        auto g = entry.get_g();
        fg_buckets[f][g].push_back(entry);
        ++size;
    }

    template<class Entry>
    Entry RegularTieBreakingOpenList<Entry>::remove_min() {
        Entry min_entry;
        assert(size > 0);

        // tiebreak by lowest f value
        for (auto f_bucket = fg_buckets.begin();
             f_bucket != fg_buckets.end(); ++f_bucket) {
            // tiebreak by highest g value
            for (auto g_bucket = f_bucket->second.rbegin();
                 g_bucket != f_bucket->second.rend(); ++g_bucket) {
                
                min_entry = g_bucket->second.back();
                g_bucket->second.pop_back();
                --size;

                // if g bucket is empty
                if (g_bucket->second.empty()) {
                    auto g = g_bucket->first;
                    f_bucket->second.erase(g);
                    // if f bucket is empty
                    if (f_bucket->second.empty()) {
                        auto f = f_bucket->first;
                        fg_buckets.erase(f);
                    }
                }
                return min_entry;
                    
            }
        }
        return Entry();
    }

    template<class Entry>
    bool RegularTieBreakingOpenList<Entry>::empty() const {
        return size == 0;
    }

    template<class Entry>
    void RegularTieBreakingOpenList<Entry>::clear() {
        fg_buckets.clear();
        size = 0;
    }

    template<class Entry>
    void RegularTieBreakingOpenList<Entry>::
    get_involved_heuristics(set<Heuristic *> &hset) {
        for (Evaluator *evaluator : evaluators)
            evaluator->get_involved_heuristics(hset);
    }

    template<class Entry>
    bool RegularTieBreakingOpenList<Entry>::
    is_dead_end(EvaluationContext &eval_context) const {
        // If one safe heuristic detects a dead end, return true.
        if (is_reliable_dead_end(eval_context))
            return true;
        
        // Otherwise, return true if all heuristics agree this is a dead-end.
        for (Evaluator *evaluator : evaluators)
            if (!eval_context.is_heuristic_infinite(evaluator))
                return false;
        return true;
    }

    template<class Entry>
    bool RegularTieBreakingOpenList<Entry>::
    is_reliable_dead_end(EvaluationContext &eval_context) const {
        for (Evaluator *evaluator : evaluators)
            if (eval_context.is_heuristic_infinite(evaluator) &&
                evaluator->dead_ends_are_reliable())
                return true;
        return false;
    }

    
    RegularTieBreakingOpenListFactory::
    RegularTieBreakingOpenListFactory(const Options &options)
        : options(options) {
    }

    unique_ptr<StateOpenList>
    RegularTieBreakingOpenListFactory::create_state_open_list() {
        return utils::make_unique_ptr
            <RegularTieBreakingOpenList<StateOpenListEntry>>(options);
    }

    
    static shared_ptr<OpenListFactory> _parse(OptionParser &parser) {
        parser.document_synopsis("Tie-breaking open list", "");
        parser.add_list_option<Evaluator *>("evals", "evaluators");

        Options opts = parser.parse();
        opts.verify_list_non_empty<Evaluator *>("evals");
        if (parser.dry_run())
            return nullptr;
        else
            return make_shared<RegularTieBreakingOpenListFactory>(opts);
    }

    static PluginShared<OpenListFactory> _plugin("regular_tiebreaking", _parse);
}


        

        

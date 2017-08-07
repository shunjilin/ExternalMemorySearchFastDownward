#include "external_tiebreaking_open_list.h"

#include "../../open_list.h"
#include "../../option_parser.h"
#include "../../plugin.h"

#include "../../utils/memory.h"

#include "../file_utility.h"
#include "../options/errors.h"

#include <utility>
#include <map>
#include <set>
#include <string>

// for constructing directory
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
using namespace std;

namespace external_tiebreaking_open_list {
    template<class Entry>
    class ExternalTieBreakingOpenList : public OpenList<Entry> {

        map<int, map<int, named_fstream> > fg_buckets;
        
        int size;

        vector<Evaluator *> evaluators;
    
        //bool allow_unsafe_pruning;

        string get_bucket_string(int f, int g) const;
        bool exists_bucket(int f, int g) const;
        void create_bucket(int f, int g);
    protected:
        virtual void do_insertion(EvaluationContext &eval_context,
                                  const Entry &entry) override;

    public:
        explicit ExternalTieBreakingOpenList(const Options &opts);
        virtual ~ExternalTieBreakingOpenList() override = default;

        virtual Entry remove_min() override;
        virtual void clear() override;
        virtual bool empty() const override;
        virtual void get_involved_heuristics(set<Heuristic *> &hset) override;
        virtual bool is_dead_end(EvaluationContext &eval_context) const override;
        virtual bool is_reliable_dead_end(EvaluationContext &eval_context) const override;
    };

    
    template<class Entry>
    ExternalTieBreakingOpenList<Entry>::ExternalTieBreakingOpenList(const Options &opts)
        : OpenList<Entry>(false), //opts.get<bool>("pref_only")),
        size(0), evaluators(opts.get_list<Evaluator *>("evals")) {
        // create directory for open list files if not exist
        mkdir("open_list_buckets", 0744);
    }

    template<class Entry>
    void ExternalTieBreakingOpenList<Entry>::
    do_insertion(EvaluationContext &eval_context, const Entry &entry) {
        assert(evaluators.size() == 1);
        auto f = eval_context.get_heuristic_value_or_infinity(evaluators[0]);
        auto g = entry.get_g();

        if (!exists_bucket(f, g)) create_bucket(f, g);
        if (!entry.write(fg_buckets[f][g]))
            throw IOException("Fail to write state to fstream.");
        ++size;
    }

    template<class Entry>
    Entry ExternalTieBreakingOpenList<Entry>::remove_min() {
        //TODO is this really removing the minimum f value?
        assert(size > 0);
        Entry min_entry;

        // tiebreak by lowest f value
        for (auto f_bucket = fg_buckets.begin();
             f_bucket != fg_buckets.end(); ++f_bucket) {
            // tiebreak by highest g value
            for (auto g_bucket = f_bucket->second.rbegin();
                 g_bucket != f_bucket->second.rend(); ++g_bucket) {
                //reverse seek
                g_bucket->second.seekp(-Entry::get_size_in_bytes(), ios::cur);
                
                min_entry.read(g_bucket->second);

                // reurn pointer for subsequent write / read
                g_bucket->second.seekp(-Entry::get_size_in_bytes(), ios::cur);

                // if g bucket is empty
                if (g_bucket->second.tellp() == 0) {
                    auto g = g_bucket->first;
                    f_bucket->second.erase(g);
                    // if f bucket is empty
                    if (f_bucket->second.empty()) {
                        auto f = f_bucket->first;
                        fg_buckets.erase(f);
                    }
                }
                --size;
                return min_entry;
            }
        }
        return min_entry;
    }

    template<class Entry>
    bool ExternalTieBreakingOpenList<Entry>::empty() const {
        return size == 0;
    }

    template<class Entry>
    void ExternalTieBreakingOpenList<Entry>::clear() {
        fg_buckets.clear();
        size = 0;
        // remove empty directory, this fails if directory is not empty
        rmdir("open_list_buckets");
    }

    template<class Entry>
    void ExternalTieBreakingOpenList<Entry>::
    get_involved_heuristics(set<Heuristic *> &hset) {
        for (Evaluator *evaluator : evaluators)
            evaluator->get_involved_heuristics(hset);
    }

    template<class Entry>
    bool ExternalTieBreakingOpenList<Entry>::
    is_dead_end(EvaluationContext &eval_context) const {
        // TODO: Properly document this behaviour.
        // If one safe heuristic detects a dead end, return true.
        if (is_reliable_dead_end(eval_context))
            return true;
        /*
        // If the first heuristic detects a dead-end and we allow "unsafe
        // pruning", return true.
        if (allow_unsafe_pruning &&
            eval_context.is_heuristic_infinite(evaluators[0]))
            return true;
        */
        // Otherwise, return true if all heuristics agree this is a dead-end.
        for (Evaluator *evaluator : evaluators)
            if (!eval_context.is_heuristic_infinite(evaluator))
                return false;
        return true;
    }

    template<class Entry>
    bool ExternalTieBreakingOpenList<Entry>::
    is_reliable_dead_end(EvaluationContext &eval_context) const {
        for (Evaluator *evaluator : evaluators)
            if (eval_context.is_heuristic_infinite(evaluator) &&
                evaluator->dead_ends_are_reliable())
                return true;
        return false;
    }

    template<class Entry>
    string ExternalTieBreakingOpenList<Entry>::
    get_bucket_string(int f, int g) const {
        std::ostringstream oss;
        oss << "open_list_buckets/" <<  f << "_" << g << ".bucket";
        return oss.str();
    }

    template<class Entry>
    bool ExternalTieBreakingOpenList<Entry>::
    exists_bucket(int f, int g) const {
        auto f_bucket = fg_buckets.find(f);
        if (f_bucket == fg_buckets.end()) return false;
        auto g_bucket = f_bucket->second.find(g);
        if (g_bucket == f_bucket->second.end()) return false;
        return true;
    }

    template<class Entry>
    void ExternalTieBreakingOpenList<Entry>::
    create_bucket(int f, int g) {
        // to prevent copying of strings, in-place construction
        fg_buckets[f].emplace(piecewise_construct,
                              forward_as_tuple(g),
                              forward_as_tuple(get_bucket_string(f, g)));
        // TODO: do check
        //if (fg_buckets[f][g].is_open())
    }

    
    ExternalTieBreakingOpenListFactory::
    ExternalTieBreakingOpenListFactory(const Options &options)
        : options(options) {
    }

    unique_ptr<StateOpenList>
    ExternalTieBreakingOpenListFactory::create_state_open_list() {
        return utils::make_unique_ptr
            <ExternalTieBreakingOpenList<StateOpenListEntry>>(options);
    }

    
    static shared_ptr<OpenListFactory> _parse(OptionParser &parser) {
        parser.document_synopsis("Tie-breaking open list", "");
        parser.add_list_option<Evaluator *>("evals", "evaluators");
        // Don't give option for preferred operators and unsafe pruning
        /*
        parser.add_option<bool>(
                                "pref_only",
                                "insert only nodes generated by preferred operators", "false");
        parser.add_option<bool>(
                                "unsafe_pruning",
                                "allow unsafe pruning when the main evaluator regards a state a dead end",
                                "true");
        */
        Options opts = parser.parse();
        opts.verify_list_non_empty<Evaluator *>("evals");
        if (parser.dry_run())
            return nullptr;
        else
            return make_shared<ExternalTieBreakingOpenListFactory>(opts);
    }

    static PluginShared<OpenListFactory> _plugin("external_tiebreaking", _parse);
}


        

        

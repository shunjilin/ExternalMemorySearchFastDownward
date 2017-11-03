#include "astar_ddd_open_list.h"

#include "../../open_list.h"
#include "../../option_parser.h"
#include "../../plugin.h"

#include "../../utils/memory.h"

#include "../utils/named_fstream.h"
#include "../utils/errors.h"

#include "../../global_operator.h"
#include "../../globals.h" // for g_operator

#include "../hash_functions/state_hash.h"
#include "../hash_functions/zobrist.h"

#include <utility>
#include <vector>
#include <string>
#include <memory>
#include <set>
#include <deque>
#include <limits>
#include <cmath> // for pow

// for constructing directory
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

//#define TRANSPOSITION_TABLE // to prune recursive expansion duplicates
#ifdef TRANSPOSITION_TABLE
#include "transposition_table.h"
const size_t TT_SIZE_IN_BYTES = 900 * pow(1024, 2); // 900 mb
#endif

//#define TEST_ASTAR_DDD

#define FG_TIEBREAK // tie break on highest g as well

using namespace std;
using namespace statehash;

namespace astar_ddd_open_list {
    
    enum class BucketType { open, next, closed };
    
    template<class Entry>
    class AStarDDDOpenList : public OpenList<Entry> {

        int n_buckets = 20;

        bool reopen_closed;
        
        int min_f = 0;
#ifdef FG_TIEBREAK
        int max_g = -1;
#endif
        int next_f = numeric_limits<int>::max();
        vector<Evaluator *> evaluators; // f and g
        void remove_duplicates();
        bool first_insert = true; // to initialize min_f
        int current_bucket = 0; // current bucket being expanded
        
        vector<unique_ptr<named_fstream> > open_buckets;
        vector<unique_ptr<named_fstream> > next_buckets;
        vector<unique_ptr<named_fstream> > closed_buckets;

        unique_ptr<named_fstream> recursive_bucket; // for recursive expansion

        void create_bucket(int bucket_index, BucketType bucket_type);
        string get_bucket_string(int bucket_index, BucketType bucket_type) const;

        void initialize();

        size_t recursive_expansions = 0;
        size_t max_bucket_size_in_bytes = 0;
#ifdef TRANSPOSITION_TABLE
        TranspositionTable<Entry> transposition_table =
            TranspositionTable<Entry>(TT_SIZE_IN_BYTES);
#endif
    protected:
        virtual void do_insertion(EvaluationContext &eval_context,
                                  const Entry &entry) override;

    public:
        explicit AStarDDDOpenList(const Options &opts);
        virtual ~AStarDDDOpenList() override = default;

        virtual Entry remove_min() override;
        virtual void clear() override;
        virtual bool empty() const override;
        virtual void get_involved_heuristics(set<Heuristic *> &hset) override;
        virtual bool is_dead_end(EvaluationContext &eval_context) const override;
        virtual bool is_reliable_dead_end(EvaluationContext &eval_context) const override;

        virtual vector<const GlobalOperator*> trace_path(const Entry &entry) override;
    };

    template<class Entry>
    void AStarDDDOpenList<Entry>::initialize() {
        Entry::initialize_hash_function(utils::make_unique_ptr<ZobristHash<Entry> >());
#ifdef TRANSPOSITION_TABLE
        transposition_table.initialize();
#endif
    }
    
    template<class Entry>
    AStarDDDOpenList<Entry>::AStarDDDOpenList(const Options &opts) :
        reopen_closed(opts.get<bool>("reopen_closed")),
        evaluators(opts.get_list<Evaluator *>("evals")),
        open_buckets(n_buckets),
        next_buckets(n_buckets),
        closed_buckets(n_buckets)
    {
        // create directory for open list files if not exist
        mkdir("open_list_buckets", 0744);
        
        recursive_bucket = utils::make_unique_ptr<named_fstream>
            (string("open_list_buckets/recursive.bucket"));

        // create buckets
        for (int i = 0; i < n_buckets; ++i) {
            create_bucket(i, BucketType::open);
            create_bucket(i, BucketType::next);
            create_bucket(i, BucketType::closed);
        }

        cout << "Number of hash buckets: " << n_buckets
             << "\nMax size of transposition table in bytes: "
#ifdef TRANSPOSITION_TABLE
             << TT_SIZE_IN_BYTES
#endif
             << "\n" << endl;
    }

    template<class Entry>
    void AStarDDDOpenList<Entry>::
    remove_duplicates() {
        min_f = numeric_limits<int>::max();
#ifdef FG_TIEBREAK
        max_g = -1;
#endif
        
        for (int i = 0; i < n_buckets; ++i) { // for each bucket
            unordered_set<Entry> hash_table;
            // hash next list entries
            next_buckets[i]->clear();
            next_buckets[i]->seekg(0, ios::beg);
            Entry next_entry;
            next_entry.read(*next_buckets[i]);
            while (!next_buckets[i]->eof()) {
                auto it = hash_table.find(next_entry);
                if (it != hash_table.end()) {
                    if (it->get_g() > next_entry.get_g()) {
                        hash_table.erase(it);
                        hash_table.insert(next_entry);
                    }
                } else {
                    hash_table.insert(next_entry);
                }
                next_entry.read(*next_buckets[i]);
            }

            size_t bucket_size_in_bytes = hash_table.size() * sizeof(Entry);
            if (bucket_size_in_bytes > max_bucket_size_in_bytes)
                max_bucket_size_in_bytes = bucket_size_in_bytes;
            next_buckets[i].reset(nullptr);
            create_bucket(i, BucketType::next);
            next_buckets[i]->clear();
            next_buckets[i]->seekg(0, ios::beg);

            // hash closed list entries against next list entries, deleting
            // duplicates
            closed_buckets[i]->clear();
            closed_buckets[i]->seekg(0, ios::beg);
            Entry closed_entry;
            closed_entry.read(*closed_buckets[i]);
            while (!closed_buckets[i]->eof()) {
                auto it = hash_table.find(closed_entry);
                if (it != hash_table.end()) {
                    hash_table.erase(it);
                }
                closed_entry.read(*closed_buckets[i]);
            }
            closed_buckets[i]->clear();
            closed_buckets[i]->seekg(0, ios::end);

            open_buckets[i].reset(nullptr); // erase old open bucket
            create_bucket(i, BucketType::open);
            open_buckets[i]->clear();
            open_buckets[i]->seekg(0, ios::beg);
            
            for (auto& entry : hash_table) {
                EvaluationContext eval_context(entry, false, nullptr);
                int f = eval_context.get_heuristic_value(evaluators[0]);
                if (f < min_f) {
                    min_f = f;
#ifdef FG_TIEBREAK
                    max_g = entry.get_g(); 
#endif
                }
#ifdef FG_TIEBREAK
                else if (f == min_f && entry.get_g() > max_g) {
                    max_g = entry.get_g();
                }
#endif
                entry.write(*open_buckets[i]);
            }
            // reset open
            open_buckets[i]->clear();
            open_buckets[i]->seekg(0, ios::beg);

#ifdef TEST_ASTAR_DDD // test if duplicate free
            unordered_set<Entry> test_table;
            closed_buckets[i]->clear();
            closed_buckets[i]->seekg(0, ios::beg);
            //Entry closed_entry;
            closed_entry.read(*closed_buckets[i]);
            while (!closed_buckets[i]->eof()) {
                auto it = test_table.find(closed_entry);
                //if (it != test_table.end())
                //  cout << "duplicate closed node!" << endl;
                test_table.insert(closed_entry);
                closed_entry.read(*closed_buckets[i]);
            }
            closed_buckets[i]->clear();
            closed_buckets[i]->seekg(0, ios::end);
            
            open_buckets[i]->clear();
            open_buckets[i]->seekg(0, ios::beg);
            Entry open_entry;
            open_entry.read(*open_buckets[i]);
            while (!open_buckets[i]->eof()) {
                auto it = test_table.find(open_entry);
                if (it != test_table.end())
                    cout << "duplicate open node!" << endl;
                open_entry.read(*open_buckets[i]);
            }
            open_buckets[i]->clear();
            open_buckets[i]->seekg(0, ios::beg);
            
#endif
        
        }
        // No more entries
        if (min_f == numeric_limits<int>::max())
            throw OpenListEmpty();
    }

    template<class Entry>
    void AStarDDDOpenList<Entry>::
    do_insertion(EvaluationContext &eval_context, const Entry &entry) {
        assert(evaluators.size() == 1);
#ifdef FG_TIEBREAK
        if (!first_insert &&
            eval_context.get_heuristic_value(evaluators[0]) == min_f &&
            entry.get_g() == max_g) {
#else
        if (!first_insert &&
            eval_context.get_heuristic_value(evaluators[0]) <= min_f) {
#endif
#ifdef TRANSPOSITION_TABLE
            if (transposition_table.find_insert(entry)) return;
#endif
            entry.write(*recursive_bucket);
            ++recursive_expansions;
            return;
        }
        
        int bucket_index = entry.get_hash_value() % n_buckets;
    
        if (first_insert) {
            auto f = eval_context.get_heuristic_value_or_infinity(evaluators[0]);
            initialize();
            min_f = f;
#ifdef FG_TIEBREAK
            max_g = entry.get_g();
#endif
            entry.write(*open_buckets[bucket_index]);
            open_buckets[bucket_index]->clear();
            open_buckets[bucket_index]->seekg(0, ios::beg);
            first_insert = false;
            return;
        }
        
        if (!entry.write(*next_buckets[bucket_index]))
            throw IOException("Fail to write state to fstream.");

    }

    template<class Entry>
    Entry AStarDDDOpenList<Entry>::remove_min() {
        Entry min_entry;
        if (recursive_bucket->tellg() != 0) {
            recursive_bucket->seekg(-Entry::get_size_in_bytes(), ios::cur);
            min_entry.read(*recursive_bucket);
            recursive_bucket->seekg(-Entry::get_size_in_bytes(), ios::cur);
            min_entry.write(*closed_buckets[min_entry.get_hash_value() % n_buckets]);
            return min_entry;
        }
    
        while (current_bucket != n_buckets) {
            // attempt read from current bucket
            min_entry.read(*open_buckets[current_bucket]);
            
            if (open_buckets[current_bucket]->eof()) { // exhausted current bucket
                ++current_bucket;
                continue;
            }
           
            EvaluationContext eval_context(min_entry, false, nullptr);
            int entry_f = eval_context.get_heuristic_value(evaluators[0]);
#ifdef FG_TIEBREAK
            if (entry_f == min_f && min_entry.get_g() == max_g) {
#else
            if (entry_f == min_f) {
#endif
                min_entry.write(*closed_buckets[current_bucket]);
                return min_entry;

            } else {
                // transfer unexpanded to next bucket
                min_entry.write(*next_buckets[current_bucket]);
            }
        }
        // exhausted all buckets

#ifdef TRANSPOSITION_TABLE
        transposition_table.clear();
#endif
        remove_duplicates();
#ifdef TRANSPOSITION_TABLE
        transposition_table.initialize();
#endif
        current_bucket = 0;
        recursive_bucket.reset(nullptr);
        recursive_bucket =
            utils::make_unique_ptr<named_fstream>
            (string("open_list_buckets/recursive.bucket"));
             
        return remove_min();
    }

    // keeping track of number of states is not trivial using a counter,
    // in the case of delayed duplicate detection, and we use error handling
    // instead
    template<class Entry>
    bool AStarDDDOpenList<Entry>::empty() const {
        return false;
    }


    template<class Entry>
    void AStarDDDOpenList<Entry>::clear() {
        open_buckets.clear();
        next_buckets.clear();
        closed_buckets.clear();
        // remove empty directory, this fails if directory is not empty
        rmdir("open_list_buckets");
        cout << "Number of recursive expansions: " << recursive_expansions << "\n";
        cout << "Max bucket size in bytes: " << max_bucket_size_in_bytes << endl;
    }

    template<class Entry>
    void AStarDDDOpenList<Entry>::
    get_involved_heuristics(set<Heuristic *> &hset) {
        for (Evaluator *evaluator : evaluators)
            evaluator->get_involved_heuristics(hset);
    }

    template<class Entry>
    bool AStarDDDOpenList<Entry>::
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
    bool AStarDDDOpenList<Entry>::
    is_reliable_dead_end(EvaluationContext &eval_context) const {
        for (Evaluator *evaluator : evaluators)
            if (eval_context.is_heuristic_infinite(evaluator) &&
                evaluator->dead_ends_are_reliable())
                return true;
        return false;
    }


    template<class Entry>
    string AStarDDDOpenList<Entry>::
    get_bucket_string(int bucket_index, BucketType bucket_type) const {
        string bucket_type_str;
        if (bucket_type == BucketType::open) bucket_type_str = "open";
        if (bucket_type == BucketType::next) bucket_type_str = "next";
        if (bucket_type == BucketType::closed) bucket_type_str = "closed";
        std::ostringstream oss;
        oss << "open_list_buckets/" <<  bucket_index << "_" << bucket_type_str << ".bucket";
        return oss.str();
    }

    template<class Entry>
    void AStarDDDOpenList<Entry>::
    create_bucket(int bucket_index, BucketType bucket_type) {
        if (bucket_type == BucketType::open) {
            open_buckets[bucket_index] =
                utils::make_unique_ptr<named_fstream>
                (get_bucket_string(bucket_index, bucket_type));
            if (!open_buckets[bucket_index]->is_open())
                throw IOException("Fail to open open list fstream.");
        }
        if (bucket_type == BucketType::next) {
            next_buckets[bucket_index] =
                utils::make_unique_ptr<named_fstream>
                (get_bucket_string(bucket_index,bucket_type));
            if (!next_buckets[bucket_index]->is_open())
                throw IOException("Fail to open open list fstream.");
        }
        if (bucket_type == BucketType::closed) {
            closed_buckets[bucket_index] =
                utils::make_unique_ptr<named_fstream>
                (get_bucket_string(bucket_index, bucket_type));
            if (!closed_buckets[bucket_index]->is_open())
                throw IOException("Fail to open open list fstream.");
        }
    }

    template<class Entry>
    vector<const GlobalOperator *> AStarDDDOpenList<Entry>::
    trace_path(const Entry &entry) {
        vector<const GlobalOperator *> path;
        Entry current_state = entry;
        
        while (true) {
        startloop:
            if (current_state.get_creating_operator() == -1) {
                assert(current_state.get_parent_state_id == StateID::no_state);
                break;
            }
            const GlobalOperator *op =
                &g_operators[current_state.get_creating_operator()];
            path.push_back(op);

            // check parent hash bucket only
            int bucket_index = current_state.get_parent_hash_value() % n_buckets;
            closed_buckets[bucket_index]->clear();
            closed_buckets[bucket_index]->seekg(0, ios::beg);

            Entry closed_entry;
            closed_entry.read(*closed_buckets[bucket_index]);
            while (!closed_buckets[bucket_index]->eof()) {
                if (closed_entry.get_state_id() ==
                    current_state.get_parent_state_id()) {
                    current_state = closed_entry;
                    goto startloop;
                }
                closed_entry.read(*closed_buckets[bucket_index]);
            }
            break; // parent not found
        }
        reverse(path.begin(), path.end());
        return path;
    }

    
    AStarDDDOpenListFactory::
    AStarDDDOpenListFactory(const Options &options)
        : options(options) {
    }

    unique_ptr<StateOpenList>
    AStarDDDOpenListFactory::create_state_open_list() {
        return utils::make_unique_ptr
            <AStarDDDOpenList<StateOpenListEntry>>(options);
    }

    
    static shared_ptr<OpenListFactory> _parse(OptionParser &parser) {
        parser.document_synopsis("A*-DDD open list", "");
        parser.add_list_option<Evaluator *>("evals", "evaluators");
        Options opts = parser.parse();
        opts.verify_list_non_empty<Evaluator *>("evals");
        if (parser.dry_run())
            return nullptr;
        else
            return make_shared<AStarDDDOpenListFactory>(opts);
    }
    static PluginShared<OpenListFactory> _plugin("astar_ddd_open", _parse);
}       

#include "external_astar_open_list.h"

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
#include <memory>

// for constructing directory
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
using namespace std;

// TODO: How to check if unit cost and consistent heuristic?
// TODO: Should throw error if non unit cost domain (perhaps in search engine)

namespace external_astar_open_list {
    template<class Entry>
    class ExternalAStarOpenList : public OpenList<Entry> {

        map<int, map<int, named_fstream> > fg_buckets;

        pair<int, int> current_fg; // to track when merge needs to be performed
        
        int size;

        vector<Evaluator *> evaluators;

        //bool allow_unsafe_pruning;

        void update_fg();

        void merge_and_remove_duplicates(int f, int g);

    protected:
        virtual void do_insertion(EvaluationContext &eval_context,
                                  const Entry &entry) override;

    public:
        explicit ExternalAStarOpenList(const Options &opts);
        virtual ~ExternalAStarOpenList() override = default;

        virtual Entry remove_min() override;
        virtual void clear() override;
        virtual bool empty() const override;
        virtual void get_involved_heuristics(set<Heuristic *> &hset) override;
        virtual bool is_dead_end(EvaluationContext &eval_context) const override;
        virtual bool is_reliable_dead_end(EvaluationContext &eval_context) const override;
        string get_bucket_string(int f, int g) const;
        bool exists_bucket(int f, int g) const; // TODO: these should be private?
        void create_bucket(int f, int g);
    };

    
    template<class Entry>
    ExternalAStarOpenList<Entry>::ExternalAStarOpenList(const Options &opts)
        : OpenList<Entry>(false), //opts.get<bool>("pref_only")),
        size(0), evaluators(opts.get_list<Evaluator *>("evals")) {
        // create directory for open list files if not exist
        mkdir("open_list_buckets", 0744);
    }

    template<class Entry>
    void ExternalAStarOpenList<Entry>::
    merge_and_remove_duplicates(int f, int g) {
        assert(exists_bucket(f, g));
        auto& target_stream = fg_buckets[f][g];

        //Perform External MergeSort
        //First Pass
        int k = 0;
        vector<streampos> k_offsets;

        // create buffer for first pass (500mb)
        size_t block_entries = 4294967296 / Entry::size_in_bytes; // round down
        vector<Entry> block;
        block.reserve(block_entries);
        
        named_fstream holding(string("temp.bucket")); // holding bucket

        k_offsets.push_back(holding.tellg());

        auto end_of_bucket = target_stream.tellg();
        target_stream.seekg(0);
        // is there more efficient way
        while (target_stream.tellg() != end_of_bucket) {
            Entry entry;
            entry.read(target_stream);
            block.push_back(entry);
            // flush block
            if (block.size() == block_entries) {
                // sort
                sort(block.begin(), block.end());
                for (auto& entry: block) {
                    entry.write(holding);
                }
                k_offsets.push_back(holding.tellg());
            }
        }
        // TODO: delete target stream here

        vector<Entry>().swap(block); // clear block

        // erase bucket to remove file
        fg_buckets[f].erase(g); //careful! target_stream dangles
        
        
        // Merge step
        size_t buffer_entries = 4096 / Entry::size_in_bytes;
        auto k_value = k_offsets.size();
        vector< vector<Entry> > merge_buffers(k_value);

        auto current_k_offsets = k_offsets; // track increments to offsets

        holding.seekg(0, holding.end);
        k_offsets.push_back(holding.tellg()); // to track end of last merge buffer

        // initial fill of buffers
        for (size_t k = 0; k < k_value; ++k) {
            holding.seekg(current_k_offsets[k]);
            for (size_t i = 0; i < buffer_entries; ++i) {
                if (holding.tellg() >= k_offsets[k + 1]) break; // end of block
                Entry entry;
                entry.read(holding);
                merge_buffers[k].push_back(entry);
            }
            sort(merge_buffers[k].rbegin(), merge_buffers[k].rend());
        }

        // Pointers to other files
        shared_ptr<named_fstream> duplicate_stream_1;
        streampos duplicate_stream_1_end;
        Entry duplicate_entry_1;
        
        shared_ptr<named_fstream> duplicate_stream_2;
        streampos duplicate_stream_2_end;
        Entry duplicate_entry_2;
 
        if (exists_bucket(f-1, g-1)) {
            duplicate_stream_1 =
                make_shared<named_fstream>(fg_buckets[f-1][g-1]);
            duplicate_stream_1_end = duplicate_stream_1->tellg();
            duplicate_stream_1->seekg(0);
            duplicate_entry_1.read(duplicate_stream_1);
        }
        if (exists_bucket(f-2, g-2)) {
            duplicate_stream_2 =
                make_shared<named_fstream>(fg_buckets[f-2][g-2]);
            duplicate_stream_2_end = duplicate_stream_2->tellg();
            duplicate_stream_2->seekg(0);
            duplicate_entry_2.read(duplicate_stream_2);
        }

        // create output bucket
        create_bucket(f, g);
        target_stream = fg_buckets[f][g];
        // output buffer
        vector<Entry> output_buffer;
        std::unique_ptr<Entry> previous_entry;

        while (true) {
            bool end_of_merge = true;
            // first find min entry
            size_t min_index;
            unique_ptr<Entry> min_entry;
            //Entry min_entry = merge_buffers[min_index].back();
            for (size_t k = 1; k < k_value; ++k) {
                if (merge_buffers[k].empty()) {
                    if (current_k_offsets[k] == k_offsets[k+1]) continue; // nothing to fetch
                    // fill merge buffer
                    holding.seekg(current_k_offsets[k]);
                    for (size_t i = 0; i < buffer_entries; ++i) {
                        if (holding.tellg() >= k_offsets[k + 1]) break; // end of block
                        Entry entry;
                        entry.read(holding);
                        merge_buffers[k].push_back(entry);
                    }
                }
                end_of_merge = false;
                auto& entry = merge_buffers[k].back();
                if (!min_entry || entry < *min_entry) {
                    min_entry = utils::make_unique_ptr(entry);
                    min_index = k;
                }
            }
            if (end_of_merge) {
                for (auto& entry : output_buffer) {
                    entry.write(target_stream);
                }
                break;
            }

            // duplicate detection
            if (previous_entry) {
                if (*min_entry == *previous_entry) continue;
            }
            if (duplicate_stream_1) {
                if (*min_entry == duplicate_entry_1) continue;
                while (*min_entry > duplicate_entry_1) { // advance duplicate stream
                    if (duplicate_stream_1->tellg() == duplicate_stream_1_end) {
                        duplicate_stream_1 = nullptr;
                        break;
                    }
                    duplicate_entry_1.read(duplicate_stream_1);
                }
            }

            if (duplicate_stream_2) {
                if (*min_entry == duplicate_entry_2) continue;
                while (*min_entry > duplicate_entry_2) { // advance duplicate stream
                    if (duplicate_stream_2->tellg() == duplicate_stream_2_end) {
                        duplicate_stream_2 = nullptr;
                        break;
                    }
                    duplicate_entry_2.read(duplicate_stream_2);
                }
            }

            output_buffer.emplace_back(*min_entry);
            if (output_buffer.size() == buffer_entries) {
                for (auto& entry : output_buffer) {
                    entry.write(target_stream);
                }
            }              
        }
    }

    template<class Entry>
    void ExternalAStarOpenList<Entry>::
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
    Entry ExternalAStarOpenList<Entry>::remove_min() {
        //TODO is this really removing the minimum f value?
        assert(size > 0);
        Entry min_entry;

        // tiebreak by lowest f value
        for (auto f_bucket = fg_buckets.begin();
             f_bucket != fg_buckets.end(); ++f_bucket) {
            // tiebreak by lowest g value
            for (auto g_bucket = f_bucket->second.begin();
                 g_bucket != f_bucket->second.end(); ++g_bucket) {

                //reverse seek
                g_bucket->second.seekp(-Entry::size_in_bytes, ios::cur);
                
                min_entry.read(g_bucket->second);

                // reurn pointer for subsequent write / read
                g_bucket->second.seekp(-Entry::size_in_bytes, ios::cur);

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
    bool ExternalAStarOpenList<Entry>::empty() const {
        return size == 0;
    }

    template<class Entry>
    void ExternalAStarOpenList<Entry>::clear() {
        fg_buckets.clear();
        size = 0;
        // remove empty directory, this fails if directory is not empty
        rmdir("open_list_buckets");
    }

    template<class Entry>
    void ExternalAStarOpenList<Entry>::
    get_involved_heuristics(set<Heuristic *> &hset) {
        for (Evaluator *evaluator : evaluators)
            evaluator->get_involved_heuristics(hset);
    }

    template<class Entry>
    bool ExternalAStarOpenList<Entry>::
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
    bool ExternalAStarOpenList<Entry>::
    is_reliable_dead_end(EvaluationContext &eval_context) const {
        for (Evaluator *evaluator : evaluators)
            if (eval_context.is_heuristic_infinite(evaluator) &&
                evaluator->dead_ends_are_reliable())
                return true;
        return false;
    }

    template<class Entry>
    string ExternalAStarOpenList<Entry>::
    get_bucket_string(int f, int g) const {
        std::ostringstream oss;
        oss << "open_list_buckets/" <<  f << "_" << g << ".bucket";
        return oss.str();
    }

    template<class Entry>
    bool ExternalAStarOpenList<Entry>::
    exists_bucket(int f, int g) const {
        auto f_bucket = fg_buckets.find(f);
        if (f_bucket == fg_buckets.end()) return false;
        auto g_bucket = f_bucket->second.find(g);
        if (g_bucket == f_bucket->second.end()) return false;
        return true;
    }

    template<class Entry>
    void ExternalAStarOpenList<Entry>::
    create_bucket(int f, int g) {
        // to prevent copying of strings, in-place construction
        fg_buckets[f].emplace(piecewise_construct,
                              forward_as_tuple(g),
                              forward_as_tuple(get_bucket_string(f, g)));
        // TODO: do check
        //if (fg_buckets[f][g].is_open())
    }

    
    ExternalAStarOpenListFactory::
    ExternalAStarOpenListFactory(const Options &options)
        : options(options) {
    }

    unique_ptr<StateOpenList>
    ExternalAStarOpenListFactory::create_state_open_list() {
        return utils::make_unique_ptr
            <ExternalAStarOpenList<StateOpenListEntry>>(options);
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
            return make_shared<ExternalAStarOpenListFactory>(opts);
    }

    static PluginShared<OpenListFactory> _plugin("external_astar_open", _parse);
}


        

        

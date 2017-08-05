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
#include <deque>

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
        int size; // TODO: Track size properly! eliminations due to duplicate detection
        vector<Evaluator *> evaluators;
        void remove_duplicates(int f, int g);
        bool first_insert = true; // to initialize current_fg

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
    remove_duplicates(int f, int g) {
        // Performs k-way External Merge Sort, where k is size of the file
        // divided by the memory buffer allocated for one run (block).
        // Also performs duplicate detection against itself, and the buffers
        // f-1, g-1 and f-2, g-2, as per External A* (Edelkamp)

        named_fstream * target_stream = &fg_buckets[f][g];
        target_stream->clear();
        target_stream->seekg(0, ios::beg);
        
        vector<streampos> k_offsets; // keeps track of divisions in merge file

        // Allocate ~500mb for one block
        size_t block_entries = pow(2,32) / (8 * Entry::size_in_bytes); // round down
        vector<Entry> block;
        block.reserve(block_entries);
        
        named_fstream sorted_blocks("temp.bucket");
 
        Entry entry;
        entry.read(*target_stream);
        while (!target_stream->eof()) {
            block.push_back(entry);
            // flush block if full
            if (block.size() == block_entries) {
                // sort
                sort(block.begin(), block.end());
                for (auto& blk_entry: block) {
                    blk_entry.write(sorted_blocks);
                }
                k_offsets.push_back(sorted_blocks.tellg());
            }
            entry.read(*target_stream);
        }
        // flush remainder
        sort(block.begin(), block.end());
        for (auto& blk_entry : block) {
            blk_entry.write(sorted_blocks);
        }
        k_offsets.push_back(sorted_blocks.tellg());

        vector<Entry>().swap(block); // clear block
        fg_buckets[f].erase(g); // erase bucket to remove file
        target_stream = nullptr;
        
        // Merge step
        size_t buffer_entries = 4096 / Entry::size_in_bytes;
        auto k_value = k_offsets.size();
        vector< deque<Entry> > merge_buffers(k_value);

        vector<streampos> current_k_offsets; //track increment to offsets
        current_k_offsets.push_back(0);
        current_k_offsets.insert(current_k_offsets.end(), k_offsets.begin(),
                                 k_offsets.end() - 1);

        // initial fill of buffers
        for (size_t k = 0; k < k_value; ++k) {
            sorted_blocks.seekg(current_k_offsets[k]);
            for (size_t i = 0; i < buffer_entries; ++i) {
                if (sorted_blocks.tellg() >= k_offsets[k]) break; // end of block
                Entry entry;
                entry.read(sorted_blocks);
                merge_buffers[k].push_back(entry);
            }
            current_k_offsets[k] = sorted_blocks.tellg();
        }

        // For duplicate detection against other buckets
        named_fstream* duplicate_stream_1 = nullptr;
        Entry duplicate_entry_1;

        named_fstream* duplicate_stream_2 = nullptr;
        Entry duplicate_entry_2;
 
        if (exists_bucket(f-1, g-1)) {
            duplicate_stream_1 = &(fg_buckets[f-1][g-1]);
            duplicate_stream_1->clear();   
            duplicate_stream_1->seekg(0, ios::beg);
            duplicate_entry_1.read(*duplicate_stream_1);
        }
        if (exists_bucket(f-2, g-2)) {
            duplicate_stream_2 = &(fg_buckets[f-2][g-2]);
            duplicate_stream_2->clear();
            duplicate_stream_2->seekg(0, ios::beg);
            duplicate_entry_2.read(*duplicate_stream_2);
        }

        // create output bucket to store non-duplicate entries
        create_bucket(f, g);
        target_stream = &fg_buckets[f][g];
        target_stream->clear();
        target_stream->seekg(0, ios::beg);
        
        // output buffer
        vector<Entry> output_buffer;
        output_buffer.reserve(buffer_entries);
        std::unique_ptr<Entry> previous_entry; // track intra bucket duplicates
        
        while (true) {
            bool end_of_merge = true; // flag to terminate output step

            // look for minimum entry amongst all buffers
            size_t min_index;
            unique_ptr<Entry> min_entry = nullptr;
           
            for (size_t k = 0; k < k_value; ++k) {
                if (merge_buffers[k].empty()) {
                    if (current_k_offsets[k] == k_offsets[k]) continue; // nothing to fetch
                    sorted_blocks.seekg(current_k_offsets[k]); // fill empty merge buffer
                    for (size_t i = 0; i < buffer_entries; ++i) {
                        if (sorted_blocks.tellg() >= k_offsets[k]) break; // end of block
                        Entry entry;
                        entry.read(sorted_blocks);
                        merge_buffers[k].push_back(entry); // change this to a deque
                    }
                    current_k_offsets[k] = sorted_blocks.tellg();
                }
                end_of_merge = false; // exists an unprocessed entry
                auto& entry = merge_buffers[k].front();
                if (!min_entry || entry < *min_entry) { // set candidate min entry
                    min_entry = utils::make_unique_ptr<Entry>(entry);
                    min_index = k;
                }
            }
            if (end_of_merge) break;

            // remove min_entry from merge buffer
            merge_buffers[min_index].pop_front();

            // intra bucket duplicate detection
            if (previous_entry) {
                if (*min_entry == *previous_entry) {
                    continue;
                }
            }

            // inter bucket duplicate detection
            if (duplicate_stream_1 != nullptr) {
                while (*min_entry > duplicate_entry_1) { // align streams
                    duplicate_entry_1.read(*duplicate_stream_1);
                    if (duplicate_stream_1->eof()) {
                        duplicate_stream_1 = nullptr;
                        break;
                    }
                }
                if (*min_entry == duplicate_entry_1) {
                    continue;
                } 
            }

            // inter bucket duplicate detection
            if (duplicate_stream_2 != nullptr) {
                while (*min_entry > duplicate_entry_2) { // align streams
                    duplicate_entry_2.read(*duplicate_stream_2);
                    if (duplicate_stream_2->eof()) {
                        duplicate_stream_2 = nullptr;
                        break;
                    }
                }
                if (*min_entry == duplicate_entry_2) {
                    continue;
                }
            }

            // process minimum entry
            output_buffer.emplace_back(*min_entry);
            previous_entry = utils::make_unique_ptr<Entry>(*min_entry);
            // flush
            if (output_buffer.size() == buffer_entries) {
                for (auto& entry : output_buffer) {
                    entry.write(*target_stream);
                }
            output_buffer.clear();
            }
        }

        // flush any remainders
        for (auto& entry : output_buffer) {
            entry.write(*target_stream);
        }

        target_stream->clear();
        target_stream->seekg(0, ios::beg);
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

        if (first_insert) {
            current_fg = make_pair(f, g);
            fg_buckets[f][g].seekg(0, ios::beg);
            fg_buckets[f][g].clear();
            first_insert = false;
        }

    }

    template<class Entry>
    Entry ExternalAStarOpenList<Entry>::remove_min() {
        
        assert(size > 0);
        Entry min_entry;

        int f, g;
        tie(f, g) = current_fg;

        // attempt to read
        min_entry.read(fg_buckets[f][g]);

        // update f, g values, and perform duplicate detection
        if (fg_buckets[f][g].eof()) {
            auto g_bucket = fg_buckets[f].begin();
            while (g_bucket != fg_buckets[f].end() && g_bucket->first <= g) ++g_bucket;
            if (g_bucket == fg_buckets[f].end()) {
                // find lowest f
                auto f_bucket = fg_buckets.begin();
                while (f_bucket != fg_buckets.end() && f_bucket->first <= f) ++f_bucket;
                f = f_bucket->first;
                g = f_bucket->second.begin()->first;
            } else {
                g = g_bucket->first;
            }
            current_fg = make_pair(f, g);
            
            remove_duplicates(f, g);

            // Test if vector is duplicate free
            /*
            vector<Entry> duplicate_vector;
            if (exists_bucket(f-1, g-1)) {
                fg_buckets[f-1][g-1].clear();
                fg_buckets[f-1][g-1].seekg(0);
                Entry entry;
                entry.read(fg_buckets[f-1][g-1]);
                while (!fg_buckets[f-1][g-1].eof()) {
                    duplicate_vector.push_back(entry);
                    entry.read(fg_buckets[f-1][g-1]);
                }
            }
            if (exists_bucket(f-2, g-2)) {
                fg_buckets[f-2][g-2].clear();
                fg_buckets[f-2][g-2].seekg(0);
                Entry entry;
                entry.read(fg_buckets[f-2][g-2]);
                while (!fg_buckets[f-2][g-2].eof()) {
                    duplicate_vector.push_back(entry);
                    entry.read(fg_buckets[f-2][g-2]);
                }
            }
            Entry entry;
            entry.read(fg_buckets[f][g]);
            while (!fg_buckets[f][g].eof()) {
                duplicate_vector.push_back(entry);
                entry.read(fg_buckets[f][g]);
            }

            set<Entry> duplicate_set(duplicate_vector.begin(), duplicate_vector.end());
            cout << "duplicate set size : " << duplicate_set.size()
                 << " duplicate vec size : " << duplicate_vector.size() << endl;
            if (duplicate_set.size() != duplicate_vector.size()) throw;

            fg_buckets[f][g].clear();
            fg_buckets[f][g].seekg(0, ios::beg);
            */
            return remove_min();
        }
        
        --size;

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


        

        

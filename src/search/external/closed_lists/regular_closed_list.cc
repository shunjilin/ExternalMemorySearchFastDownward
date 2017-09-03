#include "regular_closed_list.h"

#include "../closed_list.h"
#include "../../option_parser.h"
#include "../../plugin.h"
#include "../../global_operator.h"
#include "../../utils/memory.h"
#include "../../globals.h"
#include "../hash_functions/state_hash.h"
#include "../hash_functions/zobrist.h"
#include "../utils/errors.h"

#include <vector>
#include <memory>
#include <unordered_set>
#include <cmath> // for pow

using namespace std;
using namespace statehash;

using found = bool;
using reopened = bool;

namespace regular_closed_list {
    template<class Entry>
    class RegularClosedList : public ClosedList<Entry> { 
        bool reopen_closed;
        unordered_set<Entry> closed;

       
        
    public:
        explicit RegularClosedList(const Options &opts);
        virtual ~RegularClosedList() override = default;

        virtual pair<found, reopened> find_insert(const Entry &entry) override;
        virtual vector<const GlobalOperator*> trace_path(const Entry &entry)
            const override;

        virtual void clear() override;
        virtual void print_statistics() const override;
    };

    template<class Entry>
    RegularClosedList<Entry>::RegularClosedList(const Options &opts)
        : ClosedList<Entry>(opts.get<bool>("reopen_closed"))
    {
        Entry::initialize_hash_function(utils::make_unique_ptr<ZobristHash<Entry> >());
       
    }



    template<class Entry>
    pair<found, reopened> RegularClosedList<Entry>::
    find_insert(const Entry &entry) {

        auto it = closed.find(entry);
        if (it != closed.end()) {
            if (reopen_closed && (entry.get_g() < it->get_g())) {
                closed.erase(*it);
                closed.insert(entry);
                return make_pair(true, true);
            }
            return make_pair(true, false);
        }
        closed.insert(entry);
        return make_pair(false, false);
    }

    template<class Entry>
    struct EqById {
        bool operator()(const Entry& lhs, const Entry& rhs) const {
            return lhs.get_state_id() == rhs.get_state_id();
        }
    };

    template<class Entry>
    struct HashById {
        size_t operator() (const Entry& entry) const {
            return entry.get_state_id().hash();
        }
    };
    

    template<class Entry>
    vector<const GlobalOperator *> RegularClosedList<Entry>::
    trace_path(const Entry &entry) const {
        //unordered_set<Entry, HashById<Entry>, EqById<Entry> > path_closed;
        //path_closed.insert(closed.begin(), closed.end());
        
        vector<const GlobalOperator *> path;
        Entry current_state = entry;
        /*
        while (true) {
            if (current_state.get_creating_operator() == -1) {
                assert(current_state.get_parent_state_id == StateID::no_state);
                break;
            }
            // use of g_operators creates dependency on globals.h
            const GlobalOperator *op =
                &g_operators[current_state.get_creating_operator()];
            path.push_back(op);
            
          
            auto reference_node = Entry(current_state.get_parent_state_id());
            auto parent = path_closed.find(reference_node);
            current_state = Entry(*parent);
            
        }
        reverse(path.begin(), path.end());
        */
        return path;
    }

    template<class Entry>
    void RegularClosedList<Entry>::clear() {
    }

    template<class Entry>
    void RegularClosedList<Entry>::print_statistics() const {
    }

    RegularClosedListFactory::
    RegularClosedListFactory(const Options &options)
        : options(options) {
    }

    unique_ptr<StateClosedList>
    RegularClosedListFactory::create_state_closed_list() {
        return utils::make_unique_ptr
            <RegularClosedList<StateClosedListEntry> >(options);
    }

    static shared_ptr<ClosedListFactory> _parse(OptionParser &parser) {
        // These are set in search_engines/search_common
        parser.document_synopsis("Compress closed list", "");
        parser.add_option<bool>(
                                "reopen_closed",
                                "reopen closed nodes with lower g values");
        Options opts = parser.parse();
        if (parser.dry_run())
            return nullptr;
        else
            return make_shared<RegularClosedListFactory>(opts);
    }

    static PluginShared<ClosedListFactory> _plugin("regular", _parse);
}

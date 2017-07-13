#include "compress_closed_list.h"

#include "../closed_list.h"
#include "../../option_parser.h"
#include "../../plugin.h"

#include "../../utils/memory.h"
#include <unordered_set>

using namespace std;

using found = bool;
using reopened = bool;

namespace compress_closed_list {
    template<class Entry>
    class CompressClosedList : public ClosedList<Entry> {
        unordered_set<Entry> closed;
        bool reopen_closed;
    public:
        explicit CompressClosedList(const Options &opts);
        virtual ~CompressClosedList() override = default;

        virtual std::pair<found, reopened> find_insert(const Entry &entry) override;
        
    };


    template<class Entry>
    CompressClosedList<Entry>::CompressClosedList(const Options &opts)
        : ClosedList<Entry>(opts.get<bool>("reopen_closed")) {
    }

    template<class Entry>
    std::pair<found, reopened> CompressClosedList<Entry>::
    find_insert(const Entry &entry) {
        auto it = closed.find(entry);
        if (it == closed.end()) {
            closed.insert(entry);
            return std::make_pair(false, false);
        } else {
            if (reopen_closed && entry.get_g() <= it->get_g()) {
                closed.erase(entry);
                closed.insert(entry);
                return std::make_pair(true, true);
            } else {
                return std::make_pair(true, false);
            }
        }
    }

    CompressClosedListFactory::
    CompressClosedListFactory(const Options &options)
        : options(options) {
    }

    unique_ptr<StateClosedList>
    CompressClosedListFactory::create_state_closed_list() {
        return utils::make_unique_ptr
            <CompressClosedList<StateClosedListEntry> >(options);
    }

        static shared_ptr<ClosedListFactory> _parse(OptionParser &parser) {
        parser.document_synopsis("Compress closed list", "");
        parser.add_option<bool>(
                                "reopen_closed",
                                "reopen closed nodes with lower g values");
        Options opts = parser.parse();
        if (parser.dry_run())
            return nullptr;
        else
            return make_shared<CompressClosedListFactory>(opts);
    }

    static PluginShared<ClosedListFactory> _plugin("compress", _parse);
}

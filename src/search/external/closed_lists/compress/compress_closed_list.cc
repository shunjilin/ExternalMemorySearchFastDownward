#include "compress_closed_list.h"
#include "pointer_table.h"
#include "mapping_table.h"

#include "../../closed_list.h"
#include "../../../option_parser.h"
#include "../../../plugin.h"
#include "../../../global_operator.h"
#include "../../../utils/memory.h"
#include "../../../globals.h"

#include <vector>
#include <memory>
#include <unordered_set>
#include <exception>
#include <cmath> // for pow

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

using namespace std;

using found = bool;
using reopened = bool;

namespace compress_closed_list {
    template<class Entry>
    class CompressClosedList : public ClosedList<Entry> {
        unordered_set<Entry> closed;
        bool reopen_closed;
        bool enable_partitioning;
        
        vector<unordered_set<Entry> > buffers;
        unique_ptr<MappingTable> mapping_table;
        //unordered_set<Entry> buffer;
        PointerTable internal_closed;
        int external_closed_fd;
        char *external_closed;
        size_t external_closed_index = 0;

        size_t max_buffer_size_in_bytes = 4096; // 4kb
        size_t max_buffer_entries;

        bool initialized = false; // lazy initialization

        void flush_buffer();
        bool initialize(size_t entry_size_in_bytes);

        void read_external_at(Entry& entry, size_t index) const;
        void write_external_at(const Entry& entry, size_t index);
                
    public:
        explicit CompressClosedList(const Options &opts);
        virtual ~CompressClosedList() override = default;

        virtual std::pair<found, reopened> find_insert(const Entry &entry) override;
        virtual vector<const GlobalOperator*> trace_path(const Entry &entry)
            const override;
        
    };

    /*                                                                      \
    | To initialize external closed list and mapping table.  Done lazily as |
    | size of node can only be determines when first node is inserted.  Can |
    | consider passing size of node from state registry to closed lists but |
    | that might lead to an unnecessary dependency.                         |
    \======================================================================*/
    template<class Entry>
    bool CompressClosedList<Entry>::
    initialize(size_t entry_size_in_bytes) {
        // set max buffer entries
        max_buffer_entries = max_buffer_size_in_bytes / entry_size_in_bytes;
        
        // initialize mapping table
        if (enable_partitioning) {
            mapping_table =
                utils::make_unique_ptr<MappingTable>(max_buffer_entries);
        }
        

        
        // initialize external closed list
        auto external_closed_bytes =
            internal_closed.get_max_entries() * entry_size_in_bytes;
        cout << "external closed bytes " << external_closed_bytes << endl;

        // initialize external closed list
        external_closed_fd = open("closed.bucket", O_CREAT | O_TRUNC | O_RDWR,
                                  S_IRUSR | S_IWUSR);
        if (external_closed_fd < 0) return false;
        if (fallocate(external_closed_fd, 0, 0, external_closed_bytes) < 0)
            return false;
        external_closed =
            static_cast<char *>(mmap(NULL, external_closed_bytes,
                                     PROT_READ | PROT_WRITE, MAP_SHARED, // what happens if use MAP_PRIVATE?
                                     external_closed_fd, 0));
        if (external_closed == MAP_FAILED)
            return false;

        initialized = true;
        cout << "address at " << reinterpret_cast<size_t>(external_closed) << endl;
        
        return true;
    }


    template<class Entry>
    CompressClosedList<Entry>::CompressClosedList(const Options &opts)
        : ClosedList<Entry>(opts.get<bool>("reopen_closed")),
        enable_partitioning(opts.get<bool>("enable_partitioning")), // implement this, maybe remove and set default
        internal_closed(opts.get<double>("internal_closed_gb") * pow(1024, 3)) // implement this
    {
        cout << internal_closed.get_max_size_in_bytes() << " EXTERNAL CLOSED LIST SIZE IN BYTES" << endl;
        cout << "max entries of closed list is " << internal_closed.get_max_entries() << endl;
        cout << "size of pointer in bits is " << internal_closed.get_ptr_size_in_bits() << endl;
    }

    template<class Entry>
    std::pair<found, reopened> CompressClosedList<Entry>::
    find_insert(const Entry &entry) {
        
        if (!initialized) {
            if (!initialize(Entry::bytes_per_state)) throw; // todo: modify and handle this in search engine
            initialized = true;
        }
        
        // First look in buffer
        // TODO: modify to adapt to partitioning
        // change vector to hash table?
        if (buffers.empty()) {
            buffers.resize(1);
        }
        //cout << "searching for " << entry.get_state_id() << endl;
        auto buffer_it = buffers[0].find(entry);
        if (buffer_it != buffers[0].end()) {
            cout << "found " << buffer_it->get_state_id() << endl;
            if (reopen_closed) {
                if (entry.get_g() < buffer_it->get_g()) {
                    buffers[0].erase(buffer_it);
                    buffers[0].insert(entry);
                    return make_pair(true, true);
                }
            }
            return make_pair(true, false);
        }
        
        // Then look in hash tables
        //cout << "looking in hash table for " << entry.get_state_id() << endl;
        auto hasher = hash<Entry>();
        auto hash_value = hasher(entry);
        auto ptr = internal_closed.hash_find(hash_value);
        while (!internal_closed.ptr_is_invalid(ptr)) {
            cout << " searching in external hash table for " << entry.get_state_id() << endl;
            // read node from pointer
            Entry node = Entry::dummy;
            read_external_at(node, ptr);
            if (node == entry) {
                if (reopen_closed) {
                    if (entry.get_g() < node.get_g()) {
                        write_external_at(entry, ptr);
                        return make_pair(true, true);
                    }
                }
                return make_pair(true, false);
            }
            ptr = internal_closed.hash_find(hash_value, false);
        }
        buffers[0].insert(entry);
        if (buffers[0].size() == max_buffer_entries) flush_buffer();
        // TODO:flush buffer if full here!

        return make_pair(false, false);
    }

    template<class Entry>
    void CompressClosedList<Entry>::flush_buffer() {
        cout << "FLUSH" << endl;
        for (auto& node : buffers[0]) {
            write_external_at(node, external_closed_index);
            
            auto hasher = hash<Entry>();
            internal_closed.hash_insert(external_closed_index, hasher(node));
            
            ++external_closed_index;
        }
        buffers[0].clear();
    }
    

    template<class Entry>
    vector<const GlobalOperator *> CompressClosedList<Entry>::
    trace_path(const Entry &entry) const {
        vector<const GlobalOperator *> path;
        Entry current_state = entry;
        cout << "size of buffer is " << buffers[0].size() << endl;
        while (true) {
            if (current_state.get_creating_operator() == -1) {
                assert(current_state.get_parent_state_id == StateID::no_state);
                break;
            }
            //assert(utils::in_bounds(info.creating_operator, g_operators));
            const GlobalOperator *op =
                &g_operators[current_state.get_creating_operator()];
            path.push_back(op);

            // search buffer first
            bool found_in_buffer = false;
            for (auto &elem : buffers[0]) {
                if (elem.get_state_id() == current_state.get_parent_state_id()) {
                    current_state = elem;
                    found_in_buffer = true;
                    break;
                }
            }
            if (found_in_buffer) {
                cout << "Found in buffer" << endl;
                continue;
            }

            // then look in hash tables
            cout << " looking in hash table" << endl;
            Entry target = Entry::dummy;
            for (size_t i = 0; i < internal_closed.get_max_entries(); ++i) {
                auto ptr = internal_closed.find(i);
                if (!internal_closed.ptr_is_invalid(ptr)) {
                    read_external_at(target, ptr);
                    if (target.get_state_id() == current_state.get_parent_state_id()) {
                        current_state = target;
                        break;
                    }
                }
            }
        }
        reverse(path.begin(), path.end());
        return path;
    }

    // read helper function to encapsulate pointer manipulation
    template<class Entry>
    void CompressClosedList<Entry>::
    read_external_at(Entry& entry, size_t index) const {
        entry.read(static_cast<char *>
                   (external_closed + index * Entry::bytes_per_state));
    }

    // write helper function to encapsulate pointer manipulation
    template<class Entry>
    void CompressClosedList<Entry>::
    write_external_at(const Entry& entry, size_t index) {
        entry.write(static_cast<char *>
                    (external_closed + index * Entry::bytes_per_state));
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
        parser.add_option<bool>("enable_partitioning",
                                "use partitioning table"); // TODO: set default
        parser.add_option<double>("internal_closed_gb",
                                  "internal_closed size in gb");
        Options opts = parser.parse();
        if (parser.dry_run())
            return nullptr;
        else
            return make_shared<CompressClosedListFactory>(opts);
    }

    static PluginShared<ClosedListFactory> _plugin("compress", _parse);
}

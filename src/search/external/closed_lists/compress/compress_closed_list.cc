#include "compress_closed_list.h"
#include "pointer_table.h"
#include "mapping_table.h"

#include "../../closed_list.h"
#include "../../../option_parser.h"
#include "../../../plugin.h"
#include "../../../global_operator.h"
#include "../../../utils/memory.h"
#include "../../../globals.h"
#include "../../hash_functions/state_hash.h"
#include "../../hash_functions/zobrist.h"
#include "../../options/errors.h"

#include <vector>
#include <memory>
#include <unordered_set>
#include <exception> // remove?
#include <cmath> // for pow

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

using namespace std;
using namespace statehash;

using found = bool;
using reopened = bool;

namespace compress_closed_list {
    template<class Entry>
    class CompressClosedList : public ClosedList<Entry> { 
        bool reopen_closed;
        bool enable_partitioning;

        vector<unordered_set<Entry> > buffers;

        // for compress with partitioning
        unique_ptr<MappingTable> partition_table;
        unique_ptr<StateHash<Entry> > partition_hash;
        unsigned n_partitions = 100;

        
        PointerTable internal_closed;
        int external_closed_fd;
        char *external_closed;
        size_t external_closed_index = 0;
        size_t external_closed_bytes = 0; // total size in nodes of external table

        size_t max_buffer_size_in_bytes = 4096; // 4kb
        size_t max_buffer_entries;

        bool initialized = false; // lazy initialization
        
        unsigned get_partition_value(const Entry &entry) const;
        
        void flush_buffer(size_t partition_value);
        void initialize(size_t entry_size_in_bytes);

        void read_external_at(Entry& entry, size_t index) const;
        void write_external_at(const Entry& entry, size_t index);

        // probe statistics, does not include probes for path reconstruction
        // Good probes: successful probes into buffer or external closed list
        // Bad probes: unsuccessful probes into external closed list
        // Note: The inclusion of buffer hits is because there is no
        // straightforward way of distinguishing memory access from disc access,
        // as the os is in charge of caching and paging in and out
        mutable size_t good_probes = 0;
        mutable size_t bad_probes = 0;

        void print_initial_statistics() const;
                
    public:
        explicit CompressClosedList(const Options &opts);
        virtual ~CompressClosedList() override = default;

        virtual pair<found, reopened> find_insert(const Entry &entry) override;
        virtual vector<const GlobalOperator*> trace_path(const Entry &entry)
            const override;

        void clear();
        void print_statistics() const;
    };

    /*                                                                      \
    | To initialize external closed list and mapping table.  Done lazily as |
    | size of node can only be determines when first node is inserted.  Can |
    | consider passing size of node from state registry to closed lists but |
    | that might lead to an unnecessary dependency.                         |
    \======================================================================*/
    template<class Entry>
    void CompressClosedList<Entry>::
    initialize(size_t entry_size_in_bytes) {
        // set max buffer entries
        max_buffer_entries = max_buffer_size_in_bytes / entry_size_in_bytes;

        // initialize primary hash
        // TODO: select hash functions from options
        Entry::initialize_hash_function(utils::make_unique_ptr<ZobristHash<Entry> >());
        
        // initialize partition table
        if (enable_partitioning) {
            partition_table =
                utils::make_unique_ptr<MappingTable>(max_buffer_entries);
            // TODO: select hash functions from options

            partition_hash =
                utils::make_unique_ptr<ZobristHash<Entry> >();
           
            buffers.resize(n_partitions);
        } else {
            buffers.resize(1); // use only buffers[0] if no partitioning
        }
        
        // initialize external closed list
        external_closed_bytes =
            internal_closed.get_max_entries() * entry_size_in_bytes;
        cout << "external closed bytes " << external_closed_bytes << endl;

        // initialize external closed list
        external_closed_fd = open("closed_list.bucket", O_CREAT | O_TRUNC | O_RDWR,
                                  S_IRUSR | S_IWUSR);
        if (external_closed_fd < 0)
            throw IOException("Fail to create closed list file");
        
        if (fallocate(external_closed_fd, 0, 0, external_closed_bytes) < 0)
            throw IOException("Fail to fallocate closed list file");
        
        external_closed =
            static_cast<char *>(mmap(NULL, external_closed_bytes,
                                     PROT_READ | PROT_WRITE, MAP_SHARED, // what happens if use MAP_PRIVATE?
                                     external_closed_fd, 0));
        if (external_closed == MAP_FAILED)
            throw IOException("Fail to mmap closed list file");

        initialized = true;
    }


    template<class Entry>
    CompressClosedList<Entry>::CompressClosedList(const Options &opts)
        : ClosedList<Entry>(opts.get<bool>("reopen_closed")),
        enable_partitioning(opts.get<bool>("enable_partitioning")),
        internal_closed(opts.get<double>("internal_closed_gb") * pow(1024, 3)) 
    {
        print_initial_statistics();
    }

    template<class Entry>
    void CompressClosedList<Entry>::print_initial_statistics() const {
        cout << "Using compress closed list ";
        if (enable_partitioning)
            cout << "with " << n_partitions << " partitions";
        cout << ".\n";
        cout << "Closed list max entries: " << internal_closed.get_max_entries()
             << endl;
    }
    template<class Entry>
    pair<found, reopened> CompressClosedList<Entry>::
    find_insert(const Entry &entry) {
        
        if (!initialized) initialize(Entry::bytes_per_state);
        
        // First look in buffer
        auto partition_value =
            enable_partitioning ? get_partition_value(entry) : 0;
        
        auto& buffer = buffers[partition_value];
        
        auto buffer_it = buffer.find(entry);
        if (buffer_it != buffer.end()) {
            ++good_probes;
            if (reopen_closed) {
                if (entry.get_g() < buffer_it->get_g()) {
                    buffer.erase(buffer_it);
                    buffer.insert(entry);
                    return make_pair(true, true);
                }
            }
            return make_pair(true, false);
        }
        
        // Then look in hash tables
        auto ptr = internal_closed.hash_find(entry.get_hash_value());
        while (!internal_closed.ptr_is_invalid(ptr)) {

            // first check in partition table
            if (!enable_partitioning ||
                partition_value == partition_table->get_value_from_ptr(ptr)) {
                // read node from pointer
                Entry node;
                read_external_at(node, ptr);
                if (node == entry) {
                    ++good_probes;
                    if (reopen_closed) {
                        if (entry.get_g() < node.get_g()) {
                            write_external_at(entry, ptr);
                            return make_pair(true, true);
                        }
                    }
                    return make_pair(true, false);
                }
                ++bad_probes;
            }
            // update pointer and resume while loop if partition values do not
            // match or if hash collision
            ptr = internal_closed.hash_find(entry.get_hash_value(), false);
        }
        buffers[partition_value].insert(entry);
        if (buffers[partition_value].size() == max_buffer_entries)
            flush_buffer(partition_value);

        return make_pair(false, false);
    }

    template<class Entry>
    unsigned CompressClosedList<Entry>::get_partition_value(const Entry& entry) const {
        return (*partition_hash)(entry) % n_partitions;
    }

    template<class Entry>
    void CompressClosedList<Entry>::flush_buffer(size_t partition_value) {
        //cout << "FLUSH" << endl;
        for (auto& node : buffers[partition_value]) {
            write_external_at(node, external_closed_index);
            
            internal_closed.hash_insert(external_closed_index, node.get_hash_value());
            
            ++external_closed_index;
        }
        if (enable_partitioning)
            partition_table->insert_map_value(partition_value);
        buffers[partition_value].clear();
    }
    

    template<class Entry>
    vector<const GlobalOperator *> CompressClosedList<Entry>::
    trace_path(const Entry &entry) const {
        vector<const GlobalOperator *> path;
        Entry current_state = entry;
            
        while (true) {
            startloop:
            if (current_state.get_creating_operator() == -1) {
                assert(current_state.get_parent_state_id == StateID::no_state);
                break;
            }
            //assert(utils::in_bounds(info.creating_operator, g_operators));

            // use of g_operators creates dependency on globals.h
            const GlobalOperator *op =
                &g_operators[current_state.get_creating_operator()];
            path.push_back(op);
            // first look in buffers
            for (auto& buffer : buffers) {
                for (auto& node: buffer) {
                    if (node.get_state_id()  == current_state.get_parent_state_id()) {
                        current_state = node;
                        goto startloop;
                    }
                }
            }
            // Then look in hash tables
            auto ptr = internal_closed.hash_find(current_state.get_parent_hash_value());
            while (!internal_closed.ptr_is_invalid(ptr)) {
                // read node from pointer
                Entry node;
                read_external_at(node, ptr);
                if (node.get_state_id() == current_state.get_parent_state_id()) {
                    current_state = node;
                    break;
                }
                // update pointer and resume while loop if partition values do not
                // match or if hash collision
                ptr = internal_closed.hash_find(current_state.get_parent_hash_value(), false);
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

    template<class Entry>
    void CompressClosedList<Entry>::clear() {
        // Let errors go in clear() as we do not want termination at the end of
        // search, and clean up of files is non-critical
        munmap(external_closed, external_closed_bytes);
        close(external_closed_fd);
        remove("closed_list.bucket");
    }
    

    template<class Entry>
    void CompressClosedList<Entry>::print_statistics() const {
        cout << "Size of a node: " << Entry::bytes_per_state << " bytes\n";
        cout << "Number of entries in the closed list at the end of search : "
             << internal_closed.get_n_entries() << "\n";
        cout << "Load factor of the closed list at the end of search : "
             << internal_closed.get_load_factor() << "\n";
        cout << "Successful probes into the closed list (includes buffer hits): " << good_probes
             << "\nUnsuccessful probes into the closed list: "
             << bad_probes << endl;
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
            // These are set in search_engines/search_common
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

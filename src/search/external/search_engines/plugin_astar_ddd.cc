#include "astar_ddd_search.h"
#include "../../search_engines/search_common.h"

#include "../../option_parser.h"
#include "../../plugin.h"

#include <tuple>

using namespace std;

namespace plugin_astar_ddd {
static shared_ptr<SearchEngine> _parse(OptionParser &parser) {
    parser.document_synopsis(
        "A*-DDD (Korf, Hatem)  search with hashed-based delayed duplicate detection",
        "A* is a best first search that uses g+h "
        "as f-function.");

    parser.add_option<Evaluator *>("eval", "evaluator for h-value");

    SearchEngine::add_pruning_option(parser);
    SearchEngine::add_options_to_parser(parser);
    Options opts = parser.parse();

    shared_ptr<astar_ddd_search::AStarDDDSearch> engine;
    if (!parser.dry_run()) {
        opts.set("reopen_closed", true); // engine, open & closed depends on this
        auto temp =
            search_common::
            create_astar_ddd_open_list_factory_and_f_eval(opts);
        opts.set("open", get<0>(temp));
        opts.set("f_eval", get<1>(temp));
        vector<Heuristic *> preferred_list;
        opts.set("preferred", preferred_list);
        engine = make_shared<astar_ddd_search::AStarDDDSearch>(opts);
    }
    
    return engine;
}

static PluginShared<SearchEngine> _plugin("astar_ddd", _parse);
}

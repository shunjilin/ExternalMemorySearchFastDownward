#include "external_astar_search.h"
#include "../../search_engines/search_common.h"

#include "../../option_parser.h"
#include "../../plugin.h"

#include <tuple>

using namespace std;

namespace plugin_external_astar {
static shared_ptr<SearchEngine> _parse(OptionParser &parser) {
    parser.document_synopsis(
        "Edelkamp's External A* search with delayed duplicate detection",
        "A* is a best first search that uses g+h "
        "as f-function.");

    parser.add_option<Evaluator *>("eval", "evaluator for h-value");

    SearchEngine::add_pruning_option(parser);
    SearchEngine::add_options_to_parser(parser);
    Options opts = parser.parse();

    shared_ptr<external_astar_search::ExternalAStarSearch> engine;
    if (!parser.dry_run()) {
        opts.set("reopen_closed", true); // engine, open & closed depends on this
        auto temp =
            search_common::
            create_external_astar_open_list_factory_and_f_eval(opts);
        opts.set("open", get<0>(temp));
        opts.set("f_eval", get<1>(temp));
        vector<Heuristic *> preferred_list;
        opts.set("preferred", preferred_list);
        engine = make_shared<external_astar_search::ExternalAStarSearch>(opts);
    }
    
    return engine;
}

static PluginShared<SearchEngine> _plugin("external_astar", _parse);
}

#include "eager_search.h"
#include "../../search_engines/search_common.h"

#include "../../option_parser.h"
#include "../../plugin.h"

#include <tuple>

using namespace std;

namespace plugin_regular_astar {
static shared_ptr<SearchEngine> _parse(OptionParser &parser) {
    parser.document_synopsis(
        "Regular A* search (lazy)",
        "A* is a best first search that uses g+h "
        "as f-function. "
        "We break ties using the evaluator. Closed nodes are re-opened.");

    parser.add_option<Evaluator *>("eval", "evaluator for h-value");

    SearchEngine::add_pruning_option(parser);
    SearchEngine::add_options_to_parser(parser);
    Options opts = parser.parse();

    shared_ptr<eager_search::EagerSearch> engine;
    if (!parser.dry_run()) {
        opts.set("reopen_closed", true); // engine, open & closed depends on this
        auto temp =
            search_common::
            create_regular_factories_and_f_eval(opts);
        opts.set("open", get<0>(temp));
        opts.set("closed", get<1>(temp));
        opts.set("f_eval", get<2>(temp));
        vector<Heuristic *> preferred_list;
        opts.set("preferred", preferred_list);
        engine = make_shared<eager_search::EagerSearch>(opts);
    }

    return engine;
}

static PluginShared<SearchEngine> _plugin("regular_astar", _parse);
}

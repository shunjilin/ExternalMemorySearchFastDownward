#include "heuristic.h"

#include "global_operator.h"
#include "../globals.h"
#include "../option_parser.h"
#include "../plugin.h"
#include "../task_proxy.h"
#include "node.h"

#include "../task_utils/task_properties.h"
#include "../tasks/cost_adapted_task.h"

#include <cassert>
#include <cstdlib>
#include <limits>

using namespace std;

Heuristic::Heuristic(const Options &opts)
    : description(opts.get_unparsed_config()),
      task(opts.get<shared_ptr<AbstractTask>>("transform")),
      task_proxy(*task) {
}

Heuristic::~Heuristic() {
}


State Heuristic::convert_node(const Node &node) const {
    State state(*g_root_task(), node.get_values());
    return task_proxy.convert_ancestor_state(state);
}

string Heuristic::get_description() const {
    return description;
}


static PluginTypePlugin<Heuristic> _type_plugin(
    "Heuristic",
    "A heuristic specification is either a newly created heuristic "
    "instance or a heuristic that has been defined previously. "
    "This page describes how one can specify a new heuristic instance. "
    "For re-using heuristics, see OptionSyntax#Heuristic_Predefinitions.\n\n"
    "Definitions of //properties// in the descriptions below:\n\n"
    " * **admissible:** h(s) <= h*(s) for all states s\n"
    " * **consistent:** h(s) <= c(s, s') + h(s') for all states s "
    "connected to states s' by an action with cost c(s, s')\n"
    " * **safe:** h(s) = infinity is only true for states "
    "with h*(s) = infinity\n"
    " * **preferred operators:** this heuristic identifies "
    "preferred operators ");

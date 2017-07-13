#include "closed_list_factory.h"

#include "../plugin.h"

using namespace std;

template<>
unique_ptr<StateClosedList> ClosedListFactory::create_closed_list() {
    return create_state_closed_list();
}

static PluginTypePlugin<ClosedListFactory> _type_plugin(
    "ClosedList",
    "");

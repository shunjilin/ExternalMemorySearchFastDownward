#include "state_id.h"
#include <limits>

const StateID StateID::no_state =
    StateID(std::numeric_limits<std::size_t>::max());

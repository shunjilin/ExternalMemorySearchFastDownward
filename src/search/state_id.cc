#include "state_id.h"

#include <ostream>

using namespace std;

#ifdef EXTERNAL_SEARCH
std::size_t StateID::value_counter = 0;
//#include <limits>
const StateID StateID::no_state = StateID();
#else
const StateID StateID::no_state = StateID(-1);
#endif

ostream &operator<<(ostream &os, StateID id) {
    os << "#" << id.value;
    return os;
}

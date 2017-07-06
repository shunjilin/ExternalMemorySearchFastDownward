#ifndef EXTERNAL_STATE_ID_H
#define EXTERNAL_STATE_ID_H

#include <cstddef>

class StateID {
    static std::size_t state_count;
    const std::size_t value;
    explicit StateID(std::size_t value) : value(value) {}
 public:
    static const StateID no_state;
    StateID() : value(state_count++) {}
    bool operator==(const StateID &other) const {
        return value == other.value;
    }

    bool operator!=(const StateID &other) const {
        return !(*this == other);
    }

    std::size_t hash() const {
        return value;
    }
};


#endif

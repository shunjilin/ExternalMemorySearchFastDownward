#ifndef STATE_ID_H
#define STATE_ID_H

#include <cstddef>
#include <iostream>

#ifdef EXTERNAL_SEARCH

class StateID {
    friend std::ostream &operator<<(std::ostream &os, StateID id);
    
    static std::size_t value_counter;
    
    std::size_t value;
    
    // StateID(std::size_t value) : value(value) {} //only for no_state
    
 public:
    StateID() : value(++value_counter) {}
    
    static const StateID no_state;

    bool operator==(const StateID &other) const {
        return value == other.value;
    }

    bool operator!=(const StateID &other) const {
        return !(*this == other);
    }

    size_t hash() const {
        return value;
    }
};
        
#else // #ifndef EXTERNAL_SEARCH


// For documentation on classes relevant to storing and working with registered
// states see the file state_registry.h.

class StateID {
    friend class StateRegistry;
    template<typename>
        friend class PerStateInformation;

    friend std::ostream &operator<<(std::ostream &os, StateID id);

    int value;
    explicit StateID(int value_)
        : value(value_) {
    }

    // No implementation to prevent default construction
    StateID();
 public:
    ~StateID() {
    }

    static const StateID no_state;

    bool operator==(const StateID &other) const {
        return value == other.value;
    }

    bool operator!=(const StateID &other) const {
        return !(*this == other);
    }

    size_t hash() const {
        return value;
    }
};

#endif

std::ostream &operator<<(std::ostream &os, StateID id);

namespace std {
    template<>
        struct hash<StateID> {
        size_t operator()(StateID id) const {
            return id.hash();
        }
    };
}

#endif

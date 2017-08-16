#ifndef STATE_HASH_H
#define STATE_HASH_H

#include <cstddef>
#include <random>
#include <limits>

// Base class for hash functions that work on states

namespace statehash {

    template<class Entry>
    class StateHash {
    public:
        virtual ~StateHash() {};
        // returns hash value
        virtual std::size_t operator()(const Entry& entry) const = 0;
    };      
}




#endif

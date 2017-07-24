#ifndef ZOBRIST_H
#define ZOBRIST_H

#include "../../globals.h"

#include <random>
#include <limits>
#include <vector>



namespace zobrist {
    static size_t rand_seed = 0;
    
    template<class Entry>
    class ZobristHash {
        static std::mt19937_64 mt;
        static std::uniform_int_distribution<std::size_t> dis;

        size_t get_rand_bitstring() const;

        std::vector< std::vector<size_t> > table;
        
        public:
        ZobristHash();
        //~ZobristHash() = default;
        //ZobristHash(const ZobristHash& other) = delete;
        //ZobristHash& operator=(const ZobristHash& other) = delete;
        
        std::size_t operator()(const Entry& entry) const; // hash value
    };

    template<class Entry>
    std::mt19937_64 ZobristHash<Entry>::mt(rand_seed++); // different seed for different hashers

    template<class Entry>
    std::uniform_int_distribution<std::size_t>
    ZobristHash<Entry>::dis(0, std::numeric_limits<std::size_t>::max());


    template<class Entry>
    ZobristHash<Entry>::ZobristHash() 
    {
        // resize table to match dimensions of variables and their  values
        table.resize(g_variable_domain.size());
        for (std::size_t i = 0; i < table.size(); ++i) {
            table[i].resize(g_variable_domain[i]);
        }

        // fill table with random bitstrings (64 bit)
        for (auto& var : table) {
            for (auto& val : var) {
                val = get_rand_bitstring();
            }
        }

        // Use of g_variable_domain creates dependency on globals.
        // Consider moving these to static values in Entry class?
    }

    template<class Entry>
    std::size_t ZobristHash<Entry>::get_rand_bitstring() const {
        return dis(mt);
    }

    template<class Entry>
    std::size_t ZobristHash<Entry>::operator()(const Entry& entry) const {
        std::size_t hash_value = 0;
        for (std::size_t i = 0; i < table.size(); ++i) {
            hash_value ^= table[i][entry[i]];
        }
        return hash_value;
    }

}
#endif

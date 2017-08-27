#ifndef ZOBRIST_H
#define ZOBRIST_H

#define TWISTED // optimization for twisted tabulation

#include "../../globals.h"
#include "../../utils/memory.h"

#include <random>
#include <limits>
#include <vector>
#include <iostream>
#include <array>
#include <algorithm>
#include <memory>

#include "state_hash.h"

// Zobrist hashing a.k.a Simple Tabulation Hashing
// Using Mersenne Twister 64 bit pseudorandom number generator, seeded by
// Minimal Standard generator (simple multiplicative congruential)
namespace statehash {
    template<class Entry>
    class ZobristHash : public StateHash<Entry> {
        // for seeding Mersenne Twister vector of internal states
        static size_t master_seed;
        static std::minstd_rand master_eng;
        static std::uniform_int_distribution<unsigned> master_dis;

        // actual number generator for zobrist hashing
        static unsigned get_rand_seed();
        static std::unique_ptr<std::mt19937_64> mt_ptr; 
        static std::uniform_int_distribution<std::size_t> dis;

        size_t get_rand_bitstring() const;

        std::vector< std::vector<size_t> > table;
        
    public:
        ZobristHash();
        
        std::size_t operator()(const Entry& entry) const override; // hash value
    };

    template<class Entry>
    std::size_t ZobristHash<Entry>::master_seed = 1; // default seed

    template<class Entry>
    std::minstd_rand ZobristHash<Entry>::master_eng(master_seed);

    template<class Entry>
    std::uniform_int_distribution<unsigned>
    ZobristHash<Entry>::master_dis(0, std::numeric_limits<unsigned>::max());

    template<class Entry>
    unsigned ZobristHash<Entry>::get_rand_seed() {
        return master_dis(master_eng);
    }

    template<class Entry>
    std::uniform_int_distribution<std::size_t>
    ZobristHash<Entry>::dis(0, std::numeric_limits<std::size_t>::max());

    template<class Entry>
    std::unique_ptr<std::mt19937_64> ZobristHash<Entry>::mt_ptr = nullptr;
    
    template<class Entry>
    ZobristHash<Entry>::ZobristHash() 
    {
        // initialize seeds for mersenne twister
        if (!mt_ptr) {
            std::array<unsigned, std::mt19937_64::state_size> seed_data {};
            std::generate(std::begin(seed_data), std::end(seed_data),
                          get_rand_seed);
            std::seed_seq seq(std::begin(seed_data), std::end(seed_data));
            mt_ptr = utils::make_unique_ptr<std::mt19937_64>(std::mt19937_64(seq));
        }
        
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
        return dis(*mt_ptr);
    }

    template<class Entry>
    std::size_t ZobristHash<Entry>::operator()(const Entry& entry) const {
        std::size_t hash_value = 0;
#ifdef TWISTED
        std::size_t i = 0;
        for (; i < table.size() - 1; ++i) {
            hash_value ^= table[i][entry[i]];
        }
        
        hash_value ^= table[i][(entry[i] ^ hash_value) % table[i].size()];
#else
        for (std::size_t i = 0; i < table.size(); ++i) {
            hash_value ^= table[i][entry[i]];
        }
#endif
        return hash_value;
    }
}
#endif

#ifndef COMPUNITS_H
#define COMPUNITS_H

#include <cstddef>
#include <cmath>

using std::size_t;
using std::pow;

namespace compunits
{
    constexpr size_t operator "" _B(unsigned long long const size)
    {
        return size;
    }
    constexpr size_t operator "" _KiB(unsigned long long const size)
    {
        return size * 1024;
    }
    constexpr size_t operator "" _MiB(unsigned long long const size)
    {
        return size * pow(1024, 2);
    }
    constexpr size_t operator "" _GiB(unsigned long long const size)
    {
        return size * pow(1024, 3);
    }
}

#endif // COMPUNITS_H

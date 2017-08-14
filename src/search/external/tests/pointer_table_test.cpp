// Simple test for pointer table
#include "../closed_lists/compress/pointer_table.h"
#include "iostream"
#include "cmath"
#include "cassert"

using namespace std;

int main(int argc, char *argv[])
{
    // Edge case where using the biggest possible pointer results in reduced
    // space for pointer table
    
    PointerTable ptr_table_1 = PointerTable(0.43 * pow(1024, 3));

    assert(ptr_table_1.get_ptr_size_in_bits() == 27);
    assert(ptr_table_1.get_max_entries() == 134217689);
    
    return 0;
}

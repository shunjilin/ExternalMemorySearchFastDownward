#include "file_utility.h"
#include <iostream>

named_fstream::named_fstream(const string file_name, ios_base::openmode mode) :
    fstream(file_name, mode), file_name(file_name) {}

named_fstream::~named_fstream() {
    remove(file_name.data());
}

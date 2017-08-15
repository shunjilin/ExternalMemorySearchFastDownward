#include "named_fstream.h"
#include "errors.h"
#include <iostream>

named_fstream::named_fstream(const string file_name, ios_base::openmode mode) :
    fstream(file_name, mode), file_name(file_name) {
    if (!this->is_open()) throw IOException("Fail to open fstream file.");
    //this->rdbuf()->pubsetbuf(&buffer.front(), buffer.size());
}

named_fstream::~named_fstream() {
    remove(file_name.data());
}

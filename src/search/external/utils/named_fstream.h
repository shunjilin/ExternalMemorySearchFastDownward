#ifndef NAMED_FSTREAM_H
#define NAMED_FSTREAM_H

#include <fstream>
#include <string>
#include <cstdio>
#include <vector>

constexpr int BUFFER_BYTES = 16384; // 16kb
/*                                                                           \
| Keeps file_names with their respective fstream for convenient destruction. |
| Also provides custom size buffer.                                          |
\===========================================================================*/

using namespace std;

class named_fstream : public fstream {
    string file_name;
    vector<char> buffer = vector<char>(BUFFER_BYTES);
 public:
    named_fstream() = default;
    named_fstream(const string file_name,
               ios_base::openmode mode = ios_base::in | ios_base::out
               | ios_base:: trunc | ios_base:: binary);
    
    ~named_fstream();

    named_fstream(const named_fstream &other) = delete;
    named_fstream& operator = (const named_fstream &other) = delete;
};


#endif

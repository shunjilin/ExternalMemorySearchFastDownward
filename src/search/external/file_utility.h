#ifndef FILE_UTILITY_H
#define FILE_UTILITY_H

#include <fstream>
#include <string>
#include <cstdio>


/*                                                                          \
| keeps file_names with their respective fstream for convenient destruction |
| and clean up                                                              |
\==========================================================================*/

using namespace std;

class named_fstream : public fstream {
    string file_name;
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

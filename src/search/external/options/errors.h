#ifndef EXTERNAL_OPTIONS_ERRORS_H
#define EXTERNAL_OPTIONS_ERRORS_H

// For IO related errors

#include <stdexcept>
#include <string>

class IOException : public std::runtime_error
{
public:
    explicit IOException(const std::string& message) :
        std::runtime_error("IO Exception Raised: " + message) {}
};



#endif

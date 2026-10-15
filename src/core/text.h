#pragma once
#include <string>

namespace fw {

// printf-style formatting into a std::string.
std::string strf(const char* format, ...)
#if defined(__GNUC__) && !defined(__MINGW32__)
    __attribute__((format(printf, 1, 2)))
#endif
    ;

}  // namespace fw

#include "core/text.h"
#include <cstdarg>
#include <cstdio>

namespace fw {

std::string strf(const char* format, ...) {
  va_list args;
  va_start(args, format);
  va_list copy;
  va_copy(copy, args);
  const int length = std::vsnprintf(nullptr, 0, format, copy);
  va_end(copy);
  std::string result;
  if (length > 0) {
    result.resize(static_cast<std::size_t>(length) + 1);
    std::vsnprintf(result.data(), result.size(), format, args);
    result.resize(static_cast<std::size_t>(length));
  }
  va_end(args);
  return result;
}

}  // namespace fw

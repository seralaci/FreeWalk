#pragma once
#include <string>
#include "scssdk.h"

namespace fw {

// Writes to the game log through the SDK log function, with a "[FreeWalk] " prefix.
// Only call from plugin functions called by the game, because the SDK is not thread-safe.
void log_init(scs_log_t log);
void log_info(const std::string& message);
void log_warning(const std::string& message);
void log_error(const std::string& message);

}  // namespace fw

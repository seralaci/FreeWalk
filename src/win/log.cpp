#include "win/log.h"

namespace fw {
namespace {

scs_log_t g_log = nullptr;

void write(scs_log_type_t type, const std::string& message) {
  if (g_log) g_log(type, ("[FreeWalk] " + message).c_str());
}

}  // namespace

void log_init(scs_log_t log) { g_log = log; }
void log_info(const std::string& message) { write(SCS_LOG_TYPE_message, message); }
void log_warning(const std::string& message) { write(SCS_LOG_TYPE_warning, message); }
void log_error(const std::string& message) { write(SCS_LOG_TYPE_error, message); }

}  // namespace fw

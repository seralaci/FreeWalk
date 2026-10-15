#pragma once
#include <string>
#include "scssdk.h"

namespace fw {

// The plugin's log: always written to its own file (FreeWalk.log, started anew at every game start), and on request also to the
// game log (game.log.txt) through the SDK log function, with a "[FreeWalk] " prefix.
// The game log may only be written from plugin functions called by the game, because the SDK is not thread-safe.
void log_init(scs_log_t log);
// Opens (truncates) the log file; if it cannot be written, the fallback path is tried.
void log_open(const std::wstring& path, const std::wstring& fallback_path);
// Whether the lines also go to the game log. Lines logged before the first call are kept and sent once it is known.
void log_use_game(bool use);
// The log file in use (empty if none could be opened).
std::wstring log_file();
void log_close();

void log_info(const std::string& message);
void log_warning(const std::string& message);
void log_error(const std::string& message);

}  // namespace fw

#include "win/log.h"
#include <windows.h>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <utility>
#include <vector>

namespace fw {
namespace {

std::mutex g_mutex;
scs_log_t g_game_log = nullptr;
std::ofstream g_file;
std::wstring g_file_path;
int g_use_game = -1;  // -1: not known yet (lines are kept), 0: file only, 1: also the game log
std::vector<std::pair<scs_log_type_t, std::string>> g_pending;

std::string timestamp() {
  SYSTEMTIME t;
  GetLocalTime(&t);
  char text[16];
  std::snprintf(text, sizeof text, "%02u:%02u:%02u.%03u", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);
  return text;
}

void write(scs_log_type_t type, const std::string& message) {
  std::lock_guard lock(g_mutex);
  if (g_file) {
    const char* level = type == SCS_LOG_TYPE_warning ? "<WARNING> " : type == SCS_LOG_TYPE_error ? "<ERROR> " : "";
    g_file << timestamp() << " : " << level << message << '\n';
    g_file.flush();  // the game may end without unloading the plugin
  }
  if (g_use_game < 0) g_pending.emplace_back(type, message);
  else if (g_use_game == 1 && g_game_log) g_game_log(type, ("[FreeWalk] " + message).c_str());
}

bool open_file(const std::wstring& path) {
  std::error_code ignored;
  std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ignored);
  g_file.open(std::filesystem::path(path), std::ios::binary | std::ios::trunc);
  if (!g_file) {
    g_file.clear();
    return false;
  }
  g_file_path = path;
  SYSTEMTIME t;
  GetLocalTime(&t);
  char header[64];
  std::snprintf(header, sizeof header, "FreeWalk log, %04u-%02u-%02u %02u:%02u:%02u\n", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute,
                t.wSecond);
  g_file << header;
  g_file.flush();
  return true;
}

}  // namespace

void log_init(scs_log_t log) {
  std::lock_guard lock(g_mutex);
  g_game_log = log;
}

void log_open(const std::wstring& path, const std::wstring& fallback_path) {
  std::lock_guard lock(g_mutex);
  if (g_file.is_open()) return;
  if (!open_file(path) && !fallback_path.empty()) open_file(fallback_path);
}

void log_use_game(bool use) {
  std::lock_guard lock(g_mutex);
  const int value = use ? 1 : 0;
  if (g_use_game == value) return;
  const bool first = g_use_game < 0;
  g_use_game = value;
  if (first && use && g_game_log)
    for (const auto& [type, message] : g_pending) g_game_log(type, ("[FreeWalk] " + message).c_str());
  g_pending.clear();
}

std::wstring log_file() {
  std::lock_guard lock(g_mutex);
  return g_file_path;
}

void log_close() {
  std::lock_guard lock(g_mutex);
  if (g_file.is_open()) g_file.close();
  g_file_path.clear();
  g_game_log = nullptr;
  g_use_game = -1;
  g_pending.clear();
}

void log_info(const std::string& message) { write(SCS_LOG_TYPE_message, message); }
void log_warning(const std::string& message) { write(SCS_LOG_TYPE_warning, message); }
void log_error(const std::string& message) { write(SCS_LOG_TYPE_error, message); }

}  // namespace fw

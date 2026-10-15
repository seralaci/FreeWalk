#pragma once
#include <windows.h>
#include <optional>
#include <string>
#include <vector>
#include "core/config.h"

namespace fw {

// Data needed to open the settings window.
struct SettingsWindowSetup {
  Config current;                  // the (live) settings at open time
  std::wstring ini_path;           // Save writes here
  bool ini_had_unknown_keys = false;  // the loaded INI had a key unknown to the plugin (Save drops it)
  HWND game_window = nullptr;      // gets the focus back on close
  std::string version;             // shown in the header
};

// The settings window runs on its own thread; the game thread manages it with these functions.
// Returns false if it is already open or the thread did not start.
bool settings_window_open(const SettingsWindowSetup& setup);
bool settings_window_is_open();
// Whether the foreground window is the settings window; then the game does not count as focused (it runs in the same process).
bool settings_window_is_foreground();
// Requests closing; same as Cancel.
void settings_window_close();
// Closes the window and waits at most 2 s for its thread (at plugin shutdown).
void settings_window_shutdown();

// Game thread, per frame: the new settings requested by the window, if any since then.
std::optional<Config> settings_window_take_wanted();
// Written to the log by the game thread (the SDK logger is not thread-safe).
std::vector<std::string> settings_window_take_log();

}  // namespace fw

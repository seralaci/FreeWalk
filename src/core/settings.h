#pragma once
#include <string>
#include <string_view>
#include <vector>
#include "core/config.h"

namespace fw {

enum class SettingKind { Key, Number, Flag };

// When a change takes effect in the running game. apply_settings handles delaying key changes while walking.
enum class Applies { Now, Restart };

// Description of one setting: the INI is read and written from it, and the settings window is built from it.
struct SettingInfo {
  std::string_view section;  // INI section
  std::string_view key;      // INI key
  std::string_view tab;      // settings window tab
  std::string_view label;    // label in the settings window
  std::string_view comment;  // comment in the INI
  SettingKind kind = SettingKind::Number;
  Applies applies = Applies::Now;
  int Config::*key_field = nullptr;
  bool allow_ctrl = false;  // CTRL+ combination allowed (command key)
  bool allow_none = false;  // NONE (disabled) allowed
  double Config::*number_field = nullptr;
  double min = 0;
  double max = 0;
  double step = 0;
  int decimals = 0;
  bool Config::*flag_field = nullptr;
};

// All settings, in INI order.
const std::vector<SettingInfo>& settings_schema();

// The value of a setting as text, as written to the INI.
std::string format_setting_value(const SettingInfo& s, const Config& c);

// The full INI text with comments; parse_config reads the same settings back from it.
std::string format_config(const Config& c);

// Two functions on the same key (by their labels, in table order).
struct KeyConflict {
  std::string_view first;
  std::string_view second;
};
std::vector<KeyConflict> key_conflicts(const Config& c);

// Whether every setting in the table matches.
bool same_settings(const Config& a, const Config& b);

// Applies the settings from the settings window (wanted) to the running settings (active): settings that need a restart stay,
// and so do the keys while walking (they apply from the next exit); everything else takes the new value at once.
Config apply_settings(const Config& active, const Config& wanted, bool walking);

}  // namespace fw

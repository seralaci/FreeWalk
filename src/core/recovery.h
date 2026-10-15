#pragma once
#include <string>
#include <string_view>
#include <vector>

namespace fw {

// The original value of a console variable (cvar) we changed for the walk.
struct SavedSetting {
  std::string name;
  std::string value;
};

// The recovery file: the original cvar values during a walk, one "name=value" per line.
// If the game quits during a walk it may save the changed values into its settings; on the next start we restore them from this.
std::string format_recovery(const std::vector<SavedSetting>& settings);

// Returns only valid lines: the name is lowercase letters, digits or underscores, the value printable ASCII.
std::vector<SavedSetting> parse_recovery(std::string_view text);

}  // namespace fw

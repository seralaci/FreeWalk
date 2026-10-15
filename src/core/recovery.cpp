#include "core/recovery.h"

namespace fw {
namespace {

bool valid_name(std::string_view name) {
  if (name.empty() || name.size() > 0x20) return false;
  for (const char c : name)
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) return false;
  return true;
}

bool valid_value(std::string_view value) {
  if (value.size() > 0x64) return false;
  for (const char c : value)
    if (c < 0x20 || c > 0x7E) return false;
  return true;
}

}  // namespace

std::string format_recovery(const std::vector<SavedSetting>& settings) {
  std::string text;
  for (const auto& s : settings) text += s.name + "=" + s.value + "\n";
  return text;
}

std::vector<SavedSetting> parse_recovery(std::string_view text) {
  std::vector<SavedSetting> settings;
  while (!text.empty()) {
    const auto newline = text.find('\n');
    std::string_view line = text.substr(0, newline);
    text = newline == std::string_view::npos ? std::string_view{} : text.substr(newline + 1);
    if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
    const auto equals = line.find('=');
    if (equals == std::string_view::npos) continue;
    const std::string_view name = line.substr(0, equals);
    const std::string_view value = line.substr(equals + 1);
    if (valid_name(name) && valid_value(value)) settings.push_back({std::string(name), std::string(value)});
  }
  return settings;
}

}  // namespace fw

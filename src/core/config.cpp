#include "core/config.h"
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <map>
#include "core/settings.h"
#include "core/text.h"

namespace fw {
namespace {

std::string_view trim(std::string_view s) {
  const char* ws = " \t\r\n";
  const auto begin = s.find_first_not_of(ws);
  if (begin == std::string_view::npos) return {};
  const auto end = s.find_last_not_of(ws);
  return s.substr(begin, end - begin + 1);
}

std::string to_upper(std::string_view s) {
  std::string r(s);
  for (char& c : r) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  return r;
}

std::string to_lower(std::string_view s) {
  std::string r(s);
  for (char& c : r) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return r;
}

bool parse_number(std::string_view text, double& out) {
  const std::string s(text);
  if (s.empty()) return false;
  char* end = nullptr;
  out = std::strtod(s.c_str(), &end);
  return end == s.c_str() + s.size() && std::isfinite(out);
}

}  // namespace

int parse_key(std::string_view name) {
  const std::string n = to_upper(trim(name));
  if (n.empty()) return -1;
  if (n == "NONE") return 0;
  if (n.rfind("CTRL+", 0) == 0) {
    const int base = parse_key(std::string_view(n).substr(5));
    return (base > 0 && base < kKeyCtrl && base != 0x11 && base != 0xA2 && base != 0xA3) ? (kKeyCtrl | base) : -1;
  }
  if (n.size() == 1 && std::isalnum(static_cast<unsigned char>(n[0]))) return n[0];

  static const std::map<std::string, int, std::less<>> named = {
      {"SHIFT", 0x10}, {"CTRL", 0x11},   {"CONTROL", 0x11}, {"ALT", 0x12},   {"SPACE", 0x20},
      {"TAB", 0x09},   {"LSHIFT", 0xA0}, {"RSHIFT", 0xA1},  {"LCTRL", 0xA2}, {"RCTRL", 0xA3},
      {"NUMPAD+", 0x6B}, {"NUMPAD-", 0x6D}};
  if (const auto it = named.find(n); it != named.end()) return it->second;

  if (n.size() > 2 && n[0] == '0' && n[1] == 'X') {
    char* end = nullptr;
    const long v = std::strtol(n.c_str() + 2, &end, 16);
    return (*end == '\0' && v > 0 && v < 255) ? static_cast<int>(v) : -1;
  }
  if (n[0] == 'F' && n.size() <= 3) {
    char* end = nullptr;
    const long v = std::strtol(n.c_str() + 1, &end, 10);
    if (*end == '\0' && v >= 1 && v <= 24) return 0x6F + static_cast<int>(v);
  }
  return -1;
}

std::string key_name(int code) {
  if (code == 0) return "NONE";
  if ((code & kKeyCtrl) != 0) return "CTRL+" + key_name(code & 0xFF);
  if ((code >= 'A' && code <= 'Z') || (code >= '0' && code <= '9')) return std::string(1, static_cast<char>(code));
  switch (code) {
    case 0x09: return "TAB";
    case 0x10: return "SHIFT";
    case 0x11: return "CTRL";
    case 0x12: return "ALT";
    case 0x20: return "SPACE";
    case 0xA0: return "LSHIFT";
    case 0xA1: return "RSHIFT";
    case 0xA2: return "LCTRL";
    case 0xA3: return "RCTRL";
    case 0x6B: return "NUMPAD+";
    case 0x6D: return "NUMPAD-";
    default: break;
  }
  if (code >= 0x70 && code <= 0x87) return strf("F%d", code - 0x6F);
  return strf("0x%02X", code);
}

Config parse_config(std::string_view text, std::vector<std::string>& warnings) {
  Config c;
  if (text.substr(0, 3) == "\xEF\xBB\xBF") text.remove_prefix(3);

  std::string section;
  int line_number = 0;
  while (!text.empty()) {
    const auto newline = text.find('\n');
    std::string_view line = text.substr(0, newline);
    text = newline == std::string_view::npos ? std::string_view{} : text.substr(newline + 1);
    ++line_number;

    if (const auto comment = line.find_first_of(";#"); comment != std::string_view::npos) line = line.substr(0, comment);
    line = trim(line);
    if (line.empty()) continue;

    if (line.front() == '[' && line.back() == ']') {
      section = to_lower(trim(line.substr(1, line.size() - 2)));
      continue;
    }
    const auto equals = line.find('=');
    if (equals == std::string_view::npos) {
      warnings.push_back(strf("line %d: expected 'name = value', ignored", line_number));
      continue;
    }
    const std::string key = section + "." + to_lower(trim(line.substr(0, equals)));
    const std::string_view value = trim(line.substr(equals + 1));
    const std::string where = strf("line %d (%s): ", line_number, key.c_str());

    const SettingInfo* setting = nullptr;
    for (const auto& candidate : settings_schema())
      if (section == candidate.section && key == std::string(candidate.section) + "." + std::string(candidate.key)) setting = &candidate;
    if (!setting) {
      warnings.push_back(where + "unknown setting, ignored");
    } else if (setting->kind == SettingKind::Key) {
      const int vk = parse_key(value);
      if (vk < 0 || (vk == 0 && !setting->allow_none) || ((vk & kKeyCtrl) != 0 && !setting->allow_ctrl))
        warnings.push_back(where + "unknown key name, the default is kept");
      else c.*setting->key_field = vk;
    } else if (setting->kind == SettingKind::Number) {
      double v = 0;
      if (!parse_number(value, v) || v < setting->min || v > setting->max)
        warnings.push_back(where + strf("must be a number between %g and %g, the default is kept", setting->min, setting->max));
      else c.*setting->number_field = v;
    } else {
      if (value == "0") c.*setting->flag_field = false;
      else if (value == "1") c.*setting->flag_field = true;
      else warnings.push_back(where + "must be 0 or 1, the default is kept");
    }
  }
  return c;
}

}  // namespace fw

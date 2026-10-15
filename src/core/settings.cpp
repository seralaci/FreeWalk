#include "core/settings.h"
#include "core/text.h"

namespace fw {
namespace {

SettingInfo key_setting(std::string_view key, std::string_view label, std::string_view comment, int Config::*field, bool allow_ctrl = false,
                        bool allow_none = false) {
  SettingInfo s;
  s.section = "keys";
  s.key = key;
  s.tab = "Keys";
  s.label = label;
  s.comment = comment;
  s.kind = SettingKind::Key;
  s.key_field = field;
  s.allow_ctrl = allow_ctrl;
  s.allow_none = allow_none;
  return s;
}

SettingInfo number_setting(std::string_view section, std::string_view key, std::string_view tab, std::string_view label,
                           std::string_view comment, double Config::*field, double min, double max, double step, int decimals) {
  SettingInfo s;
  s.section = section;
  s.key = key;
  s.tab = tab;
  s.label = label;
  s.comment = comment;
  s.kind = SettingKind::Number;
  s.number_field = field;
  s.min = min;
  s.max = max;
  s.step = step;
  s.decimals = decimals;
  return s;
}

SettingInfo flag_setting(std::string_view section, std::string_view key, std::string_view tab, std::string_view label,
                         std::string_view comment, bool Config::*field, Applies applies = Applies::Now) {
  SettingInfo s;
  s.section = section;
  s.key = key;
  s.tab = tab;
  s.label = label;
  s.comment = comment;
  s.kind = SettingKind::Flag;
  s.applies = applies;
  s.flag_field = field;
  return s;
}

// In the INI, comments start at this column.
constexpr std::size_t kCommentColumn = 26;

}  // namespace

const std::vector<SettingInfo>& settings_schema() {
  static const std::vector<SettingInfo> schema = {
      key_setting("toggle", "Leave / return to the truck", "leave and return to the truck", &Config::key_toggle, true),
      key_setting("settings", "Open this window", "the settings window (NONE = disabled)", &Config::key_settings, true, true),
      key_setting("forward", "Walk forward", "", &Config::key_forward),
      key_setting("back", "Walk back", "", &Config::key_back),
      key_setting("left", "Step left", "", &Config::key_left),
      key_setting("right", "Step right", "", &Config::key_right),
      key_setting("run", "Run (hold)", "", &Config::key_run),
      key_setting("flashlight", "Flashlight on / off", "flashlight on/off (NONE = disabled)", &Config::key_flashlight, false, true),
      key_setting("flashlight_wider", "Flashlight beam wider", "wider beam (mouse wheel up too)", &Config::key_flashlight_wider,
                  false, true),
      key_setting("flashlight_narrower", "Flashlight beam narrower", "narrower beam (mouse wheel down too)",
                  &Config::key_flashlight_narrower, false, true),
      number_setting("walker", "walk_speed", "Walking", "Walking speed (m/s)", "m/s", &Config::walk_speed, 0.1, 20.0, 0.1, 1),
      number_setting("walker", "run_speed", "Walking", "Running speed (m/s)", "m/s", &Config::run_speed, 0.1, 20.0, 0.1, 1),
      number_setting("walker", "eye_height", "Walking", "Eye height (m)", "m above the ground", &Config::eye_height, 0.5, 3.0, 0.01, 2),
      flag_setting("walker", "head_bob", "Walking", "Head bob", "1 = slight head bob while walking, 0 = off", &Config::head_bob),
      number_setting("walker", "head_bob_amplitude", "Walking", "Head bob amount (m)", "m", &Config::head_bob_amplitude, 0.0, 0.2, 0.005, 3),
      number_setting("walker", "mouse_sensitivity", "Walking", "Mouse look sensitivity",
                     "mouse look sensitivity while walking (the game's free camera is 1)", &Config::mouse_sensitivity, 0.01, 2.0, 0.01, 2),
      number_setting("walker", "max_distance", "Walking", "Maximum distance from the truck (m)",
                     "m, farther than this from the truck returns you to it automatically", &Config::max_distance, 10.0, 2000.0, 10.0, 0),
      number_setting("walker", "exit_side_distance", "Leaving the truck", "Distance beside the driver's head (m)",
                     "m beyond the driver's head position, sideways", &Config::exit_side_distance, 0.3, 5.0, 0.05, 2),
      flag_setting("walker", "exit_face_door", "Leaving the truck", "Face the door when leaving",
                   "1 = face the door when leaving, 0 = face the truck's front", &Config::exit_face_door),
      number_setting("walker", "exit_face_angle", "Leaving the truck", "Turn towards the front (degrees)",
                     "degrees, turn this much from the door towards the truck's front (negative: towards the rear)",
                     &Config::exit_face_angle, -90.0, 90.0, 1.0, 0),
      number_setting("walker", "ground_offset", "Leaving the truck", "Ground height offset (m)",
                     "m, ground height relative to the truck's telemetry position", &Config::ground_offset, -5.0, 5.0, 0.05, 2),
      flag_setting("walker", "door_animation", "Leaving the truck", "Animated door (blink, steps)",
                   "1 = blink at the door and climb the steps when leaving and entering, 0 = instant", &Config::door_animation),
      number_setting("walker", "door_step_time", "Leaving the truck", "Time on the steps (s)", "s, time spent on the steps",
                     &Config::door_step_time, 0.3, 4, 0.1, 1),
      number_setting("walker", "door_fade_time", "Leaving the truck", "Blink fade (s)", "s, blink fade to black and back",
                     &Config::door_fade_time, 0, 1, 0.05, 2),
      number_setting("walker", "door_out", "Leaving the truck", "Door sill beside the head (m)", "m, door sill sideways from the head",
                     &Config::door_out, 0.3, 1.5, 0.05, 2),
      number_setting("walker", "door_sill_below_head", "Leaving the truck", "Door sill below the head (m)", "m, door sill below the head",
                     &Config::door_sill_below_head, 0.3, 2, 0.05, 2),
      number_setting("walker", "door_min_free", "Leaving the truck", "Free space needed at the door (m)",
                     "m, free space needed in front of the door (otherwise the other door)", &Config::door_min_free, 0, 2, 0.1, 1),
      number_setting("walker", "enter_near_door", "Leaving the truck", "Full animation within (m) of the door",
                     "m, full entry animation within this, a blink from farther away", &Config::enter_near_door, 0, 10, 0.5, 1),
      flag_setting("walker", "hide_hud", "Leaving the truck", "Hide the speedometer and map while walking",
                   "1 = speedometer and map hidden while walking", &Config::hide_hud),
      flag_setting("walker", "key_hints", "Leaving the truck", "Show key hints (get in, get out)",
                   "1 = key hints at the bottom of the screen: get out in the cab, get in at the door", &Config::key_hints),
      flag_setting("collision", "enabled", "Collision", "Ground and collision from the game's physics *",
                   "1 = the walker follows the ground and does not pass through walls (from the game's physics), 0 = flat ground",
                   &Config::collision_enabled, Applies::Restart),
      number_setting("collision", "radius", "Collision", "Body radius (m)", "m, never closer than this to a wall",
                     &Config::collision_radius, 0.1, 1.0, 0.05, 2),
      number_setting("collision", "step_height", "Collision", "Highest step (m)", "m, steps up onto anything lower than this",
                     &Config::step_height, 0.05, 1.0, 0.05, 2),
      number_setting("collision", "step_down_max", "Collision", "Drop that counts as a fall (m)",
                     "m, a step down bigger than this is a fall", &Config::step_down_max, 0.1, 5.0, 0.1, 1),
      number_setting("collision", "ground_follow_time", "Collision", "Ground smoothing (s)",
                     "s, ground following smoothing (0 = instant)", &Config::ground_follow_time, 0.0, 1.0, 0.01, 2),
      number_setting("light", "angle", "Light", "Beam angle (degrees)", "degrees, beam angle at the middle setting",
                     &Config::light_angle, 5, 80, 1, 0),
      number_setting("light", "inner_angle", "Light", "Sharp inner part (degrees)", "degrees, sharp inner part of the beam",
                     &Config::light_inner_angle, 0, 80, 1, 0),
      number_setting("light", "range", "Light", "Range (m)", "m, how far the light reaches", &Config::light_range, 5, 200, 1, 0),
      number_setting("light", "brightness", "Light", "Brightness", "colour multiplier", &Config::light_brightness, 0, 3, 0.05, 2),
      number_setting("light", "r", "Light", "Colour: red", "colour, red", &Config::light_r, 0, 3, 0.01, 2),
      number_setting("light", "g", "Light", "Colour: green", "colour, green", &Config::light_g, 0, 3, 0.01, 2),
      number_setting("light", "b", "Light", "Colour: blue", "colour, blue (less = more yellow)", &Config::light_b, 0, 3, 0.01, 2),
      number_setting("light", "fade_time", "Light", "Fade in / out (s)", "s, fade when switching on and off",
                     &Config::light_fade_time, 0, 2, 0.01, 2),
      number_setting("light", "hand_right", "Light", "Hand: right of the eye (m)", "m, hand to the right of the eye",
                     &Config::light_hand_right, -1, 1, 0.01, 2),
      number_setting("light", "hand_down", "Light", "Hand: below the eye (m)", "m, hand below the eye", &Config::light_hand_down, -1, 1,
                     0.01, 2),
      number_setting("light", "flicker_chance", "Light", "Bulb plays up (per hour)",
                     "times per hour on average the bulb plays up (0 = never)",
                     &Config::light_flicker_chance, 0, 60, 1, 0),
      flag_setting("light", "own_light", "Light", "Own light where no street lamp can be borrowed",
                   "1 = own light if no street lamp can be borrowed", &Config::light_own),
      number_setting("light", "borrow_min_distance", "Light", "Borrow street lamps farther than (m)",
                     "m, street lamps closer than this are never taken", &Config::light_borrow_min, 0, 2000, 10, 0),
      number_setting("light", "borrow_max_distance", "Light", "Look for street lamps within (m)", "m, search within this first",
                     &Config::light_borrow_max, 100, 5000, 10, 0),
      flag_setting("debug", "discovery_log", "Debug", "Diagnostic log lines every second",
                   "1 = diagnostic lines in game.log.txt every second", &Config::discovery_log),
      flag_setting("debug", "write_on_frame_start", "Debug", "Write the camera at frame start",
                   "1 = write the camera position at frame start (if the image shakes)", &Config::write_on_frame_start),
      flag_setting("debug", "directinput_filter", "Debug", "DirectInput keyboard filter",
                   "1 = DirectInput keyboard filter (experimental; free the walking keys in the game's settings)",
                   &Config::directinput_filter, Applies::Restart),
      flag_setting("debug", "switch_test", "Debug", "Camera switch test only",
                   "1 = the key only switches to the free camera and back (for discovery)", &Config::switch_test, Applies::Restart),
  };
  return schema;
}

std::string format_setting_value(const SettingInfo& s, const Config& c) {
  switch (s.kind) {
    case SettingKind::Key: return key_name(c.*s.key_field);
    case SettingKind::Flag: return c.*s.flag_field ? "1" : "0";
    case SettingKind::Number: break;
  }
  std::string text = strf("%.*f", s.decimals, c.*s.number_field);
  if (text.find('.') != std::string::npos) {
    while (text.back() == '0') text.pop_back();
    if (text.back() == '.') text.pop_back();
  }
  if (text == "-0") text = "0";
  return text;
}

std::string format_config(const Config& c) {
  std::string out =
      "; FreeWalk - leave the truck and walk around in Euro Truck Simulator 2\n"
      "; Every line is optional: missing values fall back to the default.\n"
      "; The Save button of the settings window (Ctrl+F10) rewrites this whole file.\n";
  std::string_view section;
  for (const auto& s : settings_schema()) {
    if (s.section != section) {
      section = s.section;
      out += "\n[" + std::string(section) + "]\n";
    }
    std::string line = std::string(s.key) + " = " + format_setting_value(s, c);
    if (!s.comment.empty()) {
      if (line.size() < kCommentColumn) line.resize(kCommentColumn, ' ');
      else line += ' ';
      line += "; " + std::string(s.comment);
    }
    out += line + "\n";
  }
  return out;
}

std::vector<KeyConflict> key_conflicts(const Config& c) {
  std::vector<const SettingInfo*> keys;
  for (const auto& s : settings_schema())
    if (s.kind == SettingKind::Key) keys.push_back(&s);
  std::vector<KeyConflict> conflicts;
  for (std::size_t i = 0; i < keys.size(); ++i)
    for (std::size_t j = i + 1; j < keys.size(); ++j) {
      const int a = c.*keys[i]->key_field;
      if (a != 0 && a == c.*keys[j]->key_field) conflicts.push_back({keys[i]->label, keys[j]->label});
    }
  return conflicts;
}

bool same_settings(const Config& a, const Config& b) {
  for (const auto& s : settings_schema()) {
    if (s.key_field && a.*s.key_field != b.*s.key_field) return false;
    if (s.number_field && a.*s.number_field != b.*s.number_field) return false;
    if (s.flag_field && a.*s.flag_field != b.*s.flag_field) return false;
  }
  return true;
}

Config apply_settings(const Config& active, const Config& wanted, bool walking) {
  Config result = wanted;
  for (const auto& s : settings_schema()) {
    const bool keep = s.applies == Applies::Restart || (walking && s.kind == SettingKind::Key);
    if (!keep) continue;
    if (s.key_field) result.*s.key_field = active.*s.key_field;
    if (s.number_field) result.*s.number_field = active.*s.number_field;
    if (s.flag_field) result.*s.flag_field = active.*s.flag_field;
  }
  return result;
}

}  // namespace fw

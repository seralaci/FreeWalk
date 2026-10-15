#include <cmath>
#include <fstream>
#include <set>
#include <sstream>
#include "check.h"
#include "core/settings.h"

TEST(schema_entries_are_unique_and_complete) {
  std::set<std::string> names;
  for (const auto& s : fw::settings_schema()) {
    CHECK(names.insert(std::string(s.section) + "." + std::string(s.key)).second);
    const int fields = (s.key_field ? 1 : 0) + (s.number_field ? 1 : 0) + (s.flag_field ? 1 : 0);
    CHECK(fields == 1);
    CHECK(!s.label.empty());
  }
  CHECK(names.size() == 55);
}

TEST(defaults_are_inside_their_range_and_on_the_step_grid) {
  const fw::Config c;
  for (const auto& s : fw::settings_schema()) {
    if (s.kind != fw::SettingKind::Number) continue;
    const double v = c.*s.number_field;
    CHECK(v >= s.min && v <= s.max);
    const double steps = (v - s.min) / s.step;
    CHECK_NEAR(steps, std::round(steps), 1e-6);
  }
}

TEST(format_then_parse_gives_the_same_settings) {
  fw::Config c;
  c.key_toggle = fw::kKeyCtrl | 'G';
  c.key_settings = 0;
  c.key_run = 0xA2;
  c.walk_speed = 1.7;
  c.exit_face_angle = -15;
  c.head_bob = false;
  c.directinput_filter = true;
  std::vector<std::string> warnings;
  const fw::Config back = fw::parse_config(fw::format_config(c), warnings);
  CHECK(warnings.empty());
  CHECK(fw::same_settings(back, c));
  CHECK(!fw::same_settings(back, fw::Config{}));
}

// The packaged FreeWalk.ini is exactly what the settings window's Save button would write from the defaults.
TEST(shipped_ini_matches_the_formatter) {
  std::ifstream in(FREEWALK_INI_PATH, std::ios::binary);
  std::stringstream text;
  text << in.rdbuf();
  CHECK(text.str() == fw::format_config(fw::Config{}));
}

TEST(key_conflicts_are_found) {
  fw::Config c;
  CHECK(fw::key_conflicts(c).empty());
  c.key_run = 'W';
  const auto conflicts = fw::key_conflicts(c);
  CHECK(conflicts.size() == 1);
  CHECK(conflicts[0].first == "Walk forward");
  CHECK(conflicts[0].second == "Run (hold)");
}

TEST(walking_keys_cannot_use_ctrl_or_none) {
  std::vector<std::string> warnings;
  const fw::Config c = fw::parse_config("[keys]\nforward = CTRL+W\nback = NONE\nsettings = NONE\n", warnings);
  CHECK(warnings.size() == 2);
  CHECK(c.key_forward == 'W');
  CHECK(c.key_back == 'S');
  CHECK(c.key_settings == 0);
}

// During a walk the walker keys do not change on the fly; they take effect from the next exit.
TEST(keys_wait_for_the_next_exit_while_walking) {
  fw::Config active;
  fw::Config wanted;
  wanted.key_forward = 'I';
  wanted.walk_speed = 2.0;
  const fw::Config walking = fw::apply_settings(active, wanted, true);
  CHECK(walking.key_forward == 'W');
  CHECK_NEAR(walking.walk_speed, 2.0, 1e-12);
  const fw::Config driving = fw::apply_settings(walking, wanted, false);
  CHECK(driving.key_forward == 'I');
}

TEST(restart_settings_never_change_while_running) {
  fw::Config active;
  fw::Config wanted;
  wanted.directinput_filter = true;
  wanted.switch_test = true;
  wanted.discovery_log = true;
  const fw::Config result = fw::apply_settings(active, wanted, false);
  CHECK(!result.directinput_filter);
  CHECK(!result.switch_test);
  CHECK(result.discovery_log);
}

// Every slider position the window can set reads back exactly the same from the saved INI.
TEST(every_slider_position_survives_a_save) {
  for (const auto& s : fw::settings_schema()) {
    if (s.kind != fw::SettingKind::Number) continue;
    const long steps = std::lround((s.max - s.min) / s.step);
    const double scale = std::pow(10.0, s.decimals);
    for (long k = 0; k <= steps; ++k) {
      fw::Config c;
      c.*s.number_field = std::round((s.min + static_cast<double>(k) * s.step) * scale) / scale;
      std::vector<std::string> warnings;
      const fw::Config back = fw::parse_config(fw::format_config(c), warnings);
      CHECK(warnings.empty());
      CHECK(back.*s.number_field == c.*s.number_field);
    }
  }
}

TEST(flashlight_keys_join_the_conflict_check) {
  fw::Config c;
  c.key_flashlight = 'W';
  CHECK(!fw::key_conflicts(c).empty());
  c.key_flashlight = 'F';
  c.key_flashlight_narrower = 'F';
  CHECK(!fw::key_conflicts(c).empty());
}

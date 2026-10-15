#include <string>
#include <vector>
#include "check.h"
#include "core/config.h"

TEST(parse_key_names) {
  CHECK(fw::parse_key("F10") == 0x79);
  CHECK(fw::parse_key("f1") == 0x70);
  CHECK(fw::parse_key("F24") == 0x87);
  CHECK(fw::parse_key("w") == 'W');
  CHECK(fw::parse_key("7") == '7');
  CHECK(fw::parse_key("Shift") == 0x10);
  CHECK(fw::parse_key("SPACE") == 0x20);
  CHECK(fw::parse_key("0x79") == 0x79);
  CHECK(fw::parse_key("F25") == -1);
  CHECK(fw::parse_key("banana") == -1);
  CHECK(fw::parse_key("") == -1);
}

TEST(empty_text_gives_defaults_without_warnings) {
  std::vector<std::string> warnings;
  const fw::Config c = fw::parse_config("", warnings);
  CHECK(warnings.empty());
  CHECK(c.key_toggle == 0x79);
  CHECK_NEAR(c.walk_speed, 1.4, 1e-12);
  CHECK_NEAR(c.run_speed, 4.0, 1e-12);
  CHECK_NEAR(c.eye_height, 1.7, 1e-12);
  CHECK(c.head_bob);
  CHECK_NEAR(c.max_distance, 150.0, 1e-12);
  CHECK(!c.discovery_log);
  CHECK(!c.switch_test);
  CHECK_NEAR(c.mouse_sensitivity, 0.25, 1e-12);
  CHECK_NEAR(c.exit_side_distance, 1.5, 1e-12);
  CHECK(c.exit_face_door);
  CHECK_NEAR(c.exit_face_angle, 22.0, 1e-12);
  CHECK(!c.directinput_filter);
}

TEST(exit_facing_is_read) {
  std::vector<std::string> warnings;
  const fw::Config c = fw::parse_config("[walker]\nexit_face_door = 0\nexit_face_angle = -10\n", warnings);
  CHECK(warnings.empty());
  CHECK(!c.exit_face_door);
  CHECK_NEAR(c.exit_face_angle, -10.0, 1e-12);
}

TEST(values_are_read_from_sections) {
  std::vector<std::string> warnings;
  const fw::Config c = fw::parse_config(
      "; comment\n[keys]\ntoggle = F9\nrun = CTRL\n[walker]\nwalk_speed = 1.2 ; slower\neye_height=1.8\nhead_bob = 0\n[debug]\ndiscovery_log = 1\nswitch_test = 1\n",
      warnings);
  CHECK(warnings.empty());
  CHECK(c.key_toggle == 0x78);
  CHECK(c.key_run == 0x11);
  CHECK_NEAR(c.walk_speed, 1.2, 1e-12);
  CHECK_NEAR(c.eye_height, 1.8, 1e-12);
  CHECK(!c.head_bob);
  CHECK(c.discovery_log);
  CHECK(c.switch_test);
}

TEST(windows_line_endings_and_bom_are_accepted) {
  std::vector<std::string> warnings;
  const fw::Config c = fw::parse_config("\xEF\xBB\xBF[walker]\r\nrun_speed = 5\r\nmax_distance = 300\r\n", warnings);
  CHECK(warnings.empty());
  CHECK_NEAR(c.run_speed, 5.0, 1e-12);
  CHECK_NEAR(c.max_distance, 300.0, 1e-12);
}

TEST(mouse_sensitivity_is_read_and_range_checked) {
  std::vector<std::string> warnings;
  CHECK_NEAR(fw::parse_config("[walker]\nmouse_sensitivity = 0.5\n", warnings).mouse_sensitivity, 0.5, 1e-12);
  CHECK(warnings.empty());
  CHECK_NEAR(fw::parse_config("[walker]\nmouse_sensitivity = 0\n", warnings).mouse_sensitivity, 0.25, 1e-12);
  CHECK(warnings.size() == 1);
}

TEST(bad_values_keep_defaults_and_warn) {
  std::vector<std::string> warnings;
  const fw::Config c = fw::parse_config(
      "[walker]\nwalk_speed = fast\neye_height = 9\nhead_bob = yes\n[keys]\ntoggle = NOPE\n[walker]\nunknown_thing = 1\nno equals sign\n",
      warnings);
  CHECK_NEAR(c.walk_speed, 1.4, 1e-12);
  CHECK_NEAR(c.eye_height, 1.7, 1e-12);
  CHECK(c.head_bob);
  CHECK(c.key_toggle == 0x79);
  CHECK(warnings.size() == 6);
}

TEST(not_a_number_is_rejected) {
  std::vector<std::string> warnings;
  const fw::Config c = fw::parse_config("[walker]\nwalk_speed = nan\nrun_speed = inf\n", warnings);
  CHECK_NEAR(c.walk_speed, 1.4, 1e-12);
  CHECK_NEAR(c.run_speed, 4.0, 1e-12);
  CHECK(warnings.size() == 2);
}

TEST(directinput_filter_can_be_switched_on) {
  std::vector<std::string> warnings;
  const fw::Config c = fw::parse_config("[debug]\ndirectinput_filter = 1\n", warnings);
  CHECK(warnings.empty());
  CHECK(c.directinput_filter);
}

TEST(collision_settings_are_read) {
  std::vector<std::string> warnings;
  const fw::Config c = fw::parse_config(
      "[collision]\nenabled = 0\nradius = 0.3\nstep_height = 0.25\nstep_down_max = 1.5\nground_follow_time = 0.2\n", warnings);
  CHECK(warnings.empty());
  CHECK(!c.collision_enabled);
  CHECK_NEAR(c.collision_radius, 0.3, 1e-12);
  CHECK_NEAR(c.step_height, 0.25, 1e-12);
  CHECK_NEAR(c.step_down_max, 1.5, 1e-12);
  CHECK_NEAR(c.ground_follow_time, 0.2, 1e-12);
}

TEST(collision_radius_is_range_checked) {
  std::vector<std::string> warnings;
  const fw::Config c = fw::parse_config("[collision]\nradius = 3\n", warnings);
  CHECK(warnings.size() == 1);
  CHECK_NEAR(c.collision_radius, 0.40, 1e-12);
}

TEST(step_height_cannot_be_zero) {
  // The ground ray starts at feet + step_height: at 0 every ground hit would be at distance 0, and motion would see it as "inside a body".
  std::vector<std::string> warnings;
  const fw::Config c = fw::parse_config("[collision]\nstep_height = 0\n", warnings);
  CHECK(warnings.size() == 1);
  CHECK_NEAR(c.step_height, 0.45, 1e-12);
}

TEST(numpad_plus_and_minus_have_names) {
  CHECK(fw::parse_key("NUMPAD+") == 0x6B);
  CHECK(fw::parse_key("numpad-") == 0x6D);
  CHECK(fw::key_name(0x6B) == "NUMPAD+");
  CHECK(fw::key_name(0x6D) == "NUMPAD-");
}

TEST(flashlight_settings_are_read) {
  std::vector<std::string> warnings;
  const fw::Config c = fw::parse_config(
      "[keys]\nflashlight = L\nflashlight_wider = NONE\nflashlight_narrower = G\n"
      "[light]\nangle = 40\ninner_angle = 20\nrange = 60\nbrightness = 1.5\nr = 0.5\ng = 0.6\nb = 0.7\nfade_time = 0\n"
      "hand_right = -0.3\nhand_down = 0.4\nflicker_chance = 12\nown_light = 0\nborrow_min_distance = 100\nborrow_max_distance = 2000\n",
      warnings);
  CHECK(warnings.empty());
  CHECK(c.key_flashlight == 'L');
  CHECK(c.key_flashlight_wider == 0);
  CHECK(c.key_flashlight_narrower == 'G');
  CHECK_NEAR(c.light_angle, 40, 1e-12);
  CHECK_NEAR(c.light_inner_angle, 20, 1e-12);
  CHECK_NEAR(c.light_range, 60, 1e-12);
  CHECK_NEAR(c.light_brightness, 1.5, 1e-12);
  CHECK_NEAR(c.light_r, 0.5, 1e-12);
  CHECK_NEAR(c.light_g, 0.6, 1e-12);
  CHECK_NEAR(c.light_b, 0.7, 1e-12);
  CHECK_NEAR(c.light_fade_time, 0, 1e-12);
  CHECK_NEAR(c.light_hand_right, -0.3, 1e-12);
  CHECK_NEAR(c.light_hand_down, 0.4, 1e-12);
  CHECK_NEAR(c.light_flicker_chance, 12, 1e-12);
  CHECK(!c.light_own);
  CHECK_NEAR(c.light_borrow_min, 100, 1e-12);
  CHECK_NEAR(c.light_borrow_max, 2000, 1e-12);
}

TEST(door_settings_are_read) {
  std::vector<std::string> warnings;
  const fw::Config c = fw::parse_config(
      "[walker]\ndoor_animation = 0\ndoor_step_time = 2\ndoor_fade_time = 0.5\ndoor_out = 0.9\ndoor_sill_below_head = 1.2\n"
      "door_min_free = 1\nenter_near_door = 5\n",
      warnings);
  CHECK(warnings.empty());
  CHECK(!c.door_animation);
  CHECK_NEAR(c.door_step_time, 2, 1e-12);
  CHECK_NEAR(c.door_fade_time, 0.5, 1e-12);
  CHECK_NEAR(c.door_out, 0.9, 1e-12);
  CHECK_NEAR(c.door_sill_below_head, 1.2, 1e-12);
  CHECK_NEAR(c.door_min_free, 1, 1e-12);
  CHECK_NEAR(c.enter_near_door, 5, 1e-12);
}

TEST(hud_and_hint_settings_are_read) {
  std::vector<std::string> warnings;
  CHECK(fw::Config{}.hide_hud);
  CHECK(fw::Config{}.key_hints);
  const fw::Config c = fw::parse_config("[walker]\nhide_hud = 0\nkey_hints = 0\n", warnings);
  CHECK(warnings.empty());
  CHECK(!c.hide_hud);
  CHECK(!c.key_hints);
}

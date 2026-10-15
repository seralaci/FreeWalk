#include <set>
#include "check.h"
#include "core/keys.h"

namespace {
bool down_in(const std::set<int>& keys, int vk) { return keys.count(vk) != 0; }
}

TEST(map_keys_reads_configured_keys) {
  const fw::Config cfg;
  const std::set<int> held{'W', 'D', 0x10};
  const fw::WalkKeys k = fw::map_keys(true, false, [&](int vk) { return down_in(held, vk); }, cfg);
  CHECK(k.forward);
  CHECK(!k.back);
  CHECK(!k.left);
  CHECK(k.right);
  CHECK(k.run);
}

TEST(map_keys_ignores_keys_when_game_not_focused) {
  const fw::Config cfg;
  const fw::WalkKeys k = fw::map_keys(false, false, [](int) { return true; }, cfg);
  CHECK(!k.forward && !k.back && !k.left && !k.right && !k.run);
}

TEST(map_keys_ignores_keys_while_paused) {
  const fw::Config cfg;
  const fw::WalkKeys k = fw::map_keys(true, true, [](int) { return true; }, cfg);
  CHECK(!k.forward && !k.back && !k.left && !k.right && !k.run);
}

TEST(edge_detector_fires_once_per_press) {
  fw::EdgeDetector e;
  CHECK(!e.update(false));
  CHECK(e.update(true));
  CHECK(!e.update(true));
  CHECK(!e.update(true));
  CHECK(!e.update(false));
  CHECK(e.update(true));
}

TEST(key_name_and_parse_key_are_inverse) {
  for (int code = 1; code < 255; ++code) {
    CHECK(fw::parse_key(fw::key_name(code)) == code);
    if (code != 0x11 && code != 0xA2 && code != 0xA3) CHECK(fw::parse_key(fw::key_name(fw::kKeyCtrl | code)) == (fw::kKeyCtrl | code));
  }
}

TEST(key_names_are_readable) {
  CHECK(fw::key_name('W') == "W");
  CHECK(fw::key_name(0x79) == "F10");
  CHECK(fw::key_name(0x10) == "SHIFT");
  CHECK(fw::key_name(fw::kKeyCtrl | 0x79) == "CTRL+F10");
  CHECK(fw::key_name(0) == "NONE");
  CHECK(fw::parse_key("none") == 0);
  CHECK(fw::parse_key("ctrl+f10") == (fw::kKeyCtrl | 0x79));
  CHECK(fw::parse_key("CTRL+CTRL") == -1);
}

TEST(combo_needs_ctrl_exactly_when_asked) {
  bool ctrl = false;
  const auto down = [&](int k) { return k == 0x79 || (k == 0x11 && ctrl); };
  CHECK(fw::combo_down(0x79, down));
  CHECK(!fw::combo_down(fw::kKeyCtrl | 0x79, down));
  ctrl = true;
  CHECK(!fw::combo_down(0x79, down));  // Ctrl+F10 is not exit
  CHECK(fw::combo_down(fw::kKeyCtrl | 0x79, down));
  CHECK(!fw::combo_down(0, down));
}

// Code review: CTRL itself can be the command key; then we cannot require Ctrl to be released.
TEST(ctrl_itself_can_be_a_command_key) {
  const auto down = [](int k) { return k == 0x11; };
  CHECK(fw::combo_down(0x11, down));
}

TEST(double_ctrl_prefix_is_rejected) {
  CHECK(fw::parse_key("CTRL+CTRL+F10") == -1);
}

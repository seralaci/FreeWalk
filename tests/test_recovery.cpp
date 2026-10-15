#include "check.h"
#include "core/recovery.h"

TEST(recovery_round_trip) {
  const std::vector<fw::SavedSetting> saved = {{"g_flyspeed", "100.0"}, {"g_mirrors_keep_hidden", "0"}};
  const auto back = fw::parse_recovery(fw::format_recovery(saved));
  CHECK(back.size() == 2);
  CHECK(back[0].name == "g_flyspeed");
  CHECK(back[0].value == "100.0");
  CHECK(back[1].name == "g_mirrors_keep_hidden");
  CHECK(back[1].value == "0");
}

TEST(recovery_accepts_windows_line_ends_and_empty_values) {
  const auto back = fw::parse_recovery("g_developer=0\r\ng_flyspeed=\r\n");
  CHECK(back.size() == 2);
  CHECK(back[0].value == "0");
  CHECK(back[1].value.empty());
}

// Someone may edit the file by hand: we only accept lines with a valid name and value.
TEST(recovery_skips_malformed_lines) {
  const auto back = fw::parse_recovery("no equals sign\n=1\nBad Name=1\ng_ok=1\ng_ctrl=a\x01b\n");
  CHECK(back.size() == 1);
  CHECK(back[0].name == "g_ok");
}

TEST(recovery_value_keeps_equals_signs_after_the_first) {
  const auto back = fw::parse_recovery("g_x=a=b\n");
  CHECK(back.size() == 1);
  CHECK(back[0].value == "a=b");
}

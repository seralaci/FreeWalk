#include "check.h"
#include "core/focus_schedule.h"

TEST(focus_starts_at_once_with_the_smallest_box) {
  fw::FocusSchedule s;
  const auto first = s.step(0.0);
  CHECK(first.has_value());
  if (first) CHECK_NEAR(*first, fw::kFocusFirstBox, 1e-12);
  CHECK(!s.step(0.1));
}

TEST(focus_grows_the_box_quickly_then_slows_down) {
  fw::FocusSchedule s;
  int queries = 0;
  double last_box = 0;
  // Over 3 s: the first kFocusRampQueries queries every 0.25 s, then once per second.
  for (int i = 0; i <= 300; ++i) {
    if (const auto box = s.step(i == 0 ? 0.0 : 0.01)) {
      ++queries;
      CHECK(*box >= last_box);
      last_box = *box;
    }
  }
  CHECK(queries >= fw::kFocusRampQueries && queries <= fw::kFocusRampQueries + 1);
  CHECK_NEAR(last_box, fw::kFocusLastBox, 1e-12);
}

TEST(focus_reset_starts_the_ramp_again) {
  fw::FocusSchedule s;
  for (int i = 0; i < 20; ++i) s.step(1.0);
  s.reset();
  const auto box = s.step(0.0);
  CHECK(box.has_value());
  if (box) CHECK_NEAR(*box, fw::kFocusFirstBox, 1e-12);
}

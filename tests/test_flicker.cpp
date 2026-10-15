#include "check.h"
#include "core/flicker.h"

TEST(flicker_never_happens_at_zero_chance) {
  fw::Flicker f(7);
  for (int i = 0; i < 100000; ++i) CHECK_NEAR(f.step(0.05, 0.0), 1.0, 1e-12);
}

TEST(flicker_is_reproducible_with_a_seed) {
  fw::Flicker a(42), b(42);
  for (int i = 0; i < 20000; ++i) CHECK_NEAR(a.step(0.05, 60.0), b.step(0.05, 60.0), 1e-12);
}

TEST(flicker_stays_between_zero_and_one_and_recovers) {
  fw::Flicker f(3);
  bool seen_dark = false;
  for (int i = 0; i < 200000; ++i) {
    const double m = f.step(0.02, 60.0);
    CHECK(m >= 0.0 && m <= 1.0);
    if (m < 0.5) seen_dark = true;
  }
  CHECK(seen_dark);
  // If the frequency becomes 0, the running event ends and the multiplier stays 1 afterwards.
  for (int i = 0; i < 500; ++i) f.step(0.02, 0.0);
  CHECK(!f.active());
  CHECK_NEAR(f.step(0.02, 0.0), 1.0, 1e-12);
}

TEST(flicker_happens_about_as_often_as_set) {
  fw::Flicker f(11);
  int events = 0;
  bool was_active = false;
  for (int i = 0; i < 10 * 3600 * 20; ++i) {  // 10 hours, in 50 ms steps
    f.step(0.05, 60.0);
    if (f.active() && !was_active) ++events;
    was_active = f.active();
  }
  CHECK(events > 510 && events < 690);  // 600 expected, Poisson deviation about 24
}

#include <vector>
#include "check.h"
#include "core/light_borrow.h"

namespace {
fw::LightCandidate lamp(std::uint64_t id, double x, bool active = true) { return {id, {x, 5, 0}, active}; }
}  // namespace

TEST(borrow_never_takes_a_near_lamp) {
  CHECK(!fw::choose_light({0, 0, 0}, {lamp(1, 50), lamp(2, 299)}, 300, 900));
}

TEST(borrow_takes_the_nearest_within_the_window) {
  CHECK(fw::choose_light({0, 0, 0}, {lamp(1, 800), lamp(2, 400), lamp(3, 100)}, 300, 900) == 2u);
}

TEST(borrow_falls_back_to_the_nearest_far_lamp) {
  CHECK(fw::choose_light({0, 0, 0}, {lamp(1, 3000), lamp(2, 1500)}, 300, 900) == 2u);
}

TEST(borrow_skips_lamps_that_are_off) {
  CHECK(fw::choose_light({0, 0, 0}, {lamp(1, 400, false), lamp(2, 700)}, 300, 900) == 2u);
  CHECK(!fw::choose_light({0, 0, 0}, {}, 300, 900));
}

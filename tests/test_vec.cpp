#include "check.h"
#include "core/vec.h"

namespace {
constexpr double kPi = 3.14159265358979323846;
}

TEST(rotate_heading_forward_matches_forward_from_yaw) {
  for (double h : {0.0, 0.3, kPi / 2, kPi, 4.0}) {
    const fw::Vec3 a = fw::rotate_heading({0, 0, -1}, h);
    const fw::Vec3 b = fw::forward_from_yaw(h);
    CHECK_NEAR(a.x, b.x, 1e-12);
    CHECK_NEAR(a.z, b.z, 1e-12);
  }
}

TEST(heading_quarter_turn_faces_west) {
  // According to the SDK, 0.25 turn (90 degrees) is west, which is the -X axis.
  const fw::Vec3 f = fw::forward_from_yaw(kPi / 2);
  CHECK_NEAR(f.x, -1.0, 1e-12);
  CHECK_NEAR(f.z, 0.0, 1e-12);
}

TEST(right_at_heading_zero_is_east) {
  const fw::Vec3 r = fw::right_from_yaw(0.0);
  CHECK_NEAR(r.x, 1.0, 1e-12);
  CHECK_NEAR(r.z, 0.0, 1e-12);
  const fw::Vec3 local_right = fw::rotate_heading({1, 0, 0}, 0.0);
  CHECK_NEAR(local_right.x, 1.0, 1e-12);
}

TEST(horizontal_distance_ignores_height) {
  CHECK_NEAR(fw::horizontal_distance({0, 0, 0}, {3, 100, 4}), 5.0, 1e-12);
}

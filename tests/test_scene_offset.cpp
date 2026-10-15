#include <limits>
#include <vector>
#include "check.h"
#include "core/scene_offset.h"

TEST(offset_is_zero_near_the_origin) {
  const auto o = fw::offset_from_actor({100, 10, 200}, {7, {101, 10, 199}});
  CHECK(o.has_value());
  if (!o) return;
  CHECK_NEAR(o->x, 0.0, 1e-12);
  CHECK_NEAR(o->z, 0.0, 1e-12);
  CHECK_NEAR(o->residual, 2.0, 1e-12);
  CHECK(o->actor == 7);
}

TEST(offset_rounds_to_512_in_both_directions) {
  const auto o = fw::offset_from_actor({1030, 5, -1530}, {1, {6, 5, 6}});
  CHECK(o.has_value());
  if (!o) return;
  CHECK_NEAR(o->x, 1024.0, 1e-12);
  CHECK_NEAR(o->z, -1536.0, 1e-12);
  CHECK_NEAR(o->residual, 0.0, 1e-12);
}

TEST(offset_needs_a_small_residual_and_height) {
  CHECK(!fw::offset_from_actor({0, 0, 0}, {1, {-7, 0, 0}}));
  CHECK(!fw::offset_from_actor({0, 0, 0}, {1, {1, 31, 0}}));
  const double nan = std::numeric_limits<double>::quiet_NaN();
  CHECK(!fw::offset_from_actor({0, 0, 0}, {1, {nan, 0, 0}}));
}

TEST(estimate_picks_the_smallest_residual) {
  const std::vector<fw::ActorPosition> actors = {{1, {515, 0, 3}}, {2, {512.5, 0, 0.5}}, {3, {0, 50, 0}}};
  const auto o = fw::estimate_scene_offset({1024.5, 0, 0}, actors);
  CHECK(o.has_value());
  if (!o) return;
  CHECK(o->actor == 2);
  CHECK_NEAR(o->x, 512.0, 1e-12);
  CHECK_NEAR(o->residual, 0.5, 1e-12);
  CHECK(!fw::estimate_scene_offset({0, 0, 0}, {}));
}

TEST(world_to_scene_keeps_the_height) {
  const fw::SceneOffset o{1024, -512, 0, 0};
  const fw::Vec3 s = fw::world_to_scene({1030, 7, -500}, o);
  CHECK_NEAR(s.x, 6.0, 1e-12);
  CHECK_NEAR(s.y, 7.0, 1e-12);
  CHECK_NEAR(s.z, 12.0, 1e-12);
}

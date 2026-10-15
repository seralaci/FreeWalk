#include <cmath>
#include <optional>
#include "check.h"
#include "core/motion.h"
#include "test_world.h"

namespace {

fw::MotionSettings settings() { return fw::MotionSettings{}; }

// Gap between the capsule surface and the wall plane.
double gap_to(const fwtest::Wall& w, fw::Vec3 feet) {
  return (feet.x - w.point.x) * w.normal.x + (feet.z - w.point.z) * w.normal.z - settings().radius;
}

}  // namespace

TEST(body_capsule_spans_from_step_height_to_above_the_eye) {
  const auto b = fw::body_capsule({1, 10, 2}, settings());
  CHECK_NEAR(b.center.x, 1.0, 1e-12);
  CHECK_NEAR(b.center.z, 2.0, 1e-12);
  CHECK_NEAR(b.center.y - b.capsule.half_height - b.capsule.radius, 10.45, 1e-9);
  CHECK_NEAR(b.center.y + b.capsule.half_height + b.capsule.radius, 10.0 + 1.7 + 0.08, 1e-9);
  CHECK_NEAR(b.capsule.radius, 0.40, 1e-12);
}

TEST(free_path_moves_the_whole_way) {
  fwtest::TestWorld world;
  const fw::Vec3 end = fw::move_horizontal(world, {0, 0, 0}, {0, 0, -1}, 0.1, settings());
  CHECK_NEAR(end.x, 0.0, 1e-12);
  CHECK_NEAR(end.z, -1.0, 1e-12);
  CHECK_NEAR(end.y, 0.0, 1e-12);
}

TEST(standing_still_does_not_query) {
  fwtest::TestWorld world;
  const fw::Vec3 end = fw::move_horizontal(world, {3, 0, 4}, {0, 0, 0}, 0.1, settings());
  CHECK_NEAR(end.x, 3.0, 1e-12);
  CHECK(world.sweeps == 0);
}

TEST(frontal_wall_stops_at_the_skin) {
  fwtest::TestWorld world;
  world.walls.push_back({{0, 0, -1}, {0, 0, 1}});
  const fw::Vec3 end = fw::move_horizontal(world, {0, 0, 0}, {0, 0, -1}, 0.1, settings());
  CHECK_NEAR(end.z, -1.0 + 0.40 + fw::kSweepSkin, 1e-9);
  CHECK_NEAR(end.x, 0.0, 1e-12);
}

TEST(angled_wall_slides_along) {
  fwtest::TestWorld world;
  const double s = std::sqrt(0.5);
  world.walls.push_back({{0, 0, -1}, {s, 0, s}});
  const fw::Vec3 end = fw::move_horizontal(world, {0, 0, 0}, {0, 0, -1}, 0.1, settings());
  CHECK(end.x > 0.05);
  CHECK(gap_to(world.walls[0], end) >= -1e-9);
  CHECK(fw::horizontal_distance(end, {0, 0, 0}) < 1.0);
}

TEST(corner_stops_without_entering_either_wall) {
  fwtest::TestWorld world;
  world.walls.push_back({{0, 0, -1}, {0, 0, 1}});
  world.walls.push_back({{-1, 0, 0}, {1, 0, 0}});
  const fw::Vec3 end = fw::move_horizontal(world, {0, 0, 0}, {-1.4, 0, -1.4}, 0.1, settings());
  CHECK(gap_to(world.walls[0], end) >= -1e-9);
  CHECK(gap_to(world.walls[1], end) >= -1e-9);
  CHECK_NEAR(end.x, -0.586, 0.01);
  CHECK_NEAR(end.z, -0.586, 0.01);
}

TEST(low_obstacle_does_not_block) {
  fwtest::TestWorld world;
  world.walls.push_back({{0, 0, -1}, {0, 0, 1}, 0.3});  // curb: lower than step_height
  const fw::Vec3 end = fw::move_horizontal(world, {0, 0, 0}, {0, 0, -2}, 0.1, settings());
  CHECK_NEAR(end.z, -2.0, 1e-12);
}

TEST(overlap_pushes_out_at_limited_speed) {
  fwtest::TestWorld world;
  world.walls.push_back({{0, 0, -1}, {0, 0, 1}});
  // The body is 0.1 m inside the wall: the push-out is at most the depth + 5 mm, then the desired motion to the east is free.
  const fw::Vec3 end = fw::move_horizontal(world, {0, 0, -0.7}, {0.1, 0, 0}, 0.1, settings());
  CHECK_NEAR(end.z, -0.7 + 0.1 + fw::kDepenetrationExtra, 1e-9);
  CHECK_NEAR(end.x, 0.1, 1e-9);
}

TEST(deep_overlap_is_limited_per_frame) {
  fwtest::TestWorld world;
  world.walls.push_back({{0, 0, -1}, {0, 0, 1}});
  const fw::Vec3 end = fw::move_horizontal(world, {0, 0, -1.6}, {0.1, 0, 0}, 0.1, settings());
  CHECK_NEAR(end.z, -1.6 + fw::kDepenetrationSpeed * 0.1, 1e-9);
}

namespace {

// Stairs: north of z = -1 the ground is higher.
fwtest::TestWorld step_world(double height) {
  fwtest::TestWorld world;
  world.ground = [height](double, double z) { return std::optional<double>(z < -1 ? height : 0.0); };
  return world;
}

void run_vertical(fw::World& world, fw::Vec3& feet, fw::Vertical& v, double seconds) {
  for (int i = 0; i < static_cast<int>(std::lround(seconds / 0.1)); ++i) fw::move_vertical(world, feet, v, 0.1, settings());
}

}  // namespace

TEST(steps_up_smoothly) {
  auto world = step_world(0.3);
  fw::Vec3 feet{0, 0, -1.5};
  fw::Vertical v;
  fw::move_vertical(world, feet, v, 0.1, settings());
  CHECK(feet.y > 0.05 && feet.y < 0.3);
  run_vertical(world, feet, v, 1.0);
  CHECK_NEAR(feet.y, 0.3, fw::kGroundSnapTolerance);
  CHECK(!v.falling);
}

TEST(steps_down_smoothly) {
  auto world = step_world(0.3);
  fw::Vec3 feet{0, 0.3, 0.5};
  fw::Vertical v;
  run_vertical(world, feet, v, 1.0);
  CHECK_NEAR(feet.y, 0.0, fw::kGroundSnapTolerance);
  CHECK(!v.falling);
}

TEST(high_drop_falls_and_lands) {
  fwtest::TestWorld world;
  fw::Vec3 feet{0, 1.0, 0};
  fw::Vertical v;
  fw::move_vertical(world, feet, v, 0.1, settings());
  CHECK(v.falling);
  CHECK_NEAR(feet.y, 1.0 - fw::kGravity * 0.01, 1e-9);
  run_vertical(world, feet, v, 1.0);
  CHECK_NEAR(feet.y, 0.0, 1e-12);
  CHECK(!v.falling);
  CHECK_NEAR(v.speed, 0.0, 1e-12);
}

TEST(no_ground_keeps_height_and_stops_a_fall) {
  fwtest::TestWorld world;
  world.ground = [](double, double) { return std::optional<double>(); };
  fw::Vec3 feet{0, 5.0, 0};
  fw::Vertical v{true, -3.0};
  fw::move_vertical(world, feet, v, 0.1, settings());
  CHECK_NEAR(feet.y, 5.0, 1e-12);
  CHECK(!v.falling);
  run_vertical(world, feet, v, 1.0);
  CHECK_NEAR(feet.y, 5.0, 1e-12);
}

TEST(steep_surface_is_not_ground) {
  fwtest::TestWorld world;
  world.ground = [](double, double) { return std::optional<double>(0.3); };
  world.ground_normal = {0.98, 0.2, 0};
  fw::Vec3 feet{0, 0, 0};
  fw::Vertical v;
  run_vertical(world, feet, v, 1.0);
  CHECK_NEAR(feet.y, 0.0, 1e-12);
}

TEST(zero_follow_time_snaps_at_once) {
  auto world = step_world(0.3);
  fw::MotionSettings s = settings();
  s.ground_follow_time = 0;
  fw::Vec3 feet{0, 0, -1.5};
  fw::Vertical v;
  fw::move_vertical(world, feet, v, 0.1, s);
  CHECK_NEAR(feet.y, 0.3, 1e-12);
}

TEST(start_height_finds_ground_above_the_estimate) {
  fwtest::TestWorld world;
  world.ground = [](double, double) { return std::optional<double>(0.8); };
  CHECK_NEAR(fw::start_height(world, {0, 0, 0}), 0.8, 1e-12);
  world.ground = [](double, double) { return std::optional<double>(); };
  CHECK_NEAR(fw::start_height(world, {0, 0.5, 0}), 0.5, 1e-12);
}

TEST(ground_ray_from_inside_a_shape_is_not_ground) {
  // The capsule bottom (the ray origin) is inside a body: PhysX reports the origin as ground, but that must not make it climb.
  fwtest::TestWorld world;
  world.ground = [](double, double) { return std::optional<double>(0.6); };
  world.hit_from_inside = true;
  fw::Vec3 feet{0, 0, 0};
  fw::Vertical v;
  run_vertical(world, feet, v, 1.0);
  CHECK_NEAR(feet.y, 0.0, 1e-12);
  CHECK_NEAR(fw::start_height(world, {0, -2.5, 0}), -2.5, 1e-12);
}

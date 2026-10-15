#include <algorithm>
#include <cmath>
#include <optional>
#include "check.h"
#include "core/motion.h"
#include "core/walker.h"
#include "core/world.h"
#include "test_world.h"

namespace {
constexpr double kPi = 3.14159265358979323846;

fw::WalkKeys keys(bool f, bool b, bool l, bool r, bool run = false) {
  fw::WalkKeys k;
  k.forward = f;
  k.back = b;
  k.left = l;
  k.right = r;
  k.run = run;
  return k;
}

void walk_for(fw::Walker& w, const fw::WalkKeys& k, double yaw, double seconds, const fw::Config& cfg) {
  fw::FlatWorld flat(w.feet().y);
  for (int i = 0; i < static_cast<int>(std::lround(seconds / 0.1)); ++i) w.step(k, yaw, 0.1, cfg, flat);
}
}  // namespace

TEST(walks_north_at_walk_speed) {
  const fw::Config cfg;
  fw::Walker w;
  w.reset({10, 5, 20});
  walk_for(w, keys(true, false, false, false), 0.0, 1.0, cfg);
  CHECK_NEAR(w.feet().x, 10.0, 1e-9);
  CHECK_NEAR(w.feet().z, 20.0 - 1.4, 1e-9);
  CHECK_NEAR(w.feet().y, 5.0, 1e-12);
}

TEST(walks_west_when_yaw_is_quarter_turn) {
  const fw::Config cfg;
  fw::Walker w;
  w.reset({0, 0, 0});
  walk_for(w, keys(true, false, false, false), kPi / 2, 1.0, cfg);
  CHECK_NEAR(w.feet().x, -1.4, 1e-9);
  CHECK_NEAR(w.feet().z, 0.0, 1e-9);
}

TEST(diagonal_is_not_faster) {
  const fw::Config cfg;
  fw::Walker w;
  w.reset({0, 0, 0});
  walk_for(w, keys(true, false, false, true), 0.0, 1.0, cfg);
  CHECK_NEAR(fw::horizontal_distance(w.feet(), {0, 0, 0}), 1.4, 1e-9);
  CHECK(w.feet().x > 0.0);
  CHECK(w.feet().z < 0.0);
}

TEST(run_uses_run_speed) {
  const fw::Config cfg;
  fw::Walker w;
  w.reset({0, 0, 0});
  walk_for(w, keys(true, false, false, false, true), 0.0, 1.0, cfg);
  CHECK_NEAR(w.feet().z, -4.0, 1e-9);
}

TEST(opposite_keys_cancel) {
  const fw::Config cfg;
  fw::Walker w;
  w.reset({1, 2, 3});
  walk_for(w, keys(true, true, true, true), 0.7, 1.0, cfg);
  CHECK_NEAR(w.feet().x, 1.0, 1e-12);
  CHECK_NEAR(w.feet().z, 3.0, 1e-12);
}

TEST(long_frame_moves_at_most_max_step) {
  const fw::Config cfg;
  fw::Walker w;
  w.reset({0, 0, 0});
  fw::FlatWorld flat(0.0);
  w.step(keys(true, false, false, false), 0.0, 5.0, cfg, flat);
  CHECK_NEAR(w.feet().z, -1.4 * fw::kMaxStepSeconds, 1e-9);
}

TEST(eye_height_without_bob_when_standing) {
  const fw::Config cfg;
  fw::Walker w;
  w.reset({0, 10, 0});
  CHECK_NEAR(w.eye(cfg).y, 11.7, 1e-12);
  walk_for(w, keys(true, false, false, false), 0.0, 1.0, cfg);
  walk_for(w, keys(false, false, false, false), 0.0, 0.5, cfg);
  CHECK_NEAR(w.eye(cfg).y, 11.7, 1e-12);
}

TEST(head_bobs_while_walking_within_amplitude) {
  const fw::Config cfg;
  fw::Walker w;
  w.reset({0, 0, 0});
  fw::FlatWorld flat(0.0);
  double largest = 0;
  for (int i = 0; i < 40; ++i) {
    w.step(keys(true, false, false, false), 0.0, 0.05, cfg, flat);
    largest = std::max(largest, std::fabs(w.eye(cfg).y - cfg.eye_height));
  }
  CHECK(largest > 0.01);
  CHECK(largest <= cfg.head_bob_amplitude + 1e-12);
}

TEST(head_bob_can_be_switched_off) {
  fw::Config cfg;
  cfg.head_bob = false;
  fw::Walker w;
  w.reset({0, 0, 0});
  fw::FlatWorld flat(0.0);
  for (int i = 0; i < 40; ++i) {
    w.step(keys(true, false, false, false), 0.0, 0.05, cfg, flat);
    CHECK_NEAR(w.eye(cfg).y, cfg.eye_height, 1e-12);
  }
}

TEST(running_bobs_slower_per_metre_than_walking) {
  // When running the stride is longer, so the head rises at most about three times per second, it does not jitter.
  const fw::Config cfg;
  fw::Walker w;
  w.reset({0, 0, 0});
  fw::FlatWorld flat(0.0);
  int sign_changes = 0;
  double previous = 0;
  for (int i = 0; i < 100; ++i) {
    w.step(keys(true, false, false, false, true), 0.0, 0.01, cfg, flat);
    const double bob = w.eye(cfg).y - cfg.eye_height;
    if ((bob > 0) != (previous > 0) && i > 0) ++sign_changes;
    previous = bob;
  }
  CHECK(sign_changes <= 6);
}

TEST(wall_stops_the_walker_and_the_head_bob) {
  const fw::Config cfg;
  fwtest::TestWorld world;
  world.walls.push_back({{0, 0, -1}, {0, 0, 1}});
  fw::Walker w;
  w.reset({0, 0, 0});
  for (int i = 0; i < 30; ++i) w.step(keys(true, false, false, false), 0.0, 0.1, cfg, world);
  CHECK_NEAR(w.feet().z, -1.0 + cfg.collision_radius + fw::kSweepSkin, 1e-9);
  CHECK_NEAR(w.eye(cfg).y, cfg.eye_height, 1e-12);
}

TEST(walker_climbs_a_step) {
  const fw::Config cfg;
  fwtest::TestWorld world;
  world.ground = [](double, double z) { return std::optional<double>(z < -1 ? 0.3 : 0.0); };
  fw::Walker w;
  w.reset({0, 0, 0});
  for (int i = 0; i < 20; ++i) w.step(keys(true, false, false, false), 0.0, 0.1, cfg, world);
  CHECK(w.feet().z < -2.0);
  CHECK_NEAR(w.feet().y, 0.3, fw::kGroundSnapTolerance);
}

TEST(settle_moves_the_feet_to_the_real_ground) {
  fwtest::TestWorld world;
  world.ground = [](double, double) { return std::optional<double>(1.2); };
  fw::Walker w;
  w.reset({5, 0.5, 5});
  w.settle(world);
  CHECK_NEAR(w.feet().y, 1.2, 1e-12);
  CHECK_NEAR(w.feet().x, 5.0, 1e-12);
}

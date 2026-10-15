#include "core/walker.h"
#include <algorithm>
#include <cmath>

namespace fw {
namespace {

constexpr double kTwoPi = 6.28318530717958647692;
// Length of a step; the head rises and falls once per step. Running makes steps longer, so the bob does not get frantic.
constexpr double kWalkStepLength = 0.75;
constexpr double kRunStepLength = 1.6;
// Time for the head bob to start and stop, so the view does not jump when stopping.
constexpr double kBobFadeSeconds = 0.25;

}  // namespace

void Walker::reset(Vec3 feet) {
  feet_ = feet;
  vertical_ = {};
  bob_phase_ = 0;
  bob_blend_ = 0;
}

void Walker::settle(World& world) {
  feet_.y = start_height(world, feet_);
  vertical_ = {};
}

void Walker::step(const WalkKeys& keys, double yaw, double dt, const Config& cfg, World& world) {
  dt = std::clamp(dt, 0.0, kMaxStepSeconds);
  const double ahead = (keys.forward ? 1.0 : 0.0) - (keys.back ? 1.0 : 0.0);
  const double side = (keys.right ? 1.0 : 0.0) - (keys.left ? 1.0 : 0.0);
  const Vec3 direction = forward_from_yaw(yaw) * ahead + right_from_yaw(yaw) * side;
  const double length = std::hypot(direction.x, direction.z);
  const bool wants_to_move = length > 0;
  const MotionSettings motion = motion_settings(cfg);

  Vec3 wish;
  if (wants_to_move) {
    const double speed = keys.run ? cfg.run_speed : cfg.walk_speed;
    wish = direction * (speed * dt / length);
  }
  const Vec3 before = feet_;
  feet_ = move_horizontal(world, feet_, wish, dt, motion);
  move_vertical(world, feet_, vertical_, dt, motion);

  // The head bob follows the distance actually covered, so it stops at a wall.
  const double moved = horizontal_distance(before, feet_);
  const bool moving = wants_to_move && moved > kMinMove;
  if (moving) {
    const double step_length = keys.run ? kRunStepLength : kWalkStepLength;
    bob_phase_ = std::fmod(bob_phase_ + kTwoPi * moved / step_length, kTwoPi);
  }

  const double target = moving ? 1.0 : 0.0;
  const double change = dt / kBobFadeSeconds;
  bob_blend_ = bob_blend_ < target ? std::min(target, bob_blend_ + change) : std::max(target, bob_blend_ - change);
}

Vec3 Walker::eye(const Config& cfg) const {
  const double bob = cfg.head_bob ? cfg.head_bob_amplitude * std::sin(bob_phase_) * bob_blend_ : 0.0;
  return {feet_.x, feet_.y + cfg.eye_height + bob, feet_.z};
}

}  // namespace fw

#include "core/motion.h"
#include <algorithm>
#include <cmath>
#include <optional>

namespace fw {
namespace {

double horizontal_length(Vec3 v) { return std::hypot(v.x, v.z); }

// Removes the normal component from the direction of travel; returns the new unit direction and the length ratio (0 if none is left).
double project_out(Vec3& dir, Vec3 normal) {
  const double into = dir.x * normal.x + dir.z * normal.z;
  dir = {dir.x - normal.x * into, 0, dir.z - normal.z * into};
  const double length = horizontal_length(dir);
  if (length < kMinMove) return 0;
  dir = dir * (1 / length);
  return length;
}

std::optional<double> ground_height(World& world, Vec3 from, double length) {
  // For a ray starting inside a body PhysX reports the start point as the hit; that is not ground, or the walker would climb onto the body.
  const auto hit = world.ground_below(from, length);
  if (!hit || hit->distance < kMinGroundDistance || std::fabs(hit->normal.y) < kMinGroundNormalY) return std::nullopt;
  return hit->height;
}

}  // namespace

MotionSettings motion_settings(const Config& cfg) {
  MotionSettings s;
  s.radius = cfg.collision_radius;
  s.step_height = cfg.step_height;
  s.step_down_max = cfg.step_down_max;
  s.ground_follow_time = cfg.ground_follow_time;
  s.eye_height = cfg.eye_height;
  return s;
}

BodyCapsule body_capsule(Vec3 feet, const MotionSettings& s) {
  const double bottom = feet.y + s.step_height;
  const double top = std::max(feet.y + s.eye_height + kCapsuleTopAboveEye, bottom);
  BodyCapsule b;
  b.capsule.radius = s.radius;
  b.capsule.half_height = std::max(kMinHalfHeight, (top - bottom) / 2 - s.radius);
  b.center = {feet.x, (top + bottom) / 2, feet.z};
  return b;
}

Vec3 move_horizontal(World& world, Vec3 feet, Vec3 wish, double dt, const MotionSettings& s) {
  double remaining = horizontal_length(wish);
  if (remaining < kMinMove) return feet;
  Vec3 dir{wish.x / remaining, 0, wish.z / remaining};
  Vec3 position = feet;
  double pushed = 0;  // distance pushed out of overlaps in this frame
  for (int i = 0; i < kMaxSlideSteps && remaining >= kMinMove; ++i) {
    const BodyCapsule body = body_capsule(position, s);
    const auto hit = world.sweep_capsule(body.capsule, body.center, dir, remaining + kSweepSkin);
    if (!hit) return position + dir * remaining;
    Vec3 normal{hit->normal.x, 0, hit->normal.z};
    const double normal_length = horizontal_length(normal);
    if (hit->initial_overlap) {
      // Overlap with the ground (vertical normal) is not an obstacle.
      if (normal_length < kMinPushNormal) return position + dir * remaining;
      normal = normal * (1 / normal_length);
      const double push = std::clamp(kDepenetrationSpeed * dt - pushed, 0.0, hit->depth + kDepenetrationExtra);
      pushed += push;
      position = position + normal * push;
      if (dir.x * normal.x + dir.z * normal.z < 0) {
        const double ratio = project_out(dir, normal);
        if (ratio == 0) break;
        remaining *= ratio;
      }
      continue;
    }
    const double advance = std::clamp(hit->distance - kSweepSkin, 0.0, remaining);
    position = position + dir * advance;
    remaining -= advance;
    if (normal_length < kMinMove) break;
    const double ratio = project_out(dir, normal * (1 / normal_length));
    if (ratio == 0) break;
    remaining *= ratio;
  }
  return position;
}

void move_vertical(World& world, Vec3& feet, Vertical& v, double dt, const MotionSettings& s) {
  const auto ground = ground_height(world, {feet.x, feet.y + s.step_height, feet.z}, kGroundRayLength);
  if (!ground) {
    v = {};
    return;
  }
  if (!v.falling) {
    const double above = feet.y - *ground;
    if (above <= s.step_down_max) {
      if (std::fabs(above) > kGroundSnapTolerance) {
        const double follow = s.ground_follow_time > 0 ? 1 - std::exp(-dt / s.ground_follow_time) : 1.0;
        feet.y -= above * follow;
      }
      return;
    }
    v = {true, 0.0};
  }
  v.speed -= kGravity * dt;
  feet.y += v.speed * dt;
  if (feet.y <= *ground) {
    feet.y = *ground;
    v = {};
  }
}

double start_height(World& world, Vec3 feet) {
  return ground_height(world, {feet.x, feet.y + kStartSearchUp, feet.z}, kStartSearchLength).value_or(feet.y);
}

}  // namespace fw

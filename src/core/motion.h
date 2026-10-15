#pragma once
#include "core/config.h"
#include "core/world.h"

namespace fw {

inline constexpr double kGravity = 9.81;
inline constexpr double kSweepSkin = 0.02;            // m: gap left between the body and the wall
inline constexpr int kMaxSlideSteps = 3;              // sweeps per frame (sliding along walls)
inline constexpr double kMinMove = 1e-5;              // m: a smaller displacement is not movement
inline constexpr double kDepenetrationSpeed = 1.5;    // m/s: maximum speed of pushing the body out of an overlap
inline constexpr double kDepenetrationExtra = 0.005;  // m: on top of the penetration depth
inline constexpr double kMinPushNormal = 0.3;         // no push along a normal with a smaller horizontal component (ground)
inline constexpr double kCapsuleTopAboveEye = 0.08;   // m: capsule top above the eye
inline constexpr double kMinHalfHeight = 0.02;
inline constexpr double kGroundRayLength = 10.0;      // m: length of the ground ray
inline constexpr double kMinGroundNormalY = 0.3;      // a steeper surface is not ground
inline constexpr double kMinGroundDistance = 1e-4;    // m: a closer hit means the ray started inside a body
inline constexpr double kGroundSnapTolerance = 0.02;  // m: the feet do not move for a smaller difference
inline constexpr double kStartSearchUp = 2.0;         // m: when leaving, searches for the real ground from this far above the estimate
inline constexpr double kStartSearchLength = 4.0;

struct MotionSettings {
  double radius = 0.40;
  double step_height = 0.45;
  double step_down_max = 0.6;
  double ground_follow_time = 0.12;
  double eye_height = 1.7;
};

MotionSettings motion_settings(const Config& cfg);

// The body capsule and its center: bottom at feet + step_height, top at eye + kCapsuleTopAboveEye.
struct BodyCapsule {
  Capsule capsule;
  Vec3 center;
};
BodyCapsule body_capsule(Vec3 feet, const MotionSettings& s);

// The wanted horizontal displacement (wish.y is ignored) in at most kMaxSlideSteps sweeps, sliding along walls;
// pushes out of overlaps first. Returns the new feet point, height unchanged.
Vec3 move_horizontal(World& world, Vec3 feet, Vec3 wish, double dt, const MotionSettings& s);

// State of the vertical motion.
struct Vertical {
  bool falling = false;
  double speed = 0;  // m/s, positive upwards
};

// Feet height against the ground: the ground ray starts down from feet + step_height. Smoothed following on the ground;
// if the feet are more than step_down_max above the ground, a fall with gravity, landing on the ground.
// With no hit (or too steep a surface) the height stays and the fall ends.
void move_vertical(World& world, Vec3& feet, Vertical& v, double dt, const MotionSettings& s);

// When leaving, the real ground height around the estimated feet point; the estimated height if there is no hit.
double start_height(World& world, Vec3 feet);

}  // namespace fw

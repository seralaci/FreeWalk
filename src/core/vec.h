#pragma once
#include <cmath>

namespace fw {

// World coordinates by the SCS convention: x east, y up, z south (north is -Z).
struct Vec3 {
  double x = 0;
  double y = 0;
  double z = 0;
};

inline Vec3 operator+(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline Vec3 operator-(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline Vec3 operator*(Vec3 a, double s) { return {a.x * s, a.y * s, a.z * s}; }

inline double horizontal_distance(Vec3 a, Vec3 b) {
  return std::hypot(a.x - b.x, a.z - b.z);
}

// Rotates a vector given in truck coordinates (x right, y up, z back) into the world.
// heading is in radians, counter-clockwise seen from above, 0 = north.
inline Vec3 rotate_heading(Vec3 local, double heading) {
  const double c = std::cos(heading);
  const double s = std::sin(heading);
  return {local.x * c + local.z * s, local.y, -local.x * s + local.z * c};
}

// The horizontal forward direction for a yaw (same convention as heading).
inline Vec3 forward_from_yaw(double yaw) { return {-std::sin(yaw), 0.0, -std::cos(yaw)}; }
inline Vec3 right_from_yaw(double yaw) { return {std::cos(yaw), 0.0, -std::sin(yaw)}; }

}  // namespace fw

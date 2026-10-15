#pragma once
#include <optional>
#include "core/vec.h"

namespace fw {

// A ground point from the hit of a downward ray.
struct GroundHit {
  double height = 0;
  Vec3 normal{0, 1, 0};
  double distance = 0;  // from the ray start to the hit; 0 if the ray started inside a body
};

// Vertical capsule: hemisphere centers half a height above and below the center, with the radius around them.
struct Capsule {
  double radius = 0;
  double half_height = 0;
};

// First hit of a capsule sweep.
struct SweepHit {
  double distance = 0;           // distance travelled from the start to the hit
  Vec3 normal;                   // surface normal
  bool initial_overlap = false;  // the capsule already overlaps something at the start
  double depth = 0;              // penetration depth when overlapping
};

// The environment the walker queries: the game's physics, a simple model in tests, flat ground on error.
class World {
 public:
  virtual ~World() = default;
  // The first surface down from the from point, at most max_distance away.
  virtual std::optional<GroundHit> ground_below(Vec3 from, double max_distance) = 0;
  // Sweeps the capsule centered at center along the horizontal unit direction, for distance.
  virtual std::optional<SweepHit> sweep_capsule(const Capsule& capsule, Vec3 center, Vec3 direction, double distance) = 0;
};

// Flat ground at a given height with no obstacles: for walking without physics and the fallback on error.
class FlatWorld final : public World {
 public:
  explicit FlatWorld(double height) : height_(height) {}
  std::optional<GroundHit> ground_below(Vec3 from, double max_distance) override {
    if (from.y < height_ || from.y - height_ > max_distance) return std::nullopt;
    return GroundHit{height_, {0, 1, 0}, from.y - height_};
  }
  std::optional<SweepHit> sweep_capsule(const Capsule&, Vec3, Vec3, double) override { return std::nullopt; }

 private:
  double height_;
};

}  // namespace fw

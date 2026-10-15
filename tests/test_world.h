#pragma once
#include <functional>
#include <optional>
#include <vector>
#include "core/world.h"

namespace fwtest {

// A vertical wall: one of its points, its horizontal unit normal (toward the free side) and its top.
struct Wall {
  fw::Vec3 point;
  fw::Vec3 normal;
  double top = 1e9;
};

// Test world: a ground function and infinite vertical walls (half-spaces).
class TestWorld final : public fw::World {
 public:
  std::function<std::optional<double>(double x, double z)> ground = [](double, double) { return std::optional<double>(0.0); };
  fw::Vec3 ground_normal{0, 1, 0};
  // PhysX behavior: if the ray starts inside a body (the ground is above the origin), it reports a hit at the origin, at distance 0.
  bool hit_from_inside = false;
  std::vector<Wall> walls;
  int sweeps = 0;

  std::optional<fw::GroundHit> ground_below(fw::Vec3 from, double max_distance) override {
    const auto h = ground(from.x, from.z);
    if (h && *h > from.y && hit_from_inside) return fw::GroundHit{from.y, {0, 1, 0}, 0.0};
    if (!h || *h > from.y || from.y - *h > max_distance) return std::nullopt;
    return fw::GroundHit{*h, ground_normal, from.y - *h};
  }

  std::optional<fw::SweepHit> sweep_capsule(const fw::Capsule& c, fw::Vec3 center, fw::Vec3 dir, double distance) override {
    ++sweeps;
    std::optional<fw::SweepHit> best;
    for (const auto& w : walls) {
      if (center.y - c.half_height - c.radius >= w.top) continue;  // the capsule bottom is above the wall top
      const double gap = (center.x - w.point.x) * w.normal.x + (center.z - w.point.z) * w.normal.z - c.radius;
      if (gap < 0) {
        if (!best || !best->initial_overlap || -gap > best->depth) best = fw::SweepHit{0, w.normal, true, -gap};
        continue;
      }
      const double approach = -(dir.x * w.normal.x + dir.z * w.normal.z);
      if (approach <= 1e-12) continue;
      const double d = gap / approach;
      if (d > distance) continue;
      if (!best || (!best->initial_overlap && d < best->distance)) best = fw::SweepHit{d, w.normal, false, 0};
    }
    return best;
  }
};

}  // namespace fwtest

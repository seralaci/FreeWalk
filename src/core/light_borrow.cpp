#include "core/light_borrow.h"

namespace fw {

std::optional<std::uint64_t> choose_light(Vec3 walker, const std::vector<LightCandidate>& lights, double min_distance, double max_distance) {
  std::optional<std::uint64_t> best;
  double best_distance = 0;
  bool best_inside = false;
  for (const auto& l : lights) {
    if (!l.active) continue;
    const double d = horizontal_distance(walker, l.position);
    if (d < min_distance) continue;
    const bool inside = d <= max_distance;
    // One inside the limit always beats one outside it; within the same group, the nearer one.
    if (!best || (inside && !best_inside) || (inside == best_inside && d < best_distance)) {
      best = l.id;
      best_distance = d;
      best_inside = inside;
    }
  }
  return best;
}

}  // namespace fw

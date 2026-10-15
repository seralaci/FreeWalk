#include "core/door_choice.h"
#include <algorithm>

namespace fw {

std::optional<double> door_free_space(World* world, const DoorPath& path) {
  if (!world) return std::nullopt;
  constexpr double kReach = 1.5;
  const Capsule capsule{0.1, 0.6};
  const Vec3 out = forward_from_yaw(path.outward_yaw);
  for (const double offset : {0.0, 0.3, 0.6}) {
    const Vec3 center{path.sill.x + out.x * offset, path.ground.y + 1.1, path.sill.z + out.z * offset};
    const auto hit = world->sweep_capsule(capsule, center, out, kReach - offset);
    if (!hit) return kReach;
    if (!hit->initial_overlap) return std::min(kReach, offset + hit->distance);
  }
  return 0.0;
}

DoorSide choose_door(std::optional<double> driver_free, std::optional<double> passenger_free, double min_free) {
  if (!driver_free || *driver_free >= min_free) return DoorSide::Driver;
  if (!passenger_free || *passenger_free >= min_free) return DoorSide::Passenger;
  return DoorSide::None;
}

}  // namespace fw

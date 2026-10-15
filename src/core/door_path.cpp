#include "core/door_path.h"
#include <cmath>

namespace fw {
namespace {
constexpr double kPi = 3.14159265358979323846;
}  // namespace

DoorPath door_path(const TruckState& truck, const Config& cfg, int side, double ground_height, const DoorSettings& s) {
  DoorPath p;
  p.side = side;
  const Vec3 head = truck.head_in_truck;
  p.sill = truck.position + rotate_heading({side * (std::fabs(head.x) + s.out), 0.0, head.z}, truck.heading);
  p.sill.y = truck.position.y + head.y - s.sill_below_head;
  p.ground = exit_point_on(truck, cfg, side);
  p.ground.y = ground_height;
  const double drop = p.sill.y - p.ground.y;
  p.steps = drop > 0.05 ? static_cast<int>(std::ceil(drop / kMaxStepHeight - 1e-9)) : 0;
  // Heading grows turning left: facing out to the right is heading - 90 degrees, to the left heading + 90 degrees.
  p.outward_yaw = truck.heading - side * kPi / 2;
  p.door_yaw = truck.heading + side * kPi / 2;
  return p;
}

int nearer_door(const TruckState& truck, const Config& cfg, Vec3 feet, const DoorSettings& s) {
  const DoorPath right = door_path(truck, cfg, +1, feet.y, s);
  const DoorPath left = door_path(truck, cfg, -1, feet.y, s);
  return horizontal_distance(feet, right.sill) < horizontal_distance(feet, left.sill) ? +1 : -1;
}

bool near_door(const DoorPath& path, Vec3 feet, double max_distance) {
  if (horizontal_distance(feet, path.sill) > max_distance) return false;
  const Vec3 out = forward_from_yaw(path.outward_yaw);
  return (feet.x - path.sill.x) * out.x + (feet.z - path.sill.z) * out.z >= 0;
}

}  // namespace fw

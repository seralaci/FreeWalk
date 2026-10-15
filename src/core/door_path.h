#pragma once
#include "core/config.h"
#include "core/walk_session.h"

namespace fw {

inline constexpr double kMaxStepHeight = 0.45;  // m: the maximum height of one step

struct DoorSettings {
  double out = 0.7;              // m: the door sill point sideways from the head (outside the cab)
  double sill_below_head = 1.0;  // m: the door sill below head height
};

// Geometry of a door: the door sill point (top of the steps), the exit ground point, the step count, the outward and door-facing headings.
struct DoorPath {
  int side = 1;  // +1 right, -1 left (relative to the truck's direction of travel)
  Vec3 sill;
  Vec3 ground;
  int steps = 0;
  double outward_yaw = 0;
  double door_yaw = 0;
};

DoorPath door_path(const TruckState& truck, const Config& cfg, int side, double ground_height, const DoorSettings& s);

// When entering, the side of the door nearer the walker (+1 right, -1 left).
int nearer_door(const TruckState& truck, const Config& cfg, Vec3 feet, const DoorSettings& s);

// Whether the walker stands in front of the door (at most max_distance horizontally outward from the door sill):
// then it may climb the steps, but not through the cab.
bool near_door(const DoorPath& path, Vec3 feet, double max_distance);

}  // namespace fw

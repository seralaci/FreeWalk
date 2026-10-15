#pragma once
#include <optional>
#include "core/door_path.h"
#include "core/world.h"

namespace fw {

enum class DoorSide { Driver, Passenger, None };

// The exit door from the free space measured in front of both doors (m); nullopt = not measurable (no physics), counts as free.
// The free space in front of a door (m, at most 1.5): a thin capsule swept outward from below the sill. If the start already overlaps
// something (the truck's own cab can reach past the sill), it starts again 0.3 and 0.6 m farther out and adds that; against a wall
// flush with the truck every start overlaps, which is 0 m. nullopt without a world (no physics).
std::optional<double> door_free_space(World* world, const DoorPath& path);

DoorSide choose_door(std::optional<double> driver_free, std::optional<double> passenger_free, double min_free);

}  // namespace fw

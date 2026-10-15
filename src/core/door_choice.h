#pragma once
#include <optional>

namespace fw {

enum class DoorSide { Driver, Passenger, None };

// The exit door from the free space measured in front of both doors (m); nullopt = not measurable (no physics), counts as free.
DoorSide choose_door(std::optional<double> driver_free, std::optional<double> passenger_free, double min_free);

}  // namespace fw

#include "core/door_choice.h"

namespace fw {

DoorSide choose_door(std::optional<double> driver_free, std::optional<double> passenger_free, double min_free) {
  if (!driver_free || *driver_free >= min_free) return DoorSide::Driver;
  if (!passenger_free || *passenger_free >= min_free) return DoorSide::Passenger;
  return DoorSide::None;
}

}  // namespace fw

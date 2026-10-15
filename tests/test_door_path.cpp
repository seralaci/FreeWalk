#include <cmath>
#include <optional>
#include "check.h"
#include "core/door_choice.h"
#include "core/door_path.h"

namespace {
constexpr double kPi = 3.14159265358979323846;

fw::TruckState truck(double heading, double head_x = -0.7) {
  fw::TruckState t;
  t.valid = true;
  t.position = {100, 10, 200};
  t.heading = heading;
  t.head_known = true;
  t.head_in_truck = {head_x, 2.7, -1.9};
  return t;
}
}  // namespace

TEST(door_sill_is_outside_the_cab_below_the_head) {
  const fw::Config cfg;
  const auto p = fw::door_path(truck(0), cfg, -1, 10.0, fw::DoorSettings{});
  // Truck facing north, left-hand drive: the door sill is 0.7 m left (west) of the head and 1.0 m lower.
  CHECK_NEAR(p.sill.x, 100 - 0.7 - 0.7, 1e-9);
  CHECK_NEAR(p.sill.y, 10 + 2.7 - 1.0, 1e-9);
  CHECK_NEAR(p.sill.z, 200 - 1.9, 1e-9);
  CHECK_NEAR(p.ground.x, 100 - 0.7 - cfg.exit_side_distance, 1e-9);
  CHECK_NEAR(p.ground.y, 10.0, 1e-12);
  CHECK(p.steps == 4);  // 1.7 m / 0.45 -> 4 steps
}

TEST(passenger_door_mirrors_the_driver_door) {
  const fw::Config cfg;
  const auto p = fw::door_path(truck(0), cfg, +1, 10.0, fw::DoorSettings{});
  CHECK_NEAR(p.sill.x, 100 + 0.7 + 0.7, 1e-9);
  CHECK_NEAR(p.ground.x, 100 + 0.7 + cfg.exit_side_distance, 1e-9);
}

TEST(door_follows_the_truck_heading) {
  const fw::Config cfg;
  const auto p = fw::door_path(truck(kPi / 2), cfg, -1, 10.0, fw::DoorSettings{});  // truck facing west: the left side is south
  CHECK(p.sill.z > 200 + 1.0);
  CHECK_NEAR(p.sill.x, 100 - 1.9, 1e-9);
}

TEST(door_yaws_face_out_and_in) {
  const fw::Config cfg;
  const auto p = fw::door_path(truck(0), cfg, -1, 10.0, fw::DoorSettings{});
  const fw::Vec3 out = fw::forward_from_yaw(p.outward_yaw);
  const fw::Vec3 in = fw::forward_from_yaw(p.door_yaw);
  CHECK_NEAR(out.x, -1.0, 1e-9);  // out of the left door: west
  CHECK_NEAR(in.x, 1.0, 1e-9);
}

TEST(step_count_follows_the_height) {
  fw::Config cfg;
  fw::DoorSettings s;
  s.sill_below_head = 2.7 - 1.2;  // the door sill is 1.2 m above the ground
  CHECK(fw::door_path(truck(0), cfg, -1, 10.0, s).steps == 3);
  s.sill_below_head = 2.7 - 0.3;
  CHECK(fw::door_path(truck(0), cfg, -1, 10.0, s).steps == 1);
}

TEST(no_steps_when_the_ground_is_not_below_the_sill) {
  const fw::Config cfg;
  CHECK(fw::door_path(truck(0), cfg, -1, 13.0, fw::DoorSettings{}).steps == 0);
}

TEST(door_choice_prefers_the_driver_side) {
  CHECK(fw::choose_door(1.2, 1.5, 0.8) == fw::DoorSide::Driver);
  CHECK(fw::choose_door(0.3, 1.5, 0.8) == fw::DoorSide::Passenger);
  CHECK(fw::choose_door(0.3, 0.5, 0.8) == fw::DoorSide::None);
  CHECK(fw::choose_door(std::nullopt, std::nullopt, 0.8) == fw::DoorSide::Driver);
  CHECK(fw::choose_door(0.1, std::nullopt, 0.8) == fw::DoorSide::Passenger);
}

TEST(exit_on_the_passenger_side_mirrors_the_yaw) {
  const fw::Config cfg;
  const auto t = truck(0);
  const double left = fw::exit_yaw_on(t, cfg, -1);
  const double right = fw::exit_yaw_on(t, cfg, +1);
  CHECK_NEAR(left, -right, 1e-9);
  CHECK_NEAR(fw::exit_yaw(t, cfg), left, 1e-12);
}

TEST(entering_uses_the_door_on_the_walkers_side) {
  const fw::Config cfg;
  const auto t = truck(0);  // facing north, left-hand drive: the driver door is to the west
  CHECK(fw::nearer_door(t, cfg, {103.0, 10, 198}, fw::DoorSettings{}) == +1);  // on the east side of the cab: the passenger door
  CHECK(fw::nearer_door(t, cfg, {97.0, 10, 198}, fw::DoorSettings{}) == -1);
}

TEST(near_door_only_in_front_of_the_door) {
  const fw::Config cfg;
  const auto p = fw::door_path(truck(0), cfg, -1, 10.0, fw::DoorSettings{});
  CHECK(fw::near_door(p, {98.0, 10, 198}, 3.0));
  CHECK(!fw::near_door(p, {95.0, 10, 198}, 3.0));   // too far
  CHECK(!fw::near_door(p, {100.5, 10, 198}, 3.0));  // behind the door sill, toward the cab
}

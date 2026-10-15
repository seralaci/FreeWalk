#include "check.h"
#include "core/walk_session.h"

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr int kFree = 3;
constexpr int kInterior = 0;

fw::TruckState parked() {
  fw::TruckState t;
  t.valid = true;
  t.position = {1000.0, 50.0, -2000.0};
  t.heading = 0.0;
  t.speed = 0.0;
  t.parking_brake = true;
  t.head_known = true;
  t.head_in_truck = {-0.6, 2.4, -3.5};
  return t;
}

fw::SessionInput input(const fw::TruckState& t, bool toggle, int active) {
  fw::SessionInput in;
  in.truck = t;
  in.toggle_pressed = toggle;
  in.active_camera = active;
  in.free_camera = kFree;
  in.dt = 0.1;
  in.walker_feet = t.position;
  return in;
}

// Starts a walk: an exit request, then the free camera becoming active.
fw::WalkSession walking_session(const fw::Config& cfg) {
  fw::WalkSession s;
  s.update(input(parked(), true, kInterior), cfg);
  s.update(input(parked(), false, kFree), cfg);
  return s;
}
}  // namespace

TEST(nothing_happens_without_toggle) {
  const fw::Config cfg;
  fw::WalkSession s;
  const auto out = s.update(input(parked(), false, kInterior), cfg);
  CHECK(out.command == fw::Command::None);
  CHECK(out.reason.empty());
  CHECK(s.phase() == fw::Phase::Driving);
}

TEST(refuses_when_preconditions_fail) {
  const fw::Config cfg;
  struct Case { fw::TruckState truck; int free; const char* reason; };
  fw::TruckState moving = parked();
  moving.speed = 1.0;
  fw::TruckState no_brake = parked();
  no_brake.parking_brake = false;
  fw::TruckState no_config = parked();
  no_config.head_known = false;
  fw::TruckState no_data = parked();
  no_data.valid = false;
  const Case cases[] = {{moving, kFree, "the truck is moving"},
                        {no_brake, kFree, "the parking brake is off"},
                        {no_config, kFree, "the truck configuration has not arrived yet"},
                        {no_data, kFree, "no truck position from the telemetry yet"},
                        {parked(), -1, "the game's free camera was not found"}};
  for (const auto& c : cases) {
    fw::WalkSession s;
    auto in = input(c.truck, true, kInterior);
    in.free_camera = c.free;
    const auto out = s.update(in, cfg);
    CHECK(out.command == fw::Command::None);
    CHECK(out.reason == c.reason);
    CHECK(s.phase() == fw::Phase::Driving);
  }
}

TEST(toggle_requests_free_camera_then_starts_walk_at_driver_door) {
  const fw::Config cfg;
  fw::WalkSession s;
  const auto first = s.update(input(parked(), true, kInterior), cfg);
  CHECK(first.command == fw::Command::RequestFreeCamera);
  CHECK(s.phase() == fw::Phase::Switching);

  const auto waiting = s.update(input(parked(), false, kInterior), cfg);
  CHECK(waiting.command == fw::Command::None);

  const auto started = s.update(input(parked(), false, kFree), cfg);
  CHECK(started.command == fw::Command::StartWalk);
  CHECK(s.phase() == fw::Phase::Walking);
  // Left-hand drive truck (head x is negative): 1.5 m beside the head point, to the left, on the ground.
  CHECK_NEAR(started.start_feet.x, 1000.0 - 2.1, 1e-9);
  CHECK_NEAR(started.start_feet.y, 50.0, 1e-12);
  CHECK_NEAR(started.start_feet.z, -2000.0 - 3.5, 1e-9);
  // Faces the door (to the right), turned 22 degrees toward the truck's nose.
  CHECK_NEAR(started.start_yaw, -kPi / 2 + 22.0 * kPi / 180.0, 1e-12);
}

TEST(exit_point_follows_truck_heading) {
  fw::Config cfg;
  cfg.exit_side_distance = 1.0;
  fw::TruckState t = parked();
  t.heading = kPi;  // facing south, the driver's left hand is toward the east
  const fw::Vec3 p = fw::exit_point(t, cfg);
  CHECK_NEAR(p.x, 1001.6, 1e-9);
  CHECK_NEAR(p.z, -1996.5, 1e-9);
}

TEST(right_hand_drive_exits_on_the_right) {
  fw::Config cfg;
  cfg.exit_side_distance = 1.0;
  fw::TruckState t = parked();
  t.head_in_truck.x = 0.6;
  const fw::Vec3 p = fw::exit_point(t, cfg);
  CHECK_NEAR(p.x, 1001.6, 1e-9);
}

TEST(ground_offset_moves_exit_point_vertically) {
  fw::Config cfg;
  cfg.ground_offset = -0.4;
  CHECK_NEAR(fw::exit_point(parked(), cfg).y, 49.6, 1e-12);
}

TEST(switch_timeout_returns) {
  const fw::Config cfg;
  fw::WalkSession s;
  s.update(input(parked(), true, kInterior), cfg);
  // With 0.1 s frames the 3 s timeout expires on the 30th or 31st frame (due to floating-point addition).
  int frames = 0;
  fw::SessionOutput out;
  while (out.command == fw::Command::None && frames < 40) {
    out = s.update(input(parked(), false, kInterior), cfg);
    ++frames;
  }
  CHECK(out.command == fw::Command::Return);
  CHECK(frames >= 30 && frames <= 31);
  CHECK(s.phase() == fw::Phase::Driving);
}

TEST(walking_continues_normally) {
  const fw::Config cfg;
  auto s = walking_session(cfg);
  const auto out = s.update(input(parked(), false, kFree), cfg);
  CHECK(out.command == fw::Command::Walk);
  CHECK(s.phase() == fw::Phase::Walking);
}

TEST(toggle_while_walking_returns) {
  const fw::Config cfg;
  auto s = walking_session(cfg);
  const auto out = s.update(input(parked(), true, kFree), cfg);
  CHECK(out.command == fw::Command::Return);
  CHECK(s.phase() == fw::Phase::Driving);
}

TEST(other_camera_returns_without_restoring_camera) {
  const fw::Config cfg;
  auto s = walking_session(cfg);
  const auto out = s.update(input(parked(), false, 5), cfg);
  CHECK(out.command == fw::Command::ReturnKeepCamera);
  CHECK(s.phase() == fw::Phase::Driving);
}

TEST(truck_jump_returns_but_small_move_does_not) {
  const fw::Config cfg;
  auto s = walking_session(cfg);
  fw::TruckState t = parked();
  t.position.x += 4.0;
  CHECK(s.update(input(t, false, kFree), cfg).command == fw::Command::Walk);
  t.position.x += 6.0;
  CHECK(s.update(input(t, false, kFree), cfg).command == fw::Command::Return);
}

TEST(walking_too_far_returns) {
  const fw::Config cfg;
  auto s = walking_session(cfg);
  auto in = input(parked(), false, kFree);
  in.walker_feet = {1000.0 + 151.0, 50.0, -2000.0};
  CHECK(s.update(in, cfg).command == fw::Command::Return);
}

TEST(force_return_only_when_not_driving) {
  const fw::Config cfg;
  fw::WalkSession idle;
  CHECK(idle.force_return("unload").command == fw::Command::None);
  auto s = walking_session(cfg);
  const auto out = s.force_return("unload");
  CHECK(out.command == fw::Command::Return);
  CHECK(out.reason == "unload");
  CHECK(s.phase() == fw::Phase::Driving);
}

TEST(right_hand_drive_faces_the_door_turned_to_the_front) {
  const fw::Config cfg;
  fw::TruckState t = parked();
  t.head_in_truck.x = 0.6;
  t.heading = 1.0;
  // Stands on the right, faces left (toward the door), and turns 22 degrees right, toward the truck's nose.
  CHECK_NEAR(fw::exit_yaw(t, cfg), 1.0 + kPi / 2 - 22.0 * kPi / 180.0, 1e-12);
}

TEST(exit_can_face_along_the_truck) {
  fw::Config cfg;
  cfg.exit_face_door = false;
  fw::TruckState t = parked();
  t.heading = 0.7;
  CHECK_NEAR(fw::exit_yaw(t, cfg), 0.7, 1e-12);
}

TEST(exit_face_angle_is_configurable) {
  fw::Config cfg;
  cfg.exit_face_angle = 0.0;
  CHECK_NEAR(fw::exit_yaw(parked(), cfg), -kPi / 2, 1e-12);
}

// Code review (point 1): the game does not move a free camera switched on this way, so it stays where the previous walk was.
// With an already verified layout the old, distant position must not turn off the walk.
TEST(far_free_camera_is_accepted_once_the_layout_is_confirmed) {
  const fw::CameraPosition distant = fw::to_camera({1000.0 + 50000.0, 50.0, -2000.0});
  CHECK(!fw::free_camera_position_plausible(distant, parked().position, false));
  CHECK(fw::free_camera_position_plausible(distant, parked().position, true));
}

TEST(untouched_or_near_free_camera_is_plausible) {
  CHECK(fw::free_camera_position_plausible(fw::CameraPosition{}, parked().position, false));
  CHECK(fw::free_camera_position_plausible(fw::to_camera({1010.0, 50.0, -2000.0}), parked().position, false));
}

TEST(unreadable_free_camera_is_never_plausible) {
  CHECK(!fw::free_camera_position_plausible(std::nullopt, parked().position, true));
}

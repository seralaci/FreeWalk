#include "core/walk_session.h"
#include <cmath>

namespace fw {

namespace {

constexpr double kPi = 3.14159265358979323846;

}  // namespace

int driver_side(const TruckState& truck) { return truck.head_in_truck.x < 0 ? -1 : 1; }

Vec3 exit_point(const TruckState& truck, const Config& cfg) { return exit_point_on(truck, cfg, driver_side(truck)); }

double exit_yaw(const TruckState& truck, const Config& cfg) { return exit_yaw_on(truck, cfg, driver_side(truck)); }

Vec3 exit_point_on(const TruckState& truck, const Config& cfg, int side) {
  // The other door mirrors the driver's side across the truck's long axis.
  const Vec3 local{side * (std::fabs(truck.head_in_truck.x) + cfg.exit_side_distance), 0.0, truck.head_in_truck.z};
  Vec3 point = truck.position + rotate_heading(local, truck.heading);
  point.y = truck.position.y + cfg.ground_offset;
  return point;
}

double exit_yaw_on(const TruckState& truck, const Config& cfg, int side) {
  if (!cfg.exit_face_door) return truck.heading;
  // Heading grows turning left. Standing on the left, the door is to the right (heading - 90 degrees), and turning left faces the front;
  // the other way round on the right.
  return truck.heading + side * (kPi / 2 - cfg.exit_face_angle * kPi / 180.0);
}

bool free_camera_position_plausible(const std::optional<CameraPosition>& current, const Vec3& truck, bool layout_confirmed) {
  if (!current) return false;
  if (layout_confirmed) return true;
  const bool untouched = current->chunk_x == 0 && current->chunk_z == 0 && current->x == 0.0f && current->y == 0.0f && current->z == 0.0f;
  return untouched || horizontal_distance(to_world(*current), truck) <= kLayoutCheckDistance;
}

SessionOutput WalkSession::update(const SessionInput& in, const Config& cfg) {
  SessionOutput out;
  switch (phase_) {
    case Phase::Driving:
      if (!in.toggle_pressed) return out;
      if (!in.truck.valid) out.reason = "no truck position from the telemetry yet";
      else if (std::fabs(in.truck.speed) >= kMaxParkedSpeed) out.reason = "the truck is moving";
      else if (!in.truck.parking_brake) out.reason = "the parking brake is off";
      else if (!in.truck.head_known) out.reason = "the truck configuration has not arrived yet";
      else if (in.free_camera < 0) out.reason = "the game's free camera was not found";
      else {
        out.command = Command::RequestFreeCamera;
        out.reason = "leaving the truck";
        phase_ = Phase::Switching;
        switch_elapsed_ = 0;
      }
      return out;

    case Phase::Switching:
      if (in.active_camera == in.free_camera) {
        out.command = Command::StartWalk;
        out.reason = "the free camera is active";
        out.start_feet = exit_point(in.truck, cfg);
        out.start_yaw = exit_yaw(in.truck, cfg);
        last_truck_ = in.truck.position;
        phase_ = Phase::Walking;
        return out;
      }
      switch_elapsed_ += in.dt;
      if (switch_elapsed_ > kSwitchTimeout) {
        out.command = Command::Return;
        out.reason = "the game did not switch to the free camera within 3 s";
        phase_ = Phase::Driving;
      }
      return out;

    case Phase::Walking: {
      const double truck_moved = horizontal_distance(in.truck.position, last_truck_);
      last_truck_ = in.truck.position;
      if (in.toggle_pressed) {
        out.command = Command::Return;
        out.reason = "the toggle key was pressed";
      } else if (in.active_camera != in.free_camera) {
        out.command = Command::ReturnKeepCamera;
        out.reason = "another camera was selected";
      } else if (truck_moved > kTruckJump) {
        out.command = Command::Return;
        out.reason = "the truck was moved";
      } else if (horizontal_distance(in.walker_feet, in.truck.position) > cfg.max_distance) {
        out.command = Command::Return;
        out.reason = "too far from the truck";
      } else {
        out.command = Command::Walk;
        return out;
      }
      phase_ = Phase::Driving;
      return out;
    }
  }
  return out;
}

SessionOutput WalkSession::force_return(const std::string& reason) {
  SessionOutput out;
  if (phase_ != Phase::Driving) {
    out.command = Command::Return;
    out.reason = reason;
    phase_ = Phase::Driving;
  }
  return out;
}

}  // namespace fw

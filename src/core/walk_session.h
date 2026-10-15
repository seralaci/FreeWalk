#pragma once
#include <optional>
#include <string>
#include "core/config.h"
#include "core/coords.h"
#include "core/vec.h"

namespace fw {

struct TruckState {
  bool valid = false;        // position data has arrived from telemetry
  Vec3 position;             // world coordinates, metres
  double heading = 0;        // radians, counter-clockwise seen from above, 0 = north
  double speed = 0;          // m/s, negative in reverse
  bool parking_brake = false;
  bool head_known = false;   // the truck configuration has arrived
  Vec3 head_in_truck;        // the driver's head in truck coordinates (x right, y up, z back)
};

enum class Phase { Driving, Switching, Walking };

enum class Command {
  None,               // nothing to do; if reason is not empty, leaving was refused and reason says why
  RequestFreeCamera,  // leaving starts: request the free camera
  StartWalk,          // the free camera is active: the walker stands at start_feet
  Walk,               // one frame while walking
  Return,             // return to the truck, restoring the previous camera
  ReturnKeepCamera,   // return to the truck, but the camera chosen by the player stays
};

struct SessionInput {
  TruckState truck;
  bool toggle_pressed = false;  // the moment the walk key is pressed
  int active_camera = -1;       // index of the game's active camera, -1 if unreadable
  int free_camera = -1;         // index of the free camera, -1 if not found
  double dt = 0;                // time since the previous frame, seconds
  Vec3 walker_feet;
};

struct SessionOutput {
  Command command = Command::None;
  std::string reason;
  Vec3 start_feet;
  double start_yaw = 0;
};

inline constexpr double kMaxParkedSpeed = 0.5 / 3.6;  // m/s
inline constexpr double kSwitchTimeout = 3.0;         // s
inline constexpr double kLayoutCheckDistance = 1000.0;  // m: with an unverified layout a free camera farther than this is suspicious
inline constexpr double kTruckJump = 5.0;             // m, a bigger jump between two frames means a relocation

// The driver's side of the truck: -1 left-hand drive (head x negative), +1 right-hand drive.
int driver_side(const TruckState& truck);

// Exit position and heading at the door on the given side (side: -1 left, +1 right).
Vec3 exit_point_on(const TruckState& truck, const Config& cfg, int side);
double exit_yaw_on(const TruckState& truck, const Config& cfg, int side);

// Exit position: on the driver's side, outside the cab, on the ground.
Vec3 exit_point(const TruckState& truck, const Config& cfg);

// Heading when leaving (radians): towards the door, turned towards the truck's front, or along the truck.
double exit_yaw(const TruckState& truck, const Config& cfg);

// Layout check of the free camera position. The game does not move the free camera we switch on: first it is at the untouched
// (0, 0, 0) position, later at the previous walk's spot. Until a successful walk has verified the layout, only these and a position
// near the truck are accepted (any other value suggests the field is not the position); after that only readability matters.
bool free_camera_position_plausible(const std::optional<CameraPosition>& current, const Vec3& truck, bool layout_confirmed);

// Decides when a walk starts and ends; carrying it out is the caller's job.
class WalkSession {
 public:
  SessionOutput update(const SessionInput& in, const Config& cfg);

  // On game shutdown or after an execution error: if a walk or a switch is in progress, requests a return to the truck.
  SessionOutput force_return(const std::string& reason);

  Phase phase() const { return phase_; }

 private:
  Phase phase_ = Phase::Driving;
  double switch_elapsed_ = 0;
  Vec3 last_truck_;
};

}  // namespace fw

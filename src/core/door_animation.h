#pragma once
#include "core/config.h"
#include "core/door_path.h"
#include "core/vec.h"

namespace fw {

struct DoorTiming {
  double fade = 0.25;           // s, fade to black and back
  double steps = 0.8;           // s, on the steps
  double switch_timeout = 3.0;  // s, how long we wait for the camera switch
  double eye_height = 1.7;      // m, eye above the feet
};

// The in-cab parts, done with the game's interior (seat) camera: turning toward the door before getting out, and climbing into the
// seat after getting back in (from close by).
enum class SeatMotion { None, Turn, Climb };

inline constexpr double kSeatClimbTime = 1.0;  // s, climbing into the seat
inline constexpr double kSeatTurnTime = 0.5;   // s, turning toward the door before getting out
inline constexpr double kSeatTurnLead = 0.2;   // s, the fade to black starts this long after the turn

// One frame: whether to write the free camera and where (eye, yaw); the darkening (0..1); whether to switch now (fully dark);
// the seat camera's move and its progress (0..1); whether it has ended and, if so, whether it failed (the switch did not happen).
struct DoorFrame {
  SeatMotion seat = SeatMotion::None;
  double seat_t = 0;
  bool camera = false;
  Vec3 eye;
  double yaw = 0;
  double blackout = 0;
  bool switch_now = false;
  bool done = false;
  bool failed = false;
};

class DoorAnimation {
 public:
  void start_exit(const DoorPath& path, double final_yaw, const DoorTiming& t);
  void start_enter(const DoorPath& path, Vec3 walker_feet, double walker_yaw, bool near, const DoorTiming& t);
  DoorFrame step(double dt);
  // The caller signals that the camera switch happened.
  void switched() { switched_ = true; }
  void cancel() { stage_ = Stage::Idle; }
  bool active() const { return stage_ != Stage::Idle; }
  bool entering() const { return entering_; }

 private:
  enum class Stage { Idle, TurnToDoor, FadeOut, WaitSwitch, FadeIn, SeatClimb, StepsDown };
  void enter_stage(Stage s);
  Vec3 foot() const;                    // foot of the steps: in front of the door sill, on the ground (the climb goes straight up from it)
  double climb_height(double u) const;  // foot height on the steps, u = 0 on the ground, 1 at the door sill

  Stage stage_ = Stage::Idle;
  bool entering_ = false;
  bool near_ = false;
  bool switched_ = false;
  bool failed_ = false;
  bool asked_ = false;
  double time_ = 0;  // time spent in the current phase
  DoorPath path_;
  DoorTiming timing_;
  double final_yaw_ = 0;
  Vec3 start_eye_;
  double start_yaw_ = 0;
};

// The seat camera's pose: added to its seat offset (lateral: + right, up), and its look angles (absolute, radians; the yaw grows to
// the left like the truck heading). side: -1 left door, +1 right door.
struct SeatPose {
  double lateral = 0;
  double up = 0;
  double yaw = 0;
  double pitch = 0;
};

// Climbing into the seat, t = 0..1: it starts 0.32 m toward the door and step_drop lower (at most 0.45 m), looking 0.8 rad into the
// cab (away from the door) and 0.18 rad down, slides in and rises in one step, and turns to the seat's own look angles.
SeatPose seat_climb(double t, int side, double seat_yaw, double seat_pitch, double step_drop);
// Turning toward the door before getting out, t = 0..1: 1.15 rad toward the door, pitch to level.
SeatPose seat_turn(double t, int side, double seat_yaw, double seat_pitch);

// From the settings.
DoorSettings door_settings(const Config& c);
DoorTiming door_timing(const Config& c);

}  // namespace fw

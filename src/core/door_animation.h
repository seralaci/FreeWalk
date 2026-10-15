#pragma once
#include "core/config.h"
#include "core/door_path.h"
#include "core/vec.h"

namespace fw {

struct DoorTiming {
  double fade = 0.25;           // s, fade to black and back
  double steps = 1.3;           // s, on the steps
  double approach = 0.4;        // s, moving to the foot of the steps when entering
  double switch_timeout = 3.0;  // s, how long we wait for the camera switch
  double eye_height = 1.7;      // m, eye above the feet
};

// One frame: whether to write the free camera and where (eye, yaw); the darkening (0..1); whether to switch now (fully dark);
// whether it has ended and, if so, whether it failed (the switch did not happen).
struct DoorFrame {
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
  enum class Stage { Idle, Approach, StepsUp, FadeOut, WaitSwitch, FadeIn, StepsDown };
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

// From the settings.
DoorSettings door_settings(const Config& c);
DoorTiming door_timing(const Config& c);

}  // namespace fw

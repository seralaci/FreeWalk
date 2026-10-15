#pragma once
#include <cstdint>
#include "core/config.h"
#include "core/flicker.h"
#include "core/vec.h"

namespace fw {

inline constexpr int kBeamLevels = 5;
inline constexpr int kBeamMiddle = 2;
inline constexpr double kBeamFactors[kBeamLevels] = {0.5, 0.75, 1.0, 1.5, 2.0};
inline constexpr double kMinBeamAngle = 5;
inline constexpr double kMaxBeamAngle = 80;

struct LightSettings {
  double angle = 28;
  double inner_angle = 12;
  double range = 40;
  double brightness = 0.3;
  double r = 1.0;
  double g = 0.96;
  double b = 0.88;
  double fade_time = 0.18;
  double hand_right = 0.5;
  double hand_down = 0.21;
  double flicker_chance = 0;
};
LightSettings light_settings(const Config& cfg);

// Input for one frame: the on/off key edge, the beam steps (keys and mouse wheel), the eye position and the view direction
// (yaw by the telemetry convention, pitch positive upwards), the elapsed time.
struct FlashlightInput {
  bool toggle = false;
  int wider = 0;
  int narrower = 0;
  Vec3 eye;
  double yaw = 0;
  double pitch = 0;
  double dt = 0;
};

// The light to set on one of the game's lights; lit = false if no light is needed.
struct LightOutput {
  bool lit = false;
  Vec3 position;
  Vec3 direction;
  double outer_angle = 0;
  double inner_angle = 0;
  double range = 0;
  double r = 0;
  double g = 0;
  double b = 0;
};

class Flashlight {
 public:
  explicit Flashlight(std::uint32_t seed = 0x5EED) : flicker_(seed) {}
  LightOutput step(const FlashlightInput& in, const LightSettings& s);
  // Instant switch-off without fading (on return to the truck, on pause, on a new world).
  void switch_off();
  bool on() const { return on_; }
  int level() const { return level_; }

 private:
  bool on_ = false;
  double fade_ = 0;
  int level_ = kBeamMiddle;
  Flicker flicker_;
};

}  // namespace fw

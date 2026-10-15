#include "core/flashlight.h"
#include <algorithm>
#include <cmath>

namespace fw {

LightSettings light_settings(const Config& c) {
  LightSettings s;
  s.angle = c.light_angle;
  s.inner_angle = c.light_inner_angle;
  s.range = c.light_range;
  s.brightness = c.light_brightness;
  s.r = c.light_r;
  s.g = c.light_g;
  s.b = c.light_b;
  s.fade_time = c.light_fade_time;
  s.hand_right = c.light_hand_right;
  s.hand_down = c.light_hand_down;
  s.flicker_chance = c.light_flicker_chance;
  return s;
}

void Flashlight::switch_off() {
  on_ = false;
  fade_ = 0;
}

LightOutput Flashlight::step(const FlashlightInput& in, const LightSettings& s) {
  if (in.toggle) {
    on_ = !on_;
    if (on_ && fade_ == 0) level_ = kBeamMiddle;
  }
  if (on_) level_ = std::clamp(level_ + in.wider - in.narrower, 0, kBeamLevels - 1);
  const double target = on_ ? 1.0 : 0.0;
  if (s.fade_time <= 0) {
    fade_ = target;
  } else {
    const double change = in.dt / s.fade_time;
    fade_ = fade_ < target ? std::min(target, fade_ + change) : std::max(target, fade_ - change);
  }
  LightOutput out;
  if (fade_ <= 0) return out;
  const double flicker = on_ ? flicker_.step(in.dt, s.flicker_chance) : 1.0;
  const double intensity = fade_ * flicker * s.brightness;

  // The view direction and the camera's "up" direction from yaw and pitch; the hand is offset along the right and "up" directions.
  const Vec3 flat = forward_from_yaw(in.yaw);
  const double cp = std::cos(in.pitch);
  const double sp = std::sin(in.pitch);
  const Vec3 forward{flat.x * cp, sp, flat.z * cp};
  const Vec3 up{-flat.x * sp, cp, -flat.z * sp};
  const Vec3 right = right_from_yaw(in.yaw);

  out.lit = true;
  out.position = in.eye + right * s.hand_right - up * s.hand_down;
  out.direction = forward;
  const double factor = kBeamFactors[level_];
  out.outer_angle = std::clamp(s.angle * factor, kMinBeamAngle, kMaxBeamAngle);
  out.inner_angle = std::clamp(s.inner_angle * factor, 0.0, out.outer_angle);
  out.range = s.range;
  out.r = s.r * intensity;
  out.g = s.g * intensity;
  out.b = s.b * intensity;
  return out;
}

}  // namespace fw

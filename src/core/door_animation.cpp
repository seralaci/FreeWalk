#include "core/door_animation.h"
#include <algorithm>
#include <cmath>

namespace fw {
namespace {

constexpr double kPi = 3.14159265358979323846;

constexpr double kStandOff = 0.35;     // m: on the steps, stays this far in front of the door sill, not pushed onto the door
constexpr double kStepBob = 0.04;      // m: bob per step (marks the steps)
constexpr double kStepOutStart = 0.65;  // going down, steps out to the ground point and turns after this fraction of the time

double smooth(double t) { return t * t * (3 - 2 * t); }

Vec3 lerp(Vec3 a, Vec3 b, double t) { return a + (b - a) * t; }

// The shortest way between two headings.
double lerp_angle(double a, double b, double t) {
  double d = std::fmod(b - a, 2 * kPi);
  if (d > kPi) d -= 2 * kPi;
  if (d < -kPi) d += 2 * kPi;
  return a + d * t;
}

}  // namespace

DoorSettings door_settings(const Config& c) { return {c.door_out, c.door_sill_below_head}; }
DoorTiming door_timing(const Config& c) {
  DoorTiming t;
  t.fade = c.door_fade_time;
  t.steps = c.door_step_time;
  t.eye_height = c.eye_height;
  return t;
}

void DoorAnimation::start_exit(const DoorPath& path, double final_yaw, const DoorTiming& t) {
  path_ = path;
  timing_ = t;
  final_yaw_ = final_yaw;
  entering_ = false;
  near_ = true;
  enter_stage(Stage::FadeOut);
}

void DoorAnimation::start_enter(const DoorPath& path, Vec3 walker_feet, double walker_yaw, bool near, const DoorTiming& t) {
  path_ = path;
  timing_ = t;
  entering_ = true;
  near_ = near;
  start_eye_ = walker_feet + Vec3{0, t.eye_height, 0};
  start_yaw_ = walker_yaw;
  enter_stage(near ? Stage::Approach : Stage::FadeOut);
}

void DoorAnimation::enter_stage(Stage s) {
  stage_ = s;
  time_ = 0;
  if (s == Stage::FadeOut || s == Stage::StepsUp) {
    switched_ = false;
    failed_ = false;
    asked_ = false;
  }
}

Vec3 DoorAnimation::foot() const {
  Vec3 p = path_.sill + forward_from_yaw(path_.outward_yaw) * kStandOff;
  p.y = path_.ground.y;
  return p;
}

double DoorAnimation::climb_height(double u) const {
  const double rise = path_.sill.y - path_.ground.y;
  double h = path_.ground.y + rise * u;
  if (path_.steps > 0) {
    // Continuous climb with a slight dip per step; the wave is under a quarter of a step, so the motion stays one-directional.
    const double n = path_.steps;
    const double bob = std::min(kStepBob, std::fabs(rise) / n / 4);
    const double within = u * n - std::floor(u * n);
    h -= bob * std::sin(kPi * within);
  }
  return h;
}

DoorFrame DoorAnimation::step(double dt) {
  DoorFrame f;
  if (stage_ == Stage::Idle) return f;
  time_ += dt;
  const double fade = timing_.fade;
  const auto progress = [&](double length) { return length <= 0 ? 1.0 : std::clamp(time_ / length, 0.0, 1.0); };
  const Vec3 base = foot();
  // Never steps onto the top step, up or down: standing on the door sill the eye would be above the cab roof, and one step fewer
  // is enough going down too.
  const double enter_top = path_.steps >= 1 ? (path_.steps - 1.0) / path_.steps : 1.0;
  const Vec3 enter_eye{base.x, climb_height(enter_top) + timing_.eye_height, base.z};
  const Vec3 ground_eye = path_.ground + Vec3{0, timing_.eye_height, 0};
  // The view held in the dark: the top of the steps when leaving or entering from close, the walker's spot when entering from afar.
  const Vec3 dark_eye = !entering_ || near_ ? enter_eye : start_eye_;
  const double dark_yaw = !entering_ ? path_.outward_yaw : near_ ? path_.door_yaw : start_yaw_;

  switch (stage_) {
    case Stage::Approach: {
      const double p = smooth(progress(timing_.approach));
      f.camera = true;
      f.eye = lerp(start_eye_, base + Vec3{0, timing_.eye_height, 0}, p);
      f.yaw = lerp_angle(start_yaw_, path_.door_yaw, p);
      if (progress(timing_.approach) >= 1) enter_stage(Stage::StepsUp);
      return f;
    }
    case Stage::StepsUp: {
      const double length = timing_.steps;
      const double p = progress(length);
      f.camera = true;
      f.eye = {base.x, climb_height(p * enter_top) + timing_.eye_height, base.z};
      f.yaw = path_.door_yaw;
      // Already darkening on the last steps: it is dark by the top, so the door is never seen up close.
      const double dim = std::min(fade, length);
      f.blackout = dim <= 0 ? 0.0 : std::clamp((time_ - (length - dim)) / dim, 0.0, 1.0);
      if (p >= 1) {
        enter_stage(Stage::WaitSwitch);
        asked_ = false;
        return step(0.0);
      }
      return f;
    }
    case Stage::FadeOut: {
      const double p = progress(fade);
      f.blackout = p;
      f.camera = entering_;
      f.eye = dark_eye;
      f.yaw = dark_yaw;
      if (p >= 1) {
        enter_stage(Stage::WaitSwitch);
        return step(0.0);
      }
      return f;
    }
    case Stage::WaitSwitch: {
      f.blackout = 1;
      f.eye = dark_eye;
      f.yaw = dark_yaw;
      f.camera = !entering_ || !switched_;
      if (!asked_) {
        asked_ = true;
        f.switch_now = true;
        f.camera = true;
        return f;
      }
      if (switched_) {
        enter_stage(entering_ ? Stage::FadeIn : Stage::StepsDown);
        return step(dt);
      }
      if (time_ > timing_.switch_timeout) {
        failed_ = true;
        enter_stage(Stage::FadeIn);
        return step(0.0);
      }
      return f;
    }
    case Stage::FadeIn: {
      const double p = progress(fade);
      f.blackout = 1 - p;
      f.eye = dark_eye;
      f.yaw = dark_yaw;
      if (p >= 1) {
        stage_ = Stage::Idle;
        f.done = true;
        f.failed = failed_;
        f.blackout = 0;
      }
      return f;
    }
    case Stage::StepsDown: {
      // Starts down while fading in; on the last steps it steps out to the ground point and turns to the final heading.
      const double p = progress(timing_.steps);
      f.camera = true;
      f.blackout = fade <= 0 ? 0.0 : 1 - std::clamp(time_ / fade, 0.0, 1.0);
      const double out = smooth(std::clamp((p - kStepOutStart) / (1 - kStepOutStart), 0.0, 1.0));
      const Vec3 flat = lerp(base, path_.ground, out);
      f.eye = {flat.x, climb_height((1 - p) * enter_top) + timing_.eye_height, flat.z};
      f.yaw = lerp_angle(path_.outward_yaw, final_yaw_, out);
      if (p >= 1) {
        stage_ = Stage::Idle;
        f.eye = ground_eye;
        f.yaw = final_yaw_;
        f.blackout = 0;
        f.done = true;
      }
      return f;
    }
    case Stage::Idle: break;
  }
  return f;
}

}  // namespace fw

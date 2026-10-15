#include <cmath>
#include "check.h"
#include "core/flashlight.h"

namespace {
constexpr double kPi = 3.14159265358979323846;

fw::FlashlightInput frame(bool toggle = false, double dt = 0.1) {
  fw::FlashlightInput in;
  in.toggle = toggle;
  in.eye = {0, 1.7, 0};
  in.dt = dt;
  return in;
}
}  // namespace

TEST(flashlight_fades_in_and_out) {
  fw::LightSettings s;
  s.fade_time = 0.2;
  fw::Flashlight f;
  auto out = f.step(frame(true, 0.1), s);
  CHECK(f.on() && out.lit);
  CHECK_NEAR(out.r, s.r * s.brightness * 0.5, 1e-9);
  out = f.step(frame(false, 0.1), s);
  CHECK_NEAR(out.r, s.r * s.brightness, 1e-9);
  out = f.step(frame(true, 0.1), s);  // off: fading, but still lit
  CHECK(!f.on() && out.lit);
  CHECK_NEAR(out.r, s.r * s.brightness * 0.5, 1e-9);
  out = f.step(frame(false, 0.1), s);
  CHECK(!out.lit);
}

TEST(zero_fade_time_switches_at_once) {
  fw::LightSettings s;
  s.fade_time = 0;
  fw::Flashlight f;
  CHECK_NEAR(f.step(frame(true), s).r, s.r * s.brightness, 1e-12);
  CHECK(!f.step(frame(true), s).lit);
}

TEST(switch_off_is_immediate) {
  fw::LightSettings s;
  fw::Flashlight f;
  f.step(frame(true), s);
  f.switch_off();
  CHECK(!f.on());
  CHECK(!f.step(frame(false), s).lit);
}

TEST(beam_levels_step_and_stop_at_the_ends) {
  fw::LightSettings s;
  s.angle = 28;
  s.inner_angle = 12;
  fw::Flashlight f;
  auto in = frame(true);
  auto out = f.step(in, s);
  CHECK(f.level() == fw::kBeamMiddle);
  CHECK_NEAR(out.outer_angle, 28, 1e-9);
  in = frame();
  in.wider = 5;
  out = f.step(in, s);
  CHECK(f.level() == fw::kBeamLevels - 1);
  CHECK_NEAR(out.outer_angle, 56, 1e-9);
  CHECK_NEAR(out.inner_angle, 24, 1e-9);
  in = frame();
  in.narrower = 9;
  out = f.step(in, s);
  CHECK(f.level() == 0);
  CHECK_NEAR(out.outer_angle, 14, 1e-9);
  // Switched on again: the middle level.
  f.switch_off();
  f.step(frame(true), s);
  CHECK(f.level() == fw::kBeamMiddle);
}

TEST(beam_angle_is_clamped) {
  fw::LightSettings s;
  s.angle = 70;
  s.inner_angle = 70;
  fw::Flashlight f;
  f.step(frame(true), s);
  auto in = frame();
  in.wider = 2;
  const auto out = f.step(in, s);
  CHECK_NEAR(out.outer_angle, 80, 1e-9);
  CHECK(out.inner_angle <= out.outer_angle);
}

TEST(light_sits_in_the_hand_and_points_where_you_look) {
  fw::LightSettings s;
  s.hand_right = 0.5;
  s.hand_down = 0.21;
  fw::Flashlight f;
  auto in = frame(true);
  auto out = f.step(in, s);
  CHECK_NEAR(out.position.x, 0.5, 1e-9);
  CHECK_NEAR(out.position.y, 1.7 - 0.21, 1e-9);
  CHECK_NEAR(out.position.z, 0.0, 1e-9);
  CHECK_NEAR(out.direction.z, -1.0, 1e-9);
  in = frame();
  in.yaw = kPi / 2;  // looking west
  out = f.step(in, s);
  CHECK_NEAR(out.direction.x, -1.0, 1e-9);
  CHECK_NEAR(out.position.z, -0.5, 1e-9);
  in = frame();
  in.pitch = kPi / 2;  // looking up
  out = f.step(in, s);
  CHECK_NEAR(out.direction.y, 1.0, 1e-9);
  CHECK_NEAR(out.position.y, 1.7, 1e-9);
  CHECK_NEAR(out.position.z, -0.21, 1e-9);
}

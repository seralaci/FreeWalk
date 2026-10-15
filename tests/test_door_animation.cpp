#include <algorithm>
#include <cmath>
#include "check.h"
#include "core/door_animation.h"

namespace {
fw::DoorPath path() {
  fw::DoorPath p;
  p.side = -1;
  p.sill = {0, 1.6, 0};
  p.ground = {-0.8, 0.0, 0};
  p.steps = 4;
  p.outward_yaw = 1.5707963267948966;
  p.door_yaw = -1.5707963267948966;
  return p;
}
fw::DoorTiming timing() { return fw::DoorTiming{}; }

// Steps the animation until it requests the switch (or for at most n frames).
fw::DoorFrame run_until_switch(fw::DoorAnimation& a, int n = 1000) {
  fw::DoorFrame f;
  for (int i = 0; i < n && !(f = a.step(0.01)).switch_now; ++i) {
  }
  return f;
}
}  // namespace

TEST(exit_turns_to_the_door_then_fades_out_and_asks_for_the_switch) {
  fw::DoorAnimation a;
  a.start_exit(path(), 0.3, timing());
  auto f = a.step(0.1);
  CHECK(f.seat == fw::SeatMotion::Turn);  // first the seat camera turns toward the door
  CHECK_NEAR(f.blackout, 0.0, 1e-12);     // still light
  CHECK(!f.camera);
  f = a.step(0.2);  // 0.3 s: darkening has started
  CHECK(f.blackout > 0.0 && f.blackout < 1.0);
  CHECK(!f.switch_now);
  f = run_until_switch(a);
  CHECK(f.switch_now);
  CHECK_NEAR(f.blackout, 1.0, 1e-12);
  CHECK(f.camera);
  CHECK_NEAR(f.eye.y, 1.2 + 1.7, 1e-9);  // one step below the door sill (it does not step onto the top step)
  CHECK_NEAR(f.yaw, path().outward_yaw, 1e-12);
  // It stays dark until the switch has happened.
  f = a.step(0.5);
  CHECK(!f.switch_now);
  CHECK_NEAR(f.blackout, 1.0, 1e-12);
}

TEST(exit_steps_down_after_the_switch_and_ends_on_the_ground) {
  fw::DoorAnimation a;
  a.start_exit(path(), 0.3, timing());
  run_until_switch(a);
  a.switched();
  auto f = a.step(0.125);
  CHECK_NEAR(f.blackout, 0.5, 1e-9);
  CHECK(f.seat == fw::SeatMotion::None);
  double last_height = 10;
  int flats = 0;
  bool done = false;
  for (int i = 0; i < 400 && !done; ++i) {
    f = a.step(0.01);
    CHECK(f.eye.y <= last_height + 1e-9);
    if (std::fabs(f.eye.y - last_height) < 1e-9) ++flats;
    last_height = f.eye.y;
    done = f.done;
  }
  CHECK(done);
  CHECK(!f.failed);
  CHECK(flats < 20);  // continuous: does not stop at each step
  CHECK_NEAR(f.eye.x, -0.8, 1e-9);
  CHECK_NEAR(f.eye.y, 0.0 + 1.7, 1e-9);
  CHECK_NEAR(f.yaw, 0.3, 1e-9);
  CHECK_NEAR(f.blackout, 0.0, 1e-12);
  CHECK(!a.active());
}

TEST(exit_fails_after_the_switch_timeout) {
  fw::DoorAnimation a;
  a.start_exit(path(), 0.3, timing());
  run_until_switch(a);
  fw::DoorFrame f;
  for (int i = 0; i < 400 && !f.done; ++i) f = a.step(0.01);
  CHECK(f.done);
  CHECK(f.failed);
  CHECK_NEAR(f.blackout, 0.0, 1e-12);
}

TEST(exit_steps_stay_out_of_the_door) {
  // Going down it stays at least 0.3 m short of the door sill, it is not pushed onto the door.
  fw::DoorAnimation a;
  a.start_exit(path(), 0.3, timing());
  auto f = run_until_switch(a);
  CHECK(fw::horizontal_distance(f.eye, path().sill) >= 0.3 - 1e-9);
  a.switched();
  for (int i = 0; i < 1000 && !f.done; ++i) {
    f = a.step(0.01);
    CHECK(fw::horizontal_distance(f.eye, path().sill) >= 0.3 - 1e-9);
  }
}

TEST(exit_fades_in_while_starting_down) {
  fw::DoorAnimation a;
  a.start_exit(path(), 0.3, timing());
  const auto top = run_until_switch(a);
  a.switched();
  const auto f = a.step(0.125);
  CHECK_NEAR(f.blackout, 0.5, 1e-9);
  CHECK(f.eye.y < top.eye.y);  // while brightening it is already going down
}

TEST(near_enter_blinks_at_the_door_then_climbs_into_the_seat) {
  fw::DoorAnimation a;
  a.start_enter(path(), {-1.5, 0.0, 0.0}, 2.0, true, timing());
  auto f = a.step(0.01);
  CHECK(f.blackout > 0.0);  // the blink starts at once, where the walker stands
  f = run_until_switch(a);
  CHECK(f.switch_now);
  CHECK_NEAR(f.eye.x, -1.5, 1e-9);
  a.switched();
  f = a.step(0.01);
  CHECK(f.seat == fw::SeatMotion::Climb);  // back in the cab: the seat camera climbs in
  CHECK(!f.camera);
  CHECK(f.blackout > 0.0 && f.blackout < 1.0);  // brightening at the start of the climb
  double t = f.seat_t;
  for (int i = 0; i < 300 && !f.done; ++i) {
    f = a.step(0.01);
    if (f.done) break;
    CHECK(f.seat == fw::SeatMotion::Climb);
    CHECK(f.seat_t >= t);
    t = f.seat_t;
  }
  CHECK(f.done && !f.failed);
  CHECK_NEAR(f.blackout, 0.0, 1e-12);
  CHECK(!a.active());
}

TEST(near_enter_climb_takes_its_time) {
  fw::DoorAnimation a;
  a.start_enter(path(), {-1.5, 0.0, 0.0}, 2.0, true, timing());
  run_until_switch(a);
  a.switched();
  int frames = 0;
  fw::DoorFrame f;
  for (; frames < 1000 && !f.done; ++frames) f = a.step(0.01);
  CHECK(frames >= static_cast<int>(fw::kSeatClimbTime / 0.01) - 1);
}

TEST(far_enter_only_blinks) {
  fw::DoorAnimation a;
  a.start_enter(path(), {-30, 0, 0}, 0.0, false, timing());
  auto f = a.step(0.01);
  CHECK(f.blackout > 0.0);
  f = run_until_switch(a);
  CHECK(f.switch_now);
  CHECK_NEAR(f.eye.x, -30, 1e-9);  // from far away it stays at the walker's position
  a.switched();
  for (int i = 0; i < 100 && !f.done; ++i) {
    f = a.step(0.01);
    CHECK(f.seat == fw::SeatMotion::None);
  }
  CHECK(f.done && !f.failed);
}

TEST(zero_fade_time_switches_after_the_turn) {
  fw::DoorTiming t;
  t.fade = 0;
  fw::DoorAnimation a;
  a.start_exit(path(), 0.3, t);
  auto f = a.step(0.01);
  CHECK(!f.switch_now);
  f = run_until_switch(a);
  CHECK(f.switch_now);
  CHECK_NEAR(f.blackout, 1.0, 1e-12);
}

TEST(zero_dt_pauses_the_animation) {
  fw::DoorAnimation a;
  a.start_exit(path(), 0.3, timing());
  const auto f1 = a.step(0.3);
  const auto f2 = a.step(0.0);
  CHECK_NEAR(f1.blackout, f2.blackout, 1e-12);
  CHECK_NEAR(f1.seat_t, f2.seat_t, 1e-12);
}

TEST(cancel_clears_the_blackout) {
  fw::DoorAnimation a;
  a.start_exit(path(), 0.3, timing());
  a.step(0.3);
  a.cancel();
  CHECK(!a.active());
  CHECK_NEAR(a.step(0.01).blackout, 0.0, 1e-12);
}

// The seat camera's moves (added to its seat offset; the look angles absolute).

TEST(seat_climb_starts_beside_the_door_low_and_looking_into_the_cab) {
  const fw::SeatPose p = fw::seat_climb(0.0, -1, 0.1, 0.05, 0.45);
  CHECK_NEAR(p.lateral, -0.32, 1e-9);  // toward the left door
  CHECK_NEAR(p.up, -0.45, 1e-9);
  CHECK_NEAR(p.yaw, 0.1 - 0.8, 1e-9);  // left door: looking right, into the cab (the yaw grows to the left)
  CHECK_NEAR(p.pitch, -0.18, 1e-9);    // looking down
  const fw::SeatPose r = fw::seat_climb(0.0, +1, 0.0, 0.0, 0.45);
  CHECK_NEAR(r.lateral, 0.32, 1e-9);
  CHECK_NEAR(r.yaw, 0.8, 1e-9);
}

TEST(seat_climb_ends_exactly_on_the_seat) {
  const fw::SeatPose p = fw::seat_climb(1.0, -1, 0.1, 0.05, 0.45);
  CHECK_NEAR(p.lateral, 0.0, 1e-12);
  CHECK_NEAR(p.up, 0.0, 1e-12);
  CHECK_NEAR(p.yaw, 0.1, 1e-12);
  CHECK_NEAR(p.pitch, 0.05, 1e-12);
}

TEST(seat_climb_rises_in_one_step) {
  // One smooth step up, risen by t = 0.6: rising all the way, never above the seat (no second step, no overshoot).
  CHECK(fw::seat_climb(0.6, -1, 0, 0, 0.45).up > -1e-9);
  double last = -1;
  for (int i = 0; i <= 100; ++i) {
    const double up = fw::seat_climb(i / 100.0, -1, 0, 0, 0.45).up;
    CHECK(up >= last - 1e-12);
    CHECK(up <= 1e-12);
    last = up;
  }
  // Halfway through the rise it is about halfway up (a single step, not two).
  CHECK_NEAR(fw::seat_climb(0.3, -1, 0, 0, 0.45).up, -0.225, 0.02);
  CHECK_NEAR(fw::seat_climb(0.0, -1, 0, 0, 0.12).up, -0.12, 1e-9);
}

TEST(seat_turn_looks_toward_the_door) {
  const fw::SeatPose start = fw::seat_turn(0.0, -1, 0.2, 0.1);
  CHECK_NEAR(start.yaw, 0.2, 1e-12);
  CHECK_NEAR(start.pitch, 0.1, 1e-12);
  const fw::SeatPose end = fw::seat_turn(1.0, -1, 0.2, 0.1);
  CHECK_NEAR(end.yaw, 0.2 + 1.15, 1e-12);  // left door: turned left
  CHECK_NEAR(end.pitch, 0.0, 1e-12);
  CHECK_NEAR(fw::seat_turn(1.0, +1, 0.0, 0.0).yaw, -1.15, 1e-12);
}

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

TEST(exit_fades_out_then_asks_for_the_switch_in_darkness) {
  fw::DoorAnimation a;
  a.start_exit(path(), 0.3, timing());
  auto f = a.step(0.125);
  CHECK(!f.camera);
  CHECK_NEAR(f.blackout, 0.5, 1e-9);
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
  CHECK(flats < 20);  // continuous: does not stop at each step (it only stays still while brightening)
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

TEST(near_enter_walks_to_the_steps_climbs_and_switches_in_darkness) {
  fw::DoorAnimation a;
  a.start_enter(path(), {-2.5, 0.0, 1.0}, 2.0, true, timing());
  auto f = a.step(0.2);  // walks to the bottom of the steps
  CHECK(f.camera);
  CHECK_NEAR(f.blackout, 0.0, 1e-12);
  f = run_until_switch(a);
  CHECK(f.switch_now);
  CHECK_NEAR(f.blackout, 1.0, 1e-12);
  CHECK_NEAR(f.eye.y, 1.2 + 1.7, 1e-9);  // one step below the door sill
  CHECK_NEAR(f.yaw, path().door_yaw, 1e-9);
  a.switched();
  f = a.step(0.01);
  CHECK(!f.camera);
  for (int i = 0; i < 100 && !f.done; ++i) f = a.step(0.01);
  CHECK(f.done && !f.failed);
  CHECK_NEAR(f.blackout, 0.0, 1e-12);
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
  for (int i = 0; i < 100 && !f.done; ++i) f = a.step(0.01);
  CHECK(f.done && !f.failed);
}

TEST(zero_fade_time_is_immediate) {
  fw::DoorTiming t;
  t.fade = 0;
  fw::DoorAnimation a;
  a.start_exit(path(), 0.3, t);
  const auto f = a.step(0.01);
  CHECK(f.switch_now);
  CHECK_NEAR(f.blackout, 1.0, 1e-12);
}

TEST(zero_dt_pauses_the_animation) {
  fw::DoorAnimation a;
  a.start_exit(path(), 0.3, timing());
  const auto f1 = a.step(0.1);
  const auto f2 = a.step(0.0);
  CHECK_NEAR(f1.blackout, f2.blackout, 1e-12);
}

TEST(cancel_clears_the_blackout) {
  fw::DoorAnimation a;
  a.start_exit(path(), 0.3, timing());
  a.step(0.2);
  a.cancel();
  CHECK(!a.active());
  CHECK_NEAR(a.step(0.01).blackout, 0.0, 1e-12);
}

TEST(climb_stays_out_of_the_door) {
  // Both up and down it stays at least 0.3 m short of the door sill, it is not pushed onto the door.
  fw::DoorAnimation a;
  a.start_enter(path(), {-2.5, 0.0, 1.0}, 2.0, true, timing());
  fw::DoorFrame f;
  for (int i = 0; i < 1000 && !f.switch_now; ++i) {
    f = a.step(0.01);
    if (f.camera) CHECK(fw::horizontal_distance(f.eye, path().sill) >= 0.3 - 1e-9);
  }
  a.switched();
  fw::DoorAnimation b;
  b.start_exit(path(), 0.3, timing());
  f = run_until_switch(b);
  CHECK(fw::horizontal_distance(f.eye, path().sill) >= 0.3 - 1e-9);
  b.switched();
  for (int i = 0; i < 1000 && !f.done; ++i) {
    f = b.step(0.01);
    CHECK(fw::horizontal_distance(f.eye, path().sill) >= 0.3 - 1e-9);
  }
}

TEST(near_enter_blinks_during_the_last_steps) {
  // Darkening starts while still on the steps: by the time the eye reaches the door sill height it is already dark.
  fw::DoorAnimation a;
  a.start_enter(path(), {-2.5, 0.0, 1.0}, 2.0, true, timing());
  fw::DoorFrame f;
  bool started_below = false;
  for (int i = 0; i < 1000 && !f.switch_now; ++i) {
    f = a.step(0.01);
    if (f.blackout > 0 && f.blackout < 1 && f.eye.y < path().sill.y + 1.7 - 0.1) started_below = true;
  }
  CHECK(started_below);
  CHECK(f.switch_now);
}

TEST(climb_rises_without_stopping) {
  // Going up it rises continuously (no stop at each step), the steps are only shown by a slight bobbing.
  fw::DoorAnimation a;
  a.start_enter(path(), {-0.4, 0.0, 0.0}, -1.5707963267948966, true, timing());
  fw::DoorFrame f = a.step(0.4);  // at the bottom of the steps
  double last = f.eye.y;
  int climbing = 0;
  for (int i = 0; i < 1000 && !f.switch_now; ++i) {
    f = a.step(0.01);
    if (f.switch_now) break;
    CHECK(f.eye.y > last - 1e-9);
    if (f.eye.y > last + 1e-6) ++climbing;
    last = f.eye.y;
  }
  CHECK(climbing > 120);  // over 1.3 s it rises in almost every frame
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

TEST(entering_stops_one_step_below_the_sill) {
  // When boarding it does not step onto the last step (or the eye would end up above the cab roof): 3 of 4 steps, step is 0.4 m.
  fw::DoorAnimation a;
  a.start_enter(path(), {-2.5, 0.0, 1.0}, 2.0, true, timing());
  double highest = -1;
  fw::DoorFrame f;
  for (int i = 0; i < 1000 && !f.switch_now; ++i) {
    f = a.step(0.01);
    highest = std::max(highest, f.eye.y);
  }
  CHECK_NEAR(f.eye.y, 1.2 + 1.7, 1e-9);
  CHECK(highest <= 1.2 + 1.7 + 1e-9);
}

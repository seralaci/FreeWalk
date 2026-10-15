#include "check.h"
#include "core/key_filter.h"

namespace {
constexpr int kW = 'W';
constexpr int kShift = 0x10;
constexpr int kLeftShift = 0xA0;
constexpr int kF5 = 0x74;
}  // namespace

TEST(unwatched_key_is_never_blocked) {
  fw::KeyFilter f;
  f.watch({kW});
  CHECK(!f.on_key(kF5, true, true));
  CHECK(!f.on_key(kF5, false, true));
}

TEST(walking_key_press_and_release_are_both_swallowed) {
  fw::KeyFilter f;
  f.watch({kW});
  CHECK(f.on_key(kW, true, true));
  CHECK(f.is_down(kW));
  CHECK(f.on_key(kW, true, true));  // repeat while held
  CHECK(f.on_key(kW, false, true));
  CHECK(!f.is_down(kW));
}

TEST(key_already_seen_by_the_game_is_left_alone) {
  fw::KeyFilter f;
  f.watch({kW});
  CHECK(!f.on_key(kW, true, false));  // pressed while driving: the game receives it
  CHECK(!f.on_key(kW, true, true));   // the repeat during the walk is not swallowed either
  CHECK(!f.on_key(kW, false, true));  // it also receives the release, so the key does not stick
  CHECK(!f.is_down(kW));
}

TEST(release_after_walk_ended_is_still_swallowed) {
  fw::KeyFilter f;
  f.watch({kW});
  CHECK(f.on_key(kW, true, true));
  CHECK(f.on_key(kW, false, false));  // the game did not receive the press either
}

TEST(state_is_tracked_without_blocking) {
  fw::KeyFilter f;
  f.watch({kW});
  CHECK(!f.on_key(kW, true, false));
  CHECK(f.is_down(kW));
}

TEST(generic_shift_matches_left_and_right_shift) {
  fw::KeyFilter f;
  f.watch({kShift});
  CHECK(f.on_key(kLeftShift, true, true));
  CHECK(f.is_down(kShift));
  CHECK(f.on_key(kLeftShift, false, true));
  CHECK(!f.is_down(kShift));
}

// The game's own (DirectInput) key reading sees the same as Windows: the filter only lets through events
// that are consistent from the game's point of view (release only after a press).
TEST(device_events_of_a_swallowed_press_are_hidden) {
  fw::KeyFilter f;
  f.watch({kW});
  f.on_key(kW, true, true);
  CHECK(f.hide_in_state(kW, false));  // the walk has already ended, but we swallowed the press
  CHECK(!f.pass_device_event(kW, true, true));
  f.on_key(kW, false, true);
  CHECK(!f.pass_device_event(kW, false, true));
  CHECK(!f.hide_in_state(kW, false));
}

TEST(device_keeps_a_press_the_game_already_saw) {
  fw::KeyFilter f;
  f.watch({kW});
  f.on_key(kW, true, false);
  CHECK(f.pass_device_event(kW, true, false));  // while driving: the game receives it
  CHECK(!f.hide_in_state(kW, false));
  f.on_key(kW, false, true);
  CHECK(f.pass_device_event(kW, false, true));  // during the walk it also receives the release
}

TEST(device_hides_new_presses_while_blocking_even_if_hook_was_late) {
  fw::KeyFilter f;
  f.watch({kW});
  CHECK(!f.pass_device_event(kW, true, true));   // the hook has not signaled yet, but a walk is active: the game does not receive it
  CHECK(!f.pass_device_event(kW, false, true));  // so not the release either
}

TEST(device_ignores_unwatched_keys) {
  fw::KeyFilter f;
  f.watch({kW});
  CHECK(f.pass_device_event(kF5, true, true));
  CHECK(f.pass_device_event(kF5, false, true));
  CHECK(!f.hide_in_state(kF5, true));
}

TEST(device_left_shift_follows_generic_shift) {
  fw::KeyFilter f;
  f.watch({kShift});
  f.on_key(kLeftShift, true, true);
  CHECK(f.hide_in_state(kLeftShift, false));
  CHECK(!f.pass_device_event(kLeftShift, true, true));
}

// Live test (0.3.1): W was visible in the game's snapshot even without the hook signaling it. During the walk the snapshot
// must never show a pressed walk key, regardless of the hook.
TEST(state_hides_walking_keys_while_blocking_even_if_hook_missed_them) {
  fw::KeyFilter f;
  f.watch({kW, kShift});
  CHECK(f.hide_in_state(kW, true));
  CHECK(f.hide_in_state(0xA1, true));  // right Shift
  CHECK(!f.hide_in_state(kW, false));
}

TEST(watch_only_replaces_the_watched_keys) {
  fw::KeyFilter f;
  f.watch({kW});
  f.watch_only({'D'});
  CHECK(!f.on_key(kW, true, true));
  CHECK(f.on_key('D', true, true));
}

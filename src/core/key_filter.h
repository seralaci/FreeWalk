#pragma once
#include <array>
#include <atomic>
#include <initializer_list>
#include <vector>

namespace fw {

// Decides which key events to swallow from the game, and tracks the state of the watched keys.
// While walking the watched keys do not reach the game; a key's fate is decided when it is pressed,
// so the game never gets a release without a press, nor a press without a release (nothing gets stuck).
// on_key runs on the keyboard hook thread and is_down on the game thread, so the state is atomic.
class KeyFilter {
 public:
  void watch(std::initializer_list<int> keys);
  // Replaces the watched keys (after a change in the settings window); the previous ones are no longer watched.
  void watch_only(const std::vector<int>& keys);

  // One key event; returns true if the game must not receive it.
  bool on_key(int virtual_key, bool down, bool blocking);

  bool is_down(int virtual_key) const;

  // For the game's own (DirectInput) key reading: whether the key must stay hidden in the snapshot.
  // Yes while walking (regardless of the hook, since the game's reading sees the key even without the hook),
  // and afterwards too, until the release of a swallowed press arrives.
  bool hide_in_state(int virtual_key, bool blocking) const;

  // Whether a key event arriving in the game's event queue may pass.
  // The game only gets a release whose press it also got; while walking it gets no new presses.
  bool pass_device_event(int virtual_key, bool down, bool blocking);

 private:
  bool watched(int virtual_key) const;

  std::array<std::atomic<bool>, 256> watched_{};
  std::array<std::atomic<bool>, 256> down_{};
  std::array<std::atomic<bool>, 256> swallowed_{};
  std::array<std::atomic<bool>, 256> game_down_{};
};

}  // namespace fw

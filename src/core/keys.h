#pragma once
#include <functional>
#include "core/config.h"

namespace fw {

struct WalkKeys {
  bool forward = false;
  bool back = false;
  bool left = false;
  bool right = false;
  bool run = false;
};

// The walking keys, per the settings.
// If the game window is not in the foreground or the game is paused, none counts as pressed,
// so typing in another program or a menu does not move the walker.
WalkKeys map_keys(bool game_focused, bool paused, const std::function<bool(int)>& is_down, const Config& cfg);

// Whether a command key (leave the truck, settings window) is pressed: Ctrl must be down exactly when the code asks for it,
// so F10 and Ctrl+F10 do not interfere. Code 0 (disabled) never counts as pressed.
bool combo_down(int code, const std::function<bool(int)>& is_down);

// Signals the moment a key is pressed: true only in the first frame, no repeat while held.
class EdgeDetector {
 public:
  bool update(bool down) {
    const bool pressed = down && !was_down_;
    was_down_ = down;
    return pressed;
  }

 private:
  bool was_down_ = false;
};

}  // namespace fw

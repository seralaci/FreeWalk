#pragma once
#include <string>
#include <string_view>
#include "core/door_path.h"
#include "core/walk_session.h"

namespace fw {

// m: the get-in hint shows only in front of the door, within this of the door sill (the limit for entering via the steps is wider).
inline constexpr double kDoorHintDistance = 1.5;

// The key hint shown at the bottom of the screen.
enum class Hint { None, GetIn, GetOut };

struct HintInput {
  bool enabled = false;    // whether the setting asks for hints
  bool focused = false;    // the game window is in the foreground
  bool paused = false;
  bool animating = false;  // a leave or enter animation is running
  bool driving = false;    // in the cab (not walking)
  bool walking = false;
  bool can_leave = false;  // the truck is stopped with the parking brake on (can leave)
  bool at_door = false;    // the walker stands in front of the door
};

Hint choose_hint(const HintInput& in);

// The hint text (UTF-8) in the game's language (g_lang, e.g. "hu_hu"); English for an unknown language.
std::string hint_text(Hint hint, std::string_view language);

// The truck is stopped, the parking brake is on and telemetry data is available: the conditions for leaving.
bool truck_parked(const TruckState& truck);

}  // namespace fw

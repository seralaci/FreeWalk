#pragma once
#include <optional>

namespace fw {

// The game loads the map's collision geometry into physics only near vehicles; while walking we also ask it for a query
// around the walker (with the game's own region query). Often and with a growing box at the start of a walk, so the area
// loads quickly, less often afterwards.
inline constexpr double kFocusFirstBox = 30.0;   // m: box size of the first query
inline constexpr double kFocusLastBox = 150.0;   // m: at the end of the ramp-up and after
inline constexpr int kFocusRampQueries = 12;     // number of queries in the ramp-up
inline constexpr double kFocusRampInterval = 0.25;  // s during the ramp-up
inline constexpr double kFocusInterval = 1.0;       // s afterwards

class FocusSchedule {
 public:
  // Restarts the ramp-up (at the start of a walk).
  void reset();

  // One frame: the query box size if a query is due now.
  std::optional<double> step(double dt);

 private:
  int count_ = 0;
  double timer_ = kFocusRampInterval;  // the first query is due at once
};

}  // namespace fw

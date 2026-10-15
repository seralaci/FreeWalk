#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "core/focus_schedule.h"
#include "core/game_layout.h"
#include "core/vec.h"

namespace fw {

// The game's region query around the walker: this makes the game load the nearby collision geometry (buildings, fences,
// poles) into physics, which it otherwise only does near vehicles. Calls the game's own functions, with exception protection.
class RegionFocus {
 public:
  // Checks the two functions and the scene global (at load time). False if unusable: then only the area around vehicles loads.
  bool init(std::vector<std::string>& log);

  // At walk start: the ramp-up schedule restarts.
  void reset() { schedule_.reset(); }

  // Per frame while walking: requests a query around the walker according to the schedule.
  void update(Vec3 feet, double dt, std::vector<std::string>& log);

 private:
  bool ready_ = false;
  std::uint64_t base_ = 0;
  const layout::GameBuild* build_ = nullptr;  // the running game build (its RVAs)
  FocusSchedule schedule_;
  unsigned queries_ = 0;
  int slow_ = 0;  // consecutive slow queries
};

}  // namespace fw

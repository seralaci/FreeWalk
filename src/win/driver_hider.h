#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "core/game_layout.h"
#include "win/process_memory.h"

namespace fw {

// Hides the driver in the cab while walking: the driver model pointer is taken out of its slot in the player's vehicle and put back
// later. Only data is written, the game's code is not changed. The game keeps its reference meanwhile; if the game sets the driver up
// again while hidden, the new one is hidden too (the old reference cannot be given back then, which is logged).
class DriverHider {
 public:
  bool init(std::vector<std::string>& log);
  // Hides the driver (if not hidden yet). Safe to call every frame.
  void hide(std::vector<std::string>& log);
  // While hidden, once a second: if the game set the driver up again, hides the new one too.
  void tick(double dt, std::vector<std::string>& log);
  // Puts the driver back, if its slot is still the same and still empty.
  void show(std::vector<std::string>& log);
  bool hidden() const { return slot_ != 0; }

 private:
  // The driver model slot of the player's vehicle, after checking the chain; 0 if not available (why: in the reason).
  std::uint64_t find_slot(std::string* reason = nullptr) const;
  // The game's type name of an object (from its vtable's type name getter), or "?".
  std::string type_of(std::uint64_t object) const;

  ProcessMemory memory_;
  std::uint64_t base_ = 0;
  std::uint64_t image_end_ = 0;
  const layout::GameBuild* build_ = nullptr;
  std::uint64_t slot_ = 0;   // the emptied slot (0: not hidden)
  std::uint64_t model_ = 0;  // the model pointer taken out
  double check_timer_ = 0;
  int lost_ = 0;  // references that could not be given back
  bool reported_ = false;  // why it could not be hidden was logged (once per walk)
};

}  // namespace fw

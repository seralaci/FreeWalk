#pragma once
#include <cstdint>
#include <optional>
#include <vector>
#include "core/vec.h"

namespace fw {

// A street lamp from the game's light array: identifier (its address), position, and whether it is lit.
struct LightCandidate {
  std::uint64_t id = 0;
  Vec3 position;
  bool active = false;
};

// The lamp to borrow: at least min_distance from the walker (so no nearby lamp goes dark), and of those the nearest
// within max_distance; if there is none, the nearest one farther away. Never picks a switched-off lamp.
std::optional<std::uint64_t> choose_light(Vec3 walker, const std::vector<LightCandidate>& lights, double min_distance, double max_distance);

}  // namespace fw

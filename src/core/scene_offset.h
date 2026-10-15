#pragma once
#include <cstdint>
#include <optional>
#include <vector>
#include "core/vec.h"

namespace fw {

// The origin of the game's PhysX scene is shifted from the world by a multiple of 512 m in X and Z (not in Y);
// we estimate the shift from the truck's telemetry position and the position of the truck's physics body.
inline constexpr double kSceneGrid = 512.0;
inline constexpr double kMaxSceneOffsetResidual = 6.0;   // m: with a larger residual the actor is not the truck
inline constexpr double kTruckActorHeightWindow = 30.0;  // m: how far the truck actor's height may differ from telemetry

struct SceneOffset {
  double x = 0;
  double z = 0;
  double residual = 0;      // |dx - x| + |dz - z|: truck vs. actor position difference after the shift
  std::uint64_t actor = 0;  // the actor the shift came from
};

// Position of a dynamic actor in scene coordinates.
struct ActorPosition {
  std::uint64_t actor = 0;
  Vec3 position;
};

// The shift, if the actor can be the truck (both its height and the residual are small).
std::optional<SceneOffset> offset_from_actor(Vec3 truck_world, const ActorPosition& actor);

// The shift from the actor with the smallest residual.
std::optional<SceneOffset> estimate_scene_offset(Vec3 truck_world, const std::vector<ActorPosition>& actors);

inline Vec3 world_to_scene(Vec3 world, const SceneOffset& o) { return {world.x - o.x, world.y, world.z - o.z}; }

}  // namespace fw

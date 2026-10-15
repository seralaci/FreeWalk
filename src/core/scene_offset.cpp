#include "core/scene_offset.h"
#include <cmath>

namespace fw {

std::optional<SceneOffset> offset_from_actor(Vec3 truck, const ActorPosition& a) {
  if (!(std::fabs(a.position.y - truck.y) <= kTruckActorHeightWindow)) return std::nullopt;
  const double dx = truck.x - a.position.x;
  const double dz = truck.z - a.position.z;
  SceneOffset o;
  o.x = std::round(dx / kSceneGrid) * kSceneGrid;
  o.z = std::round(dz / kSceneGrid) * kSceneGrid;
  o.residual = std::fabs(dx - o.x) + std::fabs(dz - o.z);
  o.actor = a.actor;
  if (!(o.residual < kMaxSceneOffsetResidual)) return std::nullopt;
  return o;
}

std::optional<SceneOffset> estimate_scene_offset(Vec3 truck, const std::vector<ActorPosition>& actors) {
  std::optional<SceneOffset> best;
  for (const auto& a : actors) {
    const auto o = offset_from_actor(truck, a);
    if (o && (!best || o->residual < best->residual)) best = o;
  }
  return best;
}

}  // namespace fw

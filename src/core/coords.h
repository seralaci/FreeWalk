#pragma once
#include <cstdint>
#include "core/vec.h"

namespace fw {

// The game camera stores its horizontal position in 512 m chunks: the chunk index is an integer, the position inside it a float.
// This keeps the float value small, so it loses no precision even in distant parts of the map.
inline constexpr double kChunkSize = 512.0;

struct CameraPosition {
  std::int16_t chunk_x = 0;
  std::int16_t chunk_z = 0;
  float x = 0;
  float y = 0;
  float z = 0;
};

CameraPosition to_camera(Vec3 world);
Vec3 to_world(const CameraPosition& camera);

}  // namespace fw

#include "core/coords.h"
#include <cmath>

namespace fw {
namespace {

std::int16_t chunk_of(double value) {
  return static_cast<std::int16_t>(std::floor(value / kChunkSize));
}

}  // namespace

CameraPosition to_camera(Vec3 world) {
  CameraPosition c;
  c.chunk_x = chunk_of(world.x);
  c.chunk_z = chunk_of(world.z);
  c.x = static_cast<float>(world.x - c.chunk_x * kChunkSize);
  c.y = static_cast<float>(world.y);
  c.z = static_cast<float>(world.z - c.chunk_z * kChunkSize);
  return c;
}

Vec3 to_world(const CameraPosition& c) {
  return {c.chunk_x * kChunkSize + c.x, c.y, c.chunk_z * kChunkSize + c.z};
}

}  // namespace fw

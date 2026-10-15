#pragma once
#include "core/config.h"
#include "core/keys.h"
#include "core/motion.h"
#include "core/vec.h"
#include "core/world.h"

namespace fw {

// One frame advances at most this much time, so the walker does not leap after a stall or pause.
inline constexpr double kMaxStepSeconds = 0.1;

// The walker's position and motion against the world's walls and ground.
class Walker {
 public:
  // The walker stands at the given feet point, not falling.
  void reset(Vec3 feet);

  // Puts the feet on the real ground if the world knows it (when leaving; also searches above the estimate).
  void settle(World& world);

  // Motion for one frame against the world's walls and ground.
  // yaw is the view direction in radians, counter-clockwise seen from above, 0 = north.
  void step(const WalkKeys& keys, double yaw, double dt, const Config& cfg, World& world);

  Vec3 feet() const { return feet_; }
  Vec3 eye(const Config& cfg) const;

 private:
  Vec3 feet_;
  Vertical vertical_;
  double bob_phase_ = 0;
  double bob_blend_ = 0;
};

}  // namespace fw

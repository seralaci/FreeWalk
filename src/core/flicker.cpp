#include "core/flicker.h"
#include <algorithm>

namespace fw {
namespace {
constexpr double kRecoverSeconds = 0.3;
}  // namespace

Flicker::Flicker(std::uint32_t seed) : rng_(seed) {}

double Flicker::step(double dt, double chance_per_hour) {
  if (phase_ == Phase::Idle) {
    if (chance_per_hour <= 0) {
      until_next_ = -1;
      return 1.0;
    }
    if (until_next_ < 0) until_next_ = std::exponential_distribution<double>(chance_per_hour / 3600.0)(rng_);
    until_next_ -= dt;
    if (until_next_ > 0) return 1.0;
    until_next_ = -1;
    phase_ = Phase::Flutter;
    phase_left_ = uniform(0.6, 1.2);
    flutter_left_ = 0;
  }
  phase_left_ -= dt;
  switch (phase_) {
    case Phase::Flutter:
      flutter_left_ -= dt;
      if (flutter_left_ <= 0) {
        // Alternating dark and bright flashes of random length.
        level_ = level_ > 0.5 ? uniform(0.0, 0.3) : uniform(0.6, 1.0);
        flutter_left_ = uniform(0.03, 0.12);
      }
      if (phase_left_ <= 0) {
        phase_ = Phase::Dark;
        phase_left_ = uniform(0.5, 1.0);
        level_ = 0;
      }
      return level_;
    case Phase::Dark:
      if (phase_left_ <= 0) {
        phase_ = Phase::Recover;
        phase_left_ = kRecoverSeconds;
      }
      return 0.0;
    case Phase::Recover:
      if (phase_left_ <= 0) {
        phase_ = Phase::Idle;
        level_ = 1;
        return 1.0;
      }
      return std::clamp(1.0 - phase_left_ / kRecoverSeconds, 0.0, 1.0);
    case Phase::Idle: break;
  }
  return 1.0;
}

}  // namespace fw

#pragma once
#include <cstdint>
#include <random>

namespace fw {

// The flashlight "playing up": on average chance_per_hour events per hour of use, at random times.
// One event: 0.6-1.2 s of fast flicker, 0.5-1 s dark (the hand shakes it), then back to full over 0.3 s.
// step returns the brightness multiplier (0..1); the random generator seed can be fixed for tests.
class Flicker {
 public:
  explicit Flicker(std::uint32_t seed = 0x5EED);
  double step(double dt, double chance_per_hour);
  bool active() const { return phase_ != Phase::Idle; }

 private:
  enum class Phase { Idle, Flutter, Dark, Recover };
  double uniform(double lo, double hi) { return std::uniform_real_distribution<double>(lo, hi)(rng_); }

  std::mt19937 rng_;
  Phase phase_ = Phase::Idle;
  double until_next_ = -1;  // s until the next event (-1: not drawn yet)
  double phase_left_ = 0;
  double flutter_left_ = 0;
  double level_ = 1;
};

}  // namespace fw

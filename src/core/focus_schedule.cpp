#include "core/focus_schedule.h"

namespace fw {

void FocusSchedule::reset() {
  count_ = 0;
  timer_ = kFocusRampInterval;
}

std::optional<double> FocusSchedule::step(double dt) {
  const bool ramping = count_ < kFocusRampQueries;
  const double interval = ramping ? kFocusRampInterval : kFocusInterval;
  timer_ += dt;
  // A small tolerance so floating point summing does not push a due query to the next frame.
  if (timer_ + 1e-9 < interval) return std::nullopt;
  timer_ -= interval;
  if (timer_ > interval) timer_ = 0;  // after a long pause the missed ones are not made up
  const double box = ramping ? kFocusFirstBox + (kFocusLastBox - kFocusFirstBox) * count_ / (kFocusRampQueries - 1) : kFocusLastBox;
  ++count_;
  return box;
}

}  // namespace fw

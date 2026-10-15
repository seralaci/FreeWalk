#include "core/key_filter.h"

namespace fw {
namespace {

// The keyboard hook reports left and right modifier keys with separate codes; the settings use the generic code.
int generic_of(int virtual_key) {
  switch (virtual_key) {
    case 0xA0: case 0xA1: return 0x10;  // Shift
    case 0xA2: case 0xA3: return 0x11;  // Ctrl
    case 0xA4: case 0xA5: return 0x12;  // Alt
    default: return virtual_key;
  }
}

bool valid(int virtual_key) { return virtual_key > 0 && virtual_key < 256; }

}  // namespace

void KeyFilter::watch(std::initializer_list<int> keys) {
  for (const int k : keys)
    if (valid(k)) watched_[k] = true;
}

void KeyFilter::watch_only(const std::vector<int>& keys) {
  for (auto& w : watched_) w = false;
  for (const int k : keys)
    if (valid(k)) watched_[k] = true;
}

bool KeyFilter::watched(int virtual_key) const { return watched_[virtual_key] || watched_[generic_of(virtual_key)]; }

bool KeyFilter::on_key(int virtual_key, bool down, bool blocking) {
  if (!valid(virtual_key) || !watched(virtual_key)) return false;
  if (down) {
    if (!down_[virtual_key]) swallowed_[virtual_key] = blocking;
    down_[virtual_key] = true;
    return swallowed_[virtual_key];
  }
  down_[virtual_key] = false;
  return swallowed_[virtual_key].exchange(false);
}

bool KeyFilter::hide_in_state(int virtual_key, bool blocking) const {
  return valid(virtual_key) && watched(virtual_key) && (blocking || swallowed_[virtual_key]);
}

bool KeyFilter::pass_device_event(int virtual_key, bool down, bool blocking) {
  if (!valid(virtual_key) || !watched(virtual_key)) return true;
  if (down) {
    if (game_down_[virtual_key]) return true;
    if (swallowed_[virtual_key] || blocking) return false;
    game_down_[virtual_key] = true;
    return true;
  }
  return game_down_[virtual_key].exchange(false);
}

bool KeyFilter::is_down(int virtual_key) const {
  if (!valid(virtual_key)) return false;
  switch (virtual_key) {
    case 0x10: return down_[0x10] || down_[0xA0] || down_[0xA1];
    case 0x11: return down_[0x11] || down_[0xA2] || down_[0xA3];
    case 0x12: return down_[0x12] || down_[0xA4] || down_[0xA5];
    default: return down_[virtual_key];
  }
}

}  // namespace fw

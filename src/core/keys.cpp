#include "core/keys.h"

namespace fw {

WalkKeys map_keys(bool game_focused, bool paused, const std::function<bool(int)>& is_down, const Config& cfg) {
  WalkKeys k;
  if (!game_focused || paused) return k;
  k.forward = is_down(cfg.key_forward);
  k.back = is_down(cfg.key_back);
  k.left = is_down(cfg.key_left);
  k.right = is_down(cfg.key_right);
  k.run = is_down(cfg.key_run);
  return k;
}

bool combo_down(int code, const std::function<bool(int)>& is_down) {
  const int base = code & 0xFF;
  if (base == 0 || !is_down(base)) return false;
  // If the key is Ctrl itself, pressing it must not exclude itself.
  if (base == 0x11 || base == 0xA2 || base == 0xA3) return true;
  const bool ctrl = is_down(0x11);
  return (code & kKeyCtrl) != 0 ? ctrl : !ctrl;
}

}  // namespace fw

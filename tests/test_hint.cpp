#include "check.h"
#include "core/hint.h"

namespace {
fw::HintInput base() {
  fw::HintInput in;
  in.enabled = true;
  in.focused = true;
  return in;
}
}  // namespace

TEST(get_out_hint_when_parked_in_the_cab) {
  auto in = base();
  in.driving = true;
  in.can_leave = true;
  CHECK(fw::choose_hint(in) == fw::Hint::GetOut);
  in.can_leave = false;
  CHECK(fw::choose_hint(in) == fw::Hint::None);
}

TEST(get_in_hint_only_at_the_door) {
  auto in = base();
  in.walking = true;
  CHECK(fw::choose_hint(in) == fw::Hint::None);
  in.at_door = true;
  CHECK(fw::choose_hint(in) == fw::Hint::GetIn);
}

TEST(no_hint_when_off_paused_unfocused_or_animating) {
  auto in = base();
  in.walking = true;
  in.at_door = true;
  for (int i = 0; i < 4; ++i) {
    auto x = in;
    if (i == 0) x.enabled = false;
    if (i == 1) x.paused = true;
    if (i == 2) x.focused = false;
    if (i == 3) x.animating = true;
    CHECK(fw::choose_hint(x) == fw::Hint::None);
  }
}

TEST(parked_truck_can_be_left) {
  fw::TruckState t;
  t.valid = true;
  t.head_known = true;
  t.parking_brake = true;
  CHECK(fw::truck_parked(t));
  t.speed = 1.0;
  CHECK(!fw::truck_parked(t));
  t.speed = 0;
  t.parking_brake = false;
  CHECK(!fw::truck_parked(t));
}

TEST(hint_text_follows_the_game_language) {
  CHECK(fw::hint_text(fw::Hint::GetIn, "hu_hu") == "Beszállás");
  CHECK(fw::hint_text(fw::Hint::GetOut, "hu_hu") == "Kiszállás");
  CHECK(fw::hint_text(fw::Hint::GetOut, "de_de") == "Aussteigen");
  CHECK(fw::hint_text(fw::Hint::GetIn, "pt_br") == "Entrar");
  CHECK(fw::hint_text(fw::Hint::GetIn, "xx_yy") == "Get in");
  CHECK(fw::hint_text(fw::Hint::GetOut, "") == "Get out");
  CHECK(fw::hint_text(fw::Hint::None, "hu_hu").empty());
}

TEST(get_in_hint_only_right_at_the_door) {
  fw::DoorPath p;
  p.sill = {0, 1.7, 0};
  p.outward_yaw = 1.5707963267948966;  // outward: west (-x)
  CHECK(fw::near_door(p, {-1.0, 0, 0}, fw::kDoorHintDistance));
  CHECK(!fw::near_door(p, {-2.5, 0, 0}, fw::kDoorHintDistance));
}

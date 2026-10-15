# Élethűbb ki- és beszállás - megvalósítási terv

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Kiszálláskor pislogás alatt a kép az ajtó küszöbére kerül, és a gyalogló lépcsőfokonként lemegy; beszálláskor az ajtó mellől felmegy, és pislogás után a fülkében van; szoros helyen a másik ajtón száll ki.

**Architecture:** A mag számolja az ajtó geometriáját (`door_path`), választja az ajtót (`door_choice`), és egy időben lépő állapotgép (`DoorAnimation`) képkockánként adja a kamera helyét, irányát, a sötétítést és a váltás pillanatát. A `WalkSession` változatlan marad; a plugin a session parancsai köré szervezi az animációt. A Windows-réteg egy sötétítő ablakot (`fade_window`) tesz a játék fölé.

**Tech Stack:** C++20, MSVC, CMake + Ninja; Win32 réteges ablak saját üzenetszálon.

**Spec:** `docs/superpowers/specs/2026-10-14-door-animation-design.md`

## Global Constraints

- A TM Real Walk kódjából semmit nem másolunk; a játék kódját nem módosítjuk.
- Naplószöveg angol, ASCII, `[FreeWalk] ` előtag; kódkomment magyar; commit üzenet angol, `Co-Authored-By` nélkül.
- MSVC `/W4` figyelmeztetésmentes; minden tesztcsomagnál `kVersion` léptetés (ez a terv a `0.7.x` sorozat).
- Alapértékek: `door_animation` 1, `door_step_time` 1.3, `door_fade_time` 0.25, `door_out` 0.7, `door_sill_below_head` 1.0, `door_min_free` 0.8, `enter_near_door` 3 (a Leaving the truck fülön).
- A sötétítés soha nem maradhat bent: minden kilépési úton (hiba, kirakás, megszakítás) 0-ra áll.
- Tesztek: `sh .superpowers/sdd/2026-10-14-door-animation/b.sh`.
- Minden eltérés `Ruling:` sorként a végrehajtási naplóba.

## Review Focus

- A váltás nem történik meg (a játék nem kapcsol át a szabad kamerára): a kép kivilágosodik, a kiszállás elmarad, semmi nem marad elsötétítve (2. feladat: `exit_fails_after_the_switch_timeout`).
- A játékos az animáció közben megnyomja a ki/beszállás billentyűt vagy a lámpát: nem indul új váltás, a lámpa nem kapcsol be (5. feladat: a billentyűk elnyelése az animáció alatt).
- Szünet az animáció közben: megáll és folytatódik; a sötét ablak nem marad fent a menü fölött kivilágosodás nélkül (2. feladat: `zero_dt_pauses_the_animation`).
- Hibából eredő visszaszállás az animáció alatt (a kamera nem írható): azonnali visszaszállás, a sötétítés 0 (5. feladat: `return_to_truck` törli az animációt).
- A talaj a küszöb fölött (lejtő, járdaszegély): nincs negatív lépcső, csak kilépés (1. feladat: `no_steps_when_the_ground_is_not_below_the_sill`).

---

### Task 1: Az ajtó geometriája és az ajtóválasztás

**Files:** Create `src/core/door_path.h/.cpp`, `src/core/door_choice.h/.cpp`, `tests/test_door_path.cpp`; Modify `src/core/walk_session.h/.cpp` (oldal-paraméteres kiszállási pont és irány), `CMakeLists.txt`

**Interfaces:**
- Produces:
  `int driver_side(const TruckState&)` (a `walk_session.cpp`-ből nyilvánosra, `double` helyett `int`: +1 jobb, -1 bal),
  `Vec3 exit_point_on(const TruckState&, const Config&, int side)`, `double exit_yaw_on(const TruckState&, const Config&, int side)` (a régi `exit_point`/`exit_yaw` ezek hívása a vezetőoldallal),
  `struct DoorSettings { double out = 0.7; double sill_below_head = 1.0; };`
  `struct DoorPath { int side = 1; Vec3 sill; Vec3 ground; int steps = 0; double outward_yaw = 0; double door_yaw = 0; };`
  `DoorPath door_path(const TruckState&, const Config&, int side, double ground_height, const DoorSettings&);`
  `inline constexpr double kMaxStepHeight = 0.45;`
  `enum class DoorSide { Driver, Passenger, None };`
  `DoorSide choose_door(std::optional<double> driver_free, std::optional<double> passenger_free, double min_free);`

- [ ] **Step 1: Piros tesztek** (`tests/test_door_path.cpp`)

```cpp
#include <cmath>
#include <optional>
#include "check.h"
#include "core/door_choice.h"
#include "core/door_path.h"

namespace {
constexpr double kPi = 3.14159265358979323846;

fw::TruckState truck(double heading, double head_x = -0.7) {
  fw::TruckState t;
  t.valid = true;
  t.position = {100, 10, 200};
  t.heading = heading;
  t.head_known = true;
  t.head_in_truck = {head_x, 2.7, -1.9};
  return t;
}
}  // namespace

TEST(door_sill_is_outside_the_cab_below_the_head) {
  const fw::Config cfg;
  const auto p = fw::door_path(truck(0), cfg, -1, 10.0, fw::DoorSettings{});
  // Észak felé álló kamion, bal oldali vezető: a küszöb a fejtől 0,7 m-rel balra (nyugatra), 1,0 m-rel lejjebb.
  CHECK_NEAR(p.sill.x, 100 - 0.7 - 0.7, 1e-9);
  CHECK_NEAR(p.sill.y, 10 + 2.7 - 1.0, 1e-9);
  CHECK_NEAR(p.sill.z, 200 - 1.9, 1e-9);
  CHECK_NEAR(p.ground.x, 100 - 0.7 - cfg.exit_side_distance, 1e-9);
  CHECK_NEAR(p.ground.y, 10.0, 1e-12);
  CHECK(p.steps == 4);  // 1,7 m / 0,45 -> 4 fok
}

TEST(passenger_door_mirrors_the_driver_door) {
  const fw::Config cfg;
  const auto p = fw::door_path(truck(0), cfg, +1, 10.0, fw::DoorSettings{});
  CHECK_NEAR(p.sill.x, 100 + 0.7 + 0.7, 1e-9);
  CHECK_NEAR(p.ground.x, 100 + 0.7 + cfg.exit_side_distance, 1e-9);
}

TEST(door_follows_the_truck_heading) {
  const fw::Config cfg;
  const auto p = fw::door_path(truck(kPi / 2), cfg, -1, 10.0, fw::DoorSettings{});  // nyugat felé álló kamion: a bal oldal dél
  CHECK(p.sill.z > 200 + 1.0);
  CHECK_NEAR(p.sill.x, 100 - 1.9, 1e-9);
}

TEST(door_yaws_face_out_and_in) {
  const fw::Config cfg;
  const auto p = fw::door_path(truck(0), cfg, -1, 10.0, fw::DoorSettings{});
  const fw::Vec3 out = fw::forward_from_yaw(p.outward_yaw);
  const fw::Vec3 in = fw::forward_from_yaw(p.door_yaw);
  CHECK_NEAR(out.x, -1.0, 1e-9);  // a bal oldali ajtóból kifelé: nyugat
  CHECK_NEAR(in.x, 1.0, 1e-9);
}

TEST(step_count_follows_the_height) {
  fw::Config cfg;
  fw::DoorSettings s;
  s.sill_below_head = 2.7 - 1.2;  // a küszöb 1,2 m-rel a talaj fölött
  CHECK(fw::door_path(truck(0), cfg, -1, 10.0, s).steps == 3);
  s.sill_below_head = 2.7 - 0.3;
  CHECK(fw::door_path(truck(0), cfg, -1, 10.0, s).steps == 1);
}

TEST(no_steps_when_the_ground_is_not_below_the_sill) {
  const fw::Config cfg;
  CHECK(fw::door_path(truck(0), cfg, -1, 13.0, fw::DoorSettings{}).steps == 0);
}

TEST(door_choice_prefers_the_driver_side) {
  CHECK(fw::choose_door(1.2, 1.5, 0.8) == fw::DoorSide::Driver);
  CHECK(fw::choose_door(0.3, 1.5, 0.8) == fw::DoorSide::Passenger);
  CHECK(fw::choose_door(0.3, 0.5, 0.8) == fw::DoorSide::None);
  CHECK(fw::choose_door(std::nullopt, std::nullopt, 0.8) == fw::DoorSide::Driver);
  CHECK(fw::choose_door(0.1, std::nullopt, 0.8) == fw::DoorSide::Passenger);
}

TEST(exit_on_the_passenger_side_mirrors_the_yaw) {
  const fw::Config cfg;
  const auto t = truck(0);
  const double left = fw::exit_yaw_on(t, cfg, -1);
  const double right = fw::exit_yaw_on(t, cfg, +1);
  CHECK_NEAR(left, -right, 1e-9);
  CHECK_NEAR(fw::exit_yaw(t, cfg), left, 1e-12);
}
```

CMake: `src/core/door_path.cpp`, `src/core/door_choice.cpp`, `tests/test_door_path.cpp`. Run - Expected: fordítási hiba.

- [ ] **Step 2: A `walk_session` oldal-paraméteres változatai.** A `walk_session.h`-ba `int driver_side(const TruckState&);`, `Vec3 exit_point_on(const TruckState&, const Config&, int side);`, `double exit_yaw_on(const TruckState&, const Config&, int side);`. A `.cpp`-ben a névtelen névtérbeli `driver_side` nyilvános lesz; az `exit_point_on`/`exit_yaw_on` a mostani `exit_point`/`exit_yaw` törzse a `side` paraméterrel; az `exit_point(t, c)` = `exit_point_on(t, c, driver_side(t))`, az `exit_yaw(t, c)` = `exit_yaw_on(t, c, driver_side(t))`.

- [ ] **Step 3: `src/core/door_path.h/.cpp`**

```cpp
// door_path.h
#pragma once
#include "core/config.h"
#include "core/walk_session.h"

namespace fw {

inline constexpr double kMaxStepHeight = 0.45;  // m: egy lépcsőfok legfeljebb ennyi

struct DoorSettings {
  double out = 0.7;              // m: a küszöbpont a fejtől oldalra (a fülkén kívül)
  double sill_below_head = 1.0;  // m: a küszöb a fej magassága alatt
};

// Egy ajtó geometriája: a küszöbpont (a lépcső teteje), a kilépés talajpontja, a lépcsőfokok száma, a kifelé és az ajtó felé néző irány.
struct DoorPath {
  int side = 1;  // +1 jobb, -1 bal (a kamion menetirányához képest)
  Vec3 sill;
  Vec3 ground;
  int steps = 0;
  double outward_yaw = 0;
  double door_yaw = 0;
};

DoorPath door_path(const TruckState& truck, const Config& cfg, int side, double ground_height, const DoorSettings& s);

}  // namespace fw
```

```cpp
// door_path.cpp
#include "core/door_path.h"
#include <cmath>

namespace fw {
namespace {
constexpr double kPi = 3.14159265358979323846;
}  // namespace

DoorPath door_path(const TruckState& truck, const Config& cfg, int side, double ground_height, const DoorSettings& s) {
  DoorPath p;
  p.side = side;
  const Vec3 head = truck.head_in_truck;
  p.sill = truck.position + rotate_heading({head.x + side * s.out, 0.0, head.z}, truck.heading);
  p.sill.y = truck.position.y + head.y - s.sill_below_head;
  p.ground = exit_point_on(truck, cfg, side);
  p.ground.y = ground_height;
  const double drop = p.sill.y - p.ground.y;
  p.steps = drop > 0.05 ? static_cast<int>(std::ceil(drop / kMaxStepHeight - 1e-9)) : 0;
  // A heading balra fordulva nő: a jobb oldalra kifelé néző irány heading - 90 fok, a bal oldalra heading + 90 fok.
  p.outward_yaw = truck.heading - side * kPi / 2;
  p.door_yaw = truck.heading + side * kPi / 2;
  return p;
}

}  // namespace fw
```

`src/core/door_choice.h/.cpp`:

```cpp
// door_choice.h
#pragma once
#include <optional>

namespace fw {

enum class DoorSide { Driver, Passenger, None };

// A kiszálló ajtó a két ajtó előtt mért szabad helyből (m); nullopt = nem mérhető (fizika nélkül), szabadnak számít.
DoorSide choose_door(std::optional<double> driver_free, std::optional<double> passenger_free, double min_free);

}  // namespace fw
```

```cpp
// door_choice.cpp
#include "core/door_choice.h"

namespace fw {

DoorSide choose_door(std::optional<double> driver_free, std::optional<double> passenger_free, double min_free) {
  if (!driver_free || *driver_free >= min_free) return DoorSide::Driver;
  if (!passenger_free || *passenger_free >= min_free) return DoorSide::Passenger;
  return DoorSide::None;
}

}  // namespace fw
```

Run - Expected: minden zöld.

- [ ] **Step 4: Commit** - `git commit -m "Compute the door geometry and choose the door to leave by"`

---

### Task 2: Az animáció állapotgépe

**Files:** Create `src/core/door_animation.h/.cpp`, `tests/test_door_animation.cpp`; Modify `CMakeLists.txt`

**Interfaces:**
- Consumes: `DoorPath` (1.).
- Produces:
  `struct DoorTiming { double fade = 0.25; double steps = 1.3; double approach = 0.4; double switch_timeout = 3.0; double eye_height = 1.7; };`
  `struct DoorFrame { bool camera = false; Vec3 eye; double yaw = 0; double blackout = 0; bool switch_now = false; bool done = false; bool failed = false; };`
  `class DoorAnimation { void start_exit(const DoorPath&, double final_yaw, const DoorTiming&); void start_enter(const DoorPath&, Vec3 walker_feet, double walker_yaw, bool near, const DoorTiming&); DoorFrame step(double dt); void switched(); void cancel(); bool active() const; bool entering() const; };`

A szakaszok: kiszállás `FadeOut → WaitSwitch → FadeIn → Steps → done`; beszállás közelről `Approach → StepsUp → FadeOut → WaitSwitch → FadeIn → done`; messziről `FadeOut → WaitSwitch → FadeIn → done`. A `switch_now` a `WaitSwitch` első képkockáján igaz (teljes sötétség); a `switched()` után a `FadeIn` jön; ha a `WaitSwitch` tovább tart `switch_timeout`-nál: `FadeIn`, majd `done` + `failed`. A `camera` azt jelzi, hogy a hívónak a szabad kamerát kell-e írnia: kiszállásnál a `switch_now`-tól, beszállásnál a `WaitSwitch`-ig.

- [ ] **Step 1: Piros tesztek** (`tests/test_door_animation.cpp`)

```cpp
#include <cmath>
#include "check.h"
#include "core/door_animation.h"

namespace {
fw::DoorPath path() {
  fw::DoorPath p;
  p.side = -1;
  p.sill = {0, 1.6, 0};
  p.ground = {-0.8, 0.0, 0};
  p.steps = 4;
  p.outward_yaw = 1.5707963267948966;
  p.door_yaw = -1.5707963267948966;
  return p;
}
fw::DoorTiming timing() { return fw::DoorTiming{}; }

// Lépteti az animációt, amíg a váltást nem kéri (vagy legfeljebb n képkockát).
fw::DoorFrame run_until_switch(fw::DoorAnimation& a, int n = 1000) {
  fw::DoorFrame f;
  for (int i = 0; i < n && !(f = a.step(0.01)).switch_now; ++i) {
  }
  return f;
}
}  // namespace

TEST(exit_fades_out_then_asks_for_the_switch_in_darkness) {
  fw::DoorAnimation a;
  a.start_exit(path(), 0.3, timing());
  auto f = a.step(0.125);
  CHECK(!f.camera);
  CHECK_NEAR(f.blackout, 0.5, 1e-9);
  CHECK(!f.switch_now);
  f = run_until_switch(a);
  CHECK(f.switch_now);
  CHECK_NEAR(f.blackout, 1.0, 1e-12);
  CHECK(f.camera);
  CHECK_NEAR(f.eye.y, 1.6 + 1.7, 1e-9);  // a küszöbön állva
  CHECK_NEAR(f.yaw, path().outward_yaw, 1e-12);
  // Amíg a váltás nem történt meg, sötét marad.
  f = a.step(0.5);
  CHECK(!f.switch_now);
  CHECK_NEAR(f.blackout, 1.0, 1e-12);
}

TEST(exit_steps_down_after_the_switch_and_ends_on_the_ground) {
  fw::DoorAnimation a;
  a.start_exit(path(), 0.3, timing());
  run_until_switch(a);
  a.switched();
  auto f = a.step(0.125);
  CHECK_NEAR(f.blackout, 0.5, 1e-9);
  double last_height = 10;
  int flats = 0;
  bool done = false;
  for (int i = 0; i < 400 && !done; ++i) {
    f = a.step(0.01);
    CHECK(f.eye.y <= last_height + 1e-9);
    if (std::fabs(f.eye.y - last_height) < 1e-9) ++flats;
    last_height = f.eye.y;
    done = f.done;
  }
  CHECK(done);
  CHECK(!f.failed);
  CHECK(flats > 10);  // lépcsős: minden fok második felében áll
  CHECK_NEAR(f.eye.x, -0.8, 1e-9);
  CHECK_NEAR(f.eye.y, 0.0 + 1.7, 1e-9);
  CHECK_NEAR(f.yaw, 0.3, 1e-9);
  CHECK_NEAR(f.blackout, 0.0, 1e-12);
  CHECK(!a.active());
}

TEST(exit_fails_after_the_switch_timeout) {
  fw::DoorAnimation a;
  a.start_exit(path(), 0.3, timing());
  run_until_switch(a);
  fw::DoorFrame f;
  for (int i = 0; i < 400 && !f.done; ++i) f = a.step(0.01);
  CHECK(f.done);
  CHECK(f.failed);
  CHECK_NEAR(f.blackout, 0.0, 1e-12);
}

TEST(near_enter_walks_to_the_steps_climbs_and_switches_in_darkness) {
  fw::DoorAnimation a;
  a.start_enter(path(), {-2.5, 0.0, 1.0}, 2.0, true, timing());
  auto f = a.step(0.2);  // a lépcső aljához megy
  CHECK(f.camera);
  CHECK_NEAR(f.blackout, 0.0, 1e-12);
  f = run_until_switch(a);
  CHECK(f.switch_now);
  CHECK_NEAR(f.blackout, 1.0, 1e-12);
  CHECK_NEAR(f.eye.y, 1.6 + 1.7, 1e-9);  // a küszöbön
  CHECK_NEAR(f.yaw, path().door_yaw, 1e-9);
  a.switched();
  f = a.step(0.01);
  CHECK(!f.camera);
  for (int i = 0; i < 100 && !f.done; ++i) f = a.step(0.01);
  CHECK(f.done && !f.failed);
  CHECK_NEAR(f.blackout, 0.0, 1e-12);
}

TEST(far_enter_only_blinks) {
  fw::DoorAnimation a;
  a.start_enter(path(), {-30, 0, 0}, 0.0, false, timing());
  auto f = a.step(0.01);
  CHECK(f.blackout > 0.0);
  f = run_until_switch(a);
  CHECK(f.switch_now);
  CHECK_NEAR(f.eye.x, -30, 1e-9);  // messziről a gyalogló helyén marad
  a.switched();
  for (int i = 0; i < 100 && !f.done; ++i) f = a.step(0.01);
  CHECK(f.done && !f.failed);
}

TEST(zero_fade_time_is_immediate) {
  fw::DoorTiming t;
  t.fade = 0;
  fw::DoorAnimation a;
  a.start_exit(path(), 0.3, t);
  const auto f = a.step(0.01);
  CHECK(f.switch_now);
  CHECK_NEAR(f.blackout, 1.0, 1e-12);
}

TEST(zero_dt_pauses_the_animation) {
  fw::DoorAnimation a;
  a.start_exit(path(), 0.3, timing());
  const auto f1 = a.step(0.1);
  const auto f2 = a.step(0.0);
  CHECK_NEAR(f1.blackout, f2.blackout, 1e-12);
}

TEST(cancel_clears_the_blackout) {
  fw::DoorAnimation a;
  a.start_exit(path(), 0.3, timing());
  a.step(0.2);
  a.cancel();
  CHECK(!a.active());
  CHECK_NEAR(a.step(0.01).blackout, 0.0, 1e-12);
}
```

CMake: `src/core/door_animation.cpp`, `tests/test_door_animation.cpp`. Run - Expected: fordítási hiba.

- [ ] **Step 2: Megvalósítás** (`src/core/door_animation.h/.cpp`)

```cpp
// door_animation.h
#pragma once
#include "core/door_path.h"
#include "core/vec.h"

namespace fw {

struct DoorTiming {
  double fade = 0.25;           // s, az elsötétülés és a kivilágosodás
  double steps = 1.3;           // s, a lépcsőzés
  double approach = 0.4;        // s, beszálláskor a lépcső aljához állás
  double switch_timeout = 3.0;  // s, ennyit várunk a kameraváltásra
  double eye_height = 1.7;      // m, a szem a láb fölött
};

// Egy képkocka: a szabad kamerát írni kell-e, és hova (szem, yaw); a sötétítés (0..1); most kell-e váltani (teljes sötétség);
// véget ért-e, és ha igen, meghiúsult-e (a váltás nem történt meg).
struct DoorFrame {
  bool camera = false;
  Vec3 eye;
  double yaw = 0;
  double blackout = 0;
  bool switch_now = false;
  bool done = false;
  bool failed = false;
};

class DoorAnimation {
 public:
  void start_exit(const DoorPath& path, double final_yaw, const DoorTiming& t);
  void start_enter(const DoorPath& path, Vec3 walker_feet, double walker_yaw, bool near, const DoorTiming& t);
  DoorFrame step(double dt);
  // A hívó jelzi, hogy a kameraváltás megtörtént.
  void switched() { switched_ = true; }
  void cancel() { stage_ = Stage::Idle; }
  bool active() const { return stage_ != Stage::Idle; }
  bool entering() const { return entering_; }

 private:
  enum class Stage { Idle, Approach, StepsUp, FadeOut, WaitSwitch, FadeIn, StepsDown };
  void enter_stage(Stage s);
  Vec3 stairs(double t, bool down) const;  // a szem helye a lépcsőn, t = 0..1 (le: a küszöbtől a talajig)

  Stage stage_ = Stage::Idle;
  bool entering_ = false;
  bool near_ = false;
  bool switched_ = false;
  bool failed_ = false;
  bool asked_ = false;
  double time_ = 0;  // a szakaszban eltelt idő
  DoorPath path_;
  DoorTiming timing_;
  double final_yaw_ = 0;
  Vec3 start_eye_;
  double start_yaw_ = 0;
};

}  // namespace fw
```

```cpp
// door_animation.cpp
#include "core/door_animation.h"
#include <algorithm>
#include <cmath>

namespace fw {
namespace {

constexpr double kPi = 3.14159265358979323846;

double smooth(double t) { return t * t * (3 - 2 * t); }

Vec3 lerp(Vec3 a, Vec3 b, double t) { return a + (b - a) * t; }

// A legrövidebb úton két irányszög között.
double lerp_angle(double a, double b, double t) {
  double d = std::fmod(b - a, 2 * kPi);
  if (d > kPi) d -= 2 * kPi;
  if (d < -kPi) d += 2 * kPi;
  return a + d * t;
}

}  // namespace

void DoorAnimation::start_exit(const DoorPath& path, double final_yaw, const DoorTiming& t) {
  path_ = path;
  timing_ = t;
  final_yaw_ = final_yaw;
  entering_ = false;
  enter_stage(Stage::FadeOut);
}

void DoorAnimation::start_enter(const DoorPath& path, Vec3 walker_feet, double walker_yaw, bool near, const DoorTiming& t) {
  path_ = path;
  timing_ = t;
  entering_ = true;
  near_ = near;
  start_eye_ = walker_feet + Vec3{0, t.eye_height, 0};
  start_yaw_ = walker_yaw;
  enter_stage(near ? Stage::Approach : Stage::FadeOut);
}

void DoorAnimation::enter_stage(Stage s) {
  stage_ = s;
  time_ = 0;
  if (s == Stage::FadeOut) {
    switched_ = false;
    failed_ = false;
    asked_ = false;
  }
}

Vec3 DoorAnimation::stairs(double t, bool down) const {
  const double u = down ? t : 1 - t;  // u = 0 a küszöbön, 1 a talajon
  const Vec3 flat = lerp(path_.sill, path_.ground, u);
  double height = path_.ground.y + (path_.sill.y - path_.ground.y) * (1 - u);
  if (path_.steps > 0) {
    // Fokonként: az idő első felében ereszkedik (felfelé: emelkedik), a második felében áll.
    const double n = path_.steps;
    const double k = std::min(std::floor(u * n), n - 1);
    const double within = std::clamp(u * n - k, 0.0, 1.0);
    const double step = (path_.sill.y - path_.ground.y) / n;
    height = path_.sill.y - step * (k + smooth(std::min(1.0, within * 2)));
  }
  return {flat.x, height + timing_.eye_height, flat.z};
}

DoorFrame DoorAnimation::step(double dt) {
  DoorFrame f;
  if (stage_ == Stage::Idle) return f;
  time_ += dt;
  const double fade = timing_.fade;
  const auto progress = [&](double length) { return length <= 0 ? 1.0 : std::clamp(time_ / length, 0.0, 1.0); };
  const Vec3 sill_eye = path_.sill + Vec3{0, timing_.eye_height, 0};
  const Vec3 ground_eye = path_.ground + Vec3{0, timing_.eye_height, 0};

  switch (stage_) {
    case Stage::Approach: {
      const double p = smooth(progress(timing_.approach));
      f.camera = true;
      f.eye = lerp(start_eye_, ground_eye, p);
      f.yaw = lerp_angle(start_yaw_, path_.door_yaw, p);
      if (progress(timing_.approach) >= 1) enter_stage(Stage::StepsUp);
      return f;
    }
    case Stage::StepsUp: {
      const double p = progress(timing_.steps);
      f.camera = true;
      f.eye = stairs(p, false);
      f.yaw = path_.door_yaw;
      if (p >= 1) enter_stage(Stage::FadeOut);
      return f;
    }
    case Stage::FadeOut: {
      const double p = progress(fade);
      f.blackout = p;
      f.camera = entering_;
      f.eye = entering_ && !near_ ? start_eye_ : sill_eye;
      f.yaw = !entering_ ? path_.outward_yaw : near_ ? path_.door_yaw : start_yaw_;
      if (p >= 1) {
        enter_stage(Stage::WaitSwitch);
        return step(0.0);
      }
      return f;
    }
    case Stage::WaitSwitch: {
      f.blackout = 1;
      f.eye = entering_ && !near_ ? start_eye_ : sill_eye;
      f.yaw = !entering_ ? path_.outward_yaw : near_ ? path_.door_yaw : start_yaw_;
      f.camera = !entering_ || !switched_;
      if (!asked_) {
        asked_ = true;
        f.switch_now = true;
        f.camera = true;
        return f;
      }
      if (switched_) {
        enter_stage(Stage::FadeIn);
        return step(dt);
      }
      if (time_ > timing_.switch_timeout) {
        failed_ = true;
        enter_stage(Stage::FadeIn);
        return step(0.0);
      }
      return f;
    }
    case Stage::FadeIn: {
      const double p = progress(fade);
      f.blackout = 1 - p;
      f.camera = !entering_ && !failed_;
      f.eye = sill_eye;
      f.yaw = path_.outward_yaw;
      if (p >= 1) {
        if (entering_ || failed_) {
          stage_ = Stage::Idle;
          f.done = true;
          f.failed = failed_;
          f.blackout = 0;
          return f;
        }
        enter_stage(Stage::StepsDown);
      }
      return f;
    }
    case Stage::StepsDown: {
      const double p = progress(timing_.steps);
      f.camera = true;
      f.eye = stairs(p, true);
      // Az utolsó fok alatt a végső irányba fordul.
      const double last = path_.steps > 0 ? 1.0 - 1.0 / path_.steps : 0.0;
      const double turn = p <= last ? 0.0 : (p - last) / (1.0 - last);
      f.yaw = lerp_angle(path_.outward_yaw, final_yaw_, smooth(std::clamp(turn, 0.0, 1.0)));
      if (p >= 1) {
        stage_ = Stage::Idle;
        f.eye = ground_eye;
        f.yaw = final_yaw_;
        f.done = true;
      }
      return f;
    }
    case Stage::Idle: break;
  }
  return f;
}

}  // namespace fw
```

Run - Expected: minden zöld.

- [ ] **Step 3: Commit** - `git commit -m "Animate leaving and entering the cab: blink, switch in darkness, steps"`

---

### Task 3: Beállítások

**Files:** Modify `src/core/config.h`, `src/core/settings.cpp`, `FreeWalk.ini`, `tests/test_config.cpp`, `tests/test_settings.cpp`

**Interfaces:**
- Produces: `Config::door_animation` (bool, true), `door_step_time` 1.3, `door_fade_time` 0.25, `door_out` 0.7, `door_sill_below_head` 1.0, `door_min_free` 0.8, `enter_near_door` 3.0; `DoorSettings door_settings(const Config&)`, `DoorTiming door_timing(const Config&)` (a `door_animation.h`-ban, a `config.h`-t használva).

- [ ] **Step 1: Piros teszt** (`tests/test_config.cpp` végére)

```cpp
TEST(door_settings_are_read) {
  std::vector<std::string> warnings;
  const fw::Config c = fw::parse_config(
      "[walker]\ndoor_animation = 0\ndoor_step_time = 2\ndoor_fade_time = 0.5\ndoor_out = 0.9\ndoor_sill_below_head = 1.2\n"
      "door_min_free = 1\nenter_near_door = 5\n",
      warnings);
  CHECK(warnings.empty());
  CHECK(!c.door_animation);
  CHECK_NEAR(c.door_step_time, 2, 1e-12);
  CHECK_NEAR(c.door_fade_time, 0.5, 1e-12);
  CHECK_NEAR(c.door_out, 0.9, 1e-12);
  CHECK_NEAR(c.door_sill_below_head, 1.2, 1e-12);
  CHECK_NEAR(c.door_min_free, 1, 1e-12);
  CHECK_NEAR(c.enter_near_door, 5, 1e-12);
}
```

Run - Expected: FAIL (ismeretlen kulcsok).

- [ ] **Step 2:** a `Config` mezői (a `[walker]` blokkba, a `ground_offset` után) a fenti alapértékekkel; a beállítástábla a `ground_offset` sora után:

```cpp
      flag_setting("walker", "door_animation", "Leaving the truck", "Animated door (blink, steps)",
                   "1 = pislogás az ajtónál és lépcsőzés ki- és beszálláskor, 0 = azonnali", &Config::door_animation),
      number_setting("walker", "door_step_time", "Leaving the truck", "Time on the steps (s)", "s, a lépcsőzés ideje",
                     &Config::door_step_time, 0.3, 4, 0.1, 1),
      number_setting("walker", "door_fade_time", "Leaving the truck", "Blink fade (s)", "s, a pislogás elsötétülése és kivilágosodása",
                     &Config::door_fade_time, 0, 1, 0.05, 2),
      number_setting("walker", "door_out", "Leaving the truck", "Door sill beside the head (m)", "m, a küszöb a fejtől oldalra",
                     &Config::door_out, 0.3, 1.5, 0.05, 2),
      number_setting("walker", "door_sill_below_head", "Leaving the truck", "Door sill below the head (m)", "m, a küszöb a fej alatt",
                     &Config::door_sill_below_head, 0.3, 2, 0.05, 2),
      number_setting("walker", "door_min_free", "Leaving the truck", "Free space needed at the door (m)",
                     "m, ennyi szabad hely kell az ajtó előtt (különben a másik ajtó)", &Config::door_min_free, 0, 2, 0.1, 1),
      number_setting("walker", "enter_near_door", "Leaving the truck", "Full animation within (m) of the door",
                     "m, ezen belül a teljes beszállás-animáció, messzebbről pislogás", &Config::enter_near_door, 0, 10, 0.5, 1),
```

A sémaszám 44 → 51. A `door_settings`/`door_timing` (a `door_animation.cpp`-ben):

```cpp
DoorSettings door_settings(const Config& c) { return {c.door_out, c.door_sill_below_head}; }
DoorTiming door_timing(const Config& c) {
  DoorTiming t;
  t.fade = c.door_fade_time;
  t.steps = c.door_step_time;
  t.eye_height = c.eye_height;
  return t;
}
```

(a deklarációk a `door_animation.h`-ba, `#include "core/config.h"`).

- [ ] **Step 3:** az INI a formázó szerint (a hét új sor a `[walker]`-ben a `ground_offset` után); Run - Expected: minden zöld.
- [ ] **Step 4: Commit** - `git commit -m "Add the door animation settings"`

---

### Task 4: A sötétítő ablak

**Files:** Create `src/win/fade_window.h/.cpp`; Modify `CMakeLists.txt`

**Interfaces:**
- Produces: `bool fade_window_start(); void fade_window_set(double blackout); void fade_window_stop();`

Csak a játékban tesztelhető; a kapu a figyelmeztetésmentes fordítás.

- [ ] **Step 1:** `src/win/fade_window.h`:

```cpp
#pragma once

namespace fw {

// Fekete, átlátszó, kattintást átengedő, fókuszt nem vevő ablak a játékablak fölött, saját üzenetszálon; a ki- és beszállás pislogásához.
// Teljes képernyős (exkluzív) módban nem látszik.
bool fade_window_start();
// 0 = rejtett, 1 = teljesen fekete; bármelyik szálról hívható (üzenetet küld az ablak szálának).
void fade_window_set(double blackout);
void fade_window_stop();

}  // namespace fw
```

- [ ] **Step 2:** `src/win/fade_window.cpp`:

```cpp
#include "win/fade_window.h"
#include <windows.h>
#include <algorithm>
#include <atomic>

namespace fw {
namespace {

constexpr UINT kSetAlpha = WM_APP + 1;
HANDLE g_thread = nullptr;
DWORD g_thread_id = 0;
std::atomic<HWND> g_window{nullptr};
std::atomic<int> g_last_alpha{-1};

// A játék fő ablaka: a folyamat legnagyobb látható felső szintű ablaka (a sötétítő ablakon kívül).
HWND game_window() {
  struct Search {
    HWND best = nullptr;
    long area = 0;
  } s;
  EnumWindows(
      [](HWND w, LPARAM p) -> BOOL {
        auto* s = reinterpret_cast<Search*>(p);
        DWORD pid = 0;
        GetWindowThreadProcessId(w, &pid);
        if (pid != GetCurrentProcessId() || w == g_window.load() || !IsWindowVisible(w) || GetWindow(w, GW_OWNER)) return TRUE;
        RECT r{};
        GetClientRect(w, &r);
        const long area = (r.right - r.left) * (r.bottom - r.top);
        if (area > s->area) {
          s->area = area;
          s->best = w;
        }
        return TRUE;
      },
      reinterpret_cast<LPARAM>(&s));
  return s.best;
}

LRESULT CALLBACK window_proc(HWND w, UINT message, WPARAM wp, LPARAM lp) {
  if (message == kSetAlpha) {
    const int alpha = static_cast<int>(wp);
    if (alpha <= 0) {
      ShowWindow(w, SW_HIDE);
      return 0;
    }
    if (HWND game = game_window()) {
      RECT r{};
      GetClientRect(game, &r);
      POINT origin{0, 0};
      ClientToScreen(game, &origin);
      SetWindowPos(w, HWND_TOPMOST, origin.x, origin.y, r.right - r.left, r.bottom - r.top, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }
    SetLayeredWindowAttributes(w, 0, static_cast<BYTE>(std::min(alpha, 255)), LWA_ALPHA);
    return 0;
  }
  if (message == WM_PAINT) {
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(w, &ps);
    FillRect(dc, &ps.rcPaint, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
    EndPaint(w, &ps);
    return 0;
  }
  return DefWindowProcW(w, message, wp, lp);
}

DWORD WINAPI fade_thread(LPVOID started) {
  WNDCLASSW wc{};
  wc.lpfnWndProc = window_proc;
  wc.hInstance = GetModuleHandleW(nullptr);
  wc.lpszClassName = L"FreeWalkFade";
  wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
  RegisterClassW(&wc);
  HWND w = CreateWindowExW(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW, wc.lpszClassName, L"",
                           WS_POPUP, 0, 0, 1, 1, nullptr, nullptr, wc.hInstance, nullptr);
  g_window = w;
  *static_cast<std::atomic<bool>*>(started) = w != nullptr;
  if (!w) return 1;
  MSG m;
  while (GetMessageW(&m, nullptr, 0, 0) > 0) DispatchMessageW(&m);
  DestroyWindow(w);
  g_window = nullptr;
  UnregisterClassW(wc.lpszClassName, wc.hInstance);
  return 0;
}

}  // namespace

bool fade_window_start() {
  if (g_thread) return g_window.load() != nullptr;
  static std::atomic<bool> started{false};
  started = false;
  g_thread = CreateThread(nullptr, 0, fade_thread, &started, 0, &g_thread_id);
  if (!g_thread) return false;
  for (int i = 0; i < 200 && !started && WaitForSingleObject(g_thread, 10) == WAIT_TIMEOUT; ++i) {
  }
  return started;
}

void fade_window_set(double blackout) {
  HWND w = g_window.load();
  if (!w) return;
  const int alpha = static_cast<int>(std::clamp(blackout, 0.0, 1.0) * 255.0 + 0.5);
  if (alpha == g_last_alpha.exchange(alpha)) return;
  PostMessageW(w, kSetAlpha, static_cast<WPARAM>(alpha), 0);
}

void fade_window_stop() {
  if (!g_thread) return;
  PostThreadMessageW(g_thread_id, WM_QUIT, 0, 0);
  WaitForSingleObject(g_thread, 2000);
  CloseHandle(g_thread);
  g_thread = nullptr;
  g_thread_id = 0;
  g_last_alpha = -1;
}

}  // namespace fw
```

CMake: `src/win/fade_window.cpp`. Run - Expected: figyelmeztetésmentes fordítás.

- [ ] **Step 3: Commit** - `git commit -m "Add a black overlay window for the door blink"`

---

### Task 5: Bekötés a pluginba

**Files:** Modify `src/win/plugin.cpp`, `README.md`

**Interfaces:**
- Consumes: `door_path`, `choose_door`, `DoorAnimation`, `door_settings`, `door_timing` (1-3.), `fade_window_*` (4.), `exit_point_on`, `exit_yaw_on`, `driver_side`, a fizika `World` (`sweep_capsule`, `ground_below`).

A menet:
- A `State`-be: `fw::DoorAnimation door; int door_side = 0; std::string door_reason; bool door_switching = false;` (a `door_switching` alatt a `return_to_truck` nem szakítja meg az animációt).
- A `scs_telemetry_init`-ben a hook után `fw::fade_window_start()` (hibánál egy figyelmeztető sor); a shutdownban `fw::fade_window_set(0); fw::fade_window_stop();`.
- **`RequestFreeCamera`** (`door_animation` igaz): a két ajtó előtti szabad hely mérése, ha a fizika kész (`g->physx.world()`): egy vékony kapszula (sugár 0,1, félmagasság 0,6, középpont a talaj + 1,1 m magasságban) a küszöbpont vízszintes helyéről kifelé (`forward_from_yaw(outward_yaw)`) 1,5 m-re söpör; a találat távolsága a szabad hely (átfedésnél 0, találat nélkül 1,5). `choose_door` → ha `None`: `g->session.force_return(...)` és naplósor ("not leaving the truck: both doors are squeezed (driver %.2f m, passenger %.2f m)"); különben a talajmagasság (a fizika `start_height`-je a kiszállási ponton, ha kész, különben `truck.y + ground_offset`), `door_path`, `door.start_exit(path, exit_yaw_on(truck, cfg, side), door_timing(cfg))`, `door_side = side`, naplósor (ajtó, szabad helyek, küszöb, fokok). A `begin_exit` itt még NEM fut.
  `door_animation` hamis: a mostani `begin_exit`.
- **Képkockánként** (a session után), ha `door.active()`: `f = door.step(g->paused ? 0.0 : dt)`; `fade_window_set(f.blackout)`;
  - kiszállásnál `f.switch_now`: a `begin_exit` a kamera helyét `to_camera(f.eye)`-ra írja (a `walker.reset` helyett), a többi beállítás a mostani;
  - a session `StartWalk` parancsa animáció közben: a `start_walk` beállító része (elrendezés igazolva, pitch 0, egérérzékenység), de a gyalogló még nem indul; `door.switched()`;
  - `f.camera`: a szabad kamera helye `to_camera(f.eye)`, iránya `heading_to_camera_yaw(f.yaw)`, pitch 0;
  - kiszállás vége (`f.done`, nem `failed`): `start_walk` a talajponttal és a végső iránnyal (`SessionOutput{Command::StartWalk, ..., path.ground, final_yaw}`); `failed`: `return_to_truck(true, "the game did not switch to the free camera")`;
  - beszállásnál `f.switch_now`: `door_switching = true; return_to_truck(true, door_reason); door_switching = false; door.switched();`.
- **`Return`** (a session parancsa: billentyű, túl messze, a kamion elmozdult), `door_animation` igaz és nincs animáció: a lámpa ki (`flashlight_off`), `door_reason = out.reason`, a `door_path` a kiszállás oldalán (`door_side`) a gyalogló talajmagasságával; közeli, ha a billentyű volt az ok és a gyalogló a küszöbtől vízszintesen `enter_near_door`-on belül van; `door.start_enter(path, walker.feet(), yaw, near, door_timing(cfg))`. A `have_eye = false` (a `write_eye` nem ír), a kamerát az animáció írja.
- **`ReturnKeepCamera`** és minden hibából eredő visszaszállás: azonnali (a mostani).
- A `return_to_truck` elején: `if (!g->door_switching) { g->door.cancel(); fw::fade_window_set(0); }`.
- Animáció közben: a `walk()` nem fut, a session `toggle_pressed` hamis, a lámpa nem kapcsolható (a `flashlight_step` nem fut, mert a `walk()` sem).
- `kVersion` = `"0.7.0"`; README: a Használat szakaszban egy mondat a pislogásról és a lépcsőről, és hogy teljes képernyős módban a pislogás nem látszik.

- [ ] **Step 1:** a fenti bekötés; Run - Expected: figyelmeztetésmentes fordítás, minden teszt zöld.
- [ ] **Step 2: Commit, élő teszt 1-4** a spec szerint (`dist/FreeWalk-0.7.0-door/TESZT.md`).

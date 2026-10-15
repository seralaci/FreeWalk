# Zseblámpa - megvalósítási terv

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Séta közben zseblámpa: a nézés irányába mutató fénykúp a gyalogló kezéből, állítható mérettel, halványítással és villogással; éjjel kölcsönzött utcai lámpával, ha nincs ilyen, saját fénnyel.

**Architecture:** A mag (`src/core`) számolja a lámpa állapotát és a fény paramétereit (`flashlight`, `flicker`), és választja ki a kölcsönzendő lámpát (`light_borrow`); tesztelt, játékfüggetlen kód. A Windows-réteg (`src/win/game_lights`) olvassa a scene fénytömbjét, írja a fény mezőit, kölcsönöz és visszaad, illetve saját fényt hoz létre és bont le; az `input` egér-hookja adja a görgőt.

**Tech Stack:** C++20, MSVC, CMake + Ninja, `build.cmd`; Windows API (WH_MOUSE_LL, SEH a `guarded_call`-lal); a játék saját fényobjektumai (`light_source`).

**Spec:** `docs/superpowers/specs/2026-10-14-flashlight-design.md`

## Global Constraints

- A TM Real Walk kódjából semmit nem másolunk; a játéktényeket saját elemzéssel és élő teszttel igazoljuk.
- A játék kódját és vtable-jeit nem módosítjuk; adatot írunk (a kölcsönzött lámpát visszaírjuk), és a játék saját függvényeit hívjuk (konstruktor, destruktor), kivételvédetten.
- Naplószöveg angol, ASCII, `[FreeWalk] ` előtag (a `fw::log_*` adja); kódkomment magyar; commit üzenet angol, `Co-Authored-By` nélkül.
- A játék belső értékei csak a `game_layout.h`-ban, az ellenőrzés helyével.
- MSVC `/W4` figyelmeztetésmentes; minden tesztcsomagnál `kVersion` léptetés (ez a terv a `0.6.x` sorozat).
- Billentyű-alapértékek: `flashlight = F`, `flashlight_wider = 0x6B`, `flashlight_narrower = 0x6D` (mindhárom NONE-ra kapcsolható); a görgő séta közben: fel szélesebb, le szűkebb.
- A tesztek: `sh .superpowers/sdd/2026-10-14-flashlight/b.sh` (a `build.cmd`-t futtatja, a kimenet a `build.log`-ban).
- Minden eltérés a tervtől `Ruling:` sorként a végrehajtási naplóba (`.superpowers/sdd/2026-10-14-flashlight/progress.md`).

## Review Focus

- A lámpa ég, és a játékos visszaszáll, szünetet tart, menübe megy, vagy mentést tölt be: a fény kialszik, a kölcsönzött utcai lámpa visszakapja az eredeti helyét és értékeit (új világnál nem írunk a régi címekre) (7. feladat: kialvás-ágak; 4. feladat: `switch_off_is_immediate`).
- A görgőt a játék is látja (DirectInput), és séta közben megváltoztatja a szabad kamera sebességét: a gyalogló nem sodródhat el (7. feladat: a sebesség-lefagyasztás ellenőrzése az élő tesztben, 6. feladat: a hook csak séta közben nyel).
- A kölcsönzött lámpa eltűnik a fénytömbből (a játék kiürít egy szektort): nem írunk felszabadult memóriára, új fényt keresünk (7. feladat: `light_alive`).
- A beállítóablakban a lámpa billentyűje egy séta-billentyűvel ütközik: piros sor, a Save tiltva (1. feladat: `flashlight_keys_join_the_conflict_check`).
- `fade_time = 0` vagy `flicker_chance` szélső értékei: nincs osztás nullával, a lámpa azonnal kapcsol (3. és 4. feladat tesztjei).

---

## Fájlszerkezet

| Fájl | Felelősség |
|---|---|
| `src/core/config.h`, `config.cpp`, `settings.cpp`, `FreeWalk.ini` | három új billentyű, `[light]` rész, `NUMPAD+`/`NUMPAD-` billentyűnevek |
| `src/core/flicker.h/.cpp` (új) | a villogás ütemezője |
| `src/core/flashlight.h/.cpp` (új) | a lámpa állapota és a fény kimenete |
| `src/core/light_borrow.h/.cpp` (új) | a kölcsönzendő lámpa kiválasztása |
| `src/core/game_layout.h` | a fénytömb, a fény mezői, a konzolváltozók |
| `src/win/input.h/.cpp` | egér-hook a görgőhöz |
| `src/win/game_lights.h/.cpp` (új) | a játék fényei: tömb, mezők, kölcsönzés, saját fény, diagnosztika |
| `src/win/plugin.cpp`, `src/win/settings_window.cpp` | bekötés, Light fül megjegyzése |
| `tests/test_flicker.cpp`, `test_flashlight.cpp`, `test_light_borrow.cpp` (új); `test_config.cpp`, `test_settings.cpp` | tesztek |

---

### Task 1: Beállítások (billentyűk, `[light]`)

**Files:** Modify `src/core/config.h`, `src/core/config.cpp`, `src/core/settings.cpp`, `src/win/settings_window.cpp`, `FreeWalk.ini`, `tests/test_config.cpp`, `tests/test_settings.cpp`

**Interfaces:**
- Produces: `Config::key_flashlight` (int, `'F'`), `key_flashlight_wider` (0x6B), `key_flashlight_narrower` (0x6D); `light_angle` 28, `light_inner_angle` 12, `light_range` 40, `light_brightness` 0.3, `light_r` 1.0, `light_g` 0.96, `light_b` 0.88, `light_fade_time` 0.18, `light_hand_right` 0.5, `light_hand_down` 0.21, `light_flicker_chance` 0, `light_own` (bool, true), `light_borrow_min` 300, `light_borrow_max` 900 (double, kivéve ahol jelölve).

- [ ] **Step 1: Piros tesztek** (`tests/test_config.cpp` végére)

```cpp
TEST(numpad_plus_and_minus_have_names) {
  CHECK(fw::parse_key("NUMPAD+") == 0x6B);
  CHECK(fw::parse_key("numpad-") == 0x6D);
  CHECK(fw::key_name(0x6B) == "NUMPAD+");
  CHECK(fw::key_name(0x6D) == "NUMPAD-");
}

TEST(flashlight_settings_are_read) {
  std::vector<std::string> warnings;
  const fw::Config c = fw::parse_config(
      "[keys]\nflashlight = L\nflashlight_wider = NONE\nflashlight_narrower = G\n"
      "[light]\nangle = 40\ninner_angle = 20\nrange = 60\nbrightness = 1.5\nr = 0.5\ng = 0.6\nb = 0.7\nfade_time = 0\n"
      "hand_right = -0.3\nhand_down = 0.4\nflicker_chance = 12\nown_light = 0\nborrow_min_distance = 100\nborrow_max_distance = 2000\n",
      warnings);
  CHECK(warnings.empty());
  CHECK(c.key_flashlight == 'L');
  CHECK(c.key_flashlight_wider == 0);
  CHECK(c.key_flashlight_narrower == 'G');
  CHECK_NEAR(c.light_angle, 40, 1e-12);
  CHECK_NEAR(c.light_inner_angle, 20, 1e-12);
  CHECK_NEAR(c.light_range, 60, 1e-12);
  CHECK_NEAR(c.light_brightness, 1.5, 1e-12);
  CHECK_NEAR(c.light_r, 0.5, 1e-12);
  CHECK_NEAR(c.light_g, 0.6, 1e-12);
  CHECK_NEAR(c.light_b, 0.7, 1e-12);
  CHECK_NEAR(c.light_fade_time, 0, 1e-12);
  CHECK_NEAR(c.light_hand_right, -0.3, 1e-12);
  CHECK_NEAR(c.light_hand_down, 0.4, 1e-12);
  CHECK_NEAR(c.light_flicker_chance, 12, 1e-12);
  CHECK(!c.light_own);
  CHECK_NEAR(c.light_borrow_min, 100, 1e-12);
  CHECK_NEAR(c.light_borrow_max, 2000, 1e-12);
}
```

`tests/test_settings.cpp` végére:

```cpp
TEST(flashlight_keys_join_the_conflict_check) {
  fw::Config c;
  c.key_flashlight = 'W';
  CHECK(!fw::key_conflicts(c).empty());
  c.key_flashlight = 'F';
  c.key_flashlight_narrower = 'F';
  CHECK(!fw::key_conflicts(c).empty());
}
```

Run: `sh .superpowers/sdd/2026-10-14-flashlight/b.sh` - Expected: fordítási hiba (a mezők nincsenek).

- [ ] **Step 2: A `Config` mezői** (`src/core/config.h`): a `[keys]` blokk végére

```cpp
  int key_flashlight = 'F';            // a zseblámpa be/ki, 0 = kikapcsolva
  int key_flashlight_wider = 0x6B;     // NUMPAD+: a fénykúp szélesebb, 0 = csak a görgő
  int key_flashlight_narrower = 0x6D;  // NUMPAD-: a fénykúp szűkebb, 0 = csak a görgő
```

a `[collision]` blokk után:

```cpp
  // [light]
  double light_angle = 28;          // fok, a fénykúp külső szöge a középső fokozatban
  double light_inner_angle = 12;    // fok, a belső, éles rész
  double light_range = 40;          // m
  double light_brightness = 0.3;    // a szín szorzója
  double light_r = 1.0;
  double light_g = 0.96;
  double light_b = 0.88;
  double light_fade_time = 0.18;    // s, be- és kikapcsolási halványulás
  double light_hand_right = 0.5;    // m, a kéz a szemtől jobbra
  double light_hand_down = 0.21;    // m, a kéz a szem alatt
  double light_flicker_chance = 0;  // óránként átlagosan ennyiszer rakoncátlankodik
  bool light_own = true;            // ha nincs kölcsönözhető utcai lámpa, saját fény
  double light_borrow_min = 300;    // m, ennél közelebbi utcai lámpát nem veszünk el
  double light_borrow_max = 900;    // m, ezen belül keresünk elsőként
```

- [ ] **Step 3: Billentyűnevek** (`src/core/config.cpp`): a `named` táblába `{"NUMPAD+", 0x6B}, {"NUMPAD-", 0x6D}`; a `key_name` `switch`-ébe `case 0x6B: return "NUMPAD+";` és `case 0x6D: return "NUMPAD-";`.

- [ ] **Step 4: A beállítástábla** (`src/core/settings.cpp`): a `key_setting("run", ...)` sor után

```cpp
      key_setting("flashlight", "Flashlight on / off", "a zseblámpa be/ki (NONE = kikapcsolva)", &Config::key_flashlight, false, true),
      key_setting("flashlight_wider", "Flashlight beam wider", "a fénykúp szélesebb (a görgő felfelé is)", &Config::key_flashlight_wider,
                  false, true),
      key_setting("flashlight_narrower", "Flashlight beam narrower", "a fénykúp szűkebb (a görgő lefelé is)",
                  &Config::key_flashlight_narrower, false, true),
```

a `[collision]` sorai után, a `[debug]` előtt:

```cpp
      number_setting("light", "angle", "Light", "Beam angle (degrees)", "fok, a fénykúp a középső fokozatban", &Config::light_angle, 5, 80, 1, 0),
      number_setting("light", "inner_angle", "Light", "Sharp inner part (degrees)", "fok, a fénykúp éles belső része",
                     &Config::light_inner_angle, 0, 80, 1, 0),
      number_setting("light", "range", "Light", "Range (m)", "m, ameddig a fény elér", &Config::light_range, 5, 200, 1, 0),
      number_setting("light", "brightness", "Light", "Brightness", "a szín szorzója", &Config::light_brightness, 0, 3, 0.05, 2),
      number_setting("light", "r", "Light", "Colour: red", "szín, vörös", &Config::light_r, 0, 3, 0.01, 2),
      number_setting("light", "g", "Light", "Colour: green", "szín, zöld", &Config::light_g, 0, 3, 0.01, 2),
      number_setting("light", "b", "Light", "Colour: blue", "szín, kék (kevesebb = sárgább)", &Config::light_b, 0, 3, 0.01, 2),
      number_setting("light", "fade_time", "Light", "Fade in / out (s)", "s, be- és kikapcsolási halványulás", &Config::light_fade_time, 0, 2,
                     0.01, 2),
      number_setting("light", "hand_right", "Light", "Hand: right of the eye (m)", "m, a kéz a szemtől jobbra", &Config::light_hand_right, -1,
                     1, 0.01, 2),
      number_setting("light", "hand_down", "Light", "Hand: below the eye (m)", "m, a kéz a szem alatt", &Config::light_hand_down, -1, 1, 0.01,
                     2),
      number_setting("light", "flicker_chance", "Light", "Bulb plays up (per hour)", "óránként átlagosan ennyiszer rakoncátlankodik (0 = soha)",
                     &Config::light_flicker_chance, 0, 60, 1, 0),
      flag_setting("light", "own_light", "Light", "Own light where no street lamp can be borrowed",
                   "1 = ha nincs kölcsönözhető utcai lámpa, saját fény", &Config::light_own),
      number_setting("light", "borrow_min_distance", "Light", "Borrow street lamps farther than (m)",
                     "m, ennél közelebbi utcai lámpát nem veszünk el", &Config::light_borrow_min, 0, 2000, 10, 0),
      number_setting("light", "borrow_max_distance", "Light", "Look for street lamps within (m)", "m, ezen belül keresünk elsőként",
                     &Config::light_borrow_max, 100, 5000, 10, 0),
```

A `tests/test_settings.cpp` sémaszáma 27 → 44 (3 billentyű + 14 lámpa-beállítás).
A `src/win/settings_window.cpp` `tab_note`-jába a `Debug` elé:

```cpp
  if (tab == "Light")
    return "The flashlight borrows a distant street lamp at night, or makes a light of its own. Changes show at once while it is on.";
```

- [ ] **Step 5: Futtatás, majd az INI**

Run: a teszt-szkript - Expected: a `shipped_ini_matches_the_formatter` FAIL (minden más zöld). A `FreeWalk.ini`-t a formázó kimenete szerint frissítsd (a `[keys]` három új sora, az új `[light]` rész a `[collision]` után); Run újra - Expected: minden zöld.

- [ ] **Step 6: Commit** - `git commit -m "Add the flashlight keys and the [light] settings"` (a módosított fájlokkal).

---

### Task 2: A kölcsönzendő lámpa kiválasztása

**Files:** Create `src/core/light_borrow.h/.cpp`, `tests/test_light_borrow.cpp`; Modify `CMakeLists.txt`

**Interfaces:**
- Produces: `struct LightCandidate { std::uint64_t id = 0; Vec3 position; bool active = false; };`,
  `std::optional<std::uint64_t> choose_light(Vec3 walker, const std::vector<LightCandidate>& lights, double min_distance, double max_distance);`

- [ ] **Step 1: Piros tesztek** (`tests/test_light_borrow.cpp`)

```cpp
#include <vector>
#include "check.h"
#include "core/light_borrow.h"

namespace {
fw::LightCandidate lamp(std::uint64_t id, double x, bool active = true) { return {id, {x, 5, 0}, active}; }
}  // namespace

TEST(borrow_never_takes_a_near_lamp) {
  CHECK(!fw::choose_light({0, 0, 0}, {lamp(1, 50), lamp(2, 299)}, 300, 900));
}

TEST(borrow_takes_the_nearest_within_the_window) {
  CHECK(fw::choose_light({0, 0, 0}, {lamp(1, 800), lamp(2, 400), lamp(3, 100)}, 300, 900) == 2u);
}

TEST(borrow_falls_back_to_the_nearest_far_lamp) {
  CHECK(fw::choose_light({0, 0, 0}, {lamp(1, 3000), lamp(2, 1500)}, 300, 900) == 2u);
}

TEST(borrow_skips_lamps_that_are_off) {
  CHECK(fw::choose_light({0, 0, 0}, {lamp(1, 400, false), lamp(2, 700)}, 300, 900) == 2u);
  CHECK(!fw::choose_light({0, 0, 0}, {}, 300, 900));
}
```

CMake: `src/core/light_borrow.cpp` a magba, `tests/test_light_borrow.cpp` a tesztekbe. Run - Expected: fordítási hiba.

- [ ] **Step 2: Megvalósítás**

`src/core/light_borrow.h`:

```cpp
#pragma once
#include <cstdint>
#include <optional>
#include <vector>
#include "core/vec.h"

namespace fw {

// Egy utcai lámpa a játék fénytömbjéből: azonosító (a címe), helye, és hogy ég-e.
struct LightCandidate {
  std::uint64_t id = 0;
  Vec3 position;
  bool active = false;
};

// A kölcsönzendő lámpa: legalább min_distance-re a gyaloglótól (hogy a közelben ne aludjon ki egy), ezen belül a legközelebbi
// a max_distance-en belül; ha ott nincs, a legközelebbi távolabbi. Kikapcsolt lámpát nem választ.
std::optional<std::uint64_t> choose_light(Vec3 walker, const std::vector<LightCandidate>& lights, double min_distance, double max_distance);

}  // namespace fw
```

`src/core/light_borrow.cpp`:

```cpp
#include "core/light_borrow.h"

namespace fw {

std::optional<std::uint64_t> choose_light(Vec3 walker, const std::vector<LightCandidate>& lights, double min_distance, double max_distance) {
  std::optional<std::uint64_t> best;
  double best_distance = 0;
  bool best_inside = false;
  for (const auto& l : lights) {
    if (!l.active) continue;
    const double d = horizontal_distance(walker, l.position);
    if (d < min_distance) continue;
    const bool inside = d <= max_distance;
    // A határon belüli mindig jobb a határon kívülinél; azonos csoportban a közelebbi.
    if (!best || (inside && !best_inside) || (inside == best_inside && d < best_distance)) {
      best = l.id;
      best_distance = d;
      best_inside = inside;
    }
  }
  return best;
}

}  // namespace fw
```

Run - Expected: minden zöld.

- [ ] **Step 3: Commit** - `git commit -m "Choose the street lamp to borrow: farther than the minimum, nearest within the window"`

---

### Task 3: A villogás ütemezője

**Files:** Create `src/core/flicker.h/.cpp`, `tests/test_flicker.cpp`; Modify `CMakeLists.txt`

**Interfaces:**
- Produces: `class Flicker { explicit Flicker(std::uint32_t seed); double step(double dt, double chance_per_hour); bool active() const; };`

- [ ] **Step 1: Piros tesztek** (`tests/test_flicker.cpp`)

```cpp
#include "check.h"
#include "core/flicker.h"

TEST(flicker_never_happens_at_zero_chance) {
  fw::Flicker f(7);
  for (int i = 0; i < 100000; ++i) CHECK_NEAR(f.step(0.05, 0.0), 1.0, 1e-12);
}

TEST(flicker_is_reproducible_with_a_seed) {
  fw::Flicker a(42), b(42);
  for (int i = 0; i < 20000; ++i) CHECK_NEAR(a.step(0.05, 60.0), b.step(0.05, 60.0), 1e-12);
}

TEST(flicker_stays_between_zero_and_one_and_recovers) {
  fw::Flicker f(3);
  bool seen_dark = false;
  for (int i = 0; i < 200000; ++i) {
    const double m = f.step(0.02, 60.0);
    CHECK(m >= 0.0 && m <= 1.0);
    if (m < 0.5) seen_dark = true;
  }
  CHECK(seen_dark);
  // Ha a gyakoriság 0 lesz, a futó esemény véget ér, és utána a szorzó 1 marad.
  for (int i = 0; i < 500; ++i) f.step(0.02, 0.0);
  CHECK(!f.active());
  CHECK_NEAR(f.step(0.02, 0.0), 1.0, 1e-12);
}

TEST(flicker_happens_about_as_often_as_set) {
  fw::Flicker f(11);
  int events = 0;
  bool was_active = false;
  for (int i = 0; i < 10 * 3600 * 20; ++i) {  // 10 óra, 50 ms-os lépésekkel
    f.step(0.05, 60.0);
    if (f.active() && !was_active) ++events;
    was_active = f.active();
  }
  CHECK(events > 510 && events < 690);  // 600 várható, Poisson-szórás kb. 24
}
```

CMake: `src/core/flicker.cpp`, `tests/test_flicker.cpp`. Run - Expected: fordítási hiba.

- [ ] **Step 2: Megvalósítás**

`src/core/flicker.h`:

```cpp
#pragma once
#include <cstdint>
#include <random>

namespace fw {

// A zseblámpa "rakoncátlankodása": a használat óránként átlagosan chance_per_hour eseménye, véletlen időpontokban.
// Egy esemény: 0,6-1,2 s gyors villódzás, 0,5-1 s sötét (a kéz megrázza), majd 0,3 s alatt visszaerősödés.
// A step a fényerő szorzóját adja (0..1); a véletlenszám-generátor magja a tesztekhez rögzíthető.
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
  double until_next_ = -1;  // s a következő eseményig (-1: még nincs kisorsolva)
  double phase_left_ = 0;
  double flutter_left_ = 0;
  double level_ = 1;
};

}  // namespace fw
```

`src/core/flicker.cpp`:

```cpp
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
        // Felváltva sötét és világos villanások, véletlen hosszal.
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
```

Run - Expected: minden zöld.

- [ ] **Step 3: Commit** - `git commit -m "Schedule the flashlight's random flicker events"`

---

### Task 4: A lámpa állapota és a fény kimenete

**Files:** Create `src/core/flashlight.h/.cpp`, `tests/test_flashlight.cpp`; Modify `CMakeLists.txt`

**Interfaces:**
- Consumes: `Flicker` (3.), `Config` mezők (1.).
- Produces:
  `struct LightSettings` (a `[light]` értékei), `LightSettings light_settings(const Config&)`;
  `struct FlashlightInput { bool toggle = false; int wider = 0; int narrower = 0; Vec3 eye; double yaw = 0; double pitch = 0; double dt = 0; };`
  `struct LightOutput { bool lit = false; Vec3 position; Vec3 direction; double outer_angle = 0; double inner_angle = 0; double range = 0; double r = 0, g = 0, b = 0; };`
  `class Flashlight { LightOutput step(const FlashlightInput&, const LightSettings&); void switch_off(); bool on() const; int level() const; };`
  `inline constexpr int kBeamLevels = 5; inline constexpr int kBeamMiddle = 2;`

- [ ] **Step 1: Piros tesztek** (`tests/test_flashlight.cpp`)

```cpp
#include <cmath>
#include "check.h"
#include "core/flashlight.h"

namespace {
constexpr double kPi = 3.14159265358979323846;

fw::FlashlightInput frame(bool toggle = false, double dt = 0.1) {
  fw::FlashlightInput in;
  in.toggle = toggle;
  in.eye = {0, 1.7, 0};
  in.dt = dt;
  return in;
}
}  // namespace

TEST(flashlight_fades_in_and_out) {
  fw::LightSettings s;
  s.fade_time = 0.2;
  fw::Flashlight f;
  auto out = f.step(frame(true, 0.1), s);
  CHECK(f.on() && out.lit);
  CHECK_NEAR(out.r, s.r * s.brightness * 0.5, 1e-9);
  out = f.step(frame(false, 0.1), s);
  CHECK_NEAR(out.r, s.r * s.brightness, 1e-9);
  out = f.step(frame(true, 0.1), s);  // ki: halványul, de még ég
  CHECK(!f.on() && out.lit);
  CHECK_NEAR(out.r, s.r * s.brightness * 0.5, 1e-9);
  out = f.step(frame(false, 0.1), s);
  CHECK(!out.lit);
}

TEST(zero_fade_time_switches_at_once) {
  fw::LightSettings s;
  s.fade_time = 0;
  fw::Flashlight f;
  CHECK_NEAR(f.step(frame(true), s).r, s.r * s.brightness, 1e-12);
  CHECK(!f.step(frame(true), s).lit);
}

TEST(switch_off_is_immediate) {
  fw::LightSettings s;
  fw::Flashlight f;
  f.step(frame(true), s);
  f.switch_off();
  CHECK(!f.on());
  CHECK(!f.step(frame(false), s).lit);
}

TEST(beam_levels_step_and_stop_at_the_ends) {
  fw::LightSettings s;
  s.angle = 28;
  s.inner_angle = 12;
  fw::Flashlight f;
  auto in = frame(true);
  auto out = f.step(in, s);
  CHECK(f.level() == fw::kBeamMiddle);
  CHECK_NEAR(out.outer_angle, 28, 1e-9);
  in = frame();
  in.wider = 5;
  out = f.step(in, s);
  CHECK(f.level() == fw::kBeamLevels - 1);
  CHECK_NEAR(out.outer_angle, 56, 1e-9);
  CHECK_NEAR(out.inner_angle, 24, 1e-9);
  in = frame();
  in.narrower = 9;
  out = f.step(in, s);
  CHECK(f.level() == 0);
  CHECK_NEAR(out.outer_angle, 14, 1e-9);
  // Újra bekapcsolva a középső fokozat.
  f.switch_off();
  f.step(frame(true), s);
  CHECK(f.level() == fw::kBeamMiddle);
}

TEST(beam_angle_is_clamped) {
  fw::LightSettings s;
  s.angle = 70;
  s.inner_angle = 70;
  fw::Flashlight f;
  f.step(frame(true), s);
  auto in = frame();
  in.wider = 2;
  const auto out = f.step(in, s);
  CHECK_NEAR(out.outer_angle, 80, 1e-9);
  CHECK(out.inner_angle <= out.outer_angle);
}

TEST(light_sits_in_the_hand_and_points_where_you_look) {
  fw::LightSettings s;
  s.hand_right = 0.5;
  s.hand_down = 0.21;
  fw::Flashlight f;
  auto in = frame(true);
  auto out = f.step(in, s);
  CHECK_NEAR(out.position.x, 0.5, 1e-9);
  CHECK_NEAR(out.position.y, 1.7 - 0.21, 1e-9);
  CHECK_NEAR(out.position.z, 0.0, 1e-9);
  CHECK_NEAR(out.direction.z, -1.0, 1e-9);
  in = frame();
  in.yaw = kPi / 2;  // nyugatra néz
  out = f.step(in, s);
  CHECK_NEAR(out.direction.x, -1.0, 1e-9);
  CHECK_NEAR(out.position.z, -0.5, 1e-9);
  in = frame();
  in.pitch = kPi / 2;  // felfelé néz
  out = f.step(in, s);
  CHECK_NEAR(out.direction.y, 1.0, 1e-9);
  CHECK_NEAR(out.position.y, 1.7, 1e-9);
  CHECK_NEAR(out.position.z, -0.21, 1e-9);
}
```

CMake: `src/core/flashlight.cpp`, `tests/test_flashlight.cpp`. Run - Expected: fordítási hiba.

- [ ] **Step 2: Megvalósítás**

`src/core/flashlight.h`:

```cpp
#pragma once
#include <cstdint>
#include "core/config.h"
#include "core/flicker.h"
#include "core/vec.h"

namespace fw {

inline constexpr int kBeamLevels = 5;
inline constexpr int kBeamMiddle = 2;
inline constexpr double kBeamFactors[kBeamLevels] = {0.5, 0.75, 1.0, 1.5, 2.0};
inline constexpr double kMinBeamAngle = 5;
inline constexpr double kMaxBeamAngle = 80;

struct LightSettings {
  double angle = 28;
  double inner_angle = 12;
  double range = 40;
  double brightness = 0.3;
  double r = 1.0;
  double g = 0.96;
  double b = 0.88;
  double fade_time = 0.18;
  double hand_right = 0.5;
  double hand_down = 0.21;
  double flicker_chance = 0;
};
LightSettings light_settings(const Config& cfg);

// Egy képkocka bemenete: a be/ki billentyű éle, a fokozat-lépések (billentyű és görgő), a szem helye és a nézés iránya
// (a yaw a telemetria konvenciója szerint, a pitch felfelé pozitív), az eltelt idő.
struct FlashlightInput {
  bool toggle = false;
  int wider = 0;
  int narrower = 0;
  Vec3 eye;
  double yaw = 0;
  double pitch = 0;
  double dt = 0;
};

// A fény, amelyet a játék egy fényének be kell állítani; lit = hamis, ha nem kell fény.
struct LightOutput {
  bool lit = false;
  Vec3 position;
  Vec3 direction;
  double outer_angle = 0;
  double inner_angle = 0;
  double range = 0;
  double r = 0;
  double g = 0;
  double b = 0;
};

class Flashlight {
 public:
  explicit Flashlight(std::uint32_t seed = 0x5EED) : flicker_(seed) {}
  LightOutput step(const FlashlightInput& in, const LightSettings& s);
  // Azonnali kikapcsolás halványulás nélkül (visszaszálláskor, szünetben, új világnál).
  void switch_off();
  bool on() const { return on_; }
  int level() const { return level_; }

 private:
  bool on_ = false;
  double fade_ = 0;
  int level_ = kBeamMiddle;
  Flicker flicker_;
};

}  // namespace fw
```

`src/core/flashlight.cpp`:

```cpp
#include "core/flashlight.h"
#include <algorithm>
#include <cmath>

namespace fw {

LightSettings light_settings(const Config& c) {
  LightSettings s;
  s.angle = c.light_angle;
  s.inner_angle = c.light_inner_angle;
  s.range = c.light_range;
  s.brightness = c.light_brightness;
  s.r = c.light_r;
  s.g = c.light_g;
  s.b = c.light_b;
  s.fade_time = c.light_fade_time;
  s.hand_right = c.light_hand_right;
  s.hand_down = c.light_hand_down;
  s.flicker_chance = c.light_flicker_chance;
  return s;
}

void Flashlight::switch_off() {
  on_ = false;
  fade_ = 0;
}

LightOutput Flashlight::step(const FlashlightInput& in, const LightSettings& s) {
  if (in.toggle) {
    on_ = !on_;
    if (on_ && fade_ == 0) level_ = kBeamMiddle;
  }
  if (on_) level_ = std::clamp(level_ + in.wider - in.narrower, 0, kBeamLevels - 1);
  const double target = on_ ? 1.0 : 0.0;
  if (s.fade_time <= 0) {
    fade_ = target;
  } else {
    const double change = in.dt / s.fade_time;
    fade_ = fade_ < target ? std::min(target, fade_ + change) : std::max(target, fade_ - change);
  }
  LightOutput out;
  if (fade_ <= 0) return out;
  const double flicker = on_ ? flicker_.step(in.dt, s.flicker_chance) : 1.0;
  const double intensity = fade_ * flicker * s.brightness;

  const Vec3 flat = forward_from_yaw(in.yaw);
  const double cp = std::cos(in.pitch);
  const double sp = std::sin(in.pitch);
  const Vec3 forward{flat.x * cp, sp, flat.z * cp};
  const Vec3 up{-flat.x * sp, cp, -flat.z * sp};
  const Vec3 right = right_from_yaw(in.yaw);

  out.lit = true;
  out.position = in.eye + right * s.hand_right - up * s.hand_down;
  out.direction = forward;
  const double factor = kBeamFactors[level_];
  out.outer_angle = std::clamp(s.angle * factor, kMinBeamAngle, kMaxBeamAngle);
  out.inner_angle = std::clamp(s.inner_angle * factor, 0.0, out.outer_angle);
  out.range = s.range;
  out.r = s.r * intensity;
  out.g = s.g * intensity;
  out.b = s.b * intensity;
  return out;
}

}  // namespace fw
```

Megjegyzés: a `flashlight_fades_in_and_out` tesztben a villogás 0 gyakorisággal 1-et ad, így a szín csak a halványítást mutatja.
Run - Expected: minden zöld.

- [ ] **Step 3: Commit** - `git commit -m "Compute the flashlight: fade, beam levels, hand position and colour"`

---

### Task 5: Egér-hook a görgőhöz

**Files:** Modify `src/win/input.h`, `src/win/input.cpp`

**Interfaces:**
- Produces: `int mouse_wheel_take();` - a legutóbbi hívás óta görgetett lépések (felfelé pozitív, egy rovátka = 1); a hook a `keyboard_hook_block(true)` alatt és a játék fókuszában elnyeli a görgőt.

Ez a réteg csak a játékban tesztelhető; a kapu a figyelmeztetésmentes fordítás.

- [ ] **Step 1:** `src/win/input.h`, a `keyboard_hook_down` után:

```cpp
// A görgő lépései a legutóbbi hívás óta (felfelé pozitív, egy rovátka = 1). Az egér-hook a billentyűzet-hookkal együtt indul,
// és séta közben (keyboard_hook_block) a játék elől elnyeli a görgőt.
int mouse_wheel_take();
```

- [ ] **Step 2:** `src/win/input.cpp`: a névtelen névtérbe

```cpp
std::atomic<int> g_wheel{0};  // a görgő összegyűlt értéke WHEEL_DELTA egységben

LRESULT CALLBACK mouse_proc(int code, WPARAM message, LPARAM data) {
  if (code == HC_ACTION && message == WM_MOUSEWHEEL && g_blocking && game_window_focused()) {
    const auto* mouse = reinterpret_cast<const MSLLHOOKSTRUCT*>(data);
    g_wheel += static_cast<short>(HIWORD(mouse->mouseData));
    return 1;
  }
  return CallNextHookEx(nullptr, code, message, data);
}
```

a `hook_thread`-ben a billentyűzet-hook után `const HHOOK mouse = SetWindowsHookExW(WH_MOUSE_LL, mouse_proc, GetModuleHandleW(nullptr), 0);`, a ciklus után `if (mouse) UnhookWindowsHookEx(mouse);` (az egér-hook hiánya nem akadályozza a billentyűzet-hookot).
A fájl végére:

```cpp
int mouse_wheel_take() { return g_wheel.exchange(0) / WHEEL_DELTA; }
```

Megjegyzés: a `game_window_focused` a fájlban a `hook_proc` után van definiálva, a `mouse_proc` ugyanazt használja, tehát mindkettő előtt deklarálva kell legyen (ha a fordító hiányolja, a meglévő deklarációt kell előbbre tenni).
Run - Expected: figyelmeztetésmentes fordítás, minden teszt zöld.

- [ ] **Step 3: Commit** - `git commit -m "Count mouse wheel steps and keep the wheel from the game while walking"`

---

### Task 6: A játék fényei - felderítő változat (élő teszt)

**Files:** Create `src/win/game_lights.h/.cpp`; Modify `src/core/game_layout.h`, `src/win/plugin.cpp`, `CMakeLists.txt`

**Interfaces:**
- Consumes: `checked_vtable`, `code_ranges` (core/object_check), `find_type_vtable`, `find_cvar` (locator), `guarded_call`, `layout::kSceneGlobal`.
- Produces: `class GameLights { bool init(std::vector<std::string>& log); std::vector<LightCandidate> lights(); std::vector<std::string> describe(Vec3 walker); };` - a 7. és 8. feladat bővíti.

A cél: élőben igazolni a fénytömb helyét, a mezők jelentését, a `light_source` típusbejegyzés futásidejű mezőit és a konzolváltozókat. Az eredményekből lesznek a `game_layout.h` végleges értékei.

- [ ] **Step 1: A TM alapján feltételezett értékek** (`src/core/game_layout.h`, a végére, "élő teszt" jelöléssel):

```cpp
// A játék fényei (TM Real Walk alapján, a 6. feladat élő tesztje igazolja; docs/game-notes/ets2-1.61.md, "Fények").
inline constexpr std::string_view kLightClass = "light_source";
inline constexpr std::uint64_t kSceneLightArray = 0x3278;  // a scene objektumban: a fénymutatók tömbje (élő teszt)
inline constexpr std::uint64_t kSceneLightCount = 0x3280;  // ... és a darabszám (élő teszt)
inline constexpr std::uint64_t kLightColour = 0x124;       // 6 float (két RGB) (élő teszt)
inline constexpr std::uint64_t kLightIntensity = 0x108;    // float (élő teszt)
inline constexpr std::uint64_t kLightRange = 0x168;        // float, m (élő teszt)
inline constexpr std::uint64_t kLightInnerAngle = 0x174;   // float (élő teszt: fok vagy fél szög)
inline constexpr std::uint64_t kLightOuterAngle = 0x178;   // float (élő teszt)
inline constexpr std::uint64_t kLightSize = 0x1A8;         // float (élő teszt)
inline constexpr std::uint64_t kLightTransform = 0x1B0;    // hely (16 bájt, GamePosition) és forgatás (w, x, y, z) (élő teszt)
inline constexpr std::uint64_t kLightTransformCopy = 0x1D0;  // ugyanez másodszor (élő teszt)
inline constexpr std::uint64_t kTypeEntryCreator = 0x08;   // a típusbejegyzésben: a létrehozó függvény (futásidőben töltődik)
inline constexpr std::uint64_t kTypeEntrySize = 0x30;      // ... és az objektum mérete
inline constexpr std::string_view kLightDistanceCvar = "g_light_distance_factor";
inline constexpr std::string_view kLightSpanCvar = "g_light_span_factor";
```

- [ ] **Step 2: `src/win/game_lights.h`**

```cpp
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "core/light_borrow.h"
#include "core/object_check.h"
#include "core/vec.h"
#include "win/process_memory.h"

namespace fw {

// A játék fényei: a scene fénytömbje, egy fény mezői, kölcsönzés és saját fény (a 7. és 8. feladat bővíti).
class GameLights {
 public:
  // A light_source típus, a scene globális és a konzolváltozók megkeresése (betöltéskor).
  bool init(std::vector<std::string>& log);
  // A fénytömb light_source lámpái (hely, ég-e).
  std::vector<LightCandidate> lights();
  // Diagnosztika (discovery_log): a tömb mérete, a típusbejegyzés futásidejű mezői, a gyaloglóhoz legközelebbi 6 lámpa mezői.
  std::vector<std::string> describe(Vec3 walker);

 private:
  struct Array {
    std::uint64_t data = 0;
    std::uint64_t count = 0;
  };
  Array array() const;
  bool is_light(std::uint64_t light) const;

  bool ready_ = false;
  std::uint64_t base_ = 0;
  std::uint64_t light_vtable_ = 0;
  std::uint64_t type_entry_ = 0;
  std::uint64_t distance_cvar_ = 0;
  std::uint64_t span_cvar_ = 0;
  CodeRanges ranges_;
  ProcessMemory memory_;
};

}  // namespace fw
```

- [ ] **Step 3: `src/win/game_lights.cpp`** (felderítő rész)

```cpp
#include "win/game_lights.h"
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include "core/coords.h"
#include "core/game_layout.h"
#include "core/locator.h"
#include "core/pe_image.h"
#include "core/text.h"

namespace fw {
namespace {

constexpr std::uint64_t kMaxLights = 20000;

// A fény helye a transzformáció blokkjából (hely a négyzeten belül + négyzet-sorszám).
Vec3 world_of(const float* block) {
  CameraPosition c;
  c.x = block[0];
  c.y = block[1];
  c.z = block[2];
  std::int16_t chunks[2];
  std::memcpy(chunks, block + 3, 4);
  c.chunk_x = chunks[0];
  c.chunk_z = chunks[1];
  return to_world(c);
}

}  // namespace

bool GameLights::init(std::vector<std::string>& log) {
  base_ = reinterpret_cast<std::uint64_t>(GetModuleHandleW(nullptr));
  const auto image = PeImage::parse(memory_, base_);
  if (!image || !layout::is_verified_build(image->timestamp(), image->image_size())) {
    log.push_back("lights: not the verified game build; flashlight unavailable");
    return false;
  }
  const auto ranges = code_ranges(*image);
  const auto vtable = find_type_vtable(memory_, *image, layout::kLightClass, log);
  if (!ranges || !vtable) {
    log.push_back("lights: the light_source type was not found; flashlight unavailable");
    return false;
  }
  ranges_ = *ranges;
  light_vtable_ = *vtable;
  // A típusbejegyzés: a .data-ban az a qword, amely a típus nevére mutat (a vtable 5. függvénye "lea rax, [cella]"-val adja vissza).
  if (const Section* rdata = image->section(".rdata"); const Section* data = image->section(".data")) {
    const auto name = find_cstring(image->read_section(memory_, *rdata), layout::kLightClass);
    if (name)
      for (const std::uint64_t e : find_qword(image->read_section(memory_, *data), *name)) type_entry_ = e;
    (void)rdata;
  }
  if (const auto c = find_cvar(memory_, *image, layout::kLightDistanceCvar, true, log)) distance_cvar_ = *c;
  if (const auto c = find_cvar(memory_, *image, layout::kLightSpanCvar, true, log)) span_cvar_ = *c;
  ready_ = true;
  log.push_back(strf("lights: light_source vtable +0x%llX, type entry +0x%llX", static_cast<unsigned long long>(light_vtable_ - base_),
                     static_cast<unsigned long long>(type_entry_ ? type_entry_ - base_ : 0)));
  return true;
}

GameLights::Array GameLights::array() const {
  const auto scene = memory_.get<std::uint64_t>(base_ + layout::kSceneGlobal);
  if (!scene || *scene == 0) return {};
  const auto data = memory_.get<std::uint64_t>(*scene + layout::kSceneLightArray);
  const auto count = memory_.get<std::uint64_t>(*scene + layout::kSceneLightCount);
  if (!data || !count || *data == 0 || *count > kMaxLights) return {};
  return {*data, *count};
}

bool GameLights::is_light(std::uint64_t light) const { return checked_vtable(memory_, light, ranges_, {0}) == light_vtable_; }

std::vector<LightCandidate> GameLights::lights() {
  std::vector<LightCandidate> out;
  if (!ready_) return out;
  const Array a = array();
  std::vector<std::uint64_t> pointers(static_cast<std::size_t>(a.count));
  if (a.count == 0 || !memory_.read(a.data, pointers.data(), pointers.size() * 8)) return out;
  for (const std::uint64_t p : pointers) {
    if (!is_light(p)) continue;
    float block[8];
    float colour[3];
    if (!memory_.read(p + layout::kLightTransform, block, sizeof block) || !memory_.read(p + layout::kLightColour, colour, sizeof colour))
      continue;
    out.push_back({p, world_of(block), colour[0] + colour[1] + colour[2] > 0.01f});
  }
  return out;
}

std::vector<std::string> GameLights::describe(Vec3 walker) {
  std::vector<std::string> lines;
  if (!ready_) return lines;
  const Array a = array();
  std::uint64_t creator = 0;
  std::uint64_t size = 0;
  if (type_entry_) {
    creator = memory_.get<std::uint64_t>(type_entry_ + layout::kTypeEntryCreator).value_or(0);
    size = memory_.get<std::uint64_t>(type_entry_ + layout::kTypeEntrySize).value_or(0);
  }
  std::string cvars;
  for (const std::uint64_t c : {distance_cvar_, span_cvar_}) {
    char value[16] = {};
    if (c && memory_.read(c + layout::kCvarValue, value, sizeof value - 1)) cvars += std::string(" '") + value + "'";
  }
  lines.push_back(strf("lights: array %llX, %llu entries; creator exe+0x%llX, object size 0x%llX; distance/span cvars%s",
                       static_cast<unsigned long long>(a.data), static_cast<unsigned long long>(a.count),
                       static_cast<unsigned long long>(creator >= base_ ? creator - base_ : 0), static_cast<unsigned long long>(size),
                       cvars.c_str()));
  auto list = lights();
  std::sort(list.begin(), list.end(), [&](const auto& x, const auto& y) {
    return horizontal_distance(walker, x.position) < horizontal_distance(walker, y.position);
  });
  for (std::size_t i = 0; i < list.size() && i < 6; ++i) {
    float f[0x40];
    if (!memory_.read(list[i].id + 0x100, f, sizeof f)) continue;
    const float* t = f + (layout::kLightTransform - 0x100) / 4;
    lines.push_back(strf("lights:   %llX %.1f m %s pos (%.1f %.1f %.1f) rot (%.3f %.3f %.3f %.3f) colour (%.2f %.2f %.2f / %.2f %.2f %.2f) "
                         "+108 %.3f range %.1f +174 %.2f +178 %.2f +1A8 %.2f +1AC %.2f",
                         static_cast<unsigned long long>(list[i].id), horizontal_distance(walker, list[i].position), list[i].active ? "on" : "off",
                         list[i].position.x, list[i].position.y, list[i].position.z, t[4], t[5], t[6], t[7], f[(0x124 - 0x100) / 4],
                         f[(0x128 - 0x100) / 4], f[(0x12C - 0x100) / 4], f[(0x130 - 0x100) / 4], f[(0x134 - 0x100) / 4],
                         f[(0x138 - 0x100) / 4], f[(0x108 - 0x100) / 4], f[(0x168 - 0x100) / 4], f[(0x174 - 0x100) / 4],
                         f[(0x178 - 0x100) / 4], f[(0x1A8 - 0x100) / 4], f[(0x1AC - 0x100) / 4]));
  }
  return lines;
}

}  // namespace fw
```

(A `describe` a TM által feltételezett mezőket mind kiírja; a hely a `kLightTransform` blokkból.)

- [ ] **Step 4: Bekötés** (`src/win/plugin.cpp`): `#include "win/game_lights.h"`; a `State`-be `fw::GameLights lights;`; a `scs_telemetry_init`-ben a kamerakeresés után `if (g->camera_ready) { std::vector<std::string> l; g->lights.init(l); log_lines(l); }`; a `discovery()`-ben `for (const auto& line : g->lights.describe(g->session.phase() == fw::Phase::Walking ? g->walker.feet() : g->truck.position)) lines.push_back(line);`; `kVersion` = `"0.6.0"`. CMake: `src/win/game_lights.cpp`.
Run - Expected: figyelmeztetésmentes fordítás, minden teszt zöld.

- [ ] **Step 5: Commit, tesztcsomag, élő teszt 1**

`git commit -m "Discovery: read the scene's light array and log the nearest street lamps"`.
`dist/FreeWalk-0.6.0-lights/` (`discovery_log = 1`), `TESZT.md`: éjjel, utcai lámpák közelében töltsd be a mentést, állj meg egy lámpa alatt 5 másodpercre (kiszállva), majd egy másiknál; lépj ki.
A naplóból: a tömb mérete (ésszerű, néhány száz-ezer); a legközelebbi lámpa helye egyezik azzal, ahol álltál (néhány m); a mezők (szín, hatótáv, szögek) értelmes értékek; a létrehozó függvény és az objektumméret (0x200-0x400 körüli); a két konzolváltozó értéke.
Az eredmény alapján a `game_layout.h` értékei véglegesek lesznek (a "élő teszt" jelölés helyére a bizonyíték), az `ets2-1.61.md` kap egy `## Fények` szakaszt, és `dumpbin`-nel a létrehozó függvényből (az első `jmp`) a konstruktor és az aláírása. Minden eltérés `Ruling:`.

---

### Task 7: Kölcsönzés és a lámpa bekötése

**Files:** Modify `src/win/game_lights.h/.cpp`, `src/win/plugin.cpp`, `src/win/input` (a figyelt billentyűk), `README.md`

**Interfaces:**
- Consumes: `Flashlight`, `light_settings`, `FlashlightInput`, `LightOutput` (4.), `choose_light` (2.), `mouse_wheel_take` (5.), `GameLights` (6.).
- Produces: `bool GameLights::acquire(Vec3 walker, double min_distance, double max_distance, bool own_light, std::vector<std::string>& log);`, `bool GameLights::apply(const LightOutput& out, Vec3 world_quaternion_hint, std::vector<std::string>& log);` → a 8. feladatig az `own_light` paramétert figyelmen kívül hagyja; `void GameLights::release(std::vector<std::string>& log);`, `void GameLights::forget();`, `bool GameLights::holding() const;`.

A forgatás: a fény iránya a kamera forgatása (a szabad kamera `kCameraRotation` kvaterniója, w, x, y, z), ha az élő teszt szerint a fény előre-tengelye a kameráéval egyezik; különben a 6. feladat eredménye szerinti átváltás (`Ruling:`).

- [ ] **Step 1: Kölcsönzés és visszaadás** (`game_lights`): a kölcsönzött lámpa címe, az eredeti 0x40 bájtja a `kLightTransform`-tól, a szín 6 floatja, a `kLightIntensity`, `kLightRange`, `kLightInnerAngle`, `kLightOuterAngle`, `kLightSize` mentése; `apply` írja a transzformációt mindkét helyre (`kLightTransform`, `kLightTransformCopy`), a színt (mindkét RGB), a hatótávot, a szögeket a `LightOutput`-ból; minden írás előtt `is_light(p)` (különben `forget` és "lost" naplósor); `release` visszaírja a mentett bájtokat, ha a lámpa még ugyanaz; `forget` csak elfelejti.
- [ ] **Step 2: Bekötés** (`plugin.cpp`):
  - `State`: `fw::Flashlight flashlight;`, `fw::EdgeDetector flashlight_key, wider_key, narrower_key;`;
  - séta közben (`walk`): az élek a `combo_down`-nal, fókuszban; `in.wider = (wider él) + max(0, wheel)`, `in.narrower = (narrower él) + max(0, -wheel)` (`wheel = fw::mouse_wheel_take()`); a szem a `g->eye`, a yaw a kamerából, a pitch a `g->camera`-ból (új `pitch()` olvasó a `GameCamera`-ban a `kCameraPitch` mezőből);
  - `out = flashlight.step(in, light_settings(config))`; ha `out.lit` és nincs fény: `lights.acquire(...)`; ha van: `lights.apply(out, ...)`; ha nem `lit`: `lights.release(...)`;
  - a figyelt (elnyelt) billentyűk közé a három lámpa-billentyű is bekerül (`keyboard_hook_start`/`keyboard_hook_watch`);
  - kialvás: `return_to_truck`, szünet (`on_paused`), shutdown: `flashlight.switch_off(); lights.release(...)`; új világ (a kamerakezelő címe változik): `lights.forget()`; a szünet végén, ha előtte égett, újra be (`flashlight` állapot mentése a szünet előtt).
  - Ha 10 s-on belül háromszor elvész a fény (`apply` hamis): `flashlight.switch_off()` és naplósor.
- [ ] **Step 3:** `kVersion` = `"0.6.1"`; build, tesztek zöldek; README: `## Zseblámpa` szakasz (billentyűk, mit csinál, éjjel kölcsönzött lámpa, a Light fül).
- [ ] **Step 4: Commit, élő teszt 2** - éjjel be/ki, a nézés követése, a fokozatok billentyűvel és görgővel, halványulás, kikapcsolás után a távoli lámpa visszaáll, szünet és visszaszállás égő lámpával; a szabad kamera nem sodródik a görgőtől (ha igen: a sebesség lefagyasztása képkockánként, `Ruling:`).

---

### Task 8: Saját fény

**Files:** Modify `src/win/game_lights.h/.cpp`, `src/core/game_layout.h`, `docs/game-notes/ets2-1.61.md`

**Interfaces:**
- Consumes: a 6. feladat élő tesztjének eredménye (konstruktor, objektumméret, a fénylistába kerülés módja), `guarded_call`.
- Produces: az `acquire` `own_light` ága; `release` és `forget` a saját fényre is.

- [ ] **Step 1: Elemzés** (`dumpbin`): a létrehozó függvény első `jmp`-je a konstruktor; az aláírása (`rcx` = objektum, további argumentumok); a destruktor (a `light_source` vtable 0. bejegyzése, `bool` törlés-jelzővel); hogyan kerül egy fény a scene fénytömbjébe (a konstruktor maga, vagy a scene egy hozzáadó függvénye) és hogyan kerül ki. Az eredmény az `ets2-1.61.md`-be.
- [ ] **Step 2: Megvalósítás:** a saját memóriában (`_aligned_malloc(size + 0x40, 16)`, nullázva) a konstruktor hívása `guarded_call`-lal; a vtable-ellenőrzés (`is_light`); a TM szerinti kezdőértékek (`+8` jelzőbit 0x1000, `+0x198` = 1, `+0x104` = 0,05), ha az elemzés megerősíti; felvétel a fénylistába az elemzés szerint; lebontáskor kivétel a listából, destruktor (`guarded_call`), `_aligned_free`. Hiba esetén a saját fény a munkamenetre kikapcsol (napló).
- [ ] **Step 3:** `kVersion` = `"0.6.2"`; build, tesztek; commit; élő teszt 3: lakatlan helyen éjjel és nappal árnyékban.

---

### Task 9: Lezárás

- [ ] **Step 1:** élő teszt 4 (villogás: a `flicker_chance` 60-ra a beállítóablakban) és 5 (hibatűrés: ki- és beszállás, szünet, mentés betöltése égő lámpával, újraindítás után a beállítások megmaradnak); a talált hibák javítása teszttel (a mag hibáinál).
- [ ] **Step 2:** a diagnosztika (`describe`) marad a `discovery_log` alatt; a végleges csomag `discovery_log = 0`-val; README és spec frissítése a végleges tényekkel.
- [ ] **Step 3:** commit.

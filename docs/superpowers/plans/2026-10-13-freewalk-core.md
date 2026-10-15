# FreeWalk - a gyaloglás magja: megvalósítási terv

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Egy ETS2 telemetria-plugin (`FreeWalk.dll`), amellyel a játékos F10-re kiszáll a parkoló kamionból, és a játék szabad kamerájával első személyben sétál körülötte sík talajon.

**Architecture:** Két réteg.
A játékfüggetlen mag (`src/core`) sima C++20, Windows-függés nélkül, bármely hoszton natívan fordul és tesztelhető (az 1-10. feladat idején macOS-en, azóta Windowson): koordináták, beállítások, billentyűk, gyalogló, állapotgép, PE- és RTTI-olvasás, a játék objektumainak megkeresése egy absztrakt memórián.
A Windows-réteg (`src/win`) az SCS SDK belépési pontjait, a saját folyamat memóriájának védett olvasását és írását, a kamera kezelését és a billentyűzetet tartalmazza; Windowson MSVC-vel készül ugyanabban a buildben, mint a tesztek (macOS-ről MinGW-w64-gyel keresztfordítható).

**Tech Stack:** C++20, CMake 3.20+, Ninja, MSVC (Visual Studio Build Tools, statikus futtatókönyvtár), MinGW-w64 a keresztfordításhoz, SCS Telemetry SDK 1.14, Ghidra 12.1.4 (elemzéshez).

**Spec:** `docs/superpowers/specs/2026-10-13-freewalk-core-design.md`

## Global Constraints

- Célplatform: ETS2 1.61.x, Windows x64; más játéknál (`game_id != "eut2"`) a plugin `SCS_RESULT_unsupported`-tal visszalép.
- Telemetria API: csak `SCS_TELEMETRY_VERSION_1_01`.
- Név: `FreeWalk.dll`, `FreeWalk.ini`, minden naplósor `[FreeWalk] ` előtaggal kezdődik.
- A naplósorok szövege angol és csak ASCII karaktereket tartalmaz (a játék logja ASCII-ra van tervezve); a kódkommentek magyarok.
- A kiszállás feltétele: sebesség < 0,5 km/h, behúzott kézifék, megérkezett `truck` konfiguráció.
- Alapértékek: járás 1,4 m/s, futás 4,0 m/s, szemmagasság 1,7 m, fejbiccentés 0,03 m, kiszállási oldaltávolság 1,0 m, talaj eltolás 0 m, automatikus visszaszállás 150 m felett, kamion-ugrás 5 m felett, kamera-átkapcsolási időkorlát 3 s, szabadkamera-pozíció ellenőrzés 1000 m.
- A plugin a játék kódját nem módosítja (nincs hook); adatot ír, és a szabad kamera sebességéhez a konzolváltozó saját beállító függvényét hívja.
- Minden memóriaolvasás `Memory::read`-en át megy; érvénytelen címnél hibát ad, nem omlik össze.
- A visszafejtett TM Real Walk kódjából semmit nem másolunk; a `game_layout.h` értékei a játékra vonatkozó, a 8. és 9. feladatban ellenőrzött tények.
- A `content/` mappa (eredeti DLL, visszafejtés) és a játék `.exe`-je nem kerül a repóba.
- Commit üzenetekbe nem kerül `Co-Authored-By` sor (a felhasználó szabálya).
- Hosszú Markdown fájlokban minden mondat külön sorban; em dash karakter sehol, helyette sima kötőjel.
- A specifikációtól tudatos eltérés: a forgatást (kvaterniót) nem írjuk minden képkockában, csak az irányszöget egyszer a séta elején; a forgatást a játék a saját irányszögeiből számolja. Ha a 11. feladat élő tesztje mást mutat, külön feladatként pótoljuk.
- A specifikációtól tudatos egyszerűsítés: a `walk_session` három állapotú (`Driving`, `Switching`, `Walking`); a visszaszállás azonnali, ezért nincs külön állapota.

## Review Focus

- A játék ablaka nincs előtérben (alt-tab) séta közben: a gyalogló nem mozdul, és az F10 sem vált; teszt a 2. feladatban (`map_keys` fókusz nélkül).
- Hosszú képkocka (akadás, betöltés, szünet után) séta közben: a gyalogló legfeljebb 0,1 s-nyi utat tesz meg, nem ugrik; teszt a 3. feladatban.
- Windowsban szerkesztett INI (CRLF sorvégek, UTF-8 BOM): minden érték ugyanúgy beolvasódik; teszt a 2. feladatban.
- Negatív világkoordináta egy 512 m-es négyzet határán (például x = -0,0001): a négyzet sorszáma -1, a visszaváltás pontos; teszt az 1. feladatban.
- A játék kilép vagy kiveszi a plugint séta közben: a plugin visszaállítja a kamerát, a sebességet és a `g_developer`-t; teszt a 4. feladatban (`force_return`), élő ellenőrzés a 10. feladatban.

---

## Fájlszerkezet

```
FreeWalk/
  .gitignore
  CMakeLists.txt
  README.md                         telepítés, fordítás, tesztelés
  FreeWalk.ini                      alapértelmezett beállítások, magyar kommentekkel
  cmake/mingw-w64-x86_64.cmake      keresztfordító toolchain
  third_party/scs_sdk/              SCS SDK 1.14 fejlécek + licenc (MIT)
  src/core/vec.h                    3D vektor, irányszög-matematika
  src/core/coords.h/.cpp            világkoordináta <-> kamera (512 m-es négyzet + helyi)
  src/core/text.h/.cpp              printf-formázás std::string-be
  src/core/config.h/.cpp            INI beolvasás, billentyűnevek
  src/core/keys.h/.cpp              séta-billentyűk leképezése, élérzékelés
  src/core/walker.h/.cpp            gyalogló mozgása, fejbiccentés
  src/core/walk_session.h/.cpp      állapotgép, kiszállási pont
  src/core/memory.h                 absztrakt memória + puffer-memória
  src/core/pe_image.h/.cpp          PE fejléc, szakaszok, szöveg/érték/RIP-hivatkozás keresés
  src/core/file_image.h/.cpp        PE fájl betöltése puffer-memóriába (elemzőeszköz, tesztek)
  src/core/rtti.h/.cpp              MSVC RTTI osztálynév olvasása
  src/core/game_layout.h            a játék belső felépítése (eltolások, nevek), ellenőrzési jegyzetekkel
  src/core/locator.h/.cpp           kamerakezelő, kamerák, konzolváltozók megkeresése
  src/win/FreeWalk.def              exportok
  src/win/log.h/.cpp                naplózás az SDK-n át
  src/win/process_memory.h/.cpp     a saját folyamat memóriájának védett olvasása és írása
  src/win/input.h/.cpp              billentyűállapot, ablakfókusz
  src/win/game_camera.h/.cpp        kamera-átkapcsolás, irány, pozíció, konzolváltozók
  src/win/plugin.cpp                SDK belépési pontok, telemetria, a rétegek összekötése
  tests/check.h, tests/test_main.cpp      minimális tesztkeret
  tests/pe_builder.h                szintetikus PE kép a tesztekhez
  tests/test_*.cpp                  tesztek
  tools/probe.cpp                   hoszton futó elemző: a locator a játék .exe fájlján
  tools/ghidra/FindRefs.java        Ghidra szkript: szöveg/cím hivatkozásai és a hivatkozó függvények kódja
  docs/game-notes/ets2-1.61.md      a játék elemzésének eredményei
```

Parancsok (a repó gyökeréből):
- Windows (a 11. feladattól): `build.cmd` - beállítja az MSVC környezetét, és egy buildben (`build/msvc-Release`) fordítja a teszteket, a DLL-t és az elemzőt, majd futtatja a teszteket. A DLL: `build/msvc-Release/FreeWalk.dll`, az elemző: `build\msvc-Release\freewalk_probe.exe <exe>`.
- macOS (az 1-10. feladat idején, az ezekhez tartozó lépések parancsai ezt használják): teszt: `cmake -S . -B build/host -G Ninja && cmake --build build/host && ctest --test-dir build/host --output-on-failure`; DLL: `cmake -S . -B build/win -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake -DCMAKE_BUILD_TYPE=Release && cmake --build build/win`

---

### Task 1: Projektváz, tesztkeret, vektor és koordináták

**Files:**
- Create: `.gitignore`, `CMakeLists.txt`, `tests/check.h`, `tests/test_main.cpp`, `src/core/vec.h`, `src/core/coords.h`, `src/core/coords.cpp`, `tests/test_vec.cpp`, `tests/test_coords.cpp`

**Interfaces:**
- Consumes: semmi.
- Produces:
  - `struct fw::Vec3 { double x, y, z; }`, `operator+`, `operator-`, `operator*(Vec3, double)`, `double horizontal_distance(Vec3, Vec3)`, `Vec3 rotate_heading(Vec3 local, double heading)`, `Vec3 forward_from_yaw(double)`, `Vec3 right_from_yaw(double)`.
  - `constexpr double fw::kChunkSize = 512.0`, `struct fw::CameraPosition { int16_t chunk_x, chunk_z; float x, y, z; }`, `CameraPosition to_camera(Vec3)`, `Vec3 to_world(const CameraPosition&)`.
  - Tesztkeret: `TEST(name)`, `CHECK(cond)`, `CHECK_NEAR(a, b, eps)`.
  - CMake cél: `freewalk_core` (statikus könyvtár), `freewalk_tests` (csak nem-Windows hoszton).

- [ ] **Step 1: Eszközök telepítése**

Run: `brew install cmake ninja`
Expected: `cmake --version` 3.20 feletti verziót ír, `ninja --version` működik.

- [ ] **Step 2: Projektváz és tesztkeret**

`.gitignore`:
```
build/
dist/
```

`CMakeLists.txt`:
```cmake
cmake_minimum_required(VERSION 3.20)
project(FreeWalk CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT MSVC)
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()
if(MINGW)
  # A MinGW saját printf-je kell a %zu és %llu formátumokhoz.
  add_compile_definitions(__USE_MINGW_ANSI_STDIO=1)
endif()

add_library(freewalk_core STATIC
  src/core/coords.cpp
)
target_include_directories(freewalk_core PUBLIC src)

if(NOT WIN32)
  enable_testing()
  add_executable(freewalk_tests
    tests/test_main.cpp
    tests/test_vec.cpp
    tests/test_coords.cpp
  )
  target_include_directories(freewalk_tests PRIVATE tests)
  target_link_libraries(freewalk_tests PRIVATE freewalk_core)
  add_test(NAME freewalk_tests COMMAND freewalk_tests)
endif()
```

`tests/check.h`:
```cpp
#pragma once
#include <cmath>
#include <cstdio>
#include <vector>

// Minimális tesztkeret: a TEST makró regisztrálja a függvényt, a CHECK makrók számolják a hibákat.
namespace fwtest {
struct Case { const char* name; void (*fn)(); };
std::vector<Case>& registry();
inline int failures = 0;
struct Register { Register(const char* name, void (*fn)()) { registry().push_back({name, fn}); } };
}

#define TEST(name) \
  static void name(); \
  static fwtest::Register register_##name(#name, name); \
  static void name()

#define CHECK(cond) \
  do { if (!(cond)) { std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++fwtest::failures; } } while (0)

#define CHECK_NEAR(a, b, eps) \
  do { const double a_ = (a), b_ = (b); \
       if (std::fabs(a_ - b_) > (eps)) { std::printf("  FAIL %s:%d: %s = %.9g, expected %.9g\n", __FILE__, __LINE__, #a, a_, b_); ++fwtest::failures; } } while (0)
```

`tests/test_main.cpp`:
```cpp
#include "check.h"

std::vector<fwtest::Case>& fwtest::registry() {
  static std::vector<Case> cases;
  return cases;
}

int main() {
  for (const auto& c : fwtest::registry()) {
    const int before = fwtest::failures;
    c.fn();
    std::printf("%s %s\n", fwtest::failures == before ? "ok  " : "FAIL", c.name);
  }
  std::printf("%d failure(s)\n", fwtest::failures);
  return fwtest::failures == 0 ? 0 : 1;
}
```

- [ ] **Step 3: A vektor és koordináta tesztek megírása**

`tests/test_vec.cpp`:
```cpp
#include "check.h"
#include "core/vec.h"

namespace {
constexpr double kPi = 3.14159265358979323846;
}

TEST(rotate_heading_forward_matches_forward_from_yaw) {
  for (double h : {0.0, 0.3, kPi / 2, kPi, 4.0}) {
    const fw::Vec3 a = fw::rotate_heading({0, 0, -1}, h);
    const fw::Vec3 b = fw::forward_from_yaw(h);
    CHECK_NEAR(a.x, b.x, 1e-12);
    CHECK_NEAR(a.z, b.z, 1e-12);
  }
}

TEST(heading_quarter_turn_faces_west) {
  // Az SDK szerint 0.25 fordulat (90 fok) a nyugati irány, ami a -X tengely.
  const fw::Vec3 f = fw::forward_from_yaw(kPi / 2);
  CHECK_NEAR(f.x, -1.0, 1e-12);
  CHECK_NEAR(f.z, 0.0, 1e-12);
}

TEST(right_at_heading_zero_is_east) {
  const fw::Vec3 r = fw::right_from_yaw(0.0);
  CHECK_NEAR(r.x, 1.0, 1e-12);
  CHECK_NEAR(r.z, 0.0, 1e-12);
  const fw::Vec3 local_right = fw::rotate_heading({1, 0, 0}, 0.0);
  CHECK_NEAR(local_right.x, 1.0, 1e-12);
}

TEST(horizontal_distance_ignores_height) {
  CHECK_NEAR(fw::horizontal_distance({0, 0, 0}, {3, 100, 4}), 5.0, 1e-12);
}
```

`tests/test_coords.cpp`:
```cpp
#include "check.h"
#include "core/coords.h"

TEST(to_camera_splits_into_chunks) {
  const fw::CameraPosition c = fw::to_camera({100.5, 7.25, -0.25});
  CHECK(c.chunk_x == 0);
  CHECK(c.chunk_z == -1);
  CHECK_NEAR(c.x, 100.5, 1e-4);
  CHECK_NEAR(c.y, 7.25, 1e-4);
  CHECK_NEAR(c.z, 511.75, 1e-4);
}

TEST(to_camera_exact_chunk_boundary) {
  const fw::CameraPosition c = fw::to_camera({1024.0, 0.0, 512.0});
  CHECK(c.chunk_x == 2);
  CHECK(c.chunk_z == 1);
  CHECK_NEAR(c.x, 0.0, 1e-6);
  CHECK_NEAR(c.z, 0.0, 1e-6);
}

TEST(negative_coordinate_just_below_zero_is_chunk_minus_one) {
  const fw::CameraPosition c = fw::to_camera({-0.0001, 0.0, -512.0001});
  CHECK(c.chunk_x == -1);
  CHECK(c.chunk_z == -2);
  CHECK_NEAR(fw::to_world(c).x, -0.0001, 1e-3);
  CHECK_NEAR(fw::to_world(c).z, -512.0001, 1e-3);
}

TEST(round_trip_far_from_origin) {
  for (const fw::Vec3 w : {fw::Vec3{-7123.456, 12.5, 98765.4321}, fw::Vec3{250000.25, -3.0, -180000.75}}) {
    const fw::Vec3 back = fw::to_world(fw::to_camera(w));
    CHECK_NEAR(back.x, w.x, 1e-3);
    CHECK_NEAR(back.y, w.y, 1e-3);
    CHECK_NEAR(back.z, w.z, 1e-3);
  }
}
```

- [ ] **Step 4: A tesztek futtatása, hogy lássuk a hibát**

Run: `cmake -S . -B build/host -G Ninja && cmake --build build/host`
Expected: fordítási hiba, mert a `core/vec.h` és a `core/coords.h` még nem létezik.

- [ ] **Step 5: A vektor és a koordináták megírása**

`src/core/vec.h`:
```cpp
#pragma once
#include <cmath>

namespace fw {

// Világkoordináták az SCS konvenciója szerint: x kelet, y fel, z dél (az észak a -Z irány).
struct Vec3 {
  double x = 0;
  double y = 0;
  double z = 0;
};

inline Vec3 operator+(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline Vec3 operator-(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline Vec3 operator*(Vec3 a, double s) { return {a.x * s, a.y * s, a.z * s}; }

inline double horizontal_distance(Vec3 a, Vec3 b) {
  return std::hypot(a.x - b.x, a.z - b.z);
}

// A kamion saját koordinátáiban (x jobbra, y fel, z hátra) megadott vektort a világba forgatja.
// A heading radiánban értendő, felülről nézve az óramutatóval ellentétesen, 0 = észak.
inline Vec3 rotate_heading(Vec3 local, double heading) {
  const double c = std::cos(heading);
  const double s = std::sin(heading);
  return {local.x * c + local.z * s, local.y, -local.x * s + local.z * c};
}

// A vízszintes előre irány egy irányszöghöz (ugyanaz a konvenció, mint a heading).
inline Vec3 forward_from_yaw(double yaw) { return {-std::sin(yaw), 0.0, -std::cos(yaw)}; }
inline Vec3 right_from_yaw(double yaw) { return {std::cos(yaw), 0.0, -std::sin(yaw)}; }

}  // namespace fw
```

`src/core/coords.h`:
```cpp
#pragma once
#include <cstdint>
#include "core/vec.h"

namespace fw {

// A játék kamerája a vízszintes síkon 512 méteres négyzetekben tárolja a helyét: a négyzet sorszáma egész, a négyzeten belüli hely lebegőpontos.
// Így a lebegőpontos érték mindig kicsi marad, és a térkép távoli részein sem veszít pontosságot.
inline constexpr double kChunkSize = 512.0;

struct CameraPosition {
  std::int16_t chunk_x = 0;
  std::int16_t chunk_z = 0;
  float x = 0;
  float y = 0;
  float z = 0;
};

CameraPosition to_camera(Vec3 world);
Vec3 to_world(const CameraPosition& camera);

}  // namespace fw
```

`src/core/coords.cpp`:
```cpp
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
```

- [ ] **Step 6: A tesztek futtatása**

Run: `cmake -S . -B build/host -G Ninja && cmake --build build/host && ctest --test-dir build/host --output-on-failure`
Expected: minden teszt `ok`, a végén `0 failure(s)`, a ctest `100% tests passed`.

- [ ] **Step 7: Commit**

```bash
git add .gitignore CMakeLists.txt src tests
git commit -m "Add project skeleton, test harness, vector math and chunk coordinates"
```

---

### Task 2: Szövegformázás, beállítások és billentyűk

**Files:**
- Create: `src/core/text.h`, `src/core/text.cpp`, `src/core/config.h`, `src/core/config.cpp`, `src/core/keys.h`, `src/core/keys.cpp`, `tests/test_config.cpp`, `tests/test_keys.cpp`
- Modify: `CMakeLists.txt` (új forrásfájlok)

**Interfaces:**
- Consumes: semmi.
- Produces:
  - `std::string fw::strf(const char* format, ...)`.
  - `struct fw::Config` (mezők: `key_toggle`, `key_forward`, `key_back`, `key_left`, `key_right`, `key_run`, `walk_speed`, `run_speed`, `eye_height`, `head_bob`, `head_bob_amplitude`, `exit_side_distance`, `ground_offset`, `max_distance`, `discovery_log`, `write_on_frame_start`).
  - `int fw::parse_key(std::string_view name)` (-1, ha ismeretlen), `Config fw::parse_config(std::string_view text, std::vector<std::string>& warnings)`.
  - `struct fw::WalkKeys { bool forward, back, left, right, run; }`, `WalkKeys fw::map_keys(bool game_focused, bool paused, const std::function<bool(int)>& is_down, const Config&)`, `class fw::EdgeDetector { bool update(bool down); }`.

- [ ] **Step 1: A tesztek megírása**

`tests/test_config.cpp`:
```cpp
#include <string>
#include <vector>
#include "check.h"
#include "core/config.h"

TEST(parse_key_names) {
  CHECK(fw::parse_key("F10") == 0x79);
  CHECK(fw::parse_key("f1") == 0x70);
  CHECK(fw::parse_key("F24") == 0x87);
  CHECK(fw::parse_key("w") == 'W');
  CHECK(fw::parse_key("7") == '7');
  CHECK(fw::parse_key("Shift") == 0x10);
  CHECK(fw::parse_key("SPACE") == 0x20);
  CHECK(fw::parse_key("0x79") == 0x79);
  CHECK(fw::parse_key("F25") == -1);
  CHECK(fw::parse_key("banana") == -1);
  CHECK(fw::parse_key("") == -1);
}

TEST(empty_text_gives_defaults_without_warnings) {
  std::vector<std::string> warnings;
  const fw::Config c = fw::parse_config("", warnings);
  CHECK(warnings.empty());
  CHECK(c.key_toggle == 0x79);
  CHECK_NEAR(c.walk_speed, 1.4, 1e-12);
  CHECK_NEAR(c.run_speed, 4.0, 1e-12);
  CHECK_NEAR(c.eye_height, 1.7, 1e-12);
  CHECK(c.head_bob);
  CHECK_NEAR(c.max_distance, 150.0, 1e-12);
  CHECK(!c.discovery_log);
}

TEST(values_are_read_from_sections) {
  std::vector<std::string> warnings;
  const fw::Config c = fw::parse_config(
      "; comment\n[keys]\ntoggle = F9\nrun = CTRL\n[walker]\nwalk_speed = 1.2 ; slower\neye_height=1.8\nhead_bob = 0\n[debug]\ndiscovery_log = 1\n",
      warnings);
  CHECK(warnings.empty());
  CHECK(c.key_toggle == 0x78);
  CHECK(c.key_run == 0x11);
  CHECK_NEAR(c.walk_speed, 1.2, 1e-12);
  CHECK_NEAR(c.eye_height, 1.8, 1e-12);
  CHECK(!c.head_bob);
  CHECK(c.discovery_log);
}

TEST(windows_line_endings_and_bom_are_accepted) {
  std::vector<std::string> warnings;
  const fw::Config c = fw::parse_config("\xEF\xBB\xBF[walker]\r\nrun_speed = 5\r\nmax_distance = 300\r\n", warnings);
  CHECK(warnings.empty());
  CHECK_NEAR(c.run_speed, 5.0, 1e-12);
  CHECK_NEAR(c.max_distance, 300.0, 1e-12);
}

TEST(bad_values_keep_defaults_and_warn) {
  std::vector<std::string> warnings;
  const fw::Config c = fw::parse_config(
      "[walker]\nwalk_speed = fast\neye_height = 9\nhead_bob = yes\n[keys]\ntoggle = NOPE\n[walker]\nunknown_thing = 1\nno equals sign\n",
      warnings);
  CHECK_NEAR(c.walk_speed, 1.4, 1e-12);
  CHECK_NEAR(c.eye_height, 1.7, 1e-12);
  CHECK(c.head_bob);
  CHECK(c.key_toggle == 0x79);
  CHECK(warnings.size() == 6);
}
```

`tests/test_keys.cpp`:
```cpp
#include <set>
#include "check.h"
#include "core/keys.h"

namespace {
bool down_in(const std::set<int>& keys, int vk) { return keys.count(vk) != 0; }
}

TEST(map_keys_reads_configured_keys) {
  const fw::Config cfg;
  const std::set<int> held{'W', 'D', 0x10};
  const fw::WalkKeys k = fw::map_keys(true, false, [&](int vk) { return down_in(held, vk); }, cfg);
  CHECK(k.forward);
  CHECK(!k.back);
  CHECK(!k.left);
  CHECK(k.right);
  CHECK(k.run);
}

TEST(map_keys_ignores_keys_when_game_not_focused) {
  const fw::Config cfg;
  const fw::WalkKeys k = fw::map_keys(false, false, [](int) { return true; }, cfg);
  CHECK(!k.forward && !k.back && !k.left && !k.right && !k.run);
}

TEST(map_keys_ignores_keys_while_paused) {
  const fw::Config cfg;
  const fw::WalkKeys k = fw::map_keys(true, true, [](int) { return true; }, cfg);
  CHECK(!k.forward && !k.back && !k.left && !k.right && !k.run);
}

TEST(edge_detector_fires_once_per_press) {
  fw::EdgeDetector e;
  CHECK(!e.update(false));
  CHECK(e.update(true));
  CHECK(!e.update(true));
  CHECK(!e.update(true));
  CHECK(!e.update(false));
  CHECK(e.update(true));
}
```

Add to `CMakeLists.txt` the test files `tests/test_config.cpp` and `tests/test_keys.cpp` in `freewalk_tests`, and the sources `src/core/text.cpp`, `src/core/config.cpp`, `src/core/keys.cpp` in `freewalk_core`.

- [ ] **Step 2: A tesztek futtatása, hogy lássuk a hibát**

Run: `cmake --build build/host`
Expected: fordítási hiba, mert a `core/config.h` és a `core/keys.h` hiányzik.

- [ ] **Step 3: A megvalósítás**

`src/core/text.h`:
```cpp
#pragma once
#include <string>

namespace fw {

// printf-formátumú szöveg std::string-be.
std::string strf(const char* format, ...)
#if defined(__GNUC__) && !defined(__MINGW32__)
    __attribute__((format(printf, 1, 2)))
#endif
    ;

}  // namespace fw
```

`src/core/text.cpp`:
```cpp
#include "core/text.h"
#include <cstdarg>
#include <cstdio>

namespace fw {

std::string strf(const char* format, ...) {
  va_list args;
  va_start(args, format);
  va_list copy;
  va_copy(copy, args);
  const int length = std::vsnprintf(nullptr, 0, format, copy);
  va_end(copy);
  std::string result;
  if (length > 0) {
    result.resize(static_cast<std::size_t>(length) + 1);
    std::vsnprintf(result.data(), result.size(), format, args);
    result.resize(static_cast<std::size_t>(length));
  }
  va_end(args);
  return result;
}

}  // namespace fw
```

`src/core/config.h`:
```cpp
#pragma once
#include <string>
#include <string_view>
#include <vector>

namespace fw {

struct Config {
  // [keys] - Windows virtuális billentyűkódok
  int key_toggle = 0x79;  // F10
  int key_forward = 'W';
  int key_back = 'S';
  int key_left = 'A';
  int key_right = 'D';
  int key_run = 0x10;  // Shift

  // [walker]
  double walk_speed = 1.4;
  double run_speed = 4.0;
  double eye_height = 1.7;
  bool head_bob = true;
  double head_bob_amplitude = 0.03;
  double exit_side_distance = 1.0;
  double ground_offset = 0.0;
  double max_distance = 150.0;

  // [debug]
  bool discovery_log = false;
  bool write_on_frame_start = false;
};

// Windows virtuális billentyűkód egy névből ("F10", "W", "SHIFT", "0x79"); -1, ha a név ismeretlen.
int parse_key(std::string_view name);

// INI szövegből olvassa a beállításokat.
// Minden hiányzó vagy hibás érték az alapértéken marad, a hibás értékekről egy-egy sor kerül a warnings-ba.
Config parse_config(std::string_view text, std::vector<std::string>& warnings);

}  // namespace fw
```

`src/core/config.cpp`:
```cpp
#include "core/config.h"
#include <cctype>
#include <cstdlib>
#include <functional>
#include <map>
#include "core/text.h"

namespace fw {
namespace {

std::string_view trim(std::string_view s) {
  const char* ws = " \t\r\n";
  const auto begin = s.find_first_not_of(ws);
  if (begin == std::string_view::npos) return {};
  const auto end = s.find_last_not_of(ws);
  return s.substr(begin, end - begin + 1);
}

std::string to_upper(std::string_view s) {
  std::string r(s);
  for (char& c : r) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  return r;
}

std::string to_lower(std::string_view s) {
  std::string r(s);
  for (char& c : r) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return r;
}

bool parse_number(std::string_view text, double& out) {
  const std::string s(text);
  if (s.empty()) return false;
  char* end = nullptr;
  out = std::strtod(s.c_str(), &end);
  return end == s.c_str() + s.size();
}

struct NumberField {
  double* value;
  double min;
  double max;
};

}  // namespace

int parse_key(std::string_view name) {
  const std::string n = to_upper(trim(name));
  if (n.empty()) return -1;
  if (n.size() == 1 && std::isalnum(static_cast<unsigned char>(n[0]))) return n[0];

  static const std::map<std::string, int, std::less<>> named = {
      {"SHIFT", 0x10}, {"CTRL", 0x11},   {"CONTROL", 0x11}, {"ALT", 0x12},   {"SPACE", 0x20},
      {"TAB", 0x09},   {"LSHIFT", 0xA0}, {"RSHIFT", 0xA1},  {"LCTRL", 0xA2}, {"RCTRL", 0xA3}};
  if (const auto it = named.find(n); it != named.end()) return it->second;

  if (n.size() > 2 && n[0] == '0' && n[1] == 'X') {
    char* end = nullptr;
    const long v = std::strtol(n.c_str() + 2, &end, 16);
    return (*end == '\0' && v > 0 && v < 255) ? static_cast<int>(v) : -1;
  }
  if (n[0] == 'F' && n.size() <= 3) {
    char* end = nullptr;
    const long v = std::strtol(n.c_str() + 1, &end, 10);
    if (*end == '\0' && v >= 1 && v <= 24) return 0x6F + static_cast<int>(v);
  }
  return -1;
}

Config parse_config(std::string_view text, std::vector<std::string>& warnings) {
  Config c;
  const std::map<std::string, int*, std::less<>> keys = {
      {"keys.toggle", &c.key_toggle}, {"keys.forward", &c.key_forward}, {"keys.back", &c.key_back},
      {"keys.left", &c.key_left},     {"keys.right", &c.key_right},     {"keys.run", &c.key_run}};
  const std::map<std::string, NumberField, std::less<>> numbers = {
      {"walker.walk_speed", {&c.walk_speed, 0.1, 20.0}},
      {"walker.run_speed", {&c.run_speed, 0.1, 20.0}},
      {"walker.eye_height", {&c.eye_height, 0.5, 3.0}},
      {"walker.head_bob_amplitude", {&c.head_bob_amplitude, 0.0, 0.2}},
      {"walker.exit_side_distance", {&c.exit_side_distance, 0.3, 5.0}},
      {"walker.ground_offset", {&c.ground_offset, -5.0, 5.0}},
      {"walker.max_distance", {&c.max_distance, 10.0, 2000.0}}};
  const std::map<std::string, bool*, std::less<>> flags = {
      {"walker.head_bob", &c.head_bob},
      {"debug.discovery_log", &c.discovery_log},
      {"debug.write_on_frame_start", &c.write_on_frame_start}};

  if (text.substr(0, 3) == "\xEF\xBB\xBF") text.remove_prefix(3);

  std::string section;
  int line_number = 0;
  while (!text.empty()) {
    const auto newline = text.find('\n');
    std::string_view line = text.substr(0, newline);
    text = newline == std::string_view::npos ? std::string_view{} : text.substr(newline + 1);
    ++line_number;

    if (const auto comment = line.find_first_of(";#"); comment != std::string_view::npos) line = line.substr(0, comment);
    line = trim(line);
    if (line.empty()) continue;

    if (line.front() == '[' && line.back() == ']') {
      section = to_lower(trim(line.substr(1, line.size() - 2)));
      continue;
    }
    const auto equals = line.find('=');
    if (equals == std::string_view::npos) {
      warnings.push_back(strf("line %d: expected 'name = value', ignored", line_number));
      continue;
    }
    const std::string key = section + "." + to_lower(trim(line.substr(0, equals)));
    const std::string_view value = trim(line.substr(equals + 1));
    const std::string where = strf("line %d (%s): ", line_number, key.c_str());

    if (const auto k = keys.find(key); k != keys.end()) {
      const int vk = parse_key(value);
      if (vk < 0) warnings.push_back(where + "unknown key name, the default is kept");
      else *k->second = vk;
    } else if (const auto n = numbers.find(key); n != numbers.end()) {
      double v = 0;
      if (!parse_number(value, v) || v < n->second.min || v > n->second.max)
        warnings.push_back(where + strf("must be a number between %g and %g, the default is kept", n->second.min, n->second.max));
      else *n->second.value = v;
    } else if (const auto f = flags.find(key); f != flags.end()) {
      if (value == "0") *f->second = false;
      else if (value == "1") *f->second = true;
      else warnings.push_back(where + "must be 0 or 1, the default is kept");
    } else {
      warnings.push_back(where + "unknown setting, ignored");
    }
  }
  return c;
}

}  // namespace fw
```

`src/core/keys.h`:
```cpp
#pragma once
#include <functional>
#include "core/config.h"

namespace fw {

struct WalkKeys {
  bool forward = false;
  bool back = false;
  bool left = false;
  bool right = false;
  bool run = false;
};

// A séta billentyűi a beállítások szerint.
// Ha a játék ablaka nincs előtérben vagy a játék szünetel, egyik sem számít lenyomottnak, hogy más programban vagy a menüben gépelve ne mozduljon a gyalogló.
WalkKeys map_keys(bool game_focused, bool paused, const std::function<bool(int)>& is_down, const Config& cfg);

// Egy billentyű lenyomásának pillanatát jelzi: csak az első képkockában ad igazat, nyomva tartásnál nem ismétel.
class EdgeDetector {
 public:
  bool update(bool down) {
    const bool pressed = down && !was_down_;
    was_down_ = down;
    return pressed;
  }

 private:
  bool was_down_ = false;
};

}  // namespace fw
```

`src/core/keys.cpp`:
```cpp
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

}  // namespace fw
```

- [ ] **Step 4: A tesztek futtatása**

Run: `cmake --build build/host && ctest --test-dir build/host --output-on-failure`
Expected: minden teszt `ok`, `0 failure(s)`.

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt src tests
git commit -m "Add INI config parsing, key names and walk key mapping"
```

---

### Task 3: A gyalogló

**Files:**
- Create: `src/core/walker.h`, `src/core/walker.cpp`, `tests/test_walker.cpp`
- Modify: `CMakeLists.txt` (`src/core/walker.cpp` a `freewalk_core`-ba, `tests/test_walker.cpp` a tesztekbe)

**Interfaces:**
- Consumes: `fw::Vec3`, `forward_from_yaw`, `right_from_yaw` (Task 1); `fw::Config`, `fw::WalkKeys` (Task 2).
- Produces: `constexpr double fw::kMaxStepSeconds = 0.1`; `class fw::Walker { void reset(Vec3 feet); void step(const WalkKeys&, double yaw, double dt, const Config&); Vec3 feet() const; Vec3 eye(const Config&) const; }`.

- [ ] **Step 1: A tesztek megírása**

`tests/test_walker.cpp`:
```cpp
#include <algorithm>
#include <cmath>
#include "check.h"
#include "core/walker.h"

namespace {
constexpr double kPi = 3.14159265358979323846;

fw::WalkKeys keys(bool f, bool b, bool l, bool r, bool run = false) {
  fw::WalkKeys k;
  k.forward = f;
  k.back = b;
  k.left = l;
  k.right = r;
  k.run = run;
  return k;
}

void walk_for(fw::Walker& w, const fw::WalkKeys& k, double yaw, double seconds, const fw::Config& cfg) {
  for (int i = 0; i < static_cast<int>(std::lround(seconds / 0.1)); ++i) w.step(k, yaw, 0.1, cfg);
}
}  // namespace

TEST(walks_north_at_walk_speed) {
  const fw::Config cfg;
  fw::Walker w;
  w.reset({10, 5, 20});
  walk_for(w, keys(true, false, false, false), 0.0, 1.0, cfg);
  CHECK_NEAR(w.feet().x, 10.0, 1e-9);
  CHECK_NEAR(w.feet().z, 20.0 - 1.4, 1e-9);
  CHECK_NEAR(w.feet().y, 5.0, 1e-12);
}

TEST(walks_west_when_yaw_is_quarter_turn) {
  const fw::Config cfg;
  fw::Walker w;
  w.reset({0, 0, 0});
  walk_for(w, keys(true, false, false, false), kPi / 2, 1.0, cfg);
  CHECK_NEAR(w.feet().x, -1.4, 1e-9);
  CHECK_NEAR(w.feet().z, 0.0, 1e-9);
}

TEST(diagonal_is_not_faster) {
  const fw::Config cfg;
  fw::Walker w;
  w.reset({0, 0, 0});
  walk_for(w, keys(true, false, false, true), 0.0, 1.0, cfg);
  CHECK_NEAR(fw::horizontal_distance(w.feet(), {0, 0, 0}), 1.4, 1e-9);
  CHECK(w.feet().x > 0.0);
  CHECK(w.feet().z < 0.0);
}

TEST(run_uses_run_speed) {
  const fw::Config cfg;
  fw::Walker w;
  w.reset({0, 0, 0});
  walk_for(w, keys(true, false, false, false, true), 0.0, 1.0, cfg);
  CHECK_NEAR(w.feet().z, -4.0, 1e-9);
}

TEST(opposite_keys_cancel) {
  const fw::Config cfg;
  fw::Walker w;
  w.reset({1, 2, 3});
  walk_for(w, keys(true, true, true, true), 0.7, 1.0, cfg);
  CHECK_NEAR(w.feet().x, 1.0, 1e-12);
  CHECK_NEAR(w.feet().z, 3.0, 1e-12);
}

TEST(long_frame_moves_at_most_max_step) {
  const fw::Config cfg;
  fw::Walker w;
  w.reset({0, 0, 0});
  w.step(keys(true, false, false, false), 0.0, 5.0, cfg);
  CHECK_NEAR(w.feet().z, -1.4 * fw::kMaxStepSeconds, 1e-9);
}

TEST(eye_height_without_bob_when_standing) {
  const fw::Config cfg;
  fw::Walker w;
  w.reset({0, 10, 0});
  CHECK_NEAR(w.eye(cfg).y, 11.7, 1e-12);
  walk_for(w, keys(true, false, false, false), 0.0, 1.0, cfg);
  walk_for(w, keys(false, false, false, false), 0.0, 0.5, cfg);
  CHECK_NEAR(w.eye(cfg).y, 11.7, 1e-12);
}

TEST(head_bobs_while_walking_within_amplitude) {
  const fw::Config cfg;
  fw::Walker w;
  w.reset({0, 0, 0});
  double largest = 0;
  for (int i = 0; i < 40; ++i) {
    w.step(keys(true, false, false, false), 0.0, 0.05, cfg);
    largest = std::max(largest, std::fabs(w.eye(cfg).y - cfg.eye_height));
  }
  CHECK(largest > 0.01);
  CHECK(largest <= cfg.head_bob_amplitude + 1e-12);
}

TEST(head_bob_can_be_switched_off) {
  fw::Config cfg;
  cfg.head_bob = false;
  fw::Walker w;
  w.reset({0, 0, 0});
  for (int i = 0; i < 40; ++i) {
    w.step(keys(true, false, false, false), 0.0, 0.05, cfg);
    CHECK_NEAR(w.eye(cfg).y, cfg.eye_height, 1e-12);
  }
}
```

- [ ] **Step 2: A tesztek futtatása, hogy lássuk a hibát**

Run: `cmake --build build/host`
Expected: fordítási hiba, mert a `core/walker.h` hiányzik.

- [ ] **Step 3: A megvalósítás**

`src/core/walker.h`:
```cpp
#pragma once
#include "core/config.h"
#include "core/keys.h"
#include "core/vec.h"

namespace fw {

// Egy képkocka legfeljebb ennyi időt léptet, hogy egy akadás vagy szünet után a gyalogló ne ugorjon nagyot.
inline constexpr double kMaxStepSeconds = 0.1;

// A gyalogló helyzete és mozgása sík talajon.
class Walker {
 public:
  // A gyalogló a megadott talajpontra áll; a talaj ettől kezdve ebben a magasságban sík.
  void reset(Vec3 feet);

  // Egy képkocka mozgása.
  // A yaw a nézési irány radiánban, felülről nézve az óramutatóval ellentétesen, 0 = észak.
  void step(const WalkKeys& keys, double yaw, double dt, const Config& cfg);

  Vec3 feet() const { return feet_; }
  Vec3 eye(const Config& cfg) const;

 private:
  Vec3 feet_;
  double bob_phase_ = 0;
  double bob_blend_ = 0;
};

}  // namespace fw
```

`src/core/walker.cpp`:
```cpp
#include "core/walker.h"
#include <algorithm>
#include <cmath>

namespace fw {
namespace {

constexpr double kTwoPi = 6.28318530717958647692;
// Egy lépés hossza; a fej minden lépésnél egyszer emelkedik és süllyed.
constexpr double kStepLength = 0.75;
// Ennyi idő alatt indul el és áll le a fejbiccentés, hogy megálláskor ne ugorjon a kép.
constexpr double kBobFadeSeconds = 0.25;

}  // namespace

void Walker::reset(Vec3 feet) {
  feet_ = feet;
  bob_phase_ = 0;
  bob_blend_ = 0;
}

void Walker::step(const WalkKeys& keys, double yaw, double dt, const Config& cfg) {
  dt = std::clamp(dt, 0.0, kMaxStepSeconds);
  const double ahead = (keys.forward ? 1.0 : 0.0) - (keys.back ? 1.0 : 0.0);
  const double side = (keys.right ? 1.0 : 0.0) - (keys.left ? 1.0 : 0.0);
  const Vec3 direction = forward_from_yaw(yaw) * ahead + right_from_yaw(yaw) * side;
  const double length = std::hypot(direction.x, direction.z);
  const bool moving = length > 0;

  if (moving) {
    const double speed = keys.run ? cfg.run_speed : cfg.walk_speed;
    const double distance = speed * dt;
    feet_ = feet_ + direction * (distance / length);
    bob_phase_ = std::fmod(bob_phase_ + kTwoPi * distance / kStepLength, kTwoPi);
  }

  const double target = moving ? 1.0 : 0.0;
  const double change = dt / kBobFadeSeconds;
  bob_blend_ = bob_blend_ < target ? std::min(target, bob_blend_ + change) : std::max(target, bob_blend_ - change);
}

Vec3 Walker::eye(const Config& cfg) const {
  const double bob = cfg.head_bob ? cfg.head_bob_amplitude * std::sin(bob_phase_) * bob_blend_ : 0.0;
  return {feet_.x, feet_.y + cfg.eye_height + bob, feet_.z};
}

}  // namespace fw
```

- [ ] **Step 4: A tesztek futtatása**

Run: `cmake --build build/host && ctest --test-dir build/host --output-on-failure`
Expected: minden teszt `ok`, `0 failure(s)`.

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt src tests
git commit -m "Add flat-ground walker with run and head bob"
```

---

### Task 4: A séta állapotgépe

**Files:**
- Create: `src/core/walk_session.h`, `src/core/walk_session.cpp`, `tests/test_walk_session.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `fw::Vec3`, `rotate_heading`, `horizontal_distance` (Task 1); `fw::Config` (Task 2).
- Produces:
  - `struct fw::TruckState { bool valid; Vec3 position; double heading; double speed; bool parking_brake; bool head_known; Vec3 head_in_truck; }`.
  - `enum class fw::Phase { Driving, Switching, Walking }`; `enum class fw::Command { None, RequestFreeCamera, StartWalk, Walk, Return, ReturnKeepCamera }`.
  - `struct fw::SessionInput { TruckState truck; bool toggle_pressed; int active_camera; int free_camera; double dt; Vec3 walker_feet; }`; `struct fw::SessionOutput { Command command; std::string reason; Vec3 start_feet; double start_yaw; }`.
  - Konstansok: `kMaxParkedSpeed`, `kSwitchTimeout`, `kTruckJump`.
  - `Vec3 fw::exit_point(const TruckState&, const Config&)`; `class fw::WalkSession { SessionOutput update(const SessionInput&, const Config&); SessionOutput force_return(const std::string& reason); Phase phase() const; }`.

- [ ] **Step 1: A tesztek megírása**

`tests/test_walk_session.cpp`:
```cpp
#include "check.h"
#include "core/walk_session.h"

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr int kFree = 3;
constexpr int kInterior = 0;

fw::TruckState parked() {
  fw::TruckState t;
  t.valid = true;
  t.position = {1000.0, 50.0, -2000.0};
  t.heading = 0.0;
  t.speed = 0.0;
  t.parking_brake = true;
  t.head_known = true;
  t.head_in_truck = {-0.6, 2.4, -3.5};
  return t;
}

fw::SessionInput input(const fw::TruckState& t, bool toggle, int active) {
  fw::SessionInput in;
  in.truck = t;
  in.toggle_pressed = toggle;
  in.active_camera = active;
  in.free_camera = kFree;
  in.dt = 0.1;
  in.walker_feet = t.position;
  return in;
}

// Elindít egy sétát: kiszállás kérése, majd a szabad kamera aktívvá válása.
fw::WalkSession walking_session(const fw::Config& cfg) {
  fw::WalkSession s;
  s.update(input(parked(), true, kInterior), cfg);
  s.update(input(parked(), false, kFree), cfg);
  return s;
}
}  // namespace

TEST(nothing_happens_without_toggle) {
  const fw::Config cfg;
  fw::WalkSession s;
  const auto out = s.update(input(parked(), false, kInterior), cfg);
  CHECK(out.command == fw::Command::None);
  CHECK(out.reason.empty());
  CHECK(s.phase() == fw::Phase::Driving);
}

TEST(refuses_when_preconditions_fail) {
  const fw::Config cfg;
  struct Case { fw::TruckState truck; int free; const char* reason; };
  fw::TruckState moving = parked();
  moving.speed = 1.0;
  fw::TruckState no_brake = parked();
  no_brake.parking_brake = false;
  fw::TruckState no_config = parked();
  no_config.head_known = false;
  fw::TruckState no_data = parked();
  no_data.valid = false;
  const Case cases[] = {{moving, kFree, "the truck is moving"},
                        {no_brake, kFree, "the parking brake is off"},
                        {no_config, kFree, "the truck configuration has not arrived yet"},
                        {no_data, kFree, "no truck position from the telemetry yet"},
                        {parked(), -1, "the game's free camera was not found"}};
  for (const auto& c : cases) {
    fw::WalkSession s;
    auto in = input(c.truck, true, kInterior);
    in.free_camera = c.free;
    const auto out = s.update(in, cfg);
    CHECK(out.command == fw::Command::None);
    CHECK(out.reason == c.reason);
    CHECK(s.phase() == fw::Phase::Driving);
  }
}

TEST(toggle_requests_free_camera_then_starts_walk_at_driver_door) {
  const fw::Config cfg;
  fw::WalkSession s;
  const auto first = s.update(input(parked(), true, kInterior), cfg);
  CHECK(first.command == fw::Command::RequestFreeCamera);
  CHECK(s.phase() == fw::Phase::Switching);

  const auto waiting = s.update(input(parked(), false, kInterior), cfg);
  CHECK(waiting.command == fw::Command::None);

  const auto started = s.update(input(parked(), false, kFree), cfg);
  CHECK(started.command == fw::Command::StartWalk);
  CHECK(s.phase() == fw::Phase::Walking);
  // Bal kormányos kamion (a fej x-e negatív): 1 m-rel a fejpont mellett, balra, a talajon.
  CHECK_NEAR(started.start_feet.x, 1000.0 - 1.6, 1e-9);
  CHECK_NEAR(started.start_feet.y, 50.0, 1e-12);
  CHECK_NEAR(started.start_feet.z, -2000.0 - 3.5, 1e-9);
  CHECK_NEAR(started.start_yaw, 0.0, 1e-12);
}

TEST(exit_point_follows_truck_heading) {
  const fw::Config cfg;
  fw::TruckState t = parked();
  t.heading = kPi;  // dél felé néz, a vezető bal keze kelet felé van
  const fw::Vec3 p = fw::exit_point(t, cfg);
  CHECK_NEAR(p.x, 1001.6, 1e-9);
  CHECK_NEAR(p.z, -1996.5, 1e-9);
}

TEST(right_hand_drive_exits_on_the_right) {
  const fw::Config cfg;
  fw::TruckState t = parked();
  t.head_in_truck.x = 0.6;
  const fw::Vec3 p = fw::exit_point(t, cfg);
  CHECK_NEAR(p.x, 1001.6, 1e-9);
}

TEST(ground_offset_moves_exit_point_vertically) {
  fw::Config cfg;
  cfg.ground_offset = -0.4;
  CHECK_NEAR(fw::exit_point(parked(), cfg).y, 49.6, 1e-12);
}

TEST(switch_timeout_returns) {
  const fw::Config cfg;
  fw::WalkSession s;
  s.update(input(parked(), true, kInterior), cfg);
  // 0,1 s-os képkockákkal a 3 s-os időkorlát a 30. vagy a 31. képkockán jár le (a lebegőpontos összeadás miatt).
  int frames = 0;
  fw::SessionOutput out;
  while (out.command == fw::Command::None && frames < 40) {
    out = s.update(input(parked(), false, kInterior), cfg);
    ++frames;
  }
  CHECK(out.command == fw::Command::Return);
  CHECK(frames >= 30 && frames <= 31);
  CHECK(s.phase() == fw::Phase::Driving);
}

TEST(walking_continues_normally) {
  const fw::Config cfg;
  auto s = walking_session(cfg);
  const auto out = s.update(input(parked(), false, kFree), cfg);
  CHECK(out.command == fw::Command::Walk);
  CHECK(s.phase() == fw::Phase::Walking);
}

TEST(toggle_while_walking_returns) {
  const fw::Config cfg;
  auto s = walking_session(cfg);
  const auto out = s.update(input(parked(), true, kFree), cfg);
  CHECK(out.command == fw::Command::Return);
  CHECK(s.phase() == fw::Phase::Driving);
}

TEST(other_camera_returns_without_restoring_camera) {
  const fw::Config cfg;
  auto s = walking_session(cfg);
  const auto out = s.update(input(parked(), false, 5), cfg);
  CHECK(out.command == fw::Command::ReturnKeepCamera);
  CHECK(s.phase() == fw::Phase::Driving);
}

TEST(truck_jump_returns_but_small_move_does_not) {
  const fw::Config cfg;
  auto s = walking_session(cfg);
  fw::TruckState t = parked();
  t.position.x += 4.0;
  CHECK(s.update(input(t, false, kFree), cfg).command == fw::Command::Walk);
  t.position.x += 6.0;
  CHECK(s.update(input(t, false, kFree), cfg).command == fw::Command::Return);
}

TEST(walking_too_far_returns) {
  const fw::Config cfg;
  auto s = walking_session(cfg);
  auto in = input(parked(), false, kFree);
  in.walker_feet = {1000.0 + 151.0, 50.0, -2000.0};
  CHECK(s.update(in, cfg).command == fw::Command::Return);
}

TEST(force_return_only_when_not_driving) {
  const fw::Config cfg;
  fw::WalkSession idle;
  CHECK(idle.force_return("unload").command == fw::Command::None);
  auto s = walking_session(cfg);
  const auto out = s.force_return("unload");
  CHECK(out.command == fw::Command::Return);
  CHECK(out.reason == "unload");
  CHECK(s.phase() == fw::Phase::Driving);
}
```

- [ ] **Step 2: A tesztek futtatása, hogy lássuk a hibát**

Run: `cmake --build build/host`
Expected: fordítási hiba, mert a `core/walk_session.h` hiányzik.

- [ ] **Step 3: A megvalósítás**

`src/core/walk_session.h`:
```cpp
#pragma once
#include <string>
#include "core/config.h"
#include "core/vec.h"

namespace fw {

struct TruckState {
  bool valid = false;        // érkezett már helyzetadat a telemetriától
  Vec3 position;             // világkoordináta, méter
  double heading = 0;        // radián, felülről nézve az óramutatóval ellentétesen, 0 = észak
  double speed = 0;          // m/s, hátramenetben negatív
  bool parking_brake = false;
  bool head_known = false;   // megérkezett a kamion konfigurációja
  Vec3 head_in_truck;        // a vezető feje a kamion saját koordinátáiban (x jobbra, y fel, z hátra)
};

enum class Phase { Driving, Switching, Walking };

enum class Command {
  None,               // nincs teendő; ha a reason nem üres, a kiszállás elmaradt, és a reason az oka
  RequestFreeCamera,  // kiszállás indul: a szabad kamerát kell kérni
  StartWalk,          // a szabad kamera aktív: a gyalogló a start_feet pontra áll
  Walk,               // séta közben egy képkocka
  Return,             // visszaszállás, a korábbi kamera visszaállításával
  ReturnKeepCamera,   // visszaszállás, de a játékos által választott kamera marad
};

struct SessionInput {
  TruckState truck;
  bool toggle_pressed = false;  // a séta billentyű lenyomásának pillanata
  int active_camera = -1;       // a játék aktív kamerájának sorszáma, -1 ha nem olvasható
  int free_camera = -1;         // a szabad kamera sorszáma, -1 ha nem található
  double dt = 0;                // az előző képkocka óta eltelt idő, másodperc
  Vec3 walker_feet;
};

struct SessionOutput {
  Command command = Command::None;
  std::string reason;
  Vec3 start_feet;
  double start_yaw = 0;
};

inline constexpr double kMaxParkedSpeed = 0.5 / 3.6;  // m/s
inline constexpr double kSwitchTimeout = 3.0;         // s
inline constexpr double kTruckJump = 5.0;             // m, ennyinél nagyobb ugrás két képkocka között áthelyezést jelent

// A kiszállás helye: a vezető oldalán, a fülkén kívül, a talajon.
Vec3 exit_point(const TruckState& truck, const Config& cfg);

// Eldönti, mikor kezdődik és mikor ér véget a séta; a végrehajtás a hívó dolga.
class WalkSession {
 public:
  SessionOutput update(const SessionInput& in, const Config& cfg);

  // A játék leállásakor vagy egy végrehajtási hiba után: ha séta vagy átkapcsolás van folyamatban, visszaszállást kér.
  SessionOutput force_return(const std::string& reason);

  Phase phase() const { return phase_; }

 private:
  Phase phase_ = Phase::Driving;
  double switch_elapsed_ = 0;
  Vec3 last_truck_;
};

}  // namespace fw
```

`src/core/walk_session.cpp`:
```cpp
#include "core/walk_session.h"
#include <cmath>

namespace fw {

Vec3 exit_point(const TruckState& truck, const Config& cfg) {
  const double side = truck.head_in_truck.x < 0 ? -1.0 : 1.0;
  const Vec3 local{truck.head_in_truck.x + side * cfg.exit_side_distance, 0.0, truck.head_in_truck.z};
  Vec3 point = truck.position + rotate_heading(local, truck.heading);
  point.y = truck.position.y + cfg.ground_offset;
  return point;
}

SessionOutput WalkSession::update(const SessionInput& in, const Config& cfg) {
  SessionOutput out;
  switch (phase_) {
    case Phase::Driving:
      if (!in.toggle_pressed) return out;
      if (!in.truck.valid) out.reason = "no truck position from the telemetry yet";
      else if (std::fabs(in.truck.speed) >= kMaxParkedSpeed) out.reason = "the truck is moving";
      else if (!in.truck.parking_brake) out.reason = "the parking brake is off";
      else if (!in.truck.head_known) out.reason = "the truck configuration has not arrived yet";
      else if (in.free_camera < 0) out.reason = "the game's free camera was not found";
      else {
        out.command = Command::RequestFreeCamera;
        out.reason = "leaving the truck";
        phase_ = Phase::Switching;
        switch_elapsed_ = 0;
      }
      return out;

    case Phase::Switching:
      if (in.active_camera == in.free_camera) {
        out.command = Command::StartWalk;
        out.reason = "the free camera is active";
        out.start_feet = exit_point(in.truck, cfg);
        out.start_yaw = in.truck.heading;
        last_truck_ = in.truck.position;
        phase_ = Phase::Walking;
        return out;
      }
      switch_elapsed_ += in.dt;
      if (switch_elapsed_ > kSwitchTimeout) {
        out.command = Command::Return;
        out.reason = "the game did not switch to the free camera within 3 s";
        phase_ = Phase::Driving;
      }
      return out;

    case Phase::Walking: {
      const double truck_moved = horizontal_distance(in.truck.position, last_truck_);
      last_truck_ = in.truck.position;
      if (in.toggle_pressed) {
        out.command = Command::Return;
        out.reason = "the walk key was pressed";
      } else if (in.active_camera != in.free_camera) {
        out.command = Command::ReturnKeepCamera;
        out.reason = "another camera was selected";
      } else if (truck_moved > kTruckJump) {
        out.command = Command::Return;
        out.reason = "the truck was moved";
      } else if (horizontal_distance(in.walker_feet, in.truck.position) > cfg.max_distance) {
        out.command = Command::Return;
        out.reason = "too far from the truck";
      } else {
        out.command = Command::Walk;
        return out;
      }
      phase_ = Phase::Driving;
      return out;
    }
  }
  return out;
}

SessionOutput WalkSession::force_return(const std::string& reason) {
  SessionOutput out;
  if (phase_ != Phase::Driving) {
    out.command = Command::Return;
    out.reason = reason;
    phase_ = Phase::Driving;
  }
  return out;
}

}  // namespace fw
```

- [ ] **Step 4: A tesztek futtatása**

Run: `cmake --build build/host && ctest --test-dir build/host --output-on-failure`
Expected: minden teszt `ok`, `0 failure(s)`.

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt src tests
git commit -m "Add walk session state machine and exit point"
```

---

### Task 5: Memória, PE kép és RTTI olvasása

**Files:**
- Create: `src/core/memory.h`, `src/core/pe_image.h`, `src/core/pe_image.cpp`, `src/core/file_image.h`, `src/core/file_image.cpp`, `src/core/rtti.h`, `src/core/rtti.cpp`, `tests/pe_builder.h`, `tests/test_pe_image.cpp`, `tests/test_rtti.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: semmi a korábbiakból.
- Produces:
  - `class fw::Memory { virtual bool read(uint64_t address, void* out, size_t size) const = 0; template<class T> std::optional<T> get(uint64_t) const; }`.
  - `class fw::BufferMemory : Memory { BufferMemory(uint64_t base, std::vector<uint8_t>); template<class T> void put(uint64_t, const T&); void put_bytes(uint64_t, const void*, size_t); uint64_t base() const; }`.
  - `struct fw::Section`, `class fw::PeImage { static std::optional<PeImage> parse(const Memory&, uint64_t base); base(), preferred_base(), timestamp(), image_size(), headers_size(), sections(), section(name), in_section(name, address), read_section(const Memory&, const Section&) -> SectionBytes; }`.
  - `struct fw::SectionBytes { uint64_t address; std::vector<uint8_t> bytes; }`, `find_cstring`, `find_qword`, `struct fw::RipRef { uint64_t address; uint8_t opcode; int reg; uint64_t target; }`, `decode_rip_ref`, `find_rip_refs_to`.
  - `std::optional<BufferMemory> fw::load_pe_file(const std::string& path, std::string& error)`.
  - `std::string fw::rtti_class_name(const Memory&, uint64_t object)`.
  - Teszt segédek a `tests/pe_builder.h`-ban: `fw::BufferMemory fwtest::make_test_image()` (bázis `fwtest::kBase = 0x140000000`, `kText` = `+0x1000`, `kRdata` = `+0x2000`, `kData` = `+0x3000`, szakaszokon kívüli `kHeap` = `+0x4000`, teljes méret `0x6000`), `fwtest::put_rip_insn(m, at, rex, opcode, reg, target)` és `fwtest::put_rtti_object(m, object, vtable, locator, type_descriptor, name)`.

- [ ] **Step 1: A szintetikus PE kép segéd és a tesztek megírása**

`tests/pe_builder.h`:
```cpp
#pragma once
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include "core/memory.h"

namespace fwtest {

inline constexpr std::uint64_t kBase = 0x140000000ull;
inline constexpr std::uint64_t kText = kBase + 0x1000;
inline constexpr std::uint64_t kRdata = kBase + 0x2000;
inline constexpr std::uint64_t kData = kBase + 0x3000;
inline constexpr std::uint64_t kHeap = kBase + 0x4000;  // szakaszokon kívüli terület az objektumoknak
inline constexpr std::uint32_t kImageSize = 0x6000;
inline constexpr std::uint32_t kTimestamp = 0x6AAA8369;

// Egy memóriába leképezett 64 bites PE kép három szakasszal, a játék futtatható fájljának mintájára.
inline fw::BufferMemory make_test_image() {
  fw::BufferMemory m(kBase, std::vector<std::uint8_t>(kImageSize, 0));
  m.put<std::uint16_t>(kBase, 0x5A4D);
  m.put<std::uint32_t>(kBase + 0x3C, 0x80);
  const std::uint64_t nt = kBase + 0x80;
  m.put<std::uint32_t>(nt, 0x00004550);
  m.put<std::uint16_t>(nt + 6, 3);
  m.put<std::uint32_t>(nt + 8, kTimestamp);
  m.put<std::uint16_t>(nt + 20, 0xF0);
  const std::uint64_t opt = nt + 24;
  m.put<std::uint16_t>(opt, 0x20B);
  m.put<std::uint64_t>(opt + 24, kBase);
  m.put<std::uint32_t>(opt + 56, kImageSize);
  m.put<std::uint32_t>(opt + 60, 0x400);
  const char* names[] = {".text", ".rdata", ".data"};
  for (int i = 0; i < 3; ++i) {
    const std::uint64_t s = opt + 0xF0 + 40 * i;
    char name[8] = {};
    std::memcpy(name, names[i], std::strlen(names[i]));
    m.put_bytes(s, name, 8);
    m.put<std::uint32_t>(s + 8, 0x1000);                  // VirtualSize
    m.put<std::uint32_t>(s + 12, 0x1000 * (i + 1));       // VirtualAddress
    m.put<std::uint32_t>(s + 16, 0x1000);                 // SizeOfRawData
    m.put<std::uint32_t>(s + 20, 0x400 + 0x1000 * i);     // PointerToRawData
  }
  return m;
}

// Egy RIP-relatív REX.W utasítás bájtjai: rex, opcode, modrm (reg mező a reg alsó 3 bitje), disp32.
inline void put_rip_insn(fw::BufferMemory& m, std::uint64_t at, std::uint8_t rex, std::uint8_t opcode, int reg, std::uint64_t target) {
  const std::uint8_t bytes[3] = {rex, opcode, static_cast<std::uint8_t>(0x05 | ((reg & 7) << 3))};
  m.put_bytes(at, bytes, 3);
  m.put<std::int32_t>(at + 3, static_cast<std::int32_t>(static_cast<std::int64_t>(target) - static_cast<std::int64_t>(at + 7)));
}

// Egy polimorf objektum MSVC RTTI szerkezettel: az objektum a vtable-ra mutat, a vtable előtti mutató a locatorra, abban a típusleíró RVA-ja a névvel.
inline void put_rtti_object(fw::BufferMemory& m, std::uint64_t object, std::uint64_t vtable, std::uint64_t locator, std::uint64_t type_descriptor, const std::string& name) {
  m.put<std::uint32_t>(locator, 1);
  m.put<std::uint32_t>(locator + 4, 0);
  m.put<std::uint32_t>(locator + 8, 0);
  m.put<std::int32_t>(locator + 12, static_cast<std::int32_t>(type_descriptor - kBase));
  m.put<std::int32_t>(locator + 16, 0);
  m.put<std::int32_t>(locator + 20, static_cast<std::int32_t>(locator - kBase));
  m.put_bytes(type_descriptor + 16, name.c_str(), name.size() + 1);
  m.put<std::uint64_t>(vtable - 8, locator);
  m.put<std::uint64_t>(object, vtable);
}

}  // namespace fwtest
```

`tests/test_pe_image.cpp`:
```cpp
#include <cstring>
#include "check.h"
#include "core/pe_image.h"
#include "pe_builder.h"

TEST(parses_headers_and_sections) {
  const auto m = fwtest::make_test_image();
  const auto img = fw::PeImage::parse(m, fwtest::kBase);
  CHECK(img.has_value());
  CHECK(img->timestamp() == fwtest::kTimestamp);
  CHECK(img->image_size() == fwtest::kImageSize);
  CHECK(img->preferred_base() == fwtest::kBase);
  CHECK(img->sections().size() == 3);
  CHECK(img->section(".rdata") != nullptr);
  CHECK(img->section(".rdata")->rva == 0x2000);
  CHECK(img->in_section(".text", fwtest::kText + 0x10));
  CHECK(!img->in_section(".text", fwtest::kRdata));
  CHECK(!img->in_section(".data", fwtest::kHeap));
}

TEST(rejects_non_pe_memory) {
  fw::BufferMemory m(0x1000, std::vector<std::uint8_t>(0x200, 0));
  CHECK(!fw::PeImage::parse(m, 0x1000).has_value());
}

TEST(find_cstring_requires_exact_terminated_string) {
  auto m = fwtest::make_test_image();
  const char longer[] = "my_camera_manager";
  const char exact[] = "camera_manager";
  m.put_bytes(fwtest::kRdata + 0x10, longer, sizeof longer);
  m.put_bytes(fwtest::kRdata + 0x40, exact, sizeof exact);
  const auto img = fw::PeImage::parse(m, fwtest::kBase);
  const auto rdata = img->read_section(m, *img->section(".rdata"));
  const auto found = fw::find_cstring(rdata, "camera_manager");
  CHECK(found.has_value());
  CHECK(*found == fwtest::kRdata + 0x40);
  CHECK(!fw::find_cstring(rdata, "debug_camera").has_value());
}

TEST(find_qword_finds_aligned_values) {
  auto m = fwtest::make_test_image();
  m.put<std::uint64_t>(fwtest::kData + 0x18, 0x1122334455667788ull);
  m.put<std::uint64_t>(fwtest::kData + 0x80, 0x1122334455667788ull);
  const auto img = fw::PeImage::parse(m, fwtest::kBase);
  const auto data = img->read_section(m, *img->section(".data"));
  const auto hits = fw::find_qword(data, 0x1122334455667788ull);
  CHECK(hits.size() == 2);
  CHECK(hits[0] == fwtest::kData + 0x18);
}

TEST(decode_rip_ref_variants) {
  auto m = fwtest::make_test_image();
  fwtest::put_rip_insn(m, fwtest::kText, 0x48, 0x89, 0, fwtest::kData + 0x100);       // mov [rip+x], rax
  fwtest::put_rip_insn(m, fwtest::kText + 0x10, 0x4C, 0x8D, 0, fwtest::kRdata);       // lea r8, [rip+x]
  fwtest::put_rip_insn(m, fwtest::kText + 0x20, 0x48, 0x8B, 1, fwtest::kData);        // mov rcx, [rip+x]
  const std::uint8_t not_rip[7] = {0x48, 0x89, 0x45, 0x10, 0, 0, 0};                  // mov [rbp+0x10], rax
  m.put_bytes(fwtest::kText + 0x30, not_rip, 7);
  const auto img = fw::PeImage::parse(m, fwtest::kBase);
  const auto text = img->read_section(m, *img->section(".text"));

  const auto a = fw::decode_rip_ref(text.bytes.data(), text.bytes.size(), fwtest::kText);
  CHECK(a && a->opcode == 0x89 && a->reg == 0 && a->target == fwtest::kData + 0x100);
  const auto b = fw::decode_rip_ref(text.bytes.data() + 0x10, text.bytes.size() - 0x10, fwtest::kText + 0x10);
  CHECK(b && b->opcode == 0x8D && b->reg == 8 && b->target == fwtest::kRdata);
  const auto c = fw::decode_rip_ref(text.bytes.data() + 0x20, text.bytes.size() - 0x20, fwtest::kText + 0x20);
  CHECK(c && c->opcode == 0x8B && c->reg == 1);
  CHECK(!fw::decode_rip_ref(text.bytes.data() + 0x30, text.bytes.size() - 0x30, fwtest::kText + 0x30));
  CHECK(!fw::decode_rip_ref(text.bytes.data(), 6, fwtest::kText));

  const auto refs = fw::find_rip_refs_to(text, fwtest::kRdata);
  CHECK(refs.size() == 1);
  CHECK(refs[0].address == fwtest::kText + 0x10);
}
```

`tests/test_rtti.cpp`:
```cpp
#include "check.h"
#include "core/rtti.h"
#include "pe_builder.h"

TEST(reads_class_name_of_object) {
  auto m = fwtest::make_test_image();
  fwtest::put_rtti_object(m, fwtest::kHeap, fwtest::kRdata + 0x208, fwtest::kRdata + 0x100, fwtest::kData + 0x200, ".?AVdebug_camera@prism@@");
  CHECK(fw::rtti_class_name(m, fwtest::kHeap) == ".?AVdebug_camera@prism@@");
}

TEST(garbage_pointer_gives_empty_name) {
  auto m = fwtest::make_test_image();
  m.put<std::uint64_t>(fwtest::kHeap, 0xDEADBEEF00000000ull);
  CHECK(fw::rtti_class_name(m, fwtest::kHeap).empty());
  CHECK(fw::rtti_class_name(m, 0).empty());
}

TEST(wrong_locator_signature_gives_empty_name) {
  auto m = fwtest::make_test_image();
  fwtest::put_rtti_object(m, fwtest::kHeap, fwtest::kRdata + 0x208, fwtest::kRdata + 0x100, fwtest::kData + 0x200, ".?AVx@@");
  m.put<std::uint32_t>(fwtest::kRdata + 0x100, 0);
  CHECK(fw::rtti_class_name(m, fwtest::kHeap).empty());
}

TEST(name_must_look_like_rtti) {
  auto m = fwtest::make_test_image();
  fwtest::put_rtti_object(m, fwtest::kHeap, fwtest::kRdata + 0x208, fwtest::kRdata + 0x100, fwtest::kData + 0x200, "not a type name");
  CHECK(fw::rtti_class_name(m, fwtest::kHeap).empty());
}
```

Add `src/core/pe_image.cpp`, `src/core/file_image.cpp`, `src/core/rtti.cpp` to `freewalk_core`, and `tests/test_pe_image.cpp`, `tests/test_rtti.cpp` to `freewalk_tests`.

- [ ] **Step 2: A tesztek futtatása, hogy lássuk a hibát**

Run: `cmake --build build/host`
Expected: fordítási hiba, mert a `core/memory.h`, `core/pe_image.h`, `core/rtti.h` hiányzik.

- [ ] **Step 3: A megvalósítás**

`src/core/memory.h`:
```cpp
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <utility>
#include <vector>

namespace fw {

// Egy címtér olvasása: a játékban a saját folyamat memóriája, a tesztekben és az elemzőeszközben egy pufferbe betöltött kép.
class Memory {
 public:
  virtual ~Memory() = default;

  // Csak akkor ad igazat, ha mind a size bájtot sikerült beolvasni; érvénytelen címnél hamisat ad, nem omlik össze.
  virtual bool read(std::uint64_t address, void* out, std::size_t size) const = 0;

  template <class T>
  std::optional<T> get(std::uint64_t address) const {
    T value{};
    if (!read(address, &value, sizeof value)) return std::nullopt;
    return value;
  }
};

// Egy adott címen kezdődő bájttömb mint memória.
class BufferMemory final : public Memory {
 public:
  BufferMemory(std::uint64_t base, std::vector<std::uint8_t> bytes) : base_(base), bytes_(std::move(bytes)) {}

  bool read(std::uint64_t address, void* out, std::size_t size) const override {
    if (address < base_) return false;
    const std::uint64_t offset = address - base_;
    if (offset > bytes_.size() || size > bytes_.size() - offset) return false;
    std::memcpy(out, bytes_.data() + offset, size);
    return true;
  }

  template <class T>
  void put(std::uint64_t address, const T& value) {
    put_bytes(address, &value, sizeof value);
  }

  void put_bytes(std::uint64_t address, const void* data, std::size_t size) {
    std::memcpy(bytes_.data() + (address - base_), data, size);
  }

  std::uint64_t base() const { return base_; }

 private:
  std::uint64_t base_;
  std::vector<std::uint8_t> bytes_;
};

}  // namespace fw
```

`src/core/pe_image.h`:
```cpp
#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "core/memory.h"

namespace fw {

struct Section {
  std::string name;
  std::uint32_t rva = 0;
  std::uint32_t virtual_size = 0;
  std::uint32_t raw_offset = 0;
  std::uint32_t raw_size = 0;
};

// Egy szakasz bájtjainak másolata és a kezdőcímük.
struct SectionBytes {
  std::uint64_t address = 0;
  std::vector<std::uint8_t> bytes;
};

// Egy memóriába leképezett 64 bites PE kép fejlécei és szakaszai.
class PeImage {
 public:
  static std::optional<PeImage> parse(const Memory& memory, std::uint64_t base);

  std::uint64_t base() const { return base_; }
  std::uint64_t preferred_base() const { return preferred_base_; }
  std::uint32_t timestamp() const { return timestamp_; }
  std::uint32_t image_size() const { return image_size_; }
  std::uint32_t headers_size() const { return headers_size_; }
  const std::vector<Section>& sections() const { return sections_; }

  const Section* section(std::string_view name) const;
  bool in_section(std::string_view name, std::uint64_t address) const;
  SectionBytes read_section(const Memory& memory, const Section& section) const;

 private:
  std::uint64_t base_ = 0;
  std::uint64_t preferred_base_ = 0;
  std::uint32_t timestamp_ = 0;
  std::uint32_t image_size_ = 0;
  std::uint32_t headers_size_ = 0;
  std::vector<Section> sections_;
};

// Pontosan ezt a nullával lezárt szöveget keresi, amely előtt is nulla bájt áll, így nem egy hosszabb szöveg vége; a szöveg címét adja.
std::optional<std::uint64_t> find_cstring(const SectionBytes& data, std::string_view text);

// A 8 bájtra igazított helyek címei, ahol ez a 64 bites érték áll.
std::vector<std::uint64_t> find_qword(const SectionBytes& data, std::uint64_t value);

// Egy REX.W előtagú mov/lea utasítás, amelynek memóriaoperandusa az utasítás végéhez képest relatív (RIP-relatív).
struct RipRef {
  std::uint64_t address = 0;  // az utasítás címe
  std::uint8_t opcode = 0;    // 0x89 tárolás, 0x8B betöltés, 0x8D cím betöltése (lea)
  int reg = 0;                // a regiszter sorszáma (0 rax, 1 rcx, 2 rdx, 8 r8, ...)
  std::uint64_t target = 0;   // a hivatkozott memóriacím
};

// Egy hely bájtjait 48/4C + 89/8B/8D + modrm(00 reg 101) + disp32 utasításként értelmezi.
std::optional<RipRef> decode_rip_ref(const std::uint8_t* p, std::size_t available, std::uint64_t address);

// Minden bájtpozíción megpróbálja az értelmezést, és azokat adja, amelyek célja a megadott cím.
std::vector<RipRef> find_rip_refs_to(const SectionBytes& code, std::uint64_t target);

}  // namespace fw
```

`src/core/pe_image.cpp`:
```cpp
#include "core/pe_image.h"
#include <algorithm>
#include <cstring>

namespace fw {

std::optional<PeImage> PeImage::parse(const Memory& m, std::uint64_t base) {
  const auto mz = m.get<std::uint16_t>(base);
  const auto lfanew = m.get<std::uint32_t>(base + 0x3C);
  if (!mz || *mz != 0x5A4D || !lfanew) return std::nullopt;
  const std::uint64_t nt = base + *lfanew;
  const auto signature = m.get<std::uint32_t>(nt);
  const auto count = m.get<std::uint16_t>(nt + 6);
  const auto timestamp = m.get<std::uint32_t>(nt + 8);
  const auto optional_size = m.get<std::uint16_t>(nt + 20);
  const std::uint64_t opt = nt + 24;
  const auto magic = m.get<std::uint16_t>(opt);
  const auto preferred = m.get<std::uint64_t>(opt + 24);
  const auto image_size = m.get<std::uint32_t>(opt + 56);
  const auto headers_size = m.get<std::uint32_t>(opt + 60);
  if (!signature || *signature != 0x00004550 || !count || !timestamp || !optional_size || !magic || *magic != 0x20B ||
      !preferred || !image_size || !headers_size)
    return std::nullopt;

  PeImage img;
  img.base_ = base;
  img.preferred_base_ = *preferred;
  img.timestamp_ = *timestamp;
  img.image_size_ = *image_size;
  img.headers_size_ = *headers_size;
  const std::uint64_t table = opt + *optional_size;
  for (std::uint16_t i = 0; i < *count; ++i) {
    const std::uint64_t entry = table + 40ull * i;
    char name[9] = {};
    const auto virtual_size = m.get<std::uint32_t>(entry + 8);
    const auto rva = m.get<std::uint32_t>(entry + 12);
    const auto raw_size = m.get<std::uint32_t>(entry + 16);
    const auto raw_offset = m.get<std::uint32_t>(entry + 20);
    if (!m.read(entry, name, 8) || !virtual_size || !rva || !raw_size || !raw_offset) return std::nullopt;
    img.sections_.push_back({name, *rva, *virtual_size, *raw_offset, *raw_size});
  }
  return img;
}

const Section* PeImage::section(std::string_view name) const {
  for (const auto& s : sections_)
    if (s.name == name) return &s;
  return nullptr;
}

bool PeImage::in_section(std::string_view name, std::uint64_t address) const {
  const Section* s = section(name);
  if (!s) return false;
  const std::uint64_t start = base_ + s->rva;
  const std::uint64_t size = s->virtual_size != 0 ? s->virtual_size : s->raw_size;
  return address >= start && address < start + size;
}

SectionBytes PeImage::read_section(const Memory& memory, const Section& s) const {
  SectionBytes result;
  result.address = base_ + s.rva;
  result.bytes.resize(s.virtual_size != 0 ? s.virtual_size : s.raw_size);
  if (!memory.read(result.address, result.bytes.data(), result.bytes.size())) result.bytes.clear();
  return result;
}

std::optional<std::uint64_t> find_cstring(const SectionBytes& data, std::string_view text) {
  if (text.empty()) return std::nullopt;
  std::string needle(1, '\0');
  needle.append(text);
  needle.push_back('\0');
  const auto& b = data.bytes;
  if (b.size() > text.size() && std::memcmp(b.data(), text.data(), text.size()) == 0 && b[text.size()] == 0)
    return data.address;
  const auto it = std::search(b.begin(), b.end(), needle.begin(), needle.end());
  if (it == b.end()) return std::nullopt;
  return data.address + static_cast<std::uint64_t>(it - b.begin()) + 1;
}

std::vector<std::uint64_t> find_qword(const SectionBytes& data, std::uint64_t value) {
  std::vector<std::uint64_t> hits;
  for (std::size_t i = 0; i + 8 <= data.bytes.size(); i += 8) {
    std::uint64_t v;
    std::memcpy(&v, data.bytes.data() + i, 8);
    if (v == value) hits.push_back(data.address + i);
  }
  return hits;
}

std::optional<RipRef> decode_rip_ref(const std::uint8_t* p, std::size_t available, std::uint64_t address) {
  if (available < 7) return std::nullopt;
  const std::uint8_t rex = p[0];
  const std::uint8_t opcode = p[1];
  const std::uint8_t modrm = p[2];
  if (rex != 0x48 && rex != 0x4C) return std::nullopt;
  if (opcode != 0x89 && opcode != 0x8B && opcode != 0x8D) return std::nullopt;
  if ((modrm & 0xC7) != 0x05) return std::nullopt;
  std::int32_t displacement;
  std::memcpy(&displacement, p + 3, 4);
  RipRef r;
  r.address = address;
  r.opcode = opcode;
  r.reg = ((rex & 0x04) != 0 ? 8 : 0) | ((modrm >> 3) & 7);
  r.target = address + 7 + static_cast<std::int64_t>(displacement);
  return r;
}

std::vector<RipRef> find_rip_refs_to(const SectionBytes& code, std::uint64_t target) {
  std::vector<RipRef> hits;
  const auto& b = code.bytes;
  for (std::size_t i = 0; i + 7 <= b.size(); ++i) {
    const auto r = decode_rip_ref(b.data() + i, b.size() - i, code.address + i);
    if (r && r->target == target) hits.push_back(*r);
  }
  return hits;
}

}  // namespace fw
```

`src/core/file_image.h`:
```cpp
#pragma once
#include <optional>
#include <string>
#include "core/memory.h"

namespace fw {

// Egy PE fájlt úgy tölt be egy pufferbe, ahogy a Windows tenné: a szakaszokat a helyükre másolja, az előnyben részesített báziscímre.
// Áthelyezést nem végez, ezért a fájlban tárolt abszolút mutatók erre a báziscímre érvényesek.
std::optional<BufferMemory> load_pe_file(const std::string& path, std::string& error);

}  // namespace fw
```

`src/core/file_image.cpp`:
```cpp
#include "core/file_image.h"
#include <algorithm>
#include <fstream>
#include <iterator>
#include "core/pe_image.h"

namespace fw {

std::optional<BufferMemory> load_pe_file(const std::string& path, std::string& error) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    error = "cannot open " + path;
    return std::nullopt;
  }
  std::vector<std::uint8_t> file((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  const BufferMemory raw(0, file);
  const auto headers = PeImage::parse(raw, 0);
  if (!headers) {
    error = path + " is not a 64-bit PE file";
    return std::nullopt;
  }
  std::vector<std::uint8_t> mapped(headers->image_size(), 0);
  const std::size_t header_bytes = std::min<std::size_t>({headers->headers_size(), file.size(), mapped.size()});
  std::copy_n(file.begin(), header_bytes, mapped.begin());
  for (const auto& s : headers->sections()) {
    const std::size_t size = std::min<std::size_t>(s.raw_size, s.virtual_size != 0 ? s.virtual_size : s.raw_size);
    if (s.raw_offset + size > file.size() || s.rva + size > mapped.size()) {
      error = "section " + s.name + " does not fit";
      return std::nullopt;
    }
    std::copy_n(file.begin() + s.raw_offset, size, mapped.begin() + s.rva);
  }
  return BufferMemory(headers->preferred_base(), std::move(mapped));
}

}  // namespace fw
```

`src/core/rtti.h`:
```cpp
#pragma once
#include <cstdint>
#include <string>
#include "core/memory.h"

namespace fw {

// Egy polimorf C++ objektum osztályának MSVC RTTI nevét adja (például ".?AVdebug_camera@prism@@").
// Üres szöveget ad, ha a cím nem ilyen objektumra mutat.
std::string rtti_class_name(const Memory& memory, std::uint64_t object);

}  // namespace fw
```

`src/core/rtti.cpp`:
```cpp
#include "core/rtti.h"

namespace fw {
namespace {

// Az MSVC x64 "complete object locator" szerkezete; a vtable előtti mutató erre mutat.
struct ObjectLocator {
  std::uint32_t signature;  // x64-en 1
  std::uint32_t offset;
  std::uint32_t constructor_offset;
  std::int32_t type_descriptor;  // RVA
  std::int32_t class_hierarchy;  // RVA
  std::int32_t self;             // a locator saját RVA-ja, ebből adódik a modul báziscíme
};

constexpr std::size_t kMaxNameLength = 128;
// A típusleíróban a név a vtable-mutató és egy foglalt mező után kezdődik.
constexpr std::uint64_t kTypeNameOffset = 16;

}  // namespace

std::string rtti_class_name(const Memory& m, std::uint64_t object) {
  const auto vtable = m.get<std::uint64_t>(object);
  if (!vtable || *vtable < 8) return {};
  const auto locator = m.get<std::uint64_t>(*vtable - 8);
  if (!locator) return {};
  const auto col = m.get<ObjectLocator>(*locator);
  if (!col || col->signature != 1) return {};
  const std::uint64_t module_base = *locator - static_cast<std::uint32_t>(col->self);
  const std::uint64_t name_address = module_base + static_cast<std::uint32_t>(col->type_descriptor) + kTypeNameOffset;

  std::string name;
  for (std::size_t i = 0; i < kMaxNameLength; ++i) {
    char c = 0;
    if (!m.read(name_address + i, &c, 1)) return {};
    if (c == '\0') break;
    name.push_back(c);
  }
  return name.rfind(".?A", 0) == 0 ? name : std::string{};
}

}  // namespace fw
```

- [ ] **Step 4: A tesztek futtatása**

Run: `cmake --build build/host && ctest --test-dir build/host --output-on-failure`
Expected: minden teszt `ok`, `0 failure(s)`.

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt src tests
git commit -m "Add memory abstraction, PE image scanning and MSVC RTTI reader"
```

---

### Task 6: A játék objektumainak megkeresése és az elemzőeszköz

**Files:**
- Create: `src/core/game_layout.h`, `src/core/locator.h`, `src/core/locator.cpp`, `tests/test_locator.cpp`, `tools/probe.cpp`
- Modify: `CMakeLists.txt` (`src/core/locator.cpp` a `freewalk_core`-ba, `tests/test_locator.cpp` a tesztekbe, új `freewalk_probe` cél)

**Interfaces:**
- Consumes: `Memory`, `BufferMemory`, `PeImage`, `find_cstring`, `find_qword`, `decode_rip_ref`, `find_rip_refs_to`, `load_pe_file`, `rtti_class_name` (Task 5); `strf` (Task 2).
- Produces:
  - `namespace fw::layout` konstansai (lásd lent) és `heading_to_camera_yaw`, `camera_yaw_to_heading`.
  - `std::optional<uint64_t> fw::find_camera_manager_global(const Memory&, const PeImage&, std::vector<std::string>& log)`.
  - `std::optional<uint64_t> fw::find_cvar(const Memory&, const PeImage&, std::string_view name, bool verify_name_pointer, std::vector<std::string>& log)`.
  - `struct fw::CameraSlot { int index; uint64_t object; std::string class_name; }`, `std::vector<CameraSlot> fw::list_cameras(const Memory&, uint64_t manager, std::vector<std::string>& log)`, `std::optional<CameraSlot> fw::find_camera(const std::vector<CameraSlot>&, std::string_view class_fragment)`.
  - CMake cél: `freewalk_probe <exe>` (csak nem-Windows hoszton).

A keresési módszerek a TM Real Walk naplóiból és a 8. feladat ellenőrzéséből származó tényeken alapulnak:
- A kamerakezelő neve (`"camera_manager"`) egy `.data`-beli regisztrációs bejegyzésben áll; a bejegyzésben a név mutatója után a létrehozó függvény mutatója következik; a létrehozó függvény egyetlen RIP-relatív tárolással menti a példányt egy globális változóba.
- Egy konzolváltozót regisztráló kód a név címének betöltése előtt, legfeljebb 0x40 bájton belül, `lea rcx, [változó]` utasítással tölti be a változó objektumát.
Mindkettő feltevés, amelyet a 8. feladat a játék fájlján ellenőriz, és szükség esetén módosít.

- [ ] **Step 1: A tesztek megírása**

`tests/test_locator.cpp`:
```cpp
#include "check.h"
#include "core/game_layout.h"
#include "core/locator.h"
#include "core/pe_image.h"
#include "pe_builder.h"

namespace {

using fwtest::kData;
using fwtest::kHeap;
using fwtest::kRdata;
using fwtest::kText;

// A kamerakezelő regisztrációja: név a .rdata-ban, bejegyzés a .data-ban, létrehozó függvény a .text-ben, amely a példányt a globális változóba tárolja.
fw::BufferMemory image_with_camera_manager(int stores) {
  auto m = fwtest::make_test_image();
  const char name[] = "camera_manager";
  m.put_bytes(kRdata + 0x40, name, sizeof name);
  const std::uint64_t entry = kData + 0x100;
  const std::uint64_t creator = kText + 0x200;
  m.put<std::uint64_t>(entry, kRdata + 0x40);
  m.put<std::uint64_t>(entry + fw::layout::kRegistryCreatorOffset, creator);
  fwtest::put_rip_insn(m, creator + 0x08, 0x48, 0x89, 0, kData + 0x800);
  if (stores > 1) fwtest::put_rip_insn(m, creator + 0x20, 0x48, 0x89, 0, kData + 0x808);
  return m;
}

}  // namespace

TEST(finds_camera_manager_global) {
  const auto m = image_with_camera_manager(1);
  const auto img = fw::PeImage::parse(m, fwtest::kBase);
  std::vector<std::string> log;
  const auto global = fw::find_camera_manager_global(m, *img, log);
  CHECK(global.has_value());
  CHECK(global && *global == kData + 0x800);
  CHECK(!log.empty());
}

TEST(ambiguous_creator_is_rejected) {
  const auto m = image_with_camera_manager(2);
  const auto img = fw::PeImage::parse(m, fwtest::kBase);
  std::vector<std::string> log;
  CHECK(!fw::find_camera_manager_global(m, *img, log).has_value());
}

TEST(missing_name_is_reported) {
  const auto m = fwtest::make_test_image();
  const auto img = fw::PeImage::parse(m, fwtest::kBase);
  std::vector<std::string> log;
  CHECK(!fw::find_camera_manager_global(m, *img, log).has_value());
  CHECK(!log.empty());
}

TEST(finds_cvar_by_registration_code) {
  auto m = fwtest::make_test_image();
  const char name[] = "g_developer";
  m.put_bytes(kRdata + 0x80, name, sizeof name);
  const std::uint64_t cvar = kData + 0x400;
  fwtest::put_rip_insn(m, kText + 0x300, 0x48, 0x8D, 1, cvar);           // lea rcx, [cvar]
  fwtest::put_rip_insn(m, kText + 0x30E, 0x4C, 0x8D, 0, kRdata + 0x80);  // lea r8, [name]
  const auto img = fw::PeImage::parse(m, fwtest::kBase);

  std::vector<std::string> log;
  const auto unverified = fw::find_cvar(m, *img, "g_developer", false, log);
  CHECK(unverified && *unverified == cvar);

  CHECK(!fw::find_cvar(m, *img, "g_developer", true, log).has_value());
  m.put<std::uint64_t>(cvar + fw::layout::kCvarNameOffset, kRdata + 0x80);
  const auto verified = fw::find_cvar(m, *img, "g_developer", true, log);
  CHECK(verified && *verified == cvar);
}

TEST(lists_cameras_and_finds_free_camera) {
  auto m = fwtest::make_test_image();
  const std::uint64_t manager = kHeap;
  const std::uint64_t slots = kHeap + 0x100;
  m.put<std::uint64_t>(manager + fw::layout::kManagerSlots, slots);
  m.put<std::uint32_t>(manager + fw::layout::kManagerSlotCount, 3);
  fwtest::put_rtti_object(m, kHeap + 0x400, kRdata + 0x308, kRdata + 0x200, kData + 0x600, ".?AVinterior_camera@prism@@");
  fwtest::put_rtti_object(m, kHeap + 0x500, kRdata + 0x408, kRdata + 0x280, kData + 0x700, ".?AVdebug_camera@prism@@");
  m.put<std::uint64_t>(slots, kHeap + 0x400);
  m.put<std::uint64_t>(slots + 8, 0);
  m.put<std::uint64_t>(slots + 16, kHeap + 0x500);

  std::vector<std::string> log;
  const auto cameras = fw::list_cameras(m, manager, log);
  CHECK(cameras.size() == 3);
  CHECK(cameras[1].object == 0);
  const auto free = fw::find_camera(cameras, fw::layout::kFreeCameraClass);
  CHECK(free.has_value());
  CHECK(free && free->index == 2 && free->object == kHeap + 0x500);
}

TEST(implausible_camera_count_gives_empty_list) {
  auto m = fwtest::make_test_image();
  m.put<std::uint64_t>(kHeap + fw::layout::kManagerSlots, kHeap + 0x100);
  m.put<std::uint32_t>(kHeap + fw::layout::kManagerSlotCount, 5000);
  std::vector<std::string> log;
  CHECK(fw::list_cameras(m, kHeap, log).empty());
}
```

- [ ] **Step 2: A tesztek futtatása, hogy lássuk a hibát**

Run: `cmake --build build/host`
Expected: fordítási hiba, mert a `core/game_layout.h` és a `core/locator.h` hiányzik.

- [ ] **Step 3: A megvalósítás**

`src/core/game_layout.h`:
```cpp
#pragma once
#include <cstddef>
#include <cstdint>
#include <string_view>

// A játék belső felépítésére vonatkozó adatok egy helyen.
// A kiinduló értékek az ETS2 1.61.1-re vonatkozó feltevések; minden érték mellett ott áll, hol és hogyan ellenőriztük.
// A részletes ellenőrzési jegyzet: docs/game-notes/ets2-1.61.md.
namespace fw::layout {

// A kamerakezelő regisztrációja: a név, és a bejegyzésben a név mutatója után a létrehozó függvény mutatója.
inline constexpr std::string_view kCameraManagerName = "camera_manager";
inline constexpr std::uint64_t kRegistryCreatorOffset = 0x8;   // feltevés
inline constexpr std::size_t kCreatorScanBytes = 0x200;

// A kamerakezelő mezői.
inline constexpr std::uint64_t kManagerCurrentIndex = 0x10;    // feltevés: az aktív kamera sorszáma (int32)
inline constexpr std::uint64_t kManagerRequestedIndex = 0x14;  // feltevés: a kért kamera sorszáma (int32)
inline constexpr std::uint64_t kManagerSlots = 0x38;           // feltevés: a kameraobjektumok tömbjének mutatója
inline constexpr std::uint64_t kManagerSlotCount = 0x40;       // feltevés: a tömb hossza (uint32)
inline constexpr std::uint32_t kMaxCameraSlots = 32;

// A szabad kamera és mezői.
inline constexpr std::string_view kFreeCameraClass = "debug_camera";
inline constexpr std::uint64_t kCameraYaw = 0x10;              // feltevés: irányszög (float, radián)
inline constexpr std::uint64_t kCameraPitch = 0x14;            // feltevés
inline constexpr std::uint64_t kCameraRoll = 0x18;             // feltevés
inline constexpr std::uint64_t kCameraLocalPosition = 0x40;    // feltevés: helyi pozíció, 3 float, utána a két int16 négyzet-sorszám

// Konzolváltozók.
inline constexpr std::string_view kDeveloperCvar = "g_developer";
inline constexpr std::string_view kFreeCameraSpeedCvar = "";   // a 8. feladat tölti ki a játék fájljának elemzése alapján
inline constexpr std::size_t kCvarLeaWindow = 0x40;
inline constexpr std::uint64_t kCvarNameOffset = 0x8;          // feltevés: a változó objektumában a név mutatója
inline constexpr std::uint64_t kCvarIntValue = 0x140;          // feltevés: egész érték
inline constexpr std::uint64_t kCvarFloatValue = 0x118;        // feltevés: lebegőpontos érték
inline constexpr std::size_t kCvarSetterVtableIndex = 1;       // feltevés: void set(this, const char* value)

// A kamera irányszöge és a telemetria iránya (radián, 0 = észak, az óramutatóval ellentétesen) közötti átváltás.
// Feltevés: a kettő azonos; a 9. feladat élő tesztje ellenőrzi.
inline double heading_to_camera_yaw(double heading) { return heading; }
inline double camera_yaw_to_heading(double yaw) { return yaw; }

}  // namespace fw::layout
```

`src/core/locator.h`:
```cpp
#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "core/memory.h"
#include "core/pe_image.h"

namespace fw {

// A kamerakezelőre mutató globális változó címe; a kamerakezelő maga ennek tartalma, és csak egy világ betöltése után létezik.
std::optional<std::uint64_t> find_camera_manager_global(const Memory& memory, const PeImage& image, std::vector<std::string>& log);

// Egy konzolváltozó objektumának címe a neve alapján.
// A névmutató ellenőrzése csak a futó játékban lehetséges, mert a mutatót a játék indulásakor tölti ki; a fájl elemzésekor ki kell kapcsolni.
std::optional<std::uint64_t> find_cvar(const Memory& memory, const PeImage& image, std::string_view name, bool verify_name_pointer,
                                       std::vector<std::string>& log);

struct CameraSlot {
  int index = -1;
  std::uint64_t object = 0;
  std::string class_name;
};

// A kamerakezelő kameráinak listája az osztálynevükkel.
std::vector<CameraSlot> list_cameras(const Memory& memory, std::uint64_t manager, std::vector<std::string>& log);

// Az első kamera, amelynek osztályneve tartalmazza a megadott részletet.
std::optional<CameraSlot> find_camera(const std::vector<CameraSlot>& cameras, std::string_view class_fragment);

}  // namespace fw
```

`src/core/locator.cpp`:
```cpp
#include "core/locator.h"
#include <set>
#include "core/game_layout.h"
#include "core/rtti.h"
#include "core/text.h"

namespace fw {
namespace {

unsigned long long rva(const PeImage& image, std::uint64_t address) {
  return static_cast<unsigned long long>(address - image.base());
}

}  // namespace

std::optional<std::uint64_t> find_camera_manager_global(const Memory& m, const PeImage& image, std::vector<std::string>& log) {
  const Section* rdata = image.section(".rdata");
  const Section* data = image.section(".data");
  if (!rdata || !data || !image.section(".text")) {
    log.push_back("camera manager: the executable has no .text/.rdata/.data section");
    return std::nullopt;
  }
  const auto name = find_cstring(image.read_section(m, *rdata), layout::kCameraManagerName);
  if (!name) {
    log.push_back("camera manager: the name string was not found");
    return std::nullopt;
  }
  const auto entries = find_qword(image.read_section(m, *data), *name);
  log.push_back(strf("camera manager: name at +0x%llX, %zu reference(s) in .data", rva(image, *name), entries.size()));

  std::optional<std::uint64_t> found;
  for (const std::uint64_t entry : entries) {
    const auto creator = m.get<std::uint64_t>(entry + layout::kRegistryCreatorOffset);
    if (!creator || !image.in_section(".text", *creator)) continue;
    std::vector<std::uint8_t> code(layout::kCreatorScanBytes);
    if (!m.read(*creator, code.data(), code.size())) continue;
    std::set<std::uint64_t> stores;
    for (std::size_t i = 0; i + 7 <= code.size(); ++i) {
      const auto r = decode_rip_ref(code.data() + i, code.size() - i, *creator + i);
      if (r && r->opcode == 0x89 && image.in_section(".data", r->target)) stores.insert(r->target);
    }
    log.push_back(strf("camera manager: entry +0x%llX, creator +0x%llX, %zu global store(s)", rva(image, entry), rva(image, *creator),
                       stores.size()));
    if (stores.size() != 1) continue;
    if (found && *found != *stores.begin()) {
      log.push_back("camera manager: more than one creator stores a different global");
      return std::nullopt;
    }
    found = *stores.begin();
  }
  if (found) log.push_back(strf("camera manager: global at +0x%llX", rva(image, *found)));
  else log.push_back("camera manager: no creator with exactly one global store");
  return found;
}

std::optional<std::uint64_t> find_cvar(const Memory& m, const PeImage& image, std::string_view name, bool verify_name_pointer,
                                       std::vector<std::string>& log) {
  const std::string label = "cvar " + std::string(name) + ": ";
  const Section* rdata = image.section(".rdata");
  const Section* text = image.section(".text");
  if (!rdata || !text) {
    log.push_back(label + "the executable has no .text/.rdata section");
    return std::nullopt;
  }
  const auto name_address = find_cstring(image.read_section(m, *rdata), name);
  if (!name_address) {
    log.push_back(label + "the name string was not found");
    return std::nullopt;
  }
  const SectionBytes code = image.read_section(m, *text);
  std::set<std::uint64_t> candidates;
  for (const RipRef& ref : find_rip_refs_to(code, *name_address)) {
    if (ref.opcode != 0x8D) continue;
    const std::size_t at = static_cast<std::size_t>(ref.address - code.address);
    for (std::size_t back = 7; back <= layout::kCvarLeaWindow && back <= at; ++back) {
      const std::size_t start = at - back;
      const auto r = decode_rip_ref(code.bytes.data() + start, code.bytes.size() - start, code.address + start);
      if (!r || r->opcode != 0x8D || r->reg != 1 || !image.in_section(".data", r->target)) continue;
      if (verify_name_pointer) {
        const auto stored = m.get<std::uint64_t>(r->target + layout::kCvarNameOffset);
        if (!stored || *stored != *name_address) continue;
      }
      candidates.insert(r->target);
    }
  }
  log.push_back(strf("%sname at +0x%llX, %zu candidate object(s)%s", label.c_str(), rva(image, *name_address), candidates.size(),
                     verify_name_pointer ? " (name pointer checked)" : ""));
  for (const std::uint64_t c : candidates) log.push_back(strf("%s  candidate +0x%llX", label.c_str(), rva(image, c)));
  if (candidates.size() != 1) return std::nullopt;
  return *candidates.begin();
}

std::vector<CameraSlot> list_cameras(const Memory& m, std::uint64_t manager, std::vector<std::string>& log) {
  const auto slots = m.get<std::uint64_t>(manager + layout::kManagerSlots);
  const auto count = m.get<std::uint32_t>(manager + layout::kManagerSlotCount);
  if (!slots || !count || *count == 0 || *count > layout::kMaxCameraSlots) {
    log.push_back(strf("cameras: implausible camera list (count %u)", count ? *count : 0u));
    return {};
  }
  std::vector<CameraSlot> cameras;
  for (std::uint32_t i = 0; i < *count; ++i) {
    CameraSlot s;
    s.index = static_cast<int>(i);
    s.object = m.get<std::uint64_t>(*slots + 8ull * i).value_or(0);
    if (s.object != 0) s.class_name = rtti_class_name(m, s.object);
    cameras.push_back(s);
  }
  return cameras;
}

std::optional<CameraSlot> find_camera(const std::vector<CameraSlot>& cameras, std::string_view class_fragment) {
  for (const auto& c : cameras)
    if (c.class_name.find(class_fragment) != std::string::npos) return c;
  return std::nullopt;
}

}  // namespace fw
```

`tools/probe.cpp`:
```cpp
// A játék futtatható fájlján lefuttatja ugyanazokat a kereséseket, amelyeket a plugin a játékban végez, és kiírja az eredményt.
// Használat: freewalk_probe <eurotrucks2.exe>
#include <cstdio>
#include <string>
#include <vector>
#include "core/file_image.h"
#include "core/game_layout.h"
#include "core/locator.h"
#include "core/pe_image.h"

int main(int argc, char** argv) {
  if (argc != 2) {
    std::fprintf(stderr, "usage: freewalk_probe <eurotrucks2.exe>\n");
    return 2;
  }
  std::string error;
  const auto memory = fw::load_pe_file(argv[1], error);
  if (!memory) {
    std::fprintf(stderr, "%s\n", error.c_str());
    return 1;
  }
  const auto image = fw::PeImage::parse(*memory, memory->base());
  if (!image) {
    std::fprintf(stderr, "the mapped image could not be parsed\n");
    return 1;
  }
  std::printf("timestamp 0x%08X, image size 0x%X, preferred base 0x%llX\n", image->timestamp(), image->image_size(),
              static_cast<unsigned long long>(image->preferred_base()));
  for (const auto& s : image->sections())
    std::printf("  section %-8s rva 0x%08X size 0x%08X\n", s.name.c_str(), s.rva, s.virtual_size);

  std::vector<std::string> log;
  const auto manager = fw::find_camera_manager_global(*memory, *image, log);
  const auto developer = fw::find_cvar(*memory, *image, fw::layout::kDeveloperCvar, false, log);
  std::optional<std::uint64_t> speed;
  if (fw::layout::kFreeCameraSpeedCvar.empty()) log.push_back("free camera speed cvar: name not known yet");
  else speed = fw::find_cvar(*memory, *image, fw::layout::kFreeCameraSpeedCvar, false, log);
  for (const auto& line : log) std::printf("%s\n", line.c_str());

  std::printf("result: camera manager %s, g_developer %s, free camera speed %s\n", manager ? "found" : "NOT FOUND",
              developer ? "found" : "NOT FOUND", speed ? "found" : "NOT FOUND");
  return manager ? 0 : 1;
}
```

Add to `CMakeLists.txt`, inside `if(NOT WIN32)`:
```cmake
  add_executable(freewalk_probe tools/probe.cpp)
  target_link_libraries(freewalk_probe PRIVATE freewalk_core)
```

- [ ] **Step 4: A tesztek futtatása**

Run: `cmake --build build/host && ctest --test-dir build/host --output-on-failure`
Expected: minden teszt `ok`, `0 failure(s)`.

- [ ] **Step 5: Az elemzőeszköz kipróbálása egy valódi PE fájlon**

Run: `build/host/freewalk_probe "../content/TMRealWalk.dll"`
Expected: kiírja az időbélyeget (`0x...`), a szakaszokat (`.text`, `.rdata`, `.data`, `.pdata`, `_RDATA`, `.rsrc`, `.reloc`), a keresések naplóját, a `result:` sorban a kamerakezelő `NOT FOUND` (ebben a fájlban nincs kamerakezelő-regisztráció), és 1-es kilépési kóddal tér vissza.
A `g_developer` sora itt bármit mutathat, mert ez a DLL maga is hivatkozik erre a szövegre; ennek nincs jelentősége.
Ez csak a fájlbetöltést és a szakaszolvasást ellenőrzi egy valódi MSVC fájlon; a fájl nem kerül a repóba.

- [ ] **Step 6: Commit**

```bash
git add CMakeLists.txt src tests tools
git commit -m "Add game locator for camera manager, cameras and cvars, plus offline probe tool"
```

---

### Task 7: Keresztfordítás, SDK és a plugin váza

**Files:**
- Create: `cmake/mingw-w64-x86_64.cmake`, `third_party/scs_sdk/` (az SDK `include` mappája és `sdk_license.txt`), `src/win/FreeWalk.def`, `src/win/log.h`, `src/win/log.cpp`, `src/win/plugin.cpp`, `FreeWalk.ini`, `README.md`
- Modify: `CMakeLists.txt` (Windows cél)

**Interfaces:**
- Consumes: `fw::Config`, `parse_config` (Task 2); `fw::TruckState` (Task 4); `strf` (Task 2).
- Produces:
  - `void fw::log_init(scs_log_t)`, `void fw::log_info(const std::string&)`, `void fw::log_warning(const std::string&)`, `void fw::log_error(const std::string&)`.
  - `FreeWalk.dll` a két exporttal; betöltéskor naplóz, beolvassa a `FreeWalk.ini`-t, és frissíti a kamion állapotát a telemetriából.

- [ ] **Step 1: A keresztfordító telepítése**

Run: `brew install mingw-w64`
Expected: `x86_64-w64-mingw32-g++ --version` 14.x verziót ír.

- [ ] **Step 2: Az SDK fejlécek bemásolása**

Run:
```bash
mkdir -p /tmp/freewalk-sdk && cd /tmp/freewalk-sdk && curl -sL -o sdk.zip https://download.eurotrucksimulator2.com/scs_sdk_1_14.zip && unzip -q -o sdk.zip
cd - && mkdir -p third_party/scs_sdk && cp -R /tmp/freewalk-sdk/include third_party/scs_sdk/ && cp /tmp/freewalk-sdk/sdk_license.txt third_party/scs_sdk/
```
Expected: `third_party/scs_sdk/include/scssdk_telemetry.h` és `third_party/scs_sdk/sdk_license.txt` létezik.
Ha a letöltési cím megváltozott, az aktuális a https://modding.scssoft.com/wiki/Documentation/Engine/SDK/Telemetry oldalon található.

- [ ] **Step 3: A toolchain és a Windows cél**

`cmake/mingw-w64-x86_64.cmake`:
```cmake
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
```

Add to `CMakeLists.txt`, after the `freewalk_core` target:
```cmake
if(WIN32)
  add_library(FreeWalk SHARED
    src/win/plugin.cpp
    src/win/log.cpp
    src/win/FreeWalk.def
  )
  # Rendszer-fejlécként, hogy a külső SDK fejlécei ne adjanak figyelmeztetést a szigorú beállításokkal.
  target_include_directories(FreeWalk SYSTEM PRIVATE third_party/scs_sdk/include)
  target_link_libraries(FreeWalk PRIVATE freewalk_core)
  set_target_properties(FreeWalk PROPERTIES PREFIX "")
  if(MINGW)
    # Minden futtatókönyvtár a DLL-be kerül, így a felhasználónak nem kell külön telepítenie semmit.
    target_link_options(FreeWalk PRIVATE -static -static-libgcc -static-libstdc++)
  endif()
endif()
```

`src/win/FreeWalk.def`:
```
LIBRARY FreeWalk
EXPORTS
  scs_telemetry_init
  scs_telemetry_shutdown
```

- [ ] **Step 4: Naplózás**

`src/win/log.h`:
```cpp
#pragma once
#include <string>
#include "scssdk.h"

namespace fw {

// A játék logjába ír az SDK naplózó függvényén át, "[FreeWalk] " előtaggal.
// Csak a játék által hívott plugin-függvényekből szabad hívni, mert az SDK nem szálbiztos.
void log_init(scs_log_t log);
void log_info(const std::string& message);
void log_warning(const std::string& message);
void log_error(const std::string& message);

}  // namespace fw
```

`src/win/log.cpp`:
```cpp
#include "win/log.h"

namespace fw {
namespace {

scs_log_t g_log = nullptr;

void write(scs_log_type_t type, const std::string& message) {
  if (g_log) g_log(type, ("[FreeWalk] " + message).c_str());
}

}  // namespace

void log_init(scs_log_t log) { g_log = log; }
void log_info(const std::string& message) { write(SCS_LOG_TYPE_message, message); }
void log_warning(const std::string& message) { write(SCS_LOG_TYPE_warning, message); }
void log_error(const std::string& message) { write(SCS_LOG_TYPE_error, message); }

}  // namespace fw
```

- [ ] **Step 5: A plugin váza**

`src/win/plugin.cpp`:
```cpp
// Az SCS telemetria-SDK belépési pontjai és a telemetria fogadása.
#include <windows.h>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include "scssdk_telemetry.h"
#include "common/scssdk_telemetry_common_configs.h"
#include "common/scssdk_telemetry_truck_common_channels.h"
#include "eurotrucks2/scssdk_eut2.h"
#include "core/config.h"
#include "core/text.h"
#include "core/walk_session.h"
#include "win/log.h"

namespace {

constexpr const char* kVersion = "0.1.0";
constexpr double kTwoPi = 6.28318530717958647692;

struct State {
  fw::Config config;
  fw::TruckState truck;
  fw::Vec3 cabin_position;
  fw::Vec3 head_position;
  bool paused = true;
};

State* g = nullptr;

// A plugin DLL mappájában lévő FreeWalk.ini teljes útvonala.
std::wstring ini_path() {
  HMODULE module = nullptr;
  GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                     reinterpret_cast<LPCWSTR>(&ini_path), &module);
  wchar_t path[MAX_PATH] = {};
  GetModuleFileNameW(module, path, MAX_PATH);
  std::wstring result(path);
  const auto slash = result.find_last_of(L"\\/");
  return result.substr(0, slash == std::wstring::npos ? 0 : slash + 1) + L"FreeWalk.ini";
}

void load_config() {
  std::ifstream in(ini_path(), std::ios::binary);
  if (!in) {
    fw::log_warning("FreeWalk.ini was not found next to the DLL: the defaults are used");
    return;
  }
  std::stringstream text;
  text << in.rdbuf();
  std::vector<std::string> warnings;
  g->config = fw::parse_config(text.str(), warnings);
  for (const auto& w : warnings) fw::log_warning("FreeWalk.ini " + w);
}

SCSAPI_VOID on_placement(const scs_string_t, const scs_u32_t, const scs_value_t* const value, const scs_context_t) {
  if (!value || value->type != SCS_VALUE_TYPE_dplacement) return;
  const auto& p = value->value_dplacement;
  g->truck.position = {p.position.x, p.position.y, p.position.z};
  g->truck.heading = p.orientation.heading * kTwoPi;
  g->truck.valid = true;
}

SCSAPI_VOID on_speed(const scs_string_t, const scs_u32_t, const scs_value_t* const value, const scs_context_t) {
  if (value && value->type == SCS_VALUE_TYPE_float) g->truck.speed = value->value_float.value;
}

SCSAPI_VOID on_parking_brake(const scs_string_t, const scs_u32_t, const scs_value_t* const value, const scs_context_t) {
  if (value && value->type == SCS_VALUE_TYPE_bool) g->truck.parking_brake = value->value_bool.value != 0;
}

// A kamion konfigurációjából a vezető fejének helye a kamion koordinátáiban: a fülke helye plusz a fej helye a fülkében.
SCSAPI_VOID on_configuration(const scs_event_t, const void* const event_info, const scs_context_t) {
  const auto* info = static_cast<const scs_telemetry_configuration_t*>(event_info);
  if (std::strcmp(info->id, SCS_TELEMETRY_CONFIG_truck) != 0) return;
  g->cabin_position = {};
  bool head_found = false;
  for (const scs_named_value_t* a = info->attributes; a->name; ++a) {
    if (a->value.type != SCS_VALUE_TYPE_fvector) continue;
    const auto& v = a->value.value_fvector;
    if (std::strcmp(a->name, SCS_TELEMETRY_CONFIG_ATTRIBUTE_cabin_position) == 0) g->cabin_position = {v.x, v.y, v.z};
    if (std::strcmp(a->name, SCS_TELEMETRY_CONFIG_ATTRIBUTE_head_position) == 0) {
      g->head_position = {v.x, v.y, v.z};
      head_found = true;
    }
  }
  g->truck.head_known = head_found;
  g->truck.head_in_truck = g->cabin_position + g->head_position;
  fw::log_info(fw::strf("truck configuration: driver's head at (%.2f, %.2f, %.2f) in truck space%s", g->truck.head_in_truck.x,
                        g->truck.head_in_truck.y, g->truck.head_in_truck.z, head_found ? "" : " (head position missing)"));
}

SCSAPI_VOID on_paused(const scs_event_t, const void* const, const scs_context_t) { g->paused = true; }
SCSAPI_VOID on_started(const scs_event_t, const void* const, const scs_context_t) { g->paused = false; }
SCSAPI_VOID on_frame_end(const scs_event_t, const void* const, const scs_context_t) {}

bool register_all(const scs_telemetry_init_params_v101_t* p) {
  bool ok = true;
  ok &= p->register_for_event(SCS_TELEMETRY_EVENT_frame_end, on_frame_end, nullptr) == SCS_RESULT_ok;
  ok &= p->register_for_event(SCS_TELEMETRY_EVENT_paused, on_paused, nullptr) == SCS_RESULT_ok;
  ok &= p->register_for_event(SCS_TELEMETRY_EVENT_started, on_started, nullptr) == SCS_RESULT_ok;
  ok &= p->register_for_event(SCS_TELEMETRY_EVENT_configuration, on_configuration, nullptr) == SCS_RESULT_ok;
  ok &= p->register_for_channel(SCS_TELEMETRY_TRUCK_CHANNEL_world_placement, SCS_U32_NIL, SCS_VALUE_TYPE_dplacement,
                                SCS_TELEMETRY_CHANNEL_FLAG_none, on_placement, nullptr) == SCS_RESULT_ok;
  ok &= p->register_for_channel(SCS_TELEMETRY_TRUCK_CHANNEL_speed, SCS_U32_NIL, SCS_VALUE_TYPE_float, SCS_TELEMETRY_CHANNEL_FLAG_none,
                                on_speed, nullptr) == SCS_RESULT_ok;
  ok &= p->register_for_channel(SCS_TELEMETRY_TRUCK_CHANNEL_parking_brake, SCS_U32_NIL, SCS_VALUE_TYPE_bool,
                                SCS_TELEMETRY_CHANNEL_FLAG_none, on_parking_brake, nullptr) == SCS_RESULT_ok;
  return ok;
}

}  // namespace

extern "C" {

SCSAPI_RESULT scs_telemetry_init(const scs_u32_t version, const scs_telemetry_init_params_t* const params) {
  if (version != SCS_TELEMETRY_VERSION_1_01) return SCS_RESULT_unsupported;
  const auto* p = static_cast<const scs_telemetry_init_params_v101_t*>(params);
  fw::log_init(p->common.log);
  if (std::strcmp(p->common.game_id, SCS_GAME_ID_EUT2) != 0) {
    fw::log_warning(std::string("only Euro Truck Simulator 2 is supported, not ") + p->common.game_id);
    fw::log_init(nullptr);
    return SCS_RESULT_unsupported;
  }
  g = new State();
  fw::log_info(fw::strf("loading %s (%s, telemetry game version %u.%u)", kVersion, p->common.game_name,
                        SCS_GET_MAJOR_VERSION(p->common.game_version), SCS_GET_MINOR_VERSION(p->common.game_version)));
  load_config();
  if (!register_all(p)) {
    fw::log_error("the telemetry callbacks could not be registered");
    delete g;
    g = nullptr;
    fw::log_init(nullptr);
    return SCS_RESULT_generic_error;
  }
  fw::log_info("loaded");
  return SCS_RESULT_ok;
}

SCSAPI_VOID scs_telemetry_shutdown(void) {
  if (!g) return;
  fw::log_info("unloaded");
  fw::log_init(nullptr);
  delete g;
  g = nullptr;
}

}  // extern "C"
```

- [ ] **Step 6: Alapértelmezett INI és README**

`FreeWalk.ini`:
```ini
; FreeWalk - kiszállás és séta az Euro Truck Simulator 2-ben
; Minden sor elhagyható: a hiányzó értékek helyett az alapérték érvényes.

[keys]
toggle = F10      ; kiszállás és visszaszállás
forward = W
back = S
left = A
right = D
run = SHIFT

[walker]
walk_speed = 1.4          ; m/s
run_speed = 4.0           ; m/s
eye_height = 1.7          ; m a talaj felett
head_bob = 1              ; 1 = enyhe fejbiccentés lépés közben, 0 = ki
head_bob_amplitude = 0.03 ; m
exit_side_distance = 1.0  ; m-rel a vezető fejének helyén túl, oldalra
ground_offset = 0.0       ; m, a talaj magassága a kamion telemetriás pozíciójához képest
max_distance = 150        ; m, ennél messzebb a kamiontól automatikus visszaszállás

[debug]
discovery_log = 0         ; 1 = másodpercenként diagnosztikai sorok a game.log.txt-be
write_on_frame_start = 0  ; 1 = a kamera pozícióját a képkocka elején írja (ha a kép remeg)
```

`README.md`:
```markdown
# FreeWalk

Kiszállás és séta a kamion körül az Euro Truck Simulator 2-ben (1.61.x).

## Telepítés

1. Zárd be a játékot.
2. Másold a `FreeWalk.dll`-t és a `FreeWalk.ini`-t ide: `...\Steam\steamapps\common\Euro Truck Simulator 2\bin\win_x64\plugins\` (ha nincs `plugins` mappa, hozd létre).
3. Indítsd el a játékot, és tölts be egy mentést.
   A játék indításkor megkérdezi, engedélyezed-e a telemetria-plugint; válaszd az OK-t.

## Használat

- `F10` a parkoló, behúzott kéziféknél álló kamionban: kiszállás.
- `W A S D` séta, `Shift` futás, egér körbenézés.
- `F10` újra: visszaszállás.

A plugin a `game.log.txt`-be ír (`Dokumentumok\Euro Truck Simulator 2\game.log.txt`), minden sora `[FreeWalk]`-kel kezdődik.

## Fordítás macOS-en

    brew install cmake ninja mingw-w64
    cmake -S . -B build/host -G Ninja && cmake --build build/host && ctest --test-dir build/host --output-on-failure
    cmake -S . -B build/win -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake -DCMAKE_BUILD_TYPE=Release && cmake --build build/win

A kész DLL: `build/win/FreeWalk.dll`.
```

- [ ] **Step 7: Fordítás és az exportok ellenőrzése**

Run:
```bash
cmake -S . -B build/win -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake -DCMAKE_BUILD_TYPE=Release && cmake --build build/win
x86_64-w64-mingw32-objdump -p build/win/FreeWalk.dll | grep -E 'scs_telemetry_(init|shutdown)|DLL Name'
```
Expected: a fordítás figyelmeztetés nélkül lefut; a kimenetben szerepel a `scs_telemetry_init` és a `scs_telemetry_shutdown`, a `DLL Name` sorok között csak Windows rendszer-DLL-ek vannak (`KERNEL32.dll`, `msvcrt.dll` vagy `api-ms-win-crt-*`), `libstdc++-6.dll`, `libgcc_s_seh-1.dll` és `libwinpthread-1.dll` nincs.

- [ ] **Step 8: A macOS tesztek változatlanul futnak**

Run: `cmake --build build/host && ctest --test-dir build/host --output-on-failure`
Expected: `0 failure(s)`.

- [ ] **Step 9: Commit**

```bash
git add CMakeLists.txt cmake third_party src FreeWalk.ini README.md
git commit -m "Add MinGW cross build, SCS SDK headers and plugin skeleton with telemetry"
```

---

### Task 8: A játék futtatható fájljának elemzése

Ez kutatási feladat, nem TDD; az eredménye a `docs/game-notes/ets2-1.61.md` dokumentum és a `game_layout.h` ellenőrzött értékei.

**Files:**
- Create: `tools/ghidra/FindRefs.java`, `docs/game-notes/ets2-1.61.md`
- Modify: `src/core/game_layout.h`, szükség esetén `src/core/locator.cpp` és `tests/test_locator.cpp`

**Interfaces:**
- Consumes: `freewalk_probe` (Task 6).
- Produces: ellenőrzött `fw::layout` értékek, köztük a `kFreeCameraSpeedCvar` neve; a `find_camera_manager_global` és a `find_cvar` keresési szabálya a játék tényleges kódjához igazítva.

- [ ] **Step 1: A játék fájljának bekérése**

Kérd meg a felhasználót, hogy másolja a `...\Euro Truck Simulator 2\bin\win_x64\eurotrucks2.exe` fájlt ide: `~/Downloads/TM Real Work/game/eurotrucks2.exe` (a repón kívül), és írja meg a játék pontos verzióját a főmenü jobb alsó sarkából.
Run: `shasum -a 256 "../game/eurotrucks2.exe"`
Jegyezd fel a verziót és az ellenőrzőösszeget a jegyzet elejére.

- [ ] **Step 2: Az elemzőeszköz futtatása**

Run: `build/host/freewalk_probe "../game/eurotrucks2.exe" | tee /tmp/freewalk-probe.txt`
Expected: az időbélyeg és a szakaszok listája; a kamerakezelő keresésének eredménye; a `g_developer` jelöltjei.
Ha a kamerakezelő `.data`-beli hivatkozásainak száma 0, a regisztrációs bejegyzést a játék futás közben tölti ki; ez a fájlból nem ellenőrizhető, de a játékban működhet (a 9. feladat élő tesztje dönti el).

- [ ] **Step 3: A Ghidra-szkript**

`tools/ghidra/FindRefs.java`:
```java
// Kiírja egy szöveg (vagy egy cím) összes hivatkozását és a hivatkozó függvények visszafejtett kódját.
// Argumentumok: <mód: string|prefix|address> <szöveg vagy hexadecimális cím> <kimeneti fájl>
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.symbol.Reference;
import java.io.FileWriter;
import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

public class FindRefs extends GhidraScript {
  private DecompInterface decompiler;
  private final Set<Function> printed = new HashSet<>();

  @Override
  public void run() throws Exception {
    String[] args = getScriptArgs();
    String mode = args[0];
    decompiler = new DecompInterface();
    decompiler.openProgram(currentProgram);
    try (PrintWriter out = new PrintWriter(new FileWriter(args[2]))) {
      for (Address target : targets(mode, args[1])) {
        out.println("target " + target);
        for (Reference ref : getReferencesTo(target)) {
          out.println("  ref from " + ref.getFromAddress() + " (" + ref.getReferenceType() + ")");
          printFunction(out, getFunctionContaining(ref.getFromAddress()));
          // Adatból jövő hivatkozás (például egy regisztrációs tábla): annak a hivatkozói is kellenek.
          for (Reference second : getReferencesTo(ref.getFromAddress())) {
            out.println("    data ref from " + second.getFromAddress());
            printFunction(out, getFunctionContaining(second.getFromAddress()));
          }
        }
      }
    }
  }

  private List<Address> targets(String mode, String value) throws Exception {
    List<Address> result = new ArrayList<>();
    if (mode.equals("address")) {
      result.add(toAddr(Long.parseUnsignedLong(value.replace("0x", ""), 16)));
      return result;
    }
    Memory memory = currentProgram.getMemory();
    byte[] needle = mode.equals("prefix") ? value.getBytes("US-ASCII") : (value + "\0").getBytes("US-ASCII");
    Address at = memory.getMinAddress();
    while ((at = memory.findBytes(at, needle, null, true, monitor)) != null) {
      // Szöveg módban csak a pontos előfordulás számít, amely előtt nulla bájt áll.
      if (mode.equals("prefix") || memory.getByte(at.subtract(1)) == 0) result.add(at);
      at = at.add(1);
    }
    return result;
  }

  private void printFunction(PrintWriter out, Function function) {
    if (function == null || !printed.add(function)) return;
    DecompileResults result = decompiler.decompileFunction(function, 120, monitor);
    out.println("// ==== " + function.getName() + " @ " + function.getEntryPoint());
    out.println(result.decompileCompleted() ? result.getDecompiledFunction().getC() : "// decompile failed");
  }
}
```

- [ ] **Step 4: A Ghidra-elemzés indítása**

Run (háttérben, 30-90 perc lehet):
```bash
mkdir -p "../ghidra-ets2"
JAVA_HOME=/opt/homebrew/opt/openjdk@21/libexec/openjdk.jdk/Contents/Home \
  /opt/homebrew/Cellar/ghidra/12.1.4/libexec/support/analyzeHeadless "../ghidra-ets2" ets2 \
  -import "../game/eurotrucks2.exe" -overwrite > /tmp/ghidra-ets2.log 2>&1
```
Expected: a log végén `Analysis succeeded` és `Save succeeded`.

A további lekérdezések parancsa (a `<mód> <érték> <kimenet>` cserélendő):
```bash
JAVA_HOME=/opt/homebrew/opt/openjdk@21/libexec/openjdk.jdk/Contents/Home \
  /opt/homebrew/Cellar/ghidra/12.1.4/libexec/support/analyzeHeadless "../ghidra-ets2" ets2 \
  -process eurotrucks2.exe -noanalysis -readOnly -scriptPath tools/ghidra -postScript FindRefs.java <mód> <érték> <kimenet>
```

- [ ] **Step 5: A kamerakezelő ellenőrzése**

Futtasd a lekérdezést `string camera_manager /tmp/q-camera-manager.c` argumentumokkal, majd a megtalált globális változó címére `address <cím> /tmp/q-manager-users.c` argumentumokkal.
Ellenőrizendő és a jegyzetbe írandó:
- a regisztrációs bejegyzésben a név mutatója után 8 bájttal áll-e a létrehozó függvény (`kRegistryCreatorOffset`);
- a létrehozó függvény egyetlen tárolással menti-e a példányt a globális változóba;
- a globálist használó függvényekben az aktív és a kért kamera sorszáma a `+0x10` és `+0x14` mezőn van-e (jellemzően egy függvény összehasonlítja a kettőt, és eltérés esetén vált);
- a kameraobjektumok tömbje `+0x38`-on, a hossza `+0x40`-en van-e, és milyen típusú a hossz.
Ha bármelyik eltér, javítsd a `game_layout.h` értékét, és ha a keresési szabály is eltér, a `find_camera_manager_global`-t és a hozzá tartozó tesztet.

- [ ] **Step 6: A szabad kamera ellenőrzése**

Futtasd a lekérdezést `prefix .?AVdebug_camera@ /tmp/q-debug-camera.c` argumentumokkal (az RTTI típusleíró), és a Ghidra által felcímkézett `debug_camera::vftable` függvényeire.
Ellenőrizendő:
- a pontos RTTI név (például `.?AVdebug_camera@prism@@`), és hogy a `"debug_camera"` részlet egyedi-e a kamerák között;
- a frissítő függvényben az irányszög, a dőlésszög és a billenés float mezői (`+0x10`, `+0x14`, `+0x18`), és hogy a szögek radiánban vannak-e;
- a pozíció: 3 float `+0x40`-en, utána két int16 `+0x4C`-n és `+0x4E`-n (jellemző nyom: szorzás 512.0-val (`0x44000000`) vagy 1/512-vel (`0x3B000000`), és `floor`);
- hogy a frissítő függvény minden képkockában a szögekből számolja-e a forgatást (a `+0x50` kvaternió írása a szögek alapján).

- [ ] **Step 7: A konzolváltozók ellenőrzése**

Futtasd a lekérdezést `string g_developer /tmp/q-developer.c` argumentumokkal.
Ellenőrizendő:
- a regisztráló kód mintája: a változó objektumának címe `lea rcx`-be kerül a név betöltése előtt, legfeljebb 0x40 bájton belül; ha más regiszter vagy más sorrend, javítsd a `find_cvar` szabályát és a tesztjét;
- a konzolváltozó osztályának konstruktorában: milyen eltoláson tárolja a név mutatóját (`kCvarNameOffset`), hol van az egész és a lebegőpontos érték (`kCvarIntValue`, `kCvarFloatValue`), és van-e a lebegőpontos értékhez érvényességi jelző;
- a vtable-ben melyik indexen van az érték szövegből beállító függvény, és pontosan ez-e a szignatúrája: `void set(this, const char*)`; ha más, írd le pontosan, mert a 9. feladat ezt hívja (`kCvarSetterVtableIndex`).
Ha a konzolváltozó nem tárolja a név mutatóját, a `find_cvar` névmutató-ellenőrzését cseréld arra, hogy pontosan egy jelölt legyen, és ezt írd a jegyzetbe.

- [ ] **Step 8: A szabad kamera sebességének konzolváltozója**

Run: `strings -a "../game/eurotrucks2.exe" | grep -iE '^[a-z_]*(cam|fly)[a-z_]*$' | grep -iE 'speed|fly|free|debug' | sort -u`
Minden jelöltre futtasd a lekérdezést `string <név> /tmp/q-<név>.c` argumentumokkal, és válaszd azt, amelyet a `debug_camera` frissítő függvénye a mozgás sebességeként olvas (a játék alapértéke 100).
Ha nincs ilyen nevű változó, a `debug_camera` frissítő függvényéből kövesd vissza, honnan olvassa a sebességet, és írd le a jegyzetbe; ebben az esetben állj meg, és kérdezd meg a felhasználót, mert a megoldás eltér a specifikációtól.
Írd be a nevet: `inline constexpr std::string_view kFreeCameraSpeedCvar = "<név>";`

- [ ] **Step 9: A jegyzet és az ellenőrzött értékek**

`docs/game-notes/ets2-1.61.md` szerkezete (minden mondat külön sorban):
```markdown
# ETS2 1.61 - a játék belső felépítése

Játékverzió: <a főmenüből>
eurotrucks2.exe SHA-256: <ellenőrzőösszeg>
PE időbélyeg: 0x<...>, képméret: 0x<...>

## Kamerakezelő
<a regisztráció, a létrehozó, a globális változó, a mezők; mindegyiknél a Ghidra-függvény címe, amelyből kiderült>

## Szabad kamera
<RTTI név, a mezők, a forgatás számítása>

## Konzolváltozók
<a regisztráló minta, az objektum felépítése, a beállító függvény, a sebességváltozó neve>

## Eltérések a kiinduló feltevésektől
<minden eltérés, vagy "nincs">

## Amit csak az élő teszt dönthet el
<például a futás közben kitöltött táblák>
```
A `game_layout.h` minden feltevés-kommentjét cseréld erre a formára: `// ellenőrizve: ETS2 <verzió>, Ghidra (<függvénycím>)`.

- [ ] **Step 10: Tesztek és elemzőeszköz újra**

Run: `cmake --build build/host && ctest --test-dir build/host --output-on-failure && build/host/freewalk_probe "../game/eurotrucks2.exe"`
Expected: `0 failure(s)`; az elemzőeszköz megtalálja a kamerakezelőt (vagy a jegyzet indokolja, miért csak a játékban található meg), és a `g_developer`-hez pontosan egy jelöltet ad.

- [ ] **Step 11: Commit**

```bash
git add tools/ghidra docs/game-notes src tests
git commit -m "Verify game layout against ETS2 1.61 executable"
```

---

### Task 9: Kamera-hozzáférés és felderítő verzió (1. élő teszt)

**Files:**
- Create: `src/win/process_memory.h`, `src/win/process_memory.cpp`, `src/win/input.h`, `src/win/input.cpp`, `src/win/game_camera.h`, `src/win/game_camera.cpp`
- Modify: `src/win/plugin.cpp`, `CMakeLists.txt` (új Windows források)

**Interfaces:**
- Consumes: `Memory`, `PeImage` (Task 5); `find_camera_manager_global`, `find_cvar`, `list_cameras`, `find_camera`, `fw::layout` (Task 6, Task 8); `CameraPosition`, `to_world` (Task 1); `strf` (Task 2); a plugin váza (Task 7).
- Produces:
  - `class fw::ProcessMemory : Memory { bool read(...) const override; bool write(uint64_t, const void*, size_t) const; template<class T> bool put(uint64_t, const T&) const; }`.
  - `bool fw::game_window_focused()`, `bool fw::key_down(int virtual_key)`.
  - `class fw::GameCamera` (lásd lent): `init`, `resolve_cameras`, `active_index`, `free_index`, `request`, `yaw`, `set_yaw`, `read_position`, `write_position`, `developer_enable`, `developer_restore`, `speed_freeze`, `speed_restore`, `describe`.

- [ ] **Step 1: A saját folyamat memóriája**

`src/win/process_memory.h`:
```cpp
#pragma once
#include <cstddef>
#include <cstdint>
#include "core/memory.h"

namespace fw {

// A játék (a saját folyamat) memóriája.
// Az olvasás és az írás érvénytelen címnél hibát ad vissza, nem omlik össze.
class ProcessMemory final : public Memory {
 public:
  bool read(std::uint64_t address, void* out, std::size_t size) const override;
  bool write(std::uint64_t address, const void* data, std::size_t size) const;

  template <class T>
  bool put(std::uint64_t address, const T& value) const {
    return write(address, &value, sizeof value);
  }
};

}  // namespace fw
```

`src/win/process_memory.cpp`:
```cpp
#include "win/process_memory.h"
#include <windows.h>

namespace fw {

bool ProcessMemory::read(std::uint64_t address, void* out, std::size_t size) const {
  SIZE_T done = 0;
  return ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<LPCVOID>(address), out, size, &done) && done == size;
}

bool ProcessMemory::write(std::uint64_t address, const void* data, std::size_t size) const {
  // Csak lefoglalt memóriába írunk; a WriteProcessMemory a csak olvasható lapokat ideiglenesen írhatóvá teszi.
  MEMORY_BASIC_INFORMATION info{};
  if (VirtualQuery(reinterpret_cast<LPCVOID>(address), &info, sizeof info) == 0 || info.State != MEM_COMMIT) return false;
  SIZE_T done = 0;
  return WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<LPVOID>(address), data, size, &done) && done == size;
}

}  // namespace fw
```

- [ ] **Step 2: Billentyűzet és ablakfókusz**

`src/win/input.h`:
```cpp
#pragma once

namespace fw {

// Igaz, ha az előtérben lévő ablak ehhez a folyamathoz (a játékhoz) tartozik.
bool game_window_focused();

// Igaz, ha a billentyű éppen le van nyomva.
bool key_down(int virtual_key);

}  // namespace fw
```

`src/win/input.cpp`:
```cpp
#include "win/input.h"
#include <windows.h>

namespace fw {

bool game_window_focused() {
  const HWND window = GetForegroundWindow();
  if (!window) return false;
  DWORD process = 0;
  GetWindowThreadProcessId(window, &process);
  return process == GetCurrentProcessId();
}

bool key_down(int virtual_key) { return (GetAsyncKeyState(virtual_key) & 0x8000) != 0; }

}  // namespace fw
```

- [ ] **Step 3: A kamera kezelése**

`src/win/game_camera.h`:
```cpp
#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include "core/coords.h"
#include "core/pe_image.h"
#include "core/vec.h"
#include "win/process_memory.h"

namespace fw {

// A játék kamerakezelője, a szabad kamera és a hozzá tartozó két konzolváltozó elérése.
class GameCamera {
 public:
  // Megkeresi a játék futtatható fájljában a kamerakezelő globális változóját és a két konzolváltozót.
  // Igazat ad, ha mind megvan; a kamerák listája csak egy világ betöltése után olvasható, azt a resolve_cameras tölti be.
  bool init(std::vector<std::string>& log);

  // Újraolvassa a kamerák listáját, és megkeresi benne a szabad kamerát; a játék világonként újra létrehozhatja a kamerákat.
  bool resolve_cameras(std::vector<std::string>& log);

  int active_index() const;  // -1, ha nem olvasható vagy nem hihető
  int free_index() const { return free_index_; }
  bool request(int index);

  std::optional<double> yaw() const;
  bool set_yaw(double yaw);
  std::optional<CameraPosition> read_position() const;
  bool write_position(const CameraPosition& position);

  bool developer_enable();
  void developer_restore();
  bool speed_freeze();
  void speed_restore();

  // Diagnosztikai sorok a felderítéshez: a kamerakezelő mezői, a szabad kamera nyers mezői, a konzolváltozók értéke.
  void describe(const Vec3& truck, std::vector<std::string>& lines) const;

 private:
  std::uint64_t manager() const;
  bool set_cvar(std::uint64_t cvar, const std::string& value);

  ProcessMemory memory_;
  std::optional<PeImage> image_;
  std::uint64_t manager_global_ = 0;
  std::uint64_t developer_cvar_ = 0;
  std::uint64_t speed_cvar_ = 0;
  std::uint64_t free_camera_ = 0;
  int free_index_ = -1;
  int camera_count_ = 0;
  std::optional<std::int32_t> saved_developer_;
  std::optional<float> saved_speed_;
};

}  // namespace fw
```

`src/win/game_camera.cpp`:
```cpp
#include "win/game_camera.h"
#include <windows.h>
#include "core/game_layout.h"
#include "core/locator.h"
#include "core/text.h"

namespace fw {
namespace {

// A szabad kamera pozíciója a memóriában: 3 float, utána a két int16 négyzet-sorszám, összesen 16 bájt.
#pragma pack(push, 1)
struct RawPosition {
  float x, y, z;
  std::int16_t chunk_x, chunk_z;
};
#pragma pack(pop)
static_assert(sizeof(RawPosition) == 16);

}  // namespace

bool GameCamera::init(std::vector<std::string>& log) {
  const auto base = reinterpret_cast<std::uint64_t>(GetModuleHandleW(nullptr));
  image_ = PeImage::parse(memory_, base);
  if (!image_) {
    log.push_back("the game executable's headers could not be read");
    return false;
  }
  log.push_back(strf("game executable: timestamp 0x%08X, image size 0x%X", image_->timestamp(), image_->image_size()));
  if (const auto g = find_camera_manager_global(memory_, *image_, log)) manager_global_ = *g;
  if (const auto c = find_cvar(memory_, *image_, layout::kDeveloperCvar, true, log)) developer_cvar_ = *c;
  if (layout::kFreeCameraSpeedCvar.empty()) log.push_back("free camera speed cvar: name not known in this build");
  else if (const auto c = find_cvar(memory_, *image_, layout::kFreeCameraSpeedCvar, true, log)) speed_cvar_ = *c;
  return manager_global_ != 0 && developer_cvar_ != 0 && speed_cvar_ != 0;
}

std::uint64_t GameCamera::manager() const {
  if (manager_global_ == 0) return 0;
  return memory_.get<std::uint64_t>(manager_global_).value_or(0);
}

bool GameCamera::resolve_cameras(std::vector<std::string>& log) {
  free_index_ = -1;
  free_camera_ = 0;
  camera_count_ = 0;
  const std::uint64_t m = manager();
  if (m == 0) {
    log.push_back("cameras: the camera manager does not exist yet");
    return false;
  }
  const auto cameras = list_cameras(memory_, m, log);
  camera_count_ = static_cast<int>(cameras.size());
  const auto free = find_camera(cameras, layout::kFreeCameraClass);
  if (!free) {
    log.push_back(strf("cameras: %d camera(s), the free camera is not among them", camera_count_));
    return false;
  }
  free_index_ = free->index;
  free_camera_ = free->object;
  return true;
}

int GameCamera::active_index() const {
  const std::uint64_t m = manager();
  if (m == 0) return -1;
  const auto index = memory_.get<std::int32_t>(m + layout::kManagerCurrentIndex);
  if (!index || *index < 0 || *index >= camera_count_) return -1;
  return *index;
}

bool GameCamera::request(int index) {
  const std::uint64_t m = manager();
  if (m == 0 || index < 0 || index >= camera_count_) return false;
  return memory_.put<std::int32_t>(m + layout::kManagerRequestedIndex, index);
}

std::optional<double> GameCamera::yaw() const {
  if (free_camera_ == 0) return std::nullopt;
  const auto v = memory_.get<float>(free_camera_ + layout::kCameraYaw);
  if (!v) return std::nullopt;
  return *v;
}

bool GameCamera::set_yaw(double yaw) {
  return free_camera_ != 0 && memory_.put<float>(free_camera_ + layout::kCameraYaw, static_cast<float>(yaw));
}

std::optional<CameraPosition> GameCamera::read_position() const {
  if (free_camera_ == 0) return std::nullopt;
  const auto raw = memory_.get<RawPosition>(free_camera_ + layout::kCameraLocalPosition);
  if (!raw) return std::nullopt;
  return CameraPosition{raw->chunk_x, raw->chunk_z, raw->x, raw->y, raw->z};
}

bool GameCamera::write_position(const CameraPosition& p) {
  if (free_camera_ == 0) return false;
  const RawPosition raw{p.x, p.y, p.z, p.chunk_x, p.chunk_z};
  return memory_.put(free_camera_ + layout::kCameraLocalPosition, raw);
}

bool GameCamera::developer_enable() {
  const auto value = memory_.get<std::int32_t>(developer_cvar_ + layout::kCvarIntValue);
  if (developer_cvar_ == 0 || !value) return false;
  saved_developer_ = *value;
  return *value == 1 || memory_.put<std::int32_t>(developer_cvar_ + layout::kCvarIntValue, 1);
}

void GameCamera::developer_restore() {
  if (saved_developer_ && *saved_developer_ != 1) memory_.put<std::int32_t>(developer_cvar_ + layout::kCvarIntValue, *saved_developer_);
  saved_developer_.reset();
}

bool GameCamera::speed_freeze() {
  const auto value = memory_.get<float>(speed_cvar_ + layout::kCvarFloatValue);
  if (speed_cvar_ == 0 || !value) return false;
  saved_speed_ = *value;
  return set_cvar(speed_cvar_, "0");
}

void GameCamera::speed_restore() {
  if (saved_speed_) set_cvar(speed_cvar_, strf("%g", *saved_speed_));
  saved_speed_.reset();
}

bool GameCamera::set_cvar(std::uint64_t cvar, const std::string& value) {
  // A konzolváltozó saját, szövegből beállító virtuális függvényét hívjuk, így a játék minden belső értéke (szöveg, szám) együtt frissül.
  // A függvény címét csak akkor hívjuk meg, ha a játék kódszakaszába mutat.
  const auto vtable = memory_.get<std::uint64_t>(cvar);
  if (!vtable) return false;
  const auto function = memory_.get<std::uint64_t>(*vtable + 8ull * layout::kCvarSetterVtableIndex);
  if (!function || !image_ || !image_->in_section(".text", *function)) return false;
  using Setter = void (*)(std::uint64_t self, const char* text);
  reinterpret_cast<Setter>(*function)(cvar, value.c_str());
  return true;
}

void GameCamera::describe(const Vec3& truck, std::vector<std::string>& lines) const {
  const std::uint64_t m = manager();
  if (m == 0) {
    lines.push_back("camera manager: not created yet");
    return;
  }
  const auto current = memory_.get<std::int32_t>(m + layout::kManagerCurrentIndex);
  const auto requested = memory_.get<std::int32_t>(m + layout::kManagerRequestedIndex);
  lines.push_back(strf("camera manager: current %d, requested %d, cameras %d, free camera index %d", current.value_or(-999),
                       requested.value_or(-999), camera_count_, free_index_));
  if (free_camera_ != 0) {
    const auto yaw_value = memory_.get<float>(free_camera_ + layout::kCameraYaw);
    const auto pitch = memory_.get<float>(free_camera_ + layout::kCameraPitch);
    const auto roll = memory_.get<float>(free_camera_ + layout::kCameraRoll);
    lines.push_back(strf("free camera angles: yaw %.4f pitch %.4f roll %.4f", yaw_value.value_or(0.0f), pitch.value_or(0.0f),
                         roll.value_or(0.0f)));
    if (const auto p = read_position()) {
      const Vec3 w = to_world(*p);
      lines.push_back(strf("free camera position: chunk (%d, %d) local (%.2f, %.2f, %.2f) = world (%.2f, %.2f, %.2f), %.1f m from the truck",
                           p->chunk_x, p->chunk_z, p->x, p->y, p->z, w.x, w.y, w.z, horizontal_distance(w, truck)));
    }
  }
  if (developer_cvar_ != 0)
    lines.push_back(strf("g_developer value: %d", memory_.get<std::int32_t>(developer_cvar_ + layout::kCvarIntValue).value_or(-999)));
  if (speed_cvar_ != 0)
    lines.push_back(strf("free camera speed value: %g", memory_.get<float>(speed_cvar_ + layout::kCvarFloatValue).value_or(-999.0f)));
}

}  // namespace fw
```

Add to the `FreeWalk` target sources in `CMakeLists.txt`: `src/win/process_memory.cpp`, `src/win/input.cpp`, `src/win/game_camera.cpp`.

- [ ] **Step 4: Felderítő naplózás a pluginban**

`src/win/plugin.cpp` módosítása:
- Új include-ok: `"core/coords.h"`, `"win/game_camera.h"`.
- A `State`-be új mezők:
```cpp
  fw::GameCamera camera;
  bool camera_ready = false;
  LARGE_INTEGER frequency{};
  LARGE_INTEGER last_frame{};
  double discovery_timer = 0;
```
- Új függvények az `on_frame_end` elé, és az `on_frame_end` cseréje:
```cpp
double frame_seconds() {
  LARGE_INTEGER now;
  QueryPerformanceCounter(&now);
  const double dt = static_cast<double>(now.QuadPart - g->last_frame.QuadPart) / static_cast<double>(g->frequency.QuadPart);
  g->last_frame = now;
  return dt;
}

void log_lines(const std::vector<std::string>& lines, const std::string& prefix = "") {
  for (const auto& line : lines) fw::log_info(prefix + line);
}

// Másodpercenként kiírja a kamera és a kamion nyers adatait; az élő teszt ezekből ellenőrzi a game_layout.h feltevéseit.
void discovery(double dt) {
  g->discovery_timer += dt;
  if (g->discovery_timer < 1.0) return;
  g->discovery_timer = 0;
  std::vector<std::string> lines;
  g->camera.resolve_cameras(lines);
  g->camera.describe(g->truck.position, lines);
  lines.push_back(fw::strf("truck: (%.2f, %.2f, %.2f) heading %.4f rad, speed %.2f m/s, parking brake %d, paused %d", g->truck.position.x,
                           g->truck.position.y, g->truck.position.z, g->truck.heading, g->truck.speed, g->truck.parking_brake ? 1 : 0,
                           g->paused ? 1 : 0));
  log_lines(lines, "[discovery] ");
}

SCSAPI_VOID on_frame_end(const scs_event_t, const void* const, const scs_context_t) {
  const double dt = frame_seconds();
  if (g->config.discovery_log) discovery(dt);
}
```
- A `scs_telemetry_init`-ben a `load_config()` után:
```cpp
  QueryPerformanceFrequency(&g->frequency);
  QueryPerformanceCounter(&g->last_frame);
  std::vector<std::string> lines;
  g->camera_ready = g->camera.init(lines);
  log_lines(lines);
  if (!g->camera_ready) fw::log_warning("walking is off: not everything was found in this game build (see the lines above)");
```
A `discovery` függvény a felderítéshez a kamerák listáját is újraolvassa, ezért a `camera.resolve_cameras` naplóját is kiírja.

- [ ] **Step 5: Fordítás**

Run: `cmake --build build/win && cmake --build build/host && ctest --test-dir build/host --output-on-failure`
Expected: figyelmeztetés nélküli fordítás, `0 failure(s)`.

- [ ] **Step 6: A felderítő csomag összeállítása**

Run:
```bash
mkdir -p dist/FreeWalk-0.1-discovery && cp build/win/FreeWalk.dll dist/FreeWalk-0.1-discovery/
sed 's/^discovery_log = 0/discovery_log = 1/' FreeWalk.ini > dist/FreeWalk-0.1-discovery/FreeWalk.ini
```
`dist/FreeWalk-0.1-discovery/TESZT.md` (minden mondat külön sorban):
```markdown
# FreeWalk 0.1 - felderítő teszt

Ez a verzió semmit nem változtat a játékban, csak naplóz.

1. Másold a FreeWalk.dll-t és a FreeWalk.ini-t a játék bin\win_x64\plugins mappájába.
2. A Dokumentumok\Euro Truck Simulator 2\config.cfg fájlban állítsd be: uset g_developer "1" (a teszt után visszaállíthatod).
3. Indítsd el a játékot, töltsd be a mentést, és ülj 10 másodpercig a parkoló kamionban.
4. Nyomd meg a numerikus billentyűzet 0 gombját (szabad kamera), várj 3 másodpercet.
5. Fordulj az egérrel pontosan a kamion haladási irányába, várj 3 másodpercet.
6. Repülj előre körülbelül 10 métert, várj 3 másodpercet.
7. Fordulj balra 90 fokot, várj 3 másodpercet.
8. Nyomd meg az 1-es gombot (vissza a fülkébe), várj 3 másodpercet, majd lépj ki a játékból.
9. Küldd el a Dokumentumok\Euro Truck Simulator 2\game.log.txt fájlt.
```

- [ ] **Step 7: Élő teszt és kiértékelés**

Add át a felhasználónak a `dist/FreeWalk-0.1-discovery/` mappát, és várd meg a `game.log.txt`-t.
A naplóból ellenőrizendő és a `docs/game-notes/ets2-1.61.md`-be írandó:
- a betöltéskor kiírt kamerakezelő- és konzolváltozó-keresés sikeres-e;
- a kamerák listája hihető-e, és a szabad kamera indexe megegyezik-e a `current` értékkel a 4. lépés után, a fülkekamera indexe pedig az 1. és a 8. lépés idején;
- a `requested` érték követi-e a `current`-et;
- az 5. lépés után a szabad kamera irányszöge megegyezik-e a kamion headingjével (ha nem, az eltérésből adódik a `heading_to_camera_yaw` képlete, például előjel vagy eltolás);
- a 6. lépés után a pozíció kb. 10 m-rel előrébb van-e a kamion haladási irányában, és a világkoordináta hihető-e (néhány méterre a kamiontól kezdetben);
- a 7. lépés után az irányszög kb. +1,571-gyel (90 fokkal) nőtt-e;
- a `g_developer` értéke 1, a sebesség értéke hihető (alapérték 100).
Minden eltérést javíts a `game_layout.h`-ban, és jegyezd fel; ha egy keresés nem sikerült, a napló alapján javítsd a `locator.cpp`-t a hozzá tartozó teszttel együtt.

- [ ] **Step 8: Commit**

```bash
git add CMakeLists.txt src docs
git commit -m "Add game camera access and discovery logging, verified in game"
```

---

### Task 10: Kiszállás és visszaszállás (2. élő teszt)

Ebben a lépésben a gyalogló még nem mozog: az F10 a vezetőoldali ajtó mellé teszi a kamerát, ahol körül lehet nézni, és az F10 visszavisz a fülkébe.

**Files:**
- Modify: `src/win/plugin.cpp`

**Interfaces:**
- Consumes: `WalkSession`, `SessionInput`, `SessionOutput`, `Command`, `Phase` (Task 4); `Walker` (Task 3); `EdgeDetector`, `map_keys` (Task 2); `to_camera`, `to_world` (Task 1); `GameCamera`, `game_window_focused`, `key_down` (Task 9); `layout::heading_to_camera_yaw` (Task 6, Task 9).
- Produces: a kész kiszállás/visszaszállás folyamat a pluginban, a `walk(dt, focused)` függvény üres törzzsel, amelyet a 11. feladat tölt ki.

- [ ] **Step 1: A séta folyamata a pluginban**

`src/win/plugin.cpp` módosítása:
- Új include-ok: `"core/game_layout.h"`, `"core/keys.h"`, `"core/walker.h"`, `"win/input.h"`.
- A `State`-be új mezők:
```cpp
  fw::WalkSession session;
  fw::Walker walker;
  fw::EdgeDetector toggle;
  int saved_camera = -1;
  bool have_eye = false;
  fw::Vec3 eye;  // a legutóbb kiszámolt szempozíció, amelyet a kiválasztott eseményben írunk be
```
- Új függvények az `on_frame_end` elé:
```cpp
// Visszaállít mindent, amit a kiszállás megváltoztatott; többször is hívható.
void return_to_truck(bool restore_camera, const std::string& reason) {
  g->session.force_return(reason);
  g->have_eye = false;
  if (restore_camera && g->saved_camera >= 0 && !g->camera.request(g->saved_camera))
    fw::log_warning("the previous camera could not be requested back");
  g->camera.speed_restore();
  g->camera.developer_restore();
  g->saved_camera = -1;
  fw::log_info("back in the truck: " + reason);
}

void begin_exit(const std::string& reason) {
  fw::log_info(reason);
  g->saved_camera = g->camera.active_index();
  if (!g->camera.developer_enable()) fw::log_warning("g_developer could not be switched on: the free camera may not activate");
  if (!g->camera.speed_freeze()) fw::log_warning("the free camera speed could not be set to 0: the view may drift while walking");
  if (!g->camera.request(g->camera.free_index())) return_to_truck(true, "the free camera could not be requested");
}

void start_walk(const fw::SessionOutput& out) {
  const auto current = g->camera.read_position();
  if (!current || fw::horizontal_distance(fw::to_world(*current), g->truck.position) > 1000.0) {
    return_to_truck(true, "the free camera's position does not look right");
    fw::log_error("walking is switched off for this session: the free camera layout check failed");
    g->camera_ready = false;
    return;
  }
  g->walker.reset(out.start_feet);
  if (!g->camera.set_yaw(fw::layout::heading_to_camera_yaw(out.start_yaw))) fw::log_warning("the free camera's direction could not be set");
  g->eye = g->walker.eye(g->config);
  g->have_eye = true;
  fw::log_info(fw::strf("walking from (%.2f, %.2f, %.2f)", out.start_feet.x, out.start_feet.y, out.start_feet.z));
}

// Séta közben egy képkocka mozgása; a 11. feladat tölti ki.
void walk(double, bool) {}

void write_eye() {
  if (!g->have_eye || g->session.phase() != fw::Phase::Walking) return;
  if (!g->camera.write_position(fw::to_camera(g->eye))) return_to_truck(true, "the free camera's position could not be written");
}
```
- Az `on_frame_end` cseréje és egy új `on_frame_start`:
```cpp
SCSAPI_VOID on_frame_start(const scs_event_t, const void* const, const scs_context_t) {
  if (g->camera_ready && g->config.write_on_frame_start) write_eye();
}

SCSAPI_VOID on_frame_end(const scs_event_t, const void* const, const scs_context_t) {
  const double dt = frame_seconds();
  if (g->config.discovery_log) discovery(dt);
  if (!g->camera_ready) return;

  const bool focused = fw::game_window_focused();
  const bool toggle = g->toggle.update(focused && fw::key_down(g->config.key_toggle));
  if (toggle && g->session.phase() == fw::Phase::Driving) {
    std::vector<std::string> lines;
    if (!g->camera.resolve_cameras(lines)) log_lines(lines);
  }

  fw::SessionInput in;
  in.truck = g->truck;
  in.toggle_pressed = toggle;
  in.active_camera = g->camera.active_index();
  in.free_camera = g->camera.free_index();
  in.dt = dt;
  in.walker_feet = g->walker.feet();
  const fw::SessionOutput out = g->session.update(in, g->config);

  switch (out.command) {
    case fw::Command::None:
      if (!out.reason.empty()) fw::log_info("not leaving the truck: " + out.reason);
      break;
    case fw::Command::RequestFreeCamera: begin_exit(out.reason); break;
    case fw::Command::StartWalk: start_walk(out); break;
    case fw::Command::Walk: walk(dt, focused); break;
    case fw::Command::Return: return_to_truck(true, out.reason); break;
    case fw::Command::ReturnKeepCamera: return_to_truck(false, out.reason); break;
  }
  if (g->camera_ready && !g->config.write_on_frame_start) write_eye();
}
```
- A `register_all`-ba új sor: `ok &= p->register_for_event(SCS_TELEMETRY_EVENT_frame_start, on_frame_start, nullptr) == SCS_RESULT_ok;`
- A `scs_telemetry_shutdown` elejére, a `fw::log_info("unloaded")` elé:
```cpp
  if (g->camera_ready && g->session.phase() != fw::Phase::Driving) return_to_truck(true, "the game is unloading the plugin");
```

- [ ] **Step 2: Fordítás**

Run: `cmake --build build/win && cmake --build build/host && ctest --test-dir build/host --output-on-failure`
Expected: figyelmeztetés nélküli fordítás, `0 failure(s)`.

- [ ] **Step 3: A teszt csomag**

Run:
```bash
mkdir -p dist/FreeWalk-0.2-exit && cp build/win/FreeWalk.dll dist/FreeWalk-0.2-exit/
sed 's/^discovery_log = 0/discovery_log = 1/' FreeWalk.ini > dist/FreeWalk-0.2-exit/FreeWalk.ini
```
`dist/FreeWalk-0.2-exit/TESZT.md`:
```markdown
# FreeWalk 0.2 - kiszállás teszt

1. Másold a FreeWalk.dll-t és a FreeWalk.ini-t a plugins mappába (a régit írd felül).
2. A config.cfg-ben állítsd vissza a g_developer értékét 0-ra (a plugin magától bekapcsolja és visszaállítja).
3. Töltsd be a mentést, és vezetés közben nyomd meg az F10-et: nem szabad történnie semminek.
4. Állj meg, húzd be a kéziféket, nyomd meg az F10-et: a kép a vezetőoldali ajtó mellé, a talajra kerül.
5. Nézz körül az egérrel, nyomd meg a W-t: a kép nem mozdulhat el.
6. Készíts egy képernyőképet a kamionról.
7. Nyomd meg az F10-et: vissza a fülkébe, ugyanabba a nézetbe.
8. Szállj ki újra, majd séta közben lépj ki a játékból a menün át.
9. Indítsd újra a játékot, és ellenőrizd, hogy a config.cfg-ben a g_developer értéke továbbra is 0.
10. Küldd el a game.log.txt-t és a képernyőképet.
```

- [ ] **Step 4: Élő teszt és kiértékelés**

Add át a csomagot, és várd meg a naplót és a képet.
Ellenőrizendő:
- a 3. lépésnél a naplóban `not leaving the truck: the truck is moving`;
- a 4. lépésnél `leaving the truck`, majd `walking from (...)`, és a képen a kamera a vezetőoldali ajtó mellett, szemmagasságban, kifelé nem a fülkén belül;
- az 5. lépésnél a pozíció a felderítő sorokban változatlan (a sebesség nullázása működik);
- a 7. lépésnél `back in the truck: the walk key was pressed`, és a nézet ugyanaz, mint előtte;
- a 8. lépésnél `back in the truck: the game is unloading the plugin`;
- a 9. lépésnél a `g_developer` 0 maradt.
Ha a kamera nem a talajon van, a kiinduló magasság feltevése (a kamion origója a talaj szintjén) hibás: a mért különbséget írd a `FreeWalk.ini` alapértelmezett `ground_offset` értékébe és a `Config` alapértékébe (a teszttel együtt), és jegyezd fel a game-notes-ba.
Ha a kamera nem az ajtónál van, a `head_in_truck` számítását ellenőrizd a naplóban kiírt `driver's head at (...)` alapján.

- [ ] **Step 5: Commit**

```bash
git add src FreeWalk.ini docs
git commit -m "Add leaving and returning to the truck through the free camera"
```

---

### Task 11: Séta (3. élő teszt)

**Files:**
- Modify: `src/win/plugin.cpp`

**Interfaces:**
- Consumes: `map_keys` (Task 2); `Walker::step`, `Walker::eye` (Task 3); `GameCamera::yaw` (Task 9); `layout::camera_yaw_to_heading` (Task 6, Task 9); a 10. feladat `return_to_truck` függvénye.
- Produces: a kész 0.3-as plugin.

- [ ] **Step 1: A mozgás bekötése**

`src/win/plugin.cpp`-ben a `walk` függvény cseréje:
```cpp
// Séta közben egy képkocka mozgása: a billentyűk és a kamera iránya alapján lép a gyalogló, és frissül a beírandó szempozíció.
void walk(double dt, bool focused) {
  const auto yaw = g->camera.yaw();
  if (!yaw) {
    return_to_truck(true, "the free camera's direction could not be read");
    return;
  }
  const fw::WalkKeys keys = fw::map_keys(focused, g->paused, fw::key_down, g->config);
  g->walker.step(keys, fw::layout::camera_yaw_to_heading(*yaw), dt, g->config);
  g->eye = g->walker.eye(g->config);
}
```
A verziót írd át: `constexpr const char* kVersion = "0.3.0";`

- [ ] **Step 2: Fordítás**

Run: `build.cmd`
Expected: figyelmeztetés nélküli fordítás, `0 failure(s)`.

- [ ] **Step 3: A teszt csomag**

Run:
```bash
mkdir -p dist/FreeWalk-0.3-walk && cp build/msvc-Release/FreeWalk.dll FreeWalk.ini README.md dist/FreeWalk-0.3-walk/
```
`dist/FreeWalk-0.3-walk/TESZT.md`:
```markdown
# FreeWalk 0.3 - séta teszt

1. Másold a FreeWalk.dll-t és a FreeWalk.ini-t a plugins mappába (a régit írd felül).
2. Parkolj le, húzd be a kéziféket, nyomd meg az F10-et.
3. Sétálj körbe a kamion körül W A S D-vel, közben nézz körül az egérrel.
4. Fuss egy kört a Shift nyomva tartásával.
5. Figyeld, remeg-e vagy rángat-e a kép járás közben; ha igen, készíts róla rövid videót.
6. Alt-tabbal válts át egy másik ablakra, nyomd meg ott a W-t, majd válts vissza: a gyalogló nem mozdulhatott el.
7. Nyomd meg az Esc-et, a menüben nyomd meg a W-t, majd zárd be a menüt: a gyalogló nem mozdulhatott el.
8. Sétálj el 150 méternél messzebb: automatikusan vissza kell kerülnöd a fülkébe.
9. Szállj ki újra, és nyomd meg az 1-es gombot (fülkekamera): a séta véget ér.
10. Ha az 5. lépésnél remegett a kép: a FreeWalk.ini-ben állítsd be: write_on_frame_start = 1, és ismételd meg a 2-5. lépést.
11. Küldd el a game.log.txt-t, a videót, és írd meg, melyik beállítással volt sima a kép.
```

- [ ] **Step 4: Élő teszt és kiértékelés**

Add át a csomagot, és várd meg a visszajelzést.
Ellenőrizendő:
- a séta iránya mindig a nézési irány (ha fordított vagy oldalra csúszik, a `heading_to_camera_yaw` / `camera_yaw_to_heading` képletét kell javítani);
- a 6. és 7. lépésnél a gyalogló nem mozdult;
- a 8. lépésnél `back in the truck: too far from the truck`, a 9. lépésnél `back in the truck: another camera was selected`;
- a nézési irány séta közben helyes, vagyis a játék a saját irányszögeiből számolja a forgatást; ha nem, ez a specifikáció forgatás-írási pontja, és külön feladatként kell pótolni;
- melyik írási időpont adott sima képet; ha a `write_on_frame_start = 1` volt a jobb, ez legyen az alapérték a `Config`-ban (a teszttel együtt) és a `FreeWalk.ini`-ben.
Minden eltérést javíts, jegyezz fel a game-notes-ba, és szükség esetén készíts új csomagot.

- [ ] **Step 5: Végső ellenőrzés és commit**

Run: `build.cmd`
Expected: `0 failure(s)`, a DLL elkészül.

```bash
git add src FreeWalk.ini docs
git commit -m "Add walking with run and head bob"
```

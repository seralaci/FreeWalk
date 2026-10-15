# FreeWalk beállítóablak - megvalósítási terv

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Egy `Ctrl+F10`-re nyíló, angol nyelvű Windows-ablak, amelyben a játékos a FreeWalk minden beállítását játék közben, azonnali hatással módosíthatja, és a Save gombbal az INI-be mentheti.

**Architecture:** A beállítások egyetlen leírótáblája a magban (`settings_schema`) hajtja az INI beolvasását, az INI írását és az ablak felépítését.
Az ablak a DLL saját szálán fut (Win32, Common Controls 6), a módosított beállítást zárral védett helyen adja át; a játék szála a `frame_end` elején veszi át, és egy tesztelt szabály (`apply_settings`) szerint alkalmazza.

**Tech Stack:** C++20, Win32 API, Common Controls 6 (aktivációs környezettel a DLL saját manifestjéből), CMake + Ninja, MSVC (`build.cmd`).

**Spec:** `docs/superpowers/specs/2026-10-14-settings-window-design.md`

## Global Constraints

- A naplósorok angolok, csak ASCII, `[FreeWalk] ` előtaggal (a `fw::log_*` adja az előtagot); a kódkommentek magyarok.
- A felület nyelve angol.
- A fordítás `/W4` mellett figyelmeztetésmentes, a futtatókönyvtár statikus.
- Minden új logika teszttel kezdődik (piros, majd zöld); a játékfüggetlen rész a magba (`src/core`) kerül.
- Minden tesztcsomagnál nő a `kVersion` (`src/win/plugin.cpp`); ez az alprojekt: `0.4.0`.
- Az SDK naplózója nem szálbiztos: az ablak szála nem naplóz közvetlenül, a sorokat a játék szála írja ki.
- Commit üzenetekben nincs `Co-Authored-By` sor.

## Review Focus

- Billentyűütközés (két funkció ugyanazon a billentyűn): a Save tiltva, piros állapotsor; teszt: `key_conflicts_are_found` (Task 2).
- Kézzel beírt hibás szám (betű, tartományon kívül): nem kerül át, a csúszka marad; a mag oldalán a tartomány a táblából jön, teszt: `defaults_are_inside_their_range_and_on_the_step_grid` (Task 2).
- Billentyűváltás séta közben: a gyalogló billentyűi nem változnak meg menet közben; teszt: `keys_wait_for_the_next_exit_while_walking` (Task 3).
- A csomagolt `FreeWalk.ini` eltér attól, amit a Save írna: teszt: `shipped_ini_matches_the_formatter` (Task 2).
- A plugin leállítása nyitott ablakkal: az ablak bezárul, a leállítás megvárja (élő teszt, Task 5).

---

### Task 1: Billentyűnevek és billentyűkombinációk

**Files:**
- Modify: `src/core/config.h`, `src/core/config.cpp` (a `parse_key` mellett `key_name`, `CTRL+` és `NONE`)
- Modify: `src/core/keys.h`, `src/core/keys.cpp` (`combo_down`)
- Test: `tests/test_keys.cpp`

**Interfaces:**
- Produces: `inline constexpr int kKeyCtrl = 0x100;`, `int parse_key(std::string_view)` (új: `"NONE"` -> 0, `"CTRL+F10"` -> `kKeyCtrl | 0x79`), `std::string key_name(int code)`, `bool combo_down(int code, const std::function<bool(int)>& is_down)`.

- [ ] **Step 1: Write the failing tests** (`tests/test_keys.cpp` végére)

```cpp
TEST(key_name_and_parse_key_are_inverse) {
  for (int code = 1; code < 255; ++code) {
    CHECK(fw::parse_key(fw::key_name(code)) == code);
    if (code != 0x11 && code != 0xA2 && code != 0xA3) CHECK(fw::parse_key(fw::key_name(fw::kKeyCtrl | code)) == (fw::kKeyCtrl | code));
  }
}

TEST(key_names_are_readable) {
  CHECK(fw::key_name('W') == "W");
  CHECK(fw::key_name(0x79) == "F10");
  CHECK(fw::key_name(0x10) == "SHIFT");
  CHECK(fw::key_name(fw::kKeyCtrl | 0x79) == "CTRL+F10");
  CHECK(fw::key_name(0) == "NONE");
  CHECK(fw::parse_key("none") == 0);
  CHECK(fw::parse_key("ctrl+f10") == (fw::kKeyCtrl | 0x79));
  CHECK(fw::parse_key("CTRL+CTRL") == -1);
}

TEST(combo_needs_ctrl_exactly_when_asked) {
  bool ctrl = false;
  const auto down = [&](int k) { return k == 0x79 || (k == 0x11 && ctrl); };
  CHECK(fw::combo_down(0x79, down));
  CHECK(!fw::combo_down(fw::kKeyCtrl | 0x79, down));
  ctrl = true;
  CHECK(!fw::combo_down(0x79, down));  // Ctrl+F10 nem kiszállás
  CHECK(fw::combo_down(fw::kKeyCtrl | 0x79, down));
  CHECK(!fw::combo_down(0, down));
}
```

- [ ] **Step 2: Run to verify it fails** - `build.cmd`; várt: fordítási hiba (`key_name`, `kKeyCtrl`, `combo_down` nem létezik).

- [ ] **Step 3: Implementation**

`src/core/config.h` (a `Config` előtt, és a `parse_key` deklarációja után):
```cpp
// A billentyűkódhoz adva: a billentyű a Ctrl-lal együtt (csak a parancsbillentyűknél: kiszállás, beállítóablak).
inline constexpr int kKeyCtrl = 0x100;
```
```cpp
// A billentyűkód neve az INI-hez és a felülethez ("W", "F10", "SHIFT", "CTRL+F10", "NONE", "0x6A"); a parse_key párja.
std::string key_name(int code);
```
A `parse_key` megjegyzése bővül: `"NONE"` = 0 (kikapcsolva), `"CTRL+<név>"` = `kKeyCtrl | kód`.

`src/core/config.cpp`, a `parse_key` elején (a `trim`/`to_upper` után):
```cpp
  if (n == "NONE") return 0;
  if (n.rfind("CTRL+", 0) == 0) {
    const int base = parse_key(std::string_view(n).substr(5));
    return (base > 0 && base != 0x11 && base != 0xA2 && base != 0xA3) ? (kKeyCtrl | base) : -1;
  }
```
és utána:
```cpp
std::string key_name(int code) {
  if (code == 0) return "NONE";
  if ((code & kKeyCtrl) != 0) return "CTRL+" + key_name(code & 0xFF);
  if ((code >= 'A' && code <= 'Z') || (code >= '0' && code <= '9')) return std::string(1, static_cast<char>(code));
  switch (code) {
    case 0x09: return "TAB";
    case 0x10: return "SHIFT";
    case 0x11: return "CTRL";
    case 0x12: return "ALT";
    case 0x20: return "SPACE";
    case 0xA0: return "LSHIFT";
    case 0xA1: return "RSHIFT";
    case 0xA2: return "LCTRL";
    case 0xA3: return "RCTRL";
    default: break;
  }
  if (code >= 0x70 && code <= 0x87) return strf("F%d", code - 0x6F);
  return strf("0x%02X", code);
}
```

`src/core/keys.h`:
```cpp
// Egy parancsbillentyű (kiszállás, beállítóablak) le van-e nyomva: a Ctrl-nak pontosan akkor kell lenyomva lennie, ha a kód kéri,
// így az F10 és a Ctrl+F10 nem zavarja egymást. A 0 kód (kikapcsolva) sosem számít lenyomottnak.
bool combo_down(int code, const std::function<bool(int)>& is_down);
```
`src/core/keys.cpp`:
```cpp
bool combo_down(int code, const std::function<bool(int)>& is_down) {
  const int base = code & 0xFF;
  if (base == 0 || !is_down(base)) return false;
  const bool ctrl = is_down(0x11);
  return (code & kKeyCtrl) != 0 ? ctrl : !ctrl;
}
```

- [ ] **Step 4: Run** - `build.cmd`; várt: `100% tests passed`, figyelmeztetés nélkül.
- [ ] **Step 5: Commit** - `git commit -m "Add key names, NONE and CTRL+ key combinations"`

---

### Task 2: A beállítások leírótáblája, az INI írása és beolvasása

**Files:**
- Create: `src/core/settings.h`, `src/core/settings.cpp`
- Modify: `src/core/config.h` (új mező: `int key_settings = kKeyCtrl | 0x79;`), `src/core/config.cpp` (a `parse_config` a táblára épül)
- Modify: `CMakeLists.txt` (`src/core/settings.cpp`, `tests/test_settings.cpp`, `FREEWALK_INI_PATH` a teszteknek)
- Modify: `FreeWalk.ini` (pontosan a `format_config(Config{})` kimenete)
- Test: `tests/test_settings.cpp`

**Interfaces:**
- Produces:
```cpp
enum class SettingKind { Key, Number, Flag };
enum class Applies { Now, Restart };  // a billentyűk séta közbeni késleltetését az apply_settings kezeli
struct SettingInfo {
  std::string_view section, key, tab, label, comment;
  SettingKind kind = SettingKind::Number;
  Applies applies = Applies::Now;
  int Config::*key_field = nullptr;
  bool allow_ctrl = false, allow_none = false;
  double Config::*number_field = nullptr;
  double min = 0, max = 0, step = 0;
  int decimals = 0;
  bool Config::*flag_field = nullptr;
};
const std::vector<SettingInfo>& settings_schema();
std::string format_setting_value(const SettingInfo& s, const Config& c);
std::string format_config(const Config& c);
struct KeyConflict { std::string_view first, second; };
std::vector<KeyConflict> key_conflicts(const Config& c);
bool same_settings(const Config& a, const Config& b);
```

- [ ] **Step 1: Write the failing tests** (`tests/test_settings.cpp`)

```cpp
#include <fstream>
#include <set>
#include <sstream>
#include "check.h"
#include "core/settings.h"

TEST(schema_entries_are_unique_and_complete) {
  std::set<std::string> names;
  for (const auto& s : fw::settings_schema()) {
    CHECK(names.insert(std::string(s.section) + "." + std::string(s.key)).second);
    const int fields = (s.key_field ? 1 : 0) + (s.number_field ? 1 : 0) + (s.flag_field ? 1 : 0);
    CHECK(fields == 1);
    CHECK(!s.label.empty());
  }
  CHECK(names.size() == 22);
}

TEST(defaults_are_inside_their_range_and_on_the_step_grid) {
  const fw::Config c;
  for (const auto& s : fw::settings_schema()) {
    if (s.kind != fw::SettingKind::Number) continue;
    const double v = c.*s.number_field;
    CHECK(v >= s.min && v <= s.max);
    const double steps = (v - s.min) / s.step;
    CHECK_NEAR(steps, std::round(steps), 1e-6);
  }
}

TEST(format_then_parse_gives_the_same_settings) {
  fw::Config c;
  c.key_toggle = fw::kKeyCtrl | 'G';
  c.key_settings = 0;
  c.key_run = 0xA2;
  c.walk_speed = 1.7;
  c.exit_face_angle = -15;
  c.head_bob = false;
  c.directinput_filter = true;
  std::vector<std::string> warnings;
  const fw::Config back = fw::parse_config(fw::format_config(c), warnings);
  CHECK(warnings.empty());
  CHECK(fw::same_settings(back, c));
  CHECK(!fw::same_settings(back, fw::Config{}));
}

TEST(shipped_ini_matches_the_formatter) {
  std::ifstream in(FREEWALK_INI_PATH, std::ios::binary);
  std::stringstream text;
  text << in.rdbuf();
  CHECK(text.str() == fw::format_config(fw::Config{}));
}

TEST(key_conflicts_are_found) {
  fw::Config c;
  CHECK(fw::key_conflicts(c).empty());
  c.key_run = 'W';
  const auto conflicts = fw::key_conflicts(c);
  CHECK(conflicts.size() == 1);
  CHECK(conflicts[0].first == "Walk forward");
  CHECK(conflicts[0].second == "Run (hold)");
}

TEST(walking_keys_cannot_use_ctrl_or_none) {
  std::vector<std::string> warnings;
  const fw::Config c = fw::parse_config("[keys]\nforward = CTRL+W\nback = NONE\nsettings = NONE\n", warnings);
  CHECK(warnings.size() == 2);
  CHECK(c.key_forward == 'W');
  CHECK(c.key_back == 'S');
  CHECK(c.key_settings == 0);
}
```
(`#include <cmath>` a `std::round`-hoz.)

- [ ] **Step 2: Run** - `build.cmd`; várt: fordítási hiba (`core/settings.h` nincs).

- [ ] **Step 3: Implementation**

`src/core/settings.cpp` táblája (sorrend = INI-sorrend; a `tab` az ablak füle):

| section | key | tab | label | kind | tartomány / lépés / tizedes | applies | megjegyzés (INI, magyar) |
|---|---|---|---|---|---|---|---|
| keys | toggle | Keys | Leave / return to the truck | Key, allow_ctrl | | Now | kiszállás és visszaszállás |
| keys | settings | Keys | Open this window | Key, allow_ctrl, allow_none | | Now | a beállítóablak (NONE = kikapcsolva) |
| keys | forward | Keys | Walk forward | Key | | Now | |
| keys | back | Keys | Walk back | Key | | Now | |
| keys | left | Keys | Step left | Key | | Now | |
| keys | right | Keys | Step right | Key | | Now | |
| keys | run | Keys | Run (hold) | Key | | Now | |
| walker | walk_speed | Walking | Walking speed (m/s) | Number | 0.1-20 / 0.1 / 1 | Now | m/s |
| walker | run_speed | Walking | Running speed (m/s) | Number | 0.1-20 / 0.1 / 1 | Now | m/s |
| walker | eye_height | Walking | Eye height (m) | Number | 0.5-3 / 0.01 / 2 | Now | m a talaj felett |
| walker | head_bob | Walking | Head bob | Flag | | Now | 1 = enyhe fejbiccentés lépés közben, 0 = ki |
| walker | head_bob_amplitude | Walking | Head bob amount (m) | Number | 0-0.2 / 0.005 / 3 | Now | m |
| walker | mouse_sensitivity | Walking | Mouse look sensitivity | Number | 0.01-2 / 0.01 / 2 | Now | az egérrel körbenézés érzékenysége séta közben (a játék szabad kamerájáé 1) |
| walker | max_distance | Walking | Maximum distance from the truck (m) | Number | 10-2000 / 10 / 0 | Now | m, ennél messzebb a kamiontól automatikus visszaszállás |
| walker | exit_side_distance | Leaving the truck | Distance beside the driver's head (m) | Number | 0.3-5 / 0.05 / 2 | Now | m-rel a vezető fejének helyén túl, oldalra |
| walker | exit_face_door | Leaving the truck | Face the door when leaving | Flag | | Now | 1 = kiszálláskor az ajtó felé néz, 0 = a kamion orra felé |
| walker | exit_face_angle | Leaving the truck | Turn towards the front (degrees) | Number | -90-90 / 1 / 0 | Now | fok: ennyivel fordul az ajtó felől a kamion orra felé (negatív: a hátulja felé) |
| walker | ground_offset | Leaving the truck | Ground height offset (m) | Number | -5-5 / 0.05 / 2 | Now | m, a talaj magassága a kamion telemetriás pozíciójához képest |
| debug | discovery_log | Debug | Diagnostic log lines every second | Flag | | Now | 1 = másodpercenként diagnosztikai sorok a game.log.txt-be |
| debug | write_on_frame_start | Debug | Write the camera at frame start | Flag | | Now | 1 = a kamera pozícióját a képkocka elején írja (ha a kép remeg) |
| debug | directinput_filter | Debug | DirectInput keyboard filter | Flag | | Restart | 1 = DirectInput billentyűzet-szűrő (kísérleti; a séta billentyűit a játék beállításaiban kell felszabadítani) |
| debug | switch_test | Debug | Camera switch test only | Flag | | Restart | 1 = a billentyű csak átvált a szabad kamerára és vissza (felderítéshez) |

`format_setting_value`: billentyű -> `key_name`; kapcsoló -> `"1"`/`"0"`; szám -> `strf("%.*f", decimals, v)`, a tizedespont utáni záró nullák és a magányos pont levágva, `"-0"` -> `"0"`.

`format_config`: fejléc három sora (`; FreeWalk - kiszállás és séta az Euro Truck Simulator 2-ben`, `; Minden sor elhagyható: a hiányzó értékek helyett az alapérték érvényes.`, `; A beállítóablak (Ctrl+F10) Save gombja ezt a fájlt teljesen újraírja.`), majd szakaszonként üres sor, `[szakasz]`, és soronként `kulcs = érték`, megjegyzésnél 26 karakterre kiegészítve és `; megjegyzés`.

`key_conflicts`: a billentyűmezők minden párja, ahol a két kód egyezik és nem 0, a tábla sorrendjében (`first` a korábbi).
`same_settings`: minden táblabeli mező egyezik.

`src/core/config.cpp` `parse_config`: a három `std::map` helyett a táblában keres (`s.section == section && s.key == kulcs`); a figyelmeztető szövegek változatlanok.
Billentyűnél a `parse_key` eredménye akkor hibás, ha `< 0`, ha `0` és `!allow_none`, vagy ha `kKeyCtrl` bitje van és `!allow_ctrl` (`"unknown key name, the default is kept"`).

`CMakeLists.txt` a teszt célnál: `target_compile_definitions(freewalk_tests PRIVATE FREEWALK_INI_PATH="${CMAKE_CURRENT_SOURCE_DIR}/FreeWalk.ini")`.

`FreeWalk.ini`: a `format_config(Config{})` pontos kimenete (LF sorvégekkel).

- [ ] **Step 4: Run** - `build.cmd`; várt: `100% tests passed`, a régi INI-tesztek is zöldek.
- [ ] **Step 5: Commit** - `git commit -m "Describe every setting in one table that drives INI reading and writing"`

---

### Task 3: A módosított beállítások alkalmazása és a figyelt billentyűk cseréje

**Files:**
- Modify: `src/core/settings.h/.cpp` (`apply_settings`)
- Modify: `src/core/key_filter.h/.cpp` (`watch_only`)
- Test: `tests/test_settings.cpp`, `tests/test_key_filter.cpp`

**Interfaces:**
- Produces: `Config apply_settings(const Config& active, const Config& wanted, bool walking);`, `void KeyFilter::watch_only(const std::vector<int>& keys);`

- [ ] **Step 1: Write the failing tests**

```cpp
TEST(keys_wait_for_the_next_exit_while_walking) {
  fw::Config active;
  fw::Config wanted;
  wanted.key_forward = 'I';
  wanted.walk_speed = 2.0;
  const fw::Config walking = fw::apply_settings(active, wanted, true);
  CHECK(walking.key_forward == 'W');
  CHECK_NEAR(walking.walk_speed, 2.0, 1e-12);
  const fw::Config driving = fw::apply_settings(walking, wanted, false);
  CHECK(driving.key_forward == 'I');
}

TEST(restart_settings_never_change_while_running) {
  fw::Config active;
  fw::Config wanted;
  wanted.directinput_filter = true;
  wanted.switch_test = true;
  wanted.discovery_log = true;
  const fw::Config result = fw::apply_settings(active, wanted, false);
  CHECK(!result.directinput_filter);
  CHECK(!result.switch_test);
  CHECK(result.discovery_log);
}
```
(`tests/test_key_filter.cpp`):
```cpp
TEST(watch_only_replaces_the_watched_keys) {
  fw::KeyFilter f;
  f.watch({kW});
  f.watch_only({'D'});
  CHECK(!f.on_key(kW, true, true));
  CHECK(f.on_key('D', true, true));
}
```

- [ ] **Step 2: Run** - várt: fordítási hiba.
- [ ] **Step 3: Implementation**

```cpp
Config apply_settings(const Config& active, const Config& wanted, bool walking) {
  Config result = wanted;
  for (const auto& s : settings_schema()) {
    const bool keep = s.applies == Applies::Restart || (walking && s.kind == SettingKind::Key);
    if (!keep) continue;
    if (s.key_field) result.*s.key_field = active.*s.key_field;
    if (s.number_field) result.*s.number_field = active.*s.number_field;
    if (s.flag_field) result.*s.flag_field = active.*s.flag_field;
  }
  return result;
}
```
```cpp
void KeyFilter::watch_only(const std::vector<int>& keys) {
  for (auto& w : watched_) w = false;
  for (const int k : keys)
    if (valid(k)) watched_[k] = true;
}
```

- [ ] **Step 4: Run** - várt: zöld.
- [ ] **Step 5: Commit** - `git commit -m "Apply changed settings with keys waiting for the next exit and restart-only switches kept"`

---

### Task 4: A beállítóablak (Win32)

**Files:**
- Create: `src/win/settings_window.h`, `src/win/settings_window.cpp`, `src/win/FreeWalk.manifest`, `src/win/FreeWalk.rc.in`
- Modify: `CMakeLists.txt` (`enable_language(RC)` Windowson, `configure_file` a `.rc`-hez, `comctl32` linkelése, az új forrás)

**Interfaces:**
- Consumes: `settings_schema`, `format_config`, `format_setting_value`, `key_conflicts`, `key_name`, `kKeyCtrl` (Task 1-2).
- Produces:
```cpp
struct SettingsWindowSetup {
  Config current;               // a megnyitáskori (élő) beállítások
  std::wstring ini_path;
  bool ini_had_unknown_keys = false;
  HWND game_window = nullptr;   // bezáráskor ez kapja vissza a fókuszt
};
bool settings_window_open(const SettingsWindowSetup& setup);  // hamis, ha már nyitva van vagy nem indult el
bool settings_window_is_open();
void settings_window_close();      // bezárás kérése, a Cancel-lel azonos
void settings_window_shutdown();   // bezárja, és legfeljebb 2 s-ig vár a szálára
std::optional<Config> settings_window_take_wanted();   // a játék szála képkockánként
std::vector<std::string> settings_window_take_log();   // a játék szála írja ki
```

Felépítés (`settings_window.cpp`):
- Egy `Window` osztály a szál saját példányával; a szál indulásakor aktivációs környezetet hoz létre a DLL `101`-es `RT_MANIFEST` erőforrásából (Common Controls 6), `InitCommonControlsEx`, a rendszer üzenetbetűtípusa (`SPI_GETNONCLIENTMETRICS`), méretezés `LOGPIXELSY / 96` szerint.
- Ablak: `WS_EX_TOPMOST | WS_EX_CONTROLPARENT`, cím `FreeWalk settings`, a játékablak közepére igazítva; tetején `WC_TABCONTROL` a tábla füleivel (Keys, Walking, Leaving the truck, Debug), soronként felirat és vezérlő:
  szám: `TRACKBAR_CLASS` (0..(max-min)/step) + szerkesztőmező; kapcsoló: jelölőnégyzet; billentyű: nyomógomb a `key_name` felirattal.
  A `Restart` sorok felirata ` *`-gal végződik.
- Fülenkénti megjegyzés: Keys: "Remove these keys from the truck controls in the game (Options, Keys & Buttons), or the truck also gets them."; Debug: "* takes effect after restarting the game.".
- Alul: piros állapotsor (`WM_CTLCOLORSTATIC`), és a Defaults, Cancel (`IDCANCEL`, így az `Esc` is), Save gomb.
- Események: csúszka -> érték és mező frissül; mező `EN_CHANGE` -> érvényes, tartományon belüli számnál csúszka és beállítás frissül, különben a sor hibásnak jelölve; jelölőnégyzet; billentyűgomb -> "Press a key..." mód, a következő `WM_KEYDOWN`/`WM_SYSKEYDOWN` az üzenetciklusban (`Esc` megszakít, `Backspace` = NONE, ha megengedett, `Ctrl` lenyomva = `CTRL+`, ha megengedett).
  Minden változás után `push_wanted`, az állapotsor frissül (hibás mező, `key_conflicts`, ismeretlen INI-kulcsok figyelmeztetése), a Save tiltva hibás mező vagy ütközés esetén.
- A beállító kombináció (a megnyitáskori `key_settings`) az ablakban a Cancel-lel azonos.
- Save: `format_config` -> `<ini>.tmp`, majd `MoveFileExW(..., MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)`; hibánál üzenetablak és naplósor (`the settings could not be saved to FreeWalk.ini (error N)`), az ablak nyitva marad; sikernél naplósor (`settings saved to FreeWalk.ini`) és bezárás.
  Cancel / X / Esc: a megnyitáskori beállítás kerül vissza (`push_wanted`), bezárás. Bezáráskor `SetForegroundWindow(game_window)`.
- Leállítás: egy `stop` jelző és `WM_CLOSE`; a szál a létrehozás után is ellenőrzi a jelzőt.

- [ ] **Step 1:** A fenti fájlok megírása; `build.cmd` -> figyelmeztetésmentes fordítás, a tesztek zöldek.
- [ ] **Step 2:** `dumpbin /dependents` -> a DLL importjai közt `COMCTL32.dll` megjelenik; a `dumpbin /headers` szerint van `.rsrc` szakasz.
- [ ] **Step 3: Commit** - `git commit -m "Add the settings window"`

---

### Task 5: Beépítés a pluginba

**Files:**
- Modify: `src/win/plugin.cpp`, `src/win/input.h`, `src/win/input.cpp` (`keyboard_hook_watch`)
- Modify: `README.md` (Beállítóablak rész)

**Interfaces:**
- Consumes: Task 1-4 mindegyike; `void keyboard_hook_watch(const std::vector<int>& keys);` (új, a `KeyFilter::watch_only` köré).

- [ ] **Step 1:** `kVersion = "0.4.0"`; `State`-ben `fw::Config wanted`, `bool ini_had_unknown_keys`, `fw::EdgeDetector settings_key`.
  `load_config`: `g->wanted = g->config`; `ini_had_unknown_keys` = van-e `unknown setting` figyelmeztetés.
- [ ] **Step 2:** `on_frame_end` elején `take_settings()`: a `settings_window_take_log()` sorai `log_info`-val; ha van `take_wanted`, `g->wanted` = az; minden képkockában `next = apply_settings(g->config, g->wanted, phase != Driving)`;
  ha a séta billentyűi változtak: `keyboard_hook_watch`; ha séta közben a `mouse_sensitivity` változott: `camera.look_scale_set`; `g->config = next`.
- [ ] **Step 3:** A kiszállás billentyűje `combo_down(key_toggle, key_down)`; a beállítóablak: `g->settings_key.update(focused && combo_down(key_settings, key_down))` -> nyitva: `settings_window_close()`, különben `settings_window_open({g->wanted, ini_path(), g->ini_had_unknown_keys, GetForegroundWindow()})` (sikertelen nyitásnál naplósor).
  A switch_test ágban is a `combo_down` jelzi a kiszállás billentyűjét.
- [ ] **Step 4:** `scs_telemetry_shutdown` elején `settings_window_shutdown()`.
- [ ] **Step 5:** README: a beállítóablak (Ctrl+F10, fülek, azonnali hatás, Save, Cancel, a billentyűk séta közben a következő kiszállástól, a csillagos debug-beállítások újraindítás után).
- [ ] **Step 6:** `build.cmd` zöld, figyelmeztetés nélkül; probe mind talál. Commit: `git commit -m "Open the settings window with Ctrl+F10 and apply its changes live"`

---

### Task 6: Tesztcsomag és élő teszt

- [ ] **Step 1:** `dist/FreeWalk-0.4.0-settings/` (DLL, PDB, INI, README, `TESZT.md`), telepítés a felhasználó jóváhagyott `plugins` mappájába, amikor a játék nem fut; a régi napló áthelyezése a `game/logs`-ba.
- [ ] **Step 2:** `TESZT.md` lépései: megnyitás Ctrl+F10-re vezetés közben és séta közben; csúszka húzása séta közben (sebesség, érzékenység) és a hatás; billentyű átállítása (ütközés jelzése, Esc megszakítja); Cancel visszaállít; Save után újraindítás és megmaradás; F10 nem száll ki Ctrl+F10-re; az ablak bezárása után a játéké a fókusz.

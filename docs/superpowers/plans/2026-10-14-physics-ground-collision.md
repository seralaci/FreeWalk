# Talaj és ütközés a játék PhysX-jelenetéből - megvalósítási terv

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Séta közben a gyalogló kövesse a domborzatot és a lépcsőket, és ne menjen át a falakon, kerítéseken, a kamionon, hanem csússzon el mellettük; mindezt a játék saját PhysX-jelenetének lekérdezésével.

**Architecture:** A mag (`src/core`) egy `World` felületen át kérdezi a környezetet; a mozgás matematikája (csúszás, kitolás, talajkövetés, esés) és az origó-eltolás becslése játékfüggetlen, tesztelt kód. A Windows-réteg (`src/win/physx_scene.cpp`) megkeresi a jelenetet, frissíti az eltolást, és SEH-védett PhysX-hívásokkal valósítja meg a `World`-öt; hiba esetén a gyalogló sík talajt (`FlatWorld`) kap.

**Tech Stack:** C++20, MSVC (Visual Studio Build Tools), CMake + Ninja, `build.cmd`; Windows API (VirtualQuery, QueryWorkingSetEx, SEH); a játékba statikusan linkelt PhysX 3.4 a vtable-jén át.

**Spec:** `docs/superpowers/specs/2026-10-14-physics-ground-collision-design.md` (háttér: `docs/game-notes/physx-tmrealwalk.md`)

## Global Constraints

- A TM Real Walk kódjából semmit nem másolunk és nem írunk át; minden játéktényt a saját elemzésünkkel és élő teszttel igazolunk.
- A plugin a játék kódját nem módosítja; a PhysX-jelenetet csak lekérdezi (raycast, sweep, aktorlista, aktor helyzete).
- A naplószövegek angolok, csak ASCII karakterek, minden sor `[FreeWalk] ` előtaggal (a `fw::log_*` adja); a kódkommentek magyarok.
- A `game_layout.h` tartja a játék belső felépítésére vonatkozó értékeket, mindegyiknél az ellenőrzés helyével.
- A fordítás MSVC-vel figyelmeztetésmentes (`/W4 /permissive-`), a futtatókönyvtár statikus.
- Minden tesztcsomagnál a `kVersion` léptetése (`src/win/plugin.cpp`); ez a terv a `0.5.0`-t adja.
- Commit üzenetekbe nem kerül `Co-Authored-By` sor; a commit üzenetek angolok.
- Minden eltérés a tervtől `Ruling:` sorként kerül a végrehajtási naplóba (`.superpowers/sdd/2026-10-14-physics/progress.md`).
- A tesztek futtatása: `build.cmd` (a gyökérben: `FreeWalk\build.cmd`), amely fordít és lefuttatja a `freewalk_tests`-et; egy teszt piros futtatása ugyanígy történik, a kimenetben `FAIL` sorral.

## Review Focus

- A kezdőpont átfed a kamionnal vagy egy fallal (kiszállás szorosan a kamion mellett): a kitolás korlátos sebességgel kifelé visz, a gyalogló nem ragad be és nem ugrik nagyot (2. feladat: `overlap_pushes_out_at_limited_speed`, `deep_overlap_is_limited_per_frame`).
- A játékfájlban a szöveg vagy a vtable nem található (más játékverzió): a fizika kikapcsol, a séta sík talajjal megy, a napló megmondja az okot (1. feladat: a hibás esetek tesztjei; 7. feladat: `init` naplója).
- A jelenetjelöltek között szemét (felszabadított memória, saját verem) is van: csak az a jelölt fogadható el, amelyben a kamion aktora megtalálható (7. feladat, `validate`).
- Esés közben vagy a körzet szélén nincs talajtalálat: a láb az utolsó magasságon marad, nem zuhan végtelenbe (3. feladat: `no_ground_keeps_height_and_stops_a_fall`).
- Nagyon meredek felület (fal alja, kamion oldala) a talajsugár útjában: nem számít talajnak (3. feladat: `steep_surface_is_not_ground`).

---

## Fájlszerkezet

| Fájl | Felelősség |
|---|---|
| `src/core/pe_image.h/.cpp` (módosul) | `function_start`: a `.pdata` táblából egy cím befoglaló függvényének kezdete |
| `src/core/locator.h/.cpp` (módosul) | `find_physx_scene_vtable` |
| `src/core/game_layout.h` (módosul) | a két PhysX-üzenet, a slotok, a PhysX-hívások vtable-indexei |
| `src/core/world.h` (új) | `World`, `GroundHit`, `Capsule`, `SweepHit`, `FlatWorld` |
| `src/core/motion.h/.cpp` (új) | `MotionSettings`, `move_horizontal`, `move_vertical`, `start_height` |
| `src/core/walker.h/.cpp` (módosul) | a `step` a `World`-del dolgozik, `settle` |
| `src/core/scene_offset.h/.cpp` (új) | az 512 m-es eltolás becslése |
| `src/core/config.h`, `src/core/settings.cpp`, `FreeWalk.ini` (módosul) | `[collision]` rész |
| `src/win/physx_types.h` (új) | a PhysX 3.4 hívásokhoz szükséges szerkezetek |
| `src/win/physx_scene.h/.cpp` (új) | a jelenet keresése, állapotgép, eltolás, a `World` megvalósítása |
| `src/win/game_camera.h/.cpp` (módosul) | `world_key()`: a kamerakezelő címe a világbetöltés jelzésére |
| `src/win/plugin.cpp`, `src/win/settings_window.cpp` (módosul) | bekötés, Collision fül megjegyzése |
| `tools/probe.cpp` (módosul) | ötödik keresés |
| `tests/pe_builder.h` (módosul), `tests/test_world.h` (új) | `.pdata` a tesztképben; teszt-világ |
| `tests/test_motion.cpp`, `tests/test_scene_offset.cpp` (új) | a mag új tesztjei |

---

### Task 1: A PhysX-jelenet vtable-jének keresése (mag, probe)

**Files:**
- Modify: `src/core/pe_image.h`, `src/core/pe_image.cpp`
- Modify: `src/core/locator.h`, `src/core/locator.cpp`
- Modify: `src/core/game_layout.h`
- Modify: `tests/pe_builder.h`, `tests/test_pe_image.cpp`, `tests/test_locator.cpp`
- Modify: `tools/probe.cpp`

**Interfaces:**
- Produces: `std::optional<std::uint64_t> fw::function_start(const SectionBytes& pdata, std::uint64_t image_base, std::uint64_t address)`;
  `std::optional<std::uint64_t> fw::find_physx_scene_vtable(const Memory&, const PeImage&, std::vector<std::string>& log)`;
  `fw::layout::kPhysxReleaseMessage`, `kPhysxShiftOriginMessage`, `kPhysxReleaseSlot`, `kPhysxShiftOriginSlot`.

- [ ] **Step 1: A tesztkép bővítése `.pdata` szakasszal** (`tests/pe_builder.h`, a `put_rip_insn` után)

```cpp
inline constexpr std::uint64_t kPdata = kBase + 0x5000;

// A tesztképhez egy negyedik, .pdata szakaszt ad a 0x5000-es RVA-n, a függvénytáblának.
inline void add_pdata_section(fw::BufferMemory& m) {
  const std::uint64_t nt = kBase + 0x80;
  m.put<std::uint16_t>(nt + 6, 4);
  const std::uint64_t s = nt + 24 + 0xF0 + 40 * 3;
  const char name[8] = {'.', 'p', 'd', 'a', 't', 'a', 0, 0};
  m.put_bytes(s, name, 8);
  m.put<std::uint32_t>(s + 8, 0x1000);
  m.put<std::uint32_t>(s + 12, 0x5000);
  m.put<std::uint32_t>(s + 16, 0x1000);
  m.put<std::uint32_t>(s + 20, 0x3400);
}

// Egy függvény a .pdata táblában (RUNTIME_FUNCTION): kezdő és záró cím abszolút címként megadva.
inline void put_function_entry(fw::BufferMemory& m, int index, std::uint64_t begin, std::uint64_t end) {
  const std::uint64_t e = kPdata + 12ull * static_cast<std::uint64_t>(index);
  m.put<std::uint32_t>(e, static_cast<std::uint32_t>(begin - kBase));
  m.put<std::uint32_t>(e + 4, static_cast<std::uint32_t>(end - kBase));
  m.put<std::uint32_t>(e + 8, 0);
}
```

- [ ] **Step 2: Piros teszt a `function_start`-ra** (`tests/test_pe_image.cpp` végére)

```cpp
TEST(function_start_finds_the_containing_function) {
  auto m = fwtest::make_test_image();
  fwtest::add_pdata_section(m);
  fwtest::put_function_entry(m, 0, fwtest::kText + 0x10, fwtest::kText + 0x40);
  fwtest::put_function_entry(m, 1, fwtest::kText + 0x40, fwtest::kText + 0x90);
  fwtest::put_function_entry(m, 2, fwtest::kText + 0x100, fwtest::kText + 0x180);
  const auto img = fw::PeImage::parse(m, fwtest::kBase);
  CHECK(img && img->section(".pdata"));
  if (!img || !img->section(".pdata")) return;
  const auto pdata = img->read_section(m, *img->section(".pdata"));
  CHECK(fw::function_start(pdata, fwtest::kBase, fwtest::kText + 0x40) == fwtest::kText + 0x40);
  CHECK(fw::function_start(pdata, fwtest::kBase, fwtest::kText + 0x8F) == fwtest::kText + 0x40);
  CHECK(fw::function_start(pdata, fwtest::kBase, fwtest::kText + 0x120) == fwtest::kText + 0x100);
  CHECK(!fw::function_start(pdata, fwtest::kBase, fwtest::kText + 0x95));
  CHECK(!fw::function_start(pdata, fwtest::kBase, fwtest::kText + 0x05));
}
```

Run: `build.cmd` - Expected: fordítási hiba (`function_start` nincs deklarálva).

- [ ] **Step 3: A `function_start` megvalósítása**

`src/core/pe_image.h`, a `find_rip_refs_to` deklarációja után:

```cpp
// A .pdata függvénytáblájából (RUNTIME_FUNCTION: kezdő és záró RVA, unwind adat) annak a függvénynek a kezdőcíme, amelybe a cím esik.
// A linker a táblát kezdőcím szerint rendezi; a tábla végén álló nulla bejegyzések nem számítanak.
std::optional<std::uint64_t> function_start(const SectionBytes& pdata, std::uint64_t image_base, std::uint64_t address);
```

`src/core/pe_image.cpp`, a `find_rip_refs_to` után:

```cpp
std::optional<std::uint64_t> function_start(const SectionBytes& pdata, std::uint64_t image_base, std::uint64_t address) {
  if (address < image_base) return std::nullopt;
  const auto field = [&](std::size_t entry, std::size_t offset) {
    std::uint32_t v;
    std::memcpy(&v, pdata.bytes.data() + entry * 12 + offset, 4);
    return v;
  };
  std::size_t count = pdata.bytes.size() / 12;
  while (count > 0 && field(count - 1, 0) == 0 && field(count - 1, 4) == 0) --count;
  const std::uint64_t target = address - image_base;
  std::size_t lo = 0;
  std::size_t hi = count;
  while (lo < hi) {
    const std::size_t mid = (lo + hi) / 2;
    if (target < field(mid, 0)) hi = mid;
    else if (target >= field(mid, 4)) lo = mid + 1;
    else return image_base + field(mid, 0);
  }
  return std::nullopt;
}
```

Run: `build.cmd` - Expected: minden teszt zöld.

- [ ] **Step 4: A PhysX-állandók** (`src/core/game_layout.h`, a `camera_yaw_to_heading` után, a névtér vége előtt)

```cpp
// A játékba statikusan linkelt PhysX 3.4 jelenete (NpScene). A release és a shiftOrigin függvény ezekre a hibaüzenetekre hivatkozik;
// a jelenet vtable-je az a hely, ahol a release a kReleaseSlot, a shiftOrigin a kShiftOriginSlot helyén áll.
// Forrás: a TM Real Walk elemzése (docs/game-notes/physx-tmrealwalk.md); a játékfájlon a 2. feladat ellenőrzi.
inline constexpr std::string_view kPhysxReleaseMessage =
    "PxScene::release(): Scene is still being simulated! PxScene::fetchResults() is called implicitly.";
inline constexpr std::string_view kPhysxShiftOriginMessage =
    "PxScene::shiftOrigin() not allowed while simulation is running. Call will be ignored.";
inline constexpr std::size_t kPhysxReleaseSlot = 0;
inline constexpr std::size_t kPhysxShiftOriginSlot = 0x73;
```

- [ ] **Step 5: Piros tesztek a kereséshez** (`tests/test_locator.cpp` végére)

```cpp
namespace {

// Egy PhysX-jelenet mintája a tesztképben: a két üzenet a .rdata-ban, rájuk hivatkozó lea a két függvényben,
// a két függvény a .pdata-ban, és a vtable, amelyben a release és a shiftOrigin a várt helyen áll.
struct PhysxFixture {
  std::uint64_t release = fwtest::kText + 0x200;
  std::uint64_t shift = fwtest::kText + 0x300;
  std::uint64_t vtable = fwtest::kRdata + 0x800;
  std::uint64_t release_text = fwtest::kRdata + 0x100;
  std::uint64_t shift_text = fwtest::kRdata + 0x200;
};

fw::BufferMemory physx_image(const PhysxFixture& f) {
  auto m = fwtest::make_test_image();
  fwtest::add_pdata_section(m);
  m.put_bytes(f.release_text, fw::layout::kPhysxReleaseMessage.data(), fw::layout::kPhysxReleaseMessage.size());
  m.put_bytes(f.shift_text, fw::layout::kPhysxShiftOriginMessage.data(), fw::layout::kPhysxShiftOriginMessage.size());
  fwtest::put_rip_insn(m, f.release + 0x10, 0x48, 0x8D, 2, f.release_text);  // lea rdx, [üzenet]
  fwtest::put_rip_insn(m, f.shift + 0x20, 0x4C, 0x8D, 8, f.shift_text);      // lea r8, [üzenet]
  fwtest::put_function_entry(m, 0, f.release, f.release + 0x80);
  fwtest::put_function_entry(m, 1, f.shift, f.shift + 0x80);
  m.put<std::uint64_t>(f.vtable + 8 * fw::layout::kPhysxReleaseSlot, f.release);
  m.put<std::uint64_t>(f.vtable + 8 * fw::layout::kPhysxShiftOriginSlot, f.shift);
  return m;
}

std::optional<std::uint64_t> find_physx(const fw::BufferMemory& m, std::vector<std::string>& log) {
  const auto img = fw::PeImage::parse(m, fwtest::kBase);
  if (!img) return std::nullopt;
  return fw::find_physx_scene_vtable(m, *img, log);
}

}  // namespace

TEST(physx_scene_vtable_is_found) {
  const PhysxFixture f;
  const auto m = physx_image(f);
  std::vector<std::string> log;
  CHECK(find_physx(m, log) == f.vtable);
}

TEST(physx_scene_vtable_needs_both_messages) {
  const PhysxFixture f;
  auto m = physx_image(f);
  m.put<std::uint8_t>(f.shift_text, 'X');
  std::vector<std::string> log;
  CHECK(!find_physx(m, log));
}

TEST(physx_scene_vtable_needs_the_slot_distance) {
  const PhysxFixture f;
  auto m = physx_image(f);
  m.put<std::uint64_t>(f.vtable + 8 * fw::layout::kPhysxShiftOriginSlot, 0);
  m.put<std::uint64_t>(f.vtable + 8 * (fw::layout::kPhysxShiftOriginSlot - 1), f.shift);
  std::vector<std::string> log;
  CHECK(!find_physx(m, log));
}

TEST(physx_scene_vtable_must_be_unique) {
  const PhysxFixture f;
  auto m = physx_image(f);
  const std::uint64_t second = fwtest::kRdata + 0xC00;
  m.put<std::uint64_t>(second + 8 * fw::layout::kPhysxReleaseSlot, f.release);
  m.put<std::uint64_t>(second + 8 * fw::layout::kPhysxShiftOriginSlot, f.shift);
  std::vector<std::string> log;
  CHECK(!find_physx(m, log));
  CHECK(!log.empty() && log.back().find("2 vtable candidate(s)") != std::string::npos);
}
```

A fájl elején legyen `#include <optional>` és `#include "core/game_layout.h"` (ha még nincs).
Run: `build.cmd` - Expected: fordítási hiba (`find_physx_scene_vtable` nincs deklarálva).

- [ ] **Step 6: A keresés megvalósítása**

`src/core/locator.h`, a `find_camera` után:

```cpp
// A játékba linkelt PhysX jelenetosztályának vtable-je: a release és a shiftOrigin függvény a hibaüzenetükre hivatkozó kódból
// (a .pdata függvénytáblája adja a függvény elejét), a vtable pedig az egyetlen hely, ahol a kettő a várt slotban áll.
std::optional<std::uint64_t> find_physx_scene_vtable(const Memory& memory, const PeImage& image, std::vector<std::string>& log);
```

`src/core/locator.cpp`, a fájl végén, a névtér zárása előtt:

```cpp
std::optional<std::uint64_t> find_physx_scene_vtable(const Memory& m, const PeImage& image, std::vector<std::string>& log) {
  const std::string label = "physx scene: ";
  const Section* text = image.section(".text");
  const Section* pdata_section = image.section(".pdata");
  if (!text || !pdata_section) {
    log.push_back(label + "the executable has no .text/.pdata section");
    return std::nullopt;
  }
  const SectionBytes code = image.read_section(m, *text);
  const SectionBytes pdata = image.read_section(m, *pdata_section);
  std::vector<SectionBytes> constants;
  for (const char* name : {".rdata", "_RDATA"})
    if (const Section* s = image.section(name)) constants.push_back(image.read_section(m, *s));
  if (code.bytes.empty() || pdata.bytes.empty() || constants.empty() || constants.front().bytes.empty()) {
    log.push_back(label + "the executable's sections could not be read");
    return std::nullopt;
  }

  // Azoknak a függvényeknek a kezdőcíme, amelyekben egy lea az üzenetre mutat.
  const auto functions_using = [&](std::string_view message, const char* what) {
    std::set<std::uint64_t> starts;
    bool text_found = false;
    for (const auto& c : constants) {
      const auto at = find_cstring(c, message);
      if (!at) continue;
      text_found = true;
      for (const RipRef& ref : find_rip_refs_to(code, *at))
        if (ref.opcode == 0x8D)
          if (const auto start = function_start(pdata, image.base(), ref.address)) starts.insert(*start);
    }
    log.push_back(strf("%s%s: message %s, %zu function(s)", label.c_str(), what, text_found ? "found" : "NOT found", starts.size()));
    return starts;
  };
  const auto releases = functions_using(layout::kPhysxReleaseMessage, "release");
  const auto shifts = functions_using(layout::kPhysxShiftOriginMessage, "shiftOrigin");

  std::set<std::uint64_t> vtables;
  for (const auto& c : constants)
    for (const std::uint64_t release : releases)
      for (const std::uint64_t slot : find_qword(c, release)) {
        const std::uint64_t vtable = slot - 8 * layout::kPhysxReleaseSlot;
        const auto shift = m.get<std::uint64_t>(vtable + 8 * layout::kPhysxShiftOriginSlot);
        if (shift && shifts.count(*shift) != 0) vtables.insert(vtable);
      }
  if (vtables.size() != 1) {
    log.push_back(strf("%s%zu vtable candidate(s), expected exactly one", label.c_str(), vtables.size()));
    return std::nullopt;
  }
  log.push_back(strf("%svtable +0x%llX", label.c_str(), rva(image, *vtables.begin())));
  return *vtables.begin();
}
```

Run: `build.cmd` - Expected: minden teszt zöld.

- [ ] **Step 7: Az ötödik keresés a probe-ban** (`tools/probe.cpp`)

A `speed` keresése után:

```cpp
  const auto physx = fw::find_physx_scene_vtable(*memory, *image, log);
```

Az eredménysor és a visszatérési érték helyett:

```cpp
  std::printf("result: camera manager %s, free camera %s, g_developer %s, free camera speed %s, physx scene %s\n",
              manager ? "found" : "NOT FOUND", free_camera ? "found" : "NOT FOUND", developer ? "found" : "NOT FOUND",
              speed ? "found" : "NOT FOUND", physx ? "found" : "NOT FOUND");
  return manager && free_camera && developer && speed && physx ? 0 : 1;
```

- [ ] **Step 8: Futtatás a játékfájlon**

Run: `build.cmd`, majd `build\msvc-Release\freewalk_probe.exe ..\game\eurotrucks2.exe`
Expected: `physx scene: release: message found, 1 function(s)`, ugyanígy a shiftOrigin, `physx scene: vtable +0x...`, és az eredménysorban mind az öt `found`.
Ha valamelyik üzenet `NOT found`: a pontos szöveget a `handoff/eurotrucks2-strings.txt` 519407. és 519427. sora adja; a szöveg körüli bájtokat hexában kell megnézni (`python -I` egy rövid szkripttel a scratchpadben), és a `game_layout.h` állandóját a pontos, nullával lezárt szövegre javítani (`Ruling:` sor).
Ha több függvény vagy több vtable jön ki: a naplósorokat a végrehajtási naplóba, és a 2. feladat elemzése dönt.

- [ ] **Step 9: Commit**

```bash
git add src/core/pe_image.h src/core/pe_image.cpp src/core/locator.h src/core/locator.cpp src/core/game_layout.h tests/pe_builder.h tests/test_pe_image.cpp tests/test_locator.cpp tools/probe.cpp
git commit -m "Find the PhysX scene vtable from its two error messages and the .pdata function table"
```

---

### Task 2: A PhysX-felület ellenőrzése a játékfájlon

**Files:**
- Modify: `docs/game-notes/ets2-1.61.md`
- Modify: `src/core/game_layout.h`

**Interfaces:**
- Consumes: a probe kimenete (1. feladat).
- Produces: `fw::layout::kPxSceneGetNbActors`, `kPxSceneGetActors`, `kPxSceneRaycast`, `kPxSceneSweep`, `kPxActorGetGlobalPose` (vtable-indexek, `std::size_t`); a 7. feladat ezeket használja. Ha az ellenőrzés más értéket ad, a 7. feladat a javított értékkel dolgozik.

Eszközök: a `dumpbin` (`C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\dumpbin.exe`) és rövid Python-szkriptek a scratchpadben (`python -I`). Ghidra nincs telepítve, és nem is kell.

- [ ] **Step 1: A jelenet vtable-je az RTTI szerint.**
  A játékfájl `.rdata`-jában keresd meg a `.?AVNpScene@physx@@` típusleíró nevét (a típusleíró a név előtt 0x10 bájttal kezdődik).
  A Complete Object Locator az a 24 bájtos szerkezet, amelynek `signature` mezője 1, és a +0x0C-n a típusleíró RVA-ja áll.
  A vtable az a hely, amely előtt (a -8-on) a COL címe áll.
  Vesd össze a probe által talált vtable-lel: egyezniük kell, vagy a probe vtable-je az RTTI szerinti vtable egy másodlagos (többszörös öröklésből adódó) vtable-je.
  Ha a `NpScene` RTTI-je nincs a fájlban, ezt rögzítsd, és lépj tovább.
- [ ] **Step 2: A slotok és a release helye.**
  Írd ki a vtable első 0x80 slotját (RVA-k).
  Ellenőrizd, hogy a `kPhysxReleaseSlot` (0) helyén tényleg a release áll (a hibaüzenetre hivatkozó függvény), és hogy a vtable előtti qword a COL-ra mutat-e (vagyis a 0. slot a vtable eleje).
  A 0x90/8, 0x98/8, 0x2F0/8, 0x2F8/8 slot függvényeit disassembláld (`dumpbin /disasm /range:<start>,<start+0x200>`):
  a getNbActors és a getActors rövid, egy belső objektumra továbbít; a raycast és a sweep egy közös lekérdező sablonba hív, és mindkettőnek 8, illetve 10 paramétere van (a verem +0x28-tól olvasott argumentumok száma).
- [ ] **Step 3: Az aktor getGlobalPose-a.**
  Keresd meg a `.?AVNpRigidDynamic@physx@@` vtable-jét (RTTI, mint az 1. lépésben), és disassembláld a 0xA0/8 slotot: a második argumentumba (`rdx`) 28 bájtot ír (kvaternió és pozíció), és `rax`-ban visszaadja.
- [ ] **Step 4: A találati puffer virtuális függvényei.**
  Keresd meg a `.?AV?$PxHitBuffer@UPxRaycastHit@physx@@@physx@@` és a `.?AV?$PxHitBuffer@UPxSweepHit@physx@@@physx@@` vtable-jét, és disassembláld a slotjaikat.
  Elvárt: 0. slot `processTouches` (visszaad egy bool-t), 1. slot `finalizeQuery` (üres, `ret`), 2. slot a törlő destruktor.
  Ez dönti el a `physx_types.h` `HitBuffer` osztályának virtuális sorrendjét (7. feladat).
- [ ] **Step 5: Szálkérdés.**
  Rögzítsd a végrehajtási naplóban `Ruling:` sorként: a PhysX 3.4-ben a jelenet-lekérdezések a szimuláció futása alatt is megengedettek, és ilyenkor a legutóbbi `fetchResults` állapotát látják; a TM a `frame_end`-ből kérdez zárolás nélkül.
  Ezért a lekérdezések a `frame_end`-ben maradnak; az élő teszt (9. feladat) igazolja, hogy a hívások nem hibáznak és a játék stabil.
  Ha az 1-4. lépés közben olyan kód kerül elő, amely ennek ellentmond (például a lekérdezés a szimuláció alatt hibát ad), a döntést ennek megfelelően módosítsd.
- [ ] **Step 6: A vtable-indexek a `game_layout.h`-ba** (a PhysX-állandók után):

```cpp
// A PhysX 3.4 hívásai vtable-indexként (a bájteltolás / 8). A jelenet (NpScene) és a dinamikus aktor (NpRigidDynamic) vtable-je;
// a játékfájlon a 2. feladat ellenőrizte (docs/game-notes/ets2-1.61.md, "PhysX-jelenet").
inline constexpr std::size_t kPxSceneGetNbActors = 0x90 / 8;    // PxU32 getNbActors(PxActorTypeFlags) const
inline constexpr std::size_t kPxSceneGetActors = 0x98 / 8;      // PxU32 getActors(PxActorTypeFlags, PxActor**, PxU32, PxU32) const
inline constexpr std::size_t kPxSceneRaycast = 0x2F0 / 8;       // bool raycast(origin, unitDir, distance, hitCall, hitFlags, filterData, filterCall, cache) const
inline constexpr std::size_t kPxSceneSweep = 0x2F8 / 8;         // bool sweep(geometry, pose, unitDir, distance, hitCall, hitFlags, filterData, filterCall, cache, inflation) const
inline constexpr std::size_t kPxActorGetGlobalPose = 0xA0 / 8;  // PxTransform getGlobalPose() const
```

  Ha az ellenőrzés más indexet ad, a javított értéket írd ide, és a megjegyzésben jelöld az eltérést a TM-től.
- [ ] **Step 7: Jegyzet.** Az `ets2-1.61.md` végére egy `## PhysX-jelenet` szakasz: a két üzenet RVA-ja, a release és a shiftOrigin függvény RVA-ja, a vtable RVA-ja (és az RTTI-összevetés eredménye), a slotok ellenőrzése, a `HitBuffer` sorrendje, a szálkérdés döntése; minden állításnál, hogy minek alapján.
- [ ] **Step 8: Commit**

```bash
git add docs/game-notes/ets2-1.61.md src/core/game_layout.h
git commit -m "Verify the PhysX scene interface in the game executable"
```

---

### Task 3: `World` felület és vízszintes mozgás (csúszás, kitolás)

**Files:**
- Create: `src/core/world.h`, `src/core/motion.h`, `src/core/motion.cpp`
- Modify: `src/core/config.h` (a `[collision]` mezők; a beállítástáblát a 6. feladat bővíti)
- Create: `tests/test_world.h`, `tests/test_motion.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `fw::World` (`ground_below(Vec3 from, double max_distance)`, `sweep_capsule(const Capsule&, Vec3 center, Vec3 direction, double distance)`), `fw::GroundHit{height, normal}`, `fw::Capsule{radius, half_height}`, `fw::SweepHit{distance, normal, initial_overlap, depth}`, `fw::FlatWorld(double height)`;
  `fw::MotionSettings{radius, step_height, step_down_max, ground_follow_time, eye_height}`, `MotionSettings motion_settings(const Config&)`,
  `struct BodyCapsule{Capsule capsule; Vec3 center;}`, `BodyCapsule body_capsule(Vec3 feet, const MotionSettings&)`,
  `Vec3 move_horizontal(World&, Vec3 feet, Vec3 wish, double dt, const MotionSettings&)`;
  `Config::collision_enabled`, `collision_radius`, `step_height`, `step_down_max`, `ground_follow_time`.

- [ ] **Step 1: A `[collision]` mezők a `Config`-ban** (`src/core/config.h`, a `[walker]` mezők után)

```cpp
  // [collision]
  bool collision_enabled = true;    // talaj és ütközés a játék fizikájából (újraindítás után érvényes)
  double collision_radius = 0.40;   // m, a test sugara
  double step_height = 0.45;        // m, ennél alacsonyabb fokra a gyalogló fellép
  double step_down_max = 0.6;       // m, ennél nagyobb lépés lefelé esés
  double ground_follow_time = 0.12; // s, a talajkövetés simítása
```

- [ ] **Step 2: `src/core/world.h`**

```cpp
#pragma once
#include <optional>
#include "core/vec.h"

namespace fw {

// A talaj egy pontja egy lefelé mutató sugár találata alapján.
struct GroundHit {
  double height = 0;
  Vec3 normal{0, 1, 0};
};

// Függőleges kapszula: a középpont fölött és alatt félmagasságnyira egy-egy félgömb középpontja, ezek körül a sugár.
struct Capsule {
  double radius = 0;
  double half_height = 0;
};

// Egy kapszula söprésének első találata.
struct SweepHit {
  double distance = 0;           // a kiindulóponttól a találatig megtett út
  Vec3 normal;                   // a felület normálisa
  bool initial_overlap = false;  // a kapszula már a kiindulóponton átfed valamivel
  double depth = 0;              // átfedésnél a behatolás mélysége
};

// A környezet, amelyet a gyalogló lekérdez: a játék fizikája, a tesztekben egy egyszerű modell, hiba esetén sík talaj.
class World {
 public:
  virtual ~World() = default;
  // A from pontból lefelé, legfeljebb max_distance távolságra az első felület.
  virtual std::optional<GroundHit> ground_below(Vec3 from, double max_distance) = 0;
  // A center középpontú kapszula söprése a vízszintes, egységnyi direction irányba, distance hosszan.
  virtual std::optional<SweepHit> sweep_capsule(const Capsule& capsule, Vec3 center, Vec3 direction, double distance) = 0;
};

// Sík talaj egy adott magasságban, akadályok nélkül: a fizika nélküli séta és a visszaesés hiba esetén.
class FlatWorld final : public World {
 public:
  explicit FlatWorld(double height) : height_(height) {}
  std::optional<GroundHit> ground_below(Vec3 from, double max_distance) override {
    if (from.y < height_ || from.y - height_ > max_distance) return std::nullopt;
    return GroundHit{height_, {0, 1, 0}};
  }
  std::optional<SweepHit> sweep_capsule(const Capsule&, Vec3, Vec3, double) override { return std::nullopt; }

 private:
  double height_;
};

}  // namespace fw
```

- [ ] **Step 3: A teszt-világ** (`tests/test_world.h`)

```cpp
#pragma once
#include <functional>
#include <optional>
#include <vector>
#include "core/world.h"

namespace fwtest {

// Egy függőleges fal: egy pontja, a vízszintes egységnormálisa (a szabad oldal felé) és a teteje.
struct Wall {
  fw::Vec3 point;
  fw::Vec3 normal;
  double top = 1e9;
};

// Teszt-világ: talajfüggvény és végtelen függőleges falak (félterek).
class TestWorld final : public fw::World {
 public:
  std::function<std::optional<double>(double x, double z)> ground = [](double, double) { return std::optional<double>(0.0); };
  fw::Vec3 ground_normal{0, 1, 0};
  std::vector<Wall> walls;
  int sweeps = 0;

  std::optional<fw::GroundHit> ground_below(fw::Vec3 from, double max_distance) override {
    const auto h = ground(from.x, from.z);
    if (!h || *h > from.y || from.y - *h > max_distance) return std::nullopt;
    return fw::GroundHit{*h, ground_normal};
  }

  std::optional<fw::SweepHit> sweep_capsule(const fw::Capsule& c, fw::Vec3 center, fw::Vec3 dir, double distance) override {
    ++sweeps;
    std::optional<fw::SweepHit> best;
    for (const auto& w : walls) {
      if (center.y - c.half_height - c.radius >= w.top) continue;  // a kapszula alja a fal teteje fölött
      const double gap = (center.x - w.point.x) * w.normal.x + (center.z - w.point.z) * w.normal.z - c.radius;
      if (gap < 0) {
        if (!best || !best->initial_overlap || -gap > best->depth) best = fw::SweepHit{0, w.normal, true, -gap};
        continue;
      }
      const double approach = -(dir.x * w.normal.x + dir.z * w.normal.z);
      if (approach <= 1e-12) continue;
      const double d = gap / approach;
      if (d > distance) continue;
      if (!best || (!best->initial_overlap && d < best->distance)) best = fw::SweepHit{d, w.normal, false, 0};
    }
    return best;
  }
};

}  // namespace fwtest
```

- [ ] **Step 4: Piros tesztek** (`tests/test_motion.cpp`)

```cpp
#include <cmath>
#include "check.h"
#include "core/motion.h"
#include "test_world.h"

namespace {

fw::MotionSettings settings() { return fw::MotionSettings{}; }

// A kapszula felszíne és a fal síkja közötti rés.
double gap_to(const fwtest::Wall& w, fw::Vec3 feet) {
  return (feet.x - w.point.x) * w.normal.x + (feet.z - w.point.z) * w.normal.z - settings().radius;
}

}  // namespace

TEST(body_capsule_spans_from_step_height_to_above_the_eye) {
  const auto b = fw::body_capsule({1, 10, 2}, settings());
  CHECK_NEAR(b.center.x, 1.0, 1e-12);
  CHECK_NEAR(b.center.z, 2.0, 1e-12);
  CHECK_NEAR(b.center.y - b.capsule.half_height - b.capsule.radius, 10.45, 1e-9);
  CHECK_NEAR(b.center.y + b.capsule.half_height + b.capsule.radius, 10.0 + 1.7 + 0.08, 1e-9);
  CHECK_NEAR(b.capsule.radius, 0.40, 1e-12);
}

TEST(free_path_moves_the_whole_way) {
  fwtest::TestWorld world;
  const fw::Vec3 end = fw::move_horizontal(world, {0, 0, 0}, {0, 0, -1}, 0.1, settings());
  CHECK_NEAR(end.x, 0.0, 1e-12);
  CHECK_NEAR(end.z, -1.0, 1e-12);
  CHECK_NEAR(end.y, 0.0, 1e-12);
}

TEST(standing_still_does_not_query) {
  fwtest::TestWorld world;
  const fw::Vec3 end = fw::move_horizontal(world, {3, 0, 4}, {0, 0, 0}, 0.1, settings());
  CHECK_NEAR(end.x, 3.0, 1e-12);
  CHECK(world.sweeps == 0);
}

TEST(frontal_wall_stops_at_the_skin) {
  fwtest::TestWorld world;
  world.walls.push_back({{0, 0, -1}, {0, 0, 1}});
  const fw::Vec3 end = fw::move_horizontal(world, {0, 0, 0}, {0, 0, -1}, 0.1, settings());
  CHECK_NEAR(end.z, -1.0 + 0.40 + fw::kSweepSkin, 1e-9);
  CHECK_NEAR(end.x, 0.0, 1e-12);
}

TEST(angled_wall_slides_along) {
  fwtest::TestWorld world;
  const double s = std::sqrt(0.5);
  world.walls.push_back({{0, 0, -1}, {s, 0, s}});
  const fw::Vec3 end = fw::move_horizontal(world, {0, 0, 0}, {0, 0, -1}, 0.1, settings());
  CHECK(end.x > 0.05);
  CHECK(gap_to(world.walls[0], end) >= -1e-9);
  CHECK(fw::horizontal_distance(end, {0, 0, 0}) < 1.0);
}

TEST(corner_stops_without_entering_either_wall) {
  fwtest::TestWorld world;
  world.walls.push_back({{0, 0, -1}, {0, 0, 1}});
  world.walls.push_back({{-1, 0, 0}, {1, 0, 0}});
  const fw::Vec3 end = fw::move_horizontal(world, {0, 0, 0}, {-1.4, 0, -1.4}, 0.1, settings());
  CHECK(gap_to(world.walls[0], end) >= -1e-9);
  CHECK(gap_to(world.walls[1], end) >= -1e-9);
  CHECK_NEAR(end.x, -0.586, 0.01);
  CHECK_NEAR(end.z, -0.586, 0.01);
}

TEST(low_obstacle_does_not_block) {
  fwtest::TestWorld world;
  world.walls.push_back({{0, 0, -1}, {0, 0, 1}, 0.3});  // járdaszegély: alacsonyabb a step_height-nál
  const fw::Vec3 end = fw::move_horizontal(world, {0, 0, 0}, {0, 0, -2}, 0.1, settings());
  CHECK_NEAR(end.z, -2.0, 1e-12);
}

TEST(overlap_pushes_out_at_limited_speed) {
  fwtest::TestWorld world;
  world.walls.push_back({{0, 0, -1}, {0, 0, 1}});
  // A test 0,1 m-re a falban: a kitolás legfeljebb a mélység + 5 mm, utána a kívánt mozgás kelet felé szabad.
  const fw::Vec3 end = fw::move_horizontal(world, {0, 0, -0.7}, {0.1, 0, 0}, 0.1, settings());
  CHECK_NEAR(end.z, -0.7 + 0.1 + fw::kDepenetrationExtra, 1e-9);
  CHECK_NEAR(end.x, 0.1, 1e-9);
}

TEST(deep_overlap_is_limited_per_frame) {
  fwtest::TestWorld world;
  world.walls.push_back({{0, 0, -1}, {0, 0, 1}});
  const fw::Vec3 end = fw::move_horizontal(world, {0, 0, -1.6}, {0.1, 0, 0}, 0.1, settings());
  CHECK_NEAR(end.z, -1.6 + fw::kDepenetrationSpeed * 0.1, 1e-9);
}
```

`CMakeLists.txt`: a `freewalk_core` listájába `src/core/motion.cpp`, a `freewalk_tests` listájába `tests/test_motion.cpp`.
Run: `build.cmd` - Expected: fordítási hiba (`core/motion.h` nincs).

- [ ] **Step 5: `src/core/motion.h`**

```cpp
#pragma once
#include "core/config.h"
#include "core/world.h"

namespace fw {

inline constexpr double kGravity = 9.81;
inline constexpr double kSweepSkin = 0.02;            // m: ekkora rés marad a test és a fal között
inline constexpr int kMaxSlideSteps = 3;              // ennyi söprés egy képkockában (csúszás a falak mentén)
inline constexpr double kMinMove = 1e-5;              // m: ennél kisebb elmozdulás nem mozgás
inline constexpr double kDepenetrationSpeed = 1.5;    // m/s: átfedésből legfeljebb ennyivel tolja ki a testet
inline constexpr double kDepenetrationExtra = 0.005;  // m: a behatolás mélységén felül
inline constexpr double kMinPushNormal = 0.3;         // ennél kisebb vízszintes összetevőjű normális mentén nem tol (talaj)
inline constexpr double kCapsuleTopAboveEye = 0.08;   // m: a kapszula teteje a szem fölött
inline constexpr double kMinHalfHeight = 0.02;
inline constexpr double kGroundRayLength = 10.0;      // m: a talajsugár hossza
inline constexpr double kMinGroundNormalY = 0.3;      // ennél meredekebb felület nem talaj
inline constexpr double kGroundSnapTolerance = 0.02;  // m: ennél kisebb eltérésnél a láb nem mozdul
inline constexpr double kStartSearchUp = 2.0;         // m: kiszálláskor ennyivel a becsült talaj fölül keresi a valódit
inline constexpr double kStartSearchLength = 4.0;

struct MotionSettings {
  double radius = 0.40;
  double step_height = 0.45;
  double step_down_max = 0.6;
  double ground_follow_time = 0.12;
  double eye_height = 1.7;
};

MotionSettings motion_settings(const Config& cfg);

// A test kapszulája és középpontja: alja a láb + step_height, teteje a szem + kCapsuleTopAboveEye.
struct BodyCapsule {
  Capsule capsule;
  Vec3 center;
};
BodyCapsule body_capsule(Vec3 feet, const MotionSettings& s);

// A kívánt vízszintes elmozdulás (a wish.y nem számít) legfeljebb kMaxSlideSteps söpréssel, a falak mentén csúszva;
// átfedésnél előbb kitol. Az új lábpontot adja, a magasság változatlan.
Vec3 move_horizontal(World& world, Vec3 feet, Vec3 wish, double dt, const MotionSettings& s);

}  // namespace fw
```

- [ ] **Step 6: `src/core/motion.cpp`**

```cpp
#include "core/motion.h"
#include <algorithm>
#include <cmath>

namespace fw {
namespace {

double horizontal_length(Vec3 v) { return std::hypot(v.x, v.z); }

// A haladási irányból elhagyja a normális menti összetevőt; az új egységirányt és a hossz arányát adja (0, ha nem marad irány).
double project_out(Vec3& dir, Vec3 normal) {
  const double into = dir.x * normal.x + dir.z * normal.z;
  dir = {dir.x - normal.x * into, 0, dir.z - normal.z * into};
  const double length = horizontal_length(dir);
  if (length < kMinMove) return 0;
  dir = dir * (1 / length);
  return length;
}

}  // namespace

MotionSettings motion_settings(const Config& cfg) {
  MotionSettings s;
  s.radius = cfg.collision_radius;
  s.step_height = cfg.step_height;
  s.step_down_max = cfg.step_down_max;
  s.ground_follow_time = cfg.ground_follow_time;
  s.eye_height = cfg.eye_height;
  return s;
}

BodyCapsule body_capsule(Vec3 feet, const MotionSettings& s) {
  const double bottom = feet.y + s.step_height;
  const double top = std::max(feet.y + s.eye_height + kCapsuleTopAboveEye, bottom);
  BodyCapsule b;
  b.capsule.radius = s.radius;
  b.capsule.half_height = std::max(kMinHalfHeight, (top - bottom) / 2 - s.radius);
  b.center = {feet.x, (top + bottom) / 2, feet.z};
  return b;
}

Vec3 move_horizontal(World& world, Vec3 feet, Vec3 wish, double dt, const MotionSettings& s) {
  double remaining = horizontal_length(wish);
  if (remaining < kMinMove) return feet;
  Vec3 dir{wish.x / remaining, 0, wish.z / remaining};
  Vec3 position = feet;
  double pushed = 0;  // az ebben a képkockában átfedésből kitolt út
  for (int i = 0; i < kMaxSlideSteps && remaining >= kMinMove; ++i) {
    const BodyCapsule body = body_capsule(position, s);
    const auto hit = world.sweep_capsule(body.capsule, body.center, dir, remaining + kSweepSkin);
    if (!hit) return position + dir * remaining;
    Vec3 normal{hit->normal.x, 0, hit->normal.z};
    const double normal_length = horizontal_length(normal);
    if (hit->initial_overlap) {
      // A talajjal (függőleges normális) való átfedés nem akadály.
      if (normal_length < kMinPushNormal) return position + dir * remaining;
      normal = normal * (1 / normal_length);
      const double push = std::clamp(kDepenetrationSpeed * dt - pushed, 0.0, hit->depth + kDepenetrationExtra);
      pushed += push;
      position = position + normal * push;
      if (dir.x * normal.x + dir.z * normal.z < 0) {
        const double ratio = project_out(dir, normal);
        if (ratio == 0) break;
        remaining *= ratio;
      }
      continue;
    }
    const double advance = std::clamp(hit->distance - kSweepSkin, 0.0, remaining);
    position = position + dir * advance;
    remaining -= advance;
    if (normal_length < kMinMove) break;
    const double ratio = project_out(dir, normal * (1 / normal_length));
    if (ratio == 0) break;
    remaining *= ratio;
  }
  return position;
}

}  // namespace fw
```

Run: `build.cmd` - Expected: minden teszt zöld.

- [ ] **Step 7: Commit**

```bash
git add src/core/config.h src/core/world.h src/core/motion.h src/core/motion.cpp tests/test_world.h tests/test_motion.cpp CMakeLists.txt
git commit -m "Add the World interface and horizontal motion: capsule sweeps sliding along walls and limited push-out"
```

---

### Task 4: Függőleges mozgás (talajkövetés, esés, kiinduló magasság)

**Files:**
- Modify: `src/core/motion.h`, `src/core/motion.cpp`
- Modify: `tests/test_motion.cpp`

**Interfaces:**
- Consumes: 3. feladat (`World`, `MotionSettings`, állandók).
- Produces: `struct Vertical{bool falling; double speed;}`, `void move_vertical(World&, Vec3& feet, Vertical&, double dt, const MotionSettings&)`, `double start_height(World&, Vec3 feet)`.

- [ ] **Step 1: Piros tesztek** (`tests/test_motion.cpp` végére)

```cpp
namespace {

// Lépcső: a z = -1-nél északabbra a talaj magasabb.
fwtest::TestWorld step_world(double height) {
  fwtest::TestWorld world;
  world.ground = [height](double, double z) { return std::optional<double>(z < -1 ? height : 0.0); };
  return world;
}

void run_vertical(fw::World& world, fw::Vec3& feet, fw::Vertical& v, double seconds) {
  for (int i = 0; i < static_cast<int>(std::lround(seconds / 0.1)); ++i) fw::move_vertical(world, feet, v, 0.1, settings());
}

}  // namespace

TEST(steps_up_smoothly) {
  auto world = step_world(0.3);
  fw::Vec3 feet{0, 0, -1.5};
  fw::Vertical v;
  fw::move_vertical(world, feet, v, 0.1, settings());
  CHECK(feet.y > 0.05 && feet.y < 0.3);
  run_vertical(world, feet, v, 1.0);
  CHECK_NEAR(feet.y, 0.3, fw::kGroundSnapTolerance);
  CHECK(!v.falling);
}

TEST(steps_down_smoothly) {
  auto world = step_world(0.3);
  fw::Vec3 feet{0, 0.3, 0.5};
  fw::Vertical v;
  run_vertical(world, feet, v, 1.0);
  CHECK_NEAR(feet.y, 0.0, fw::kGroundSnapTolerance);
  CHECK(!v.falling);
}

TEST(high_drop_falls_and_lands) {
  fwtest::TestWorld world;
  fw::Vec3 feet{0, 1.0, 0};
  fw::Vertical v;
  fw::move_vertical(world, feet, v, 0.1, settings());
  CHECK(v.falling);
  CHECK_NEAR(feet.y, 1.0 - fw::kGravity * 0.01, 1e-9);
  run_vertical(world, feet, v, 1.0);
  CHECK_NEAR(feet.y, 0.0, 1e-12);
  CHECK(!v.falling);
  CHECK_NEAR(v.speed, 0.0, 1e-12);
}

TEST(no_ground_keeps_height_and_stops_a_fall) {
  fwtest::TestWorld world;
  world.ground = [](double, double) { return std::optional<double>(); };
  fw::Vec3 feet{0, 5.0, 0};
  fw::Vertical v{true, -3.0};
  fw::move_vertical(world, feet, v, 0.1, settings());
  CHECK_NEAR(feet.y, 5.0, 1e-12);
  CHECK(!v.falling);
  run_vertical(world, feet, v, 1.0);
  CHECK_NEAR(feet.y, 5.0, 1e-12);
}

TEST(steep_surface_is_not_ground) {
  fwtest::TestWorld world;
  world.ground = [](double, double) { return std::optional<double>(0.3); };
  world.ground_normal = {0.98, 0.2, 0};
  fw::Vec3 feet{0, 0, 0};
  fw::Vertical v;
  run_vertical(world, feet, v, 1.0);
  CHECK_NEAR(feet.y, 0.0, 1e-12);
}

TEST(zero_follow_time_snaps_at_once) {
  auto world = step_world(0.3);
  fw::MotionSettings s = settings();
  s.ground_follow_time = 0;
  fw::Vec3 feet{0, 0, -1.5};
  fw::Vertical v;
  fw::move_vertical(world, feet, v, 0.1, s);
  CHECK_NEAR(feet.y, 0.3, 1e-12);
}

TEST(start_height_finds_ground_above_the_estimate) {
  fwtest::TestWorld world;
  world.ground = [](double, double) { return std::optional<double>(0.8); };
  CHECK_NEAR(fw::start_height(world, {0, 0, 0}), 0.8, 1e-12);
  world.ground = [](double, double) { return std::optional<double>(); };
  CHECK_NEAR(fw::start_height(world, {0, 0.5, 0}), 0.5, 1e-12);
}
```

Run: `build.cmd` - Expected: fordítási hiba (`move_vertical`, `Vertical`, `start_height` nincs).

- [ ] **Step 2: Deklarációk** (`src/core/motion.h`, a `move_horizontal` után)

```cpp
// A függőleges mozgás állapota.
struct Vertical {
  bool falling = false;
  double speed = 0;  // m/s, felfelé pozitív
};

// A láb magassága a talajhoz: a talajsugár a láb + step_height pontból indul lefelé. Talajon simított követés;
// ha a láb több mint step_down_max-szal a talaj fölött van, esés gravitációval, leérkezés a talajon.
// Találat nélkül (vagy túl meredek felületnél) a magasság marad, és az esés véget ér.
void move_vertical(World& world, Vec3& feet, Vertical& v, double dt, const MotionSettings& s);

// Kiszálláskor a valódi talaj magassága a becsült lábpont környezetében; ha nincs találat, a becsült magasság.
double start_height(World& world, Vec3 feet);
```

- [ ] **Step 3: Megvalósítás** (`src/core/motion.cpp`, a névtelen névtérbe a `project_out` után, majd a `move_horizontal` után)

A névtelen névtérbe:

```cpp
std::optional<double> ground_height(World& world, Vec3 from, double length) {
  const auto hit = world.ground_below(from, length);
  if (!hit || std::fabs(hit->normal.y) < kMinGroundNormalY) return std::nullopt;
  return hit->height;
}
```

A `move_horizontal` után:

```cpp
void move_vertical(World& world, Vec3& feet, Vertical& v, double dt, const MotionSettings& s) {
  const auto ground = ground_height(world, {feet.x, feet.y + s.step_height, feet.z}, kGroundRayLength);
  if (!ground) {
    v = {};
    return;
  }
  if (!v.falling) {
    const double above = feet.y - *ground;
    if (above <= s.step_down_max) {
      if (std::fabs(above) > kGroundSnapTolerance) {
        const double follow = s.ground_follow_time > 0 ? 1 - std::exp(-dt / s.ground_follow_time) : 1.0;
        feet.y -= above * follow;
      }
      return;
    }
    v = {true, 0.0};
  }
  v.speed -= kGravity * dt;
  feet.y += v.speed * dt;
  if (feet.y <= *ground) {
    feet.y = *ground;
    v = {};
  }
}

double start_height(World& world, Vec3 feet) {
  return ground_height(world, {feet.x, feet.y + kStartSearchUp, feet.z}, kStartSearchLength).value_or(feet.y);
}
```

A fájl elejére `#include <optional>`.
Run: `build.cmd` - Expected: minden teszt zöld.

- [ ] **Step 4: Commit**

```bash
git add src/core/motion.h src/core/motion.cpp tests/test_motion.cpp
git commit -m "Follow the ground with smoothing, fall from edges and find the real ground when leaving the truck"
```

---

### Task 5: A gyalogló a `World`-del

**Files:**
- Modify: `src/core/walker.h`, `src/core/walker.cpp`
- Modify: `tests/test_walker.cpp`
- Modify: `src/win/plugin.cpp` (csak a fordíthatóság: `FlatWorld`; a valódi bekötés a 8. feladat)

**Interfaces:**
- Consumes: `move_horizontal`, `move_vertical`, `start_height`, `motion_settings`, `FlatWorld`.
- Produces: `void Walker::step(const WalkKeys&, double yaw, double dt, const Config&, World& world)`, `void Walker::settle(World& world)`.

- [ ] **Step 1: A meglévő tesztek átírása és új piros tesztek** (`tests/test_walker.cpp`)

Az `#include` sorok közé: `#include "core/world.h"` és `#include "test_world.h"`.
A `walk_for` segéd új törzse:

```cpp
void walk_for(fw::Walker& w, const fw::WalkKeys& k, double yaw, double seconds, const fw::Config& cfg) {
  fw::FlatWorld flat(w.feet().y);
  for (int i = 0; i < static_cast<int>(std::lround(seconds / 0.1)); ++i) w.step(k, yaw, 0.1, cfg, flat);
}
```

Minden további közvetlen `w.step(...)` hívás (a `long_frame_moves_at_most_max_step`, `head_bobs_while_walking_within_amplitude`, `head_bob_can_be_switched_off`, `running_bobs_slower_per_metre_than_walking` tesztben) utolsó argumentuma legyen `flat`, és a teszt elején, a `w.reset` után álljon `fw::FlatWorld flat(0.0);`.

Új tesztek a fájl végére:

```cpp
TEST(wall_stops_the_walker_and_the_head_bob) {
  const fw::Config cfg;
  fwtest::TestWorld world;
  world.walls.push_back({{0, 0, -1}, {0, 0, 1}});
  fw::Walker w;
  w.reset({0, 0, 0});
  for (int i = 0; i < 30; ++i) w.step(keys(true, false, false, false), 0.0, 0.1, cfg, world);
  CHECK_NEAR(w.feet().z, -1.0 + cfg.collision_radius + fw::kSweepSkin, 1e-9);
  CHECK_NEAR(w.eye(cfg).y, cfg.eye_height, 1e-12);
}

TEST(walker_climbs_a_step) {
  const fw::Config cfg;
  fwtest::TestWorld world;
  world.ground = [](double, double z) { return std::optional<double>(z < -1 ? 0.3 : 0.0); };
  fw::Walker w;
  w.reset({0, 0, 0});
  for (int i = 0; i < 20; ++i) w.step(keys(true, false, false, false), 0.0, 0.1, cfg, world);
  CHECK(w.feet().z < -2.0);
  CHECK_NEAR(w.feet().y, 0.3, fw::kGroundSnapTolerance);
}

TEST(settle_moves_the_feet_to_the_real_ground) {
  fwtest::TestWorld world;
  world.ground = [](double, double) { return std::optional<double>(1.2); };
  fw::Walker w;
  w.reset({5, 0.5, 5});
  w.settle(world);
  CHECK_NEAR(w.feet().y, 1.2, 1e-12);
  CHECK_NEAR(w.feet().x, 5.0, 1e-12);
}
```

A fájl elejére `#include <optional>` és `#include "core/motion.h"`.
Run: `build.cmd` - Expected: fordítási hiba (a `step` öt argumentummal, a `settle` nincs).

- [ ] **Step 2: `src/core/walker.h`**

Az `#include` sorok közé `#include "core/motion.h"` és `#include "core/world.h"`. A `Walker` osztály új nyilvános része:

```cpp
  // A gyalogló a megadott lábpontra áll, nem esik.
  void reset(Vec3 feet);

  // A lábat a valódi talajra helyezi, ha a világ ismeri (kiszálláskor; a becsült pont fölött is keres).
  void settle(World& world);

  // Egy képkocka mozgása a világ falai és talaja szerint.
  // A yaw a nézési irány radiánban, felülről nézve az óramutatóval ellentétesen, 0 = észak.
  void step(const WalkKeys& keys, double yaw, double dt, const Config& cfg, World& world);
```

A privát tagok közé: `Vertical vertical_;`. Az osztály feletti megjegyzés: `// A gyalogló helyzete és mozgása a világ falai és talaja szerint.`

- [ ] **Step 3: `src/core/walker.cpp`**

```cpp
void Walker::reset(Vec3 feet) {
  feet_ = feet;
  vertical_ = {};
  bob_phase_ = 0;
  bob_blend_ = 0;
}

void Walker::settle(World& world) {
  feet_.y = start_height(world, feet_);
  vertical_ = {};
}

void Walker::step(const WalkKeys& keys, double yaw, double dt, const Config& cfg, World& world) {
  dt = std::clamp(dt, 0.0, kMaxStepSeconds);
  const double ahead = (keys.forward ? 1.0 : 0.0) - (keys.back ? 1.0 : 0.0);
  const double side = (keys.right ? 1.0 : 0.0) - (keys.left ? 1.0 : 0.0);
  const Vec3 direction = forward_from_yaw(yaw) * ahead + right_from_yaw(yaw) * side;
  const double length = std::hypot(direction.x, direction.z);
  const bool wants_to_move = length > 0;
  const MotionSettings motion = motion_settings(cfg);

  Vec3 wish;
  if (wants_to_move) {
    const double speed = keys.run ? cfg.run_speed : cfg.walk_speed;
    wish = direction * (speed * dt / length);
  }
  const Vec3 before = feet_;
  feet_ = move_horizontal(world, feet_, wish, dt, motion);
  move_vertical(world, feet_, vertical_, dt, motion);

  // A fejbiccentés a ténylegesen megtett úttal halad, így falnál megáll.
  const double moved = horizontal_distance(before, feet_);
  const bool moving = wants_to_move && moved > kMinMove;
  if (moving) {
    const double step_length = keys.run ? kRunStepLength : kWalkStepLength;
    bob_phase_ = std::fmod(bob_phase_ + kTwoPi * moved / step_length, kTwoPi);
  }

  const double target = moving ? 1.0 : 0.0;
  const double change = dt / kBobFadeSeconds;
  bob_blend_ = bob_blend_ < target ? std::min(target, bob_blend_ + change) : std::max(target, bob_blend_ - change);
}
```

- [ ] **Step 4: A plugin fordíthatósága** (`src/win/plugin.cpp`, `walk()`)

```cpp
  fw::FlatWorld flat(g->walker.feet().y);
  g->walker.step(keys, fw::layout::camera_yaw_to_heading(*yaw), dt, g->config, flat);
```

Run: `build.cmd` - Expected: minden teszt zöld, a DLL elkészül.

- [ ] **Step 5: Commit**

```bash
git add src/core/walker.h src/core/walker.cpp tests/test_walker.cpp src/win/plugin.cpp
git commit -m "Move the walker through the World: walls, steps and falls; head bob follows the distance actually walked"
```

---

### Task 6: A `[collision]` beállítások

**Files:**
- Modify: `src/core/settings.cpp`, `FreeWalk.ini`, `src/win/settings_window.cpp`
- Modify: `tests/test_config.cpp`

**Interfaces:**
- Consumes: a `Config` `[collision]` mezői (3. feladat).
- Produces: a `[collision]` INI-rész és a Collision fül.

- [ ] **Step 1: Piros teszt** (`tests/test_config.cpp` végére)

```cpp
TEST(collision_settings_are_read) {
  std::vector<std::string> warnings;
  const fw::Config c = fw::parse_config(
      "[collision]\nenabled = 0\nradius = 0.3\nstep_height = 0.25\nstep_down_max = 1.5\nground_follow_time = 0.2\n", warnings);
  CHECK(warnings.empty());
  CHECK(!c.collision_enabled);
  CHECK_NEAR(c.collision_radius, 0.3, 1e-12);
  CHECK_NEAR(c.step_height, 0.25, 1e-12);
  CHECK_NEAR(c.step_down_max, 1.5, 1e-12);
  CHECK_NEAR(c.ground_follow_time, 0.2, 1e-12);
}

TEST(collision_radius_is_range_checked) {
  std::vector<std::string> warnings;
  const fw::Config c = fw::parse_config("[collision]\nradius = 3\n", warnings);
  CHECK(warnings.size() == 1);
  CHECK_NEAR(c.collision_radius, 0.40, 1e-12);
}
```

Run: `build.cmd` - Expected: FAIL (`unknown setting` figyelmeztetések, az értékek az alapértéken).

- [ ] **Step 2: A beállítástábla** (`src/core/settings.cpp`, a `ground_offset` sora után, a `[debug]` sorok előtt)

```cpp
      flag_setting("collision", "enabled", "Collision", "Ground and collision from the game's physics *",
                   "1 = a gyalogló követi a talajt és nem megy át a falakon (a játék fizikájából), 0 = sík talaj", &Config::collision_enabled,
                   Applies::Restart),
      number_setting("collision", "radius", "Collision", "Body radius (m)", "m, ennél közelebb nem megy falhoz", &Config::collision_radius,
                     0.1, 1.0, 0.05, 2),
      number_setting("collision", "step_height", "Collision", "Highest step (m)", "m, ennél alacsonyabb fokra fellép",
                     &Config::step_height, 0.0, 1.0, 0.05, 2),
      number_setting("collision", "step_down_max", "Collision", "Drop that counts as a fall (m)",
                     "m, ennél nagyobb lépés lefelé esés", &Config::step_down_max, 0.1, 5.0, 0.1, 1),
      number_setting("collision", "ground_follow_time", "Collision", "Ground smoothing (s)",
                     "s, a talajkövetés simítása (0 = azonnal)", &Config::ground_follow_time, 0.0, 1.0, 0.01, 2),
```

- [ ] **Step 3: A Collision fül megjegyzése** (`src/win/settings_window.cpp`, `tab_note`, a `Debug` ága elé)

```cpp
  if (tab == "Collision")
    return "* takes effect after restarting the game. Without the game's physics (or far from the truck) the ground stays flat.";
```

- [ ] **Step 4: A csomagolt INI frissítése**

Run: `build.cmd` - Expected: a `shipped_ini_matches_the_formatter` FAIL, a kimenet mutatja az elvárt szöveget.
A `FreeWalk.ini`-ben a `[walker]` és a `[debug]` rész közé, pontosan a formázó kimenete szerint (a megjegyzések a 26. oszlopban):

```ini
[collision]
enabled = 1               ; 1 = a gyalogló követi a talajt és nem megy át a falakon (a játék fizikájából), 0 = sík talaj
radius = 0.4              ; m, ennél közelebb nem megy falhoz
step_height = 0.45        ; m, ennél alacsonyabb fokra fellép
step_down_max = 0.6       ; m, ennél nagyobb lépés lefelé esés
ground_follow_time = 0.12 ; s, a talajkövetés simítása (0 = azonnal)
```

Ha a teszt kimenete ettől eltér (például az oszlopigazításban), a teszt kimenete a mérvadó.
Run: `build.cmd` - Expected: minden teszt zöld.

- [ ] **Step 5: Commit**

```bash
git add src/core/settings.cpp FreeWalk.ini src/win/settings_window.cpp tests/test_config.cpp
git commit -m "Add the [collision] settings and the Collision tab"
```

---

### Task 7: Az origó-eltolás becslése (mag)

**Files:**
- Create: `src/core/scene_offset.h`, `src/core/scene_offset.cpp`, `tests/test_scene_offset.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `struct SceneOffset{double x, z, residual; std::uint64_t actor;}`, `struct ActorPosition{std::uint64_t actor; Vec3 position;}`,
  `std::optional<SceneOffset> offset_from_actor(Vec3 truck_world, const ActorPosition&)`,
  `std::optional<SceneOffset> estimate_scene_offset(Vec3 truck_world, const std::vector<ActorPosition>&)`,
  `Vec3 world_to_scene(Vec3, const SceneOffset&)`.

- [ ] **Step 1: Piros tesztek** (`tests/test_scene_offset.cpp`)

```cpp
#include <limits>
#include "check.h"
#include "core/scene_offset.h"

TEST(offset_is_zero_near_the_origin) {
  const auto o = fw::offset_from_actor({100, 10, 200}, {7, {101, 10, 199}});
  CHECK(o.has_value());
  if (!o) return;
  CHECK_NEAR(o->x, 0.0, 1e-12);
  CHECK_NEAR(o->z, 0.0, 1e-12);
  CHECK_NEAR(o->residual, 2.0, 1e-12);
  CHECK(o->actor == 7);
}

TEST(offset_rounds_to_512_in_both_directions) {
  const auto o = fw::offset_from_actor({1030, 5, -1530}, {1, {6, 5, 6}});
  CHECK(o.has_value());
  if (!o) return;
  CHECK_NEAR(o->x, 1024.0, 1e-12);
  CHECK_NEAR(o->z, -1536.0, 1e-12);
  CHECK_NEAR(o->residual, 0.0, 1e-12);
}

TEST(offset_needs_a_small_residual_and_height) {
  CHECK(!fw::offset_from_actor({0, 0, 0}, {1, {-7, 0, 0}}));
  CHECK(!fw::offset_from_actor({0, 0, 0}, {1, {1, 31, 0}}));
  const double nan = std::numeric_limits<double>::quiet_NaN();
  CHECK(!fw::offset_from_actor({0, 0, 0}, {1, {nan, 0, 0}}));
}

TEST(estimate_picks_the_smallest_residual) {
  const std::vector<fw::ActorPosition> actors = {{1, {515, 0, 3}}, {2, {512.5, 0, 0.5}}, {3, {0, 50, 0}}};
  const auto o = fw::estimate_scene_offset({1024.5, 0, 0}, actors);
  CHECK(o.has_value());
  if (!o) return;
  CHECK(o->actor == 2);
  CHECK_NEAR(o->x, 512.0, 1e-12);
  CHECK_NEAR(o->residual, 0.5, 1e-12);
  CHECK(!fw::estimate_scene_offset({0, 0, 0}, {}));
}

TEST(world_to_scene_keeps_the_height) {
  const fw::SceneOffset o{1024, -512, 0, 0};
  const fw::Vec3 s = fw::world_to_scene({1030, 7, -500}, o);
  CHECK_NEAR(s.x, 6.0, 1e-12);
  CHECK_NEAR(s.y, 7.0, 1e-12);
  CHECK_NEAR(s.z, 12.0, 1e-12);
}
```

`CMakeLists.txt`: `src/core/scene_offset.cpp` a magba, `tests/test_scene_offset.cpp` a tesztekbe.
Run: `build.cmd` - Expected: fordítási hiba.

- [ ] **Step 2: `src/core/scene_offset.h`**

```cpp
#pragma once
#include <cstdint>
#include <optional>
#include <vector>
#include "core/vec.h"

namespace fw {

// A játék PhysX-jelenetének origója X-ben és Z-ben 512 m többszörösével el van tolva a világhoz képest (Y-ban nem);
// az eltolást a kamion telemetriás helyéből és a kamion fizikai testének helyéből becsüljük.
inline constexpr double kSceneGrid = 512.0;
inline constexpr double kMaxSceneOffsetResidual = 6.0;  // m: ennél nagyobb maradéknál az aktor nem a kamion
inline constexpr double kTruckActorHeightWindow = 30.0; // m: a kamion aktorának magassága ennyire térhet el a telemetriától

struct SceneOffset {
  double x = 0;
  double z = 0;
  double residual = 0;     // |dx - x| + |dz - z|: a kamion és az aktor helyének eltérése az eltolás után
  std::uint64_t actor = 0;  // az aktor, amelyből az eltolás adódott
};

// Egy dinamikus aktor helye a jelenet koordinátáiban.
struct ActorPosition {
  std::uint64_t actor = 0;
  Vec3 position;
};

// Az eltolás, ha az aktor a kamion lehet (a magassága és a maradék is kicsi).
std::optional<SceneOffset> offset_from_actor(Vec3 truck_world, const ActorPosition& actor);

// A legkisebb maradékú aktor eltolása.
std::optional<SceneOffset> estimate_scene_offset(Vec3 truck_world, const std::vector<ActorPosition>& actors);

inline Vec3 world_to_scene(Vec3 world, const SceneOffset& o) { return {world.x - o.x, world.y, world.z - o.z}; }

}  // namespace fw
```

- [ ] **Step 3: `src/core/scene_offset.cpp`**

```cpp
#include "core/scene_offset.h"
#include <cmath>

namespace fw {

std::optional<SceneOffset> offset_from_actor(Vec3 truck, const ActorPosition& a) {
  if (!(std::fabs(a.position.y - truck.y) <= kTruckActorHeightWindow)) return std::nullopt;
  const double dx = truck.x - a.position.x;
  const double dz = truck.z - a.position.z;
  SceneOffset o;
  o.x = std::round(dx / kSceneGrid) * kSceneGrid;
  o.z = std::round(dz / kSceneGrid) * kSceneGrid;
  o.residual = std::fabs(dx - o.x) + std::fabs(dz - o.z);
  o.actor = a.actor;
  if (!(o.residual < kMaxSceneOffsetResidual)) return std::nullopt;
  return o;
}

std::optional<SceneOffset> estimate_scene_offset(Vec3 truck, const std::vector<ActorPosition>& actors) {
  std::optional<SceneOffset> best;
  for (const auto& a : actors) {
    const auto o = offset_from_actor(truck, a);
    if (o && (!best || o->residual < best->residual)) best = o;
  }
  return best;
}

}  // namespace fw
```

Run: `build.cmd` - Expected: minden teszt zöld.

- [ ] **Step 4: Commit**

```bash
git add src/core/scene_offset.h src/core/scene_offset.cpp tests/test_scene_offset.cpp CMakeLists.txt
git commit -m "Estimate the PhysX scene origin offset from the truck's telemetry and its physics actor"
```

---

### Task 8: A PhysX-jelenet a Windows-rétegben

**Files:**
- Create: `src/win/physx_types.h`, `src/win/physx_scene.h`, `src/win/physx_scene.cpp`
- Modify: `CMakeLists.txt` (forrás, `psapi`)

**Interfaces:**
- Consumes: `find_physx_scene_vtable` (1.), a vtable-indexek (2.), `World` (3.), `estimate_scene_offset`, `offset_from_actor`, `world_to_scene` (7.).
- Produces: `class fw::PhysxScene` - `bool init(std::vector<std::string>& log)`, `void update(std::uint64_t world_key, Vec3 truck_position, bool truck_valid, double dt, std::vector<std::string>& log)`, `World* world()`, `std::string status() const`, `std::vector<std::string> describe() const`, `void stop()`.

Ez a réteg csak a futó játékban tesztelhető (9. feladat); itt a fordítás figyelmeztetésmentessége és a `static_assert`-ek a kapu.

- [ ] **Step 1: `src/win/physx_types.h`**

```cpp
#pragma once
#include <cstddef>
#include <cstdint>

// A játékba linkelt PhysX 3.4 hívásaihoz szükséges adatszerkezetek, a nyilvános fejlécek elrendezése szerint, saját írással.
// Csak azt tartalmazza, amit a FreeWalk használ; az elrendezést a static_assert-ek rögzítik (docs/game-notes/ets2-1.61.md, "PhysX-jelenet").
namespace fw::physx {

struct Vec3f {
  float x = 0;
  float y = 0;
  float z = 0;
};

struct Quat {
  float x = 0;
  float y = 0;
  float z = 0;
  float w = 1;
};

struct Transform {
  Quat q;
  Vec3f p;
};
static_assert(sizeof(Transform) == 28);

// PxCapsuleGeometry: a kapszula tengelye a saját X tengelye.
inline constexpr int kGeometryCapsule = 2;
struct CapsuleGeometry {
  int type = kGeometryCapsule;
  float radius = 0;
  float half_height = 0;
};
static_assert(sizeof(CapsuleGeometry) == 12);

// PxHitFlag
inline constexpr std::uint16_t kHitPosition = 1 << 0;
inline constexpr std::uint16_t kHitNormal = 1 << 1;
inline constexpr std::uint16_t kHitMeshBothSides = 1 << 7;
inline constexpr std::uint16_t kHitMtd = 1 << 9;
// PxQueryFlag
inline constexpr std::uint16_t kQueryStatic = 1 << 0;
inline constexpr std::uint16_t kQueryDynamic = 1 << 1;
// PxActorTypeFlag
inline constexpr std::uint16_t kActorRigidDynamic = 1 << 1;

// PxQueryFilterData: négy szűrőszó és a lekérdezés flagjei.
struct QueryFilterData {
  std::uint32_t word[4] = {};
  std::uint16_t flags = 0;
};
static_assert(sizeof(QueryFilterData) == 20);

// PxRaycastHit és PxSweepHit: a PxQueryHit (aktor, alakzat, lapsorszám) 8 bájtra igazítva, utána a PxLocationHit mezői;
// a raycastnál a végén u, v, a sweepnél kitöltés - a két elrendezés azonos.
struct LocationHit {
  void* actor = nullptr;
  void* shape = nullptr;
  std::uint32_t face_index = 0xFFFFFFFF;
  std::uint32_t base_padding = 0;
  std::uint16_t flags = 0;
  Vec3f position;
  Vec3f normal;
  float distance = 0;
  float tail[2] = {};
};
static_assert(offsetof(LocationHit, flags) == 24);
static_assert(offsetof(LocationHit, position) == 28);
static_assert(offsetof(LocationHit, normal) == 40);
static_assert(offsetof(LocationHit, distance) == 52);
static_assert(sizeof(LocationHit) == 64);

// PxHitBuffer<T>: a PhysX a virtuális függvényeit hívja (processTouches, finalizeQuery, destruktor - a 2. feladat ellenőrizte a sorrendet),
// ezért a deklaráció sorrendje és a tagok elrendezése számít. Érintő találatot nem kérünk (touches = nullptr), csak a blokkolót.
class HitBuffer {
 public:
  virtual bool process_touches(const LocationHit*, std::uint32_t) { return false; }
  virtual void finalize_query() {}
  virtual ~HitBuffer() = default;

  LocationHit block;
  bool has_block = false;
  LocationHit* touches = nullptr;
  std::uint32_t max_touches = 0;
  std::uint32_t touch_count = 0;
};
static_assert(sizeof(HitBuffer) == 96);

// A hívások az x64 tagfüggvény-konvenció szerint: a this az első argumentum; a PxFlags és a hivatkozással átadott szerkezetek mutatóként,
// a visszaadott PxTransform a második argumentumként kapott helyre kerül.
using GetNbActorsFn = std::uint32_t (*)(void* scene, const std::uint16_t* types);
using GetActorsFn = std::uint32_t (*)(void* scene, const std::uint16_t* types, void** buffer, std::uint32_t size, std::uint32_t start);
using RaycastFn = bool (*)(void* scene, const Vec3f* origin, const Vec3f* direction, float distance, HitBuffer* hit,
                           const std::uint16_t* hit_flags, const QueryFilterData* filter, void* filter_call, void* cache);
using SweepFn = bool (*)(void* scene, const CapsuleGeometry* geometry, const Transform* pose, const Vec3f* direction, float distance,
                         HitBuffer* hit, const std::uint16_t* hit_flags, const QueryFilterData* filter, void* filter_call, void* cache,
                         float inflation);
using GetGlobalPoseFn = Transform* (*)(void* actor, Transform* out);

}  // namespace fw::physx
```

Ha a 2. feladat 4. lépése más virtuális sorrendet talált, a `HitBuffer` deklarációs sorrendjét ahhoz kell igazítani.

- [ ] **Step 2: `src/win/physx_scene.h`**

```cpp
#pragma once
#include <atomic>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>
#include "core/scene_offset.h"
#include "core/vec.h"
#include "core/world.h"

namespace fw {

// A játék PhysX-jelenete: a példány keresése háttérszálon, a jelöltek ellenőrzése, az origó-eltolás frissítése,
// és a World felület a jelenet lekérdezéseivel. A háttérszálon kívül minden a játék szálán fut (frame_end).
class PhysxScene final : public World {
 public:
  PhysxScene() = default;
  PhysxScene(const PhysxScene&) = delete;
  PhysxScene& operator=(const PhysxScene&) = delete;
  ~PhysxScene() override { stop(); }

  // A jelenet vtable-jét keresi meg a játékfájlban (betöltéskor). Hamis, ha nincs meg: a fizika ebben a munkamenetben ki van kapcsolva.
  bool init(std::vector<std::string>& log);

  // Képkockánként: új világnál (a world_key változik; 0 = nincs világ) újraindítja a keresést, ellenőrzi a jelölteket,
  // 250 ms-onként frissíti az eltolást, és a log-ba adja a háttérszál és a lekérdezések naplósorait.
  void update(std::uint64_t world_key, Vec3 truck_position, bool truck_valid, double dt, std::vector<std::string>& log);

  // A gyalogló világa, ha a jelenet kész és van érvényes eltolás; különben nullptr.
  World* world();

  // Rövid állapot a naplóba ("ready", "searching", ...).
  std::string status() const;
  // Diagnosztikai sorok (discovery_log).
  std::vector<std::string> describe() const;

  // A háttérszál leállítása (a plugin kirakásakor és új világnál).
  void stop();

  std::optional<GroundHit> ground_below(Vec3 from, double max_distance) override;
  std::optional<SweepHit> sweep_capsule(const Capsule& capsule, Vec3 center, Vec3 direction, double distance) override;

 private:
  enum class State { Off, Idle, Searching, Validating, Ready, Disabled };

  void start_search();
  void search_thread();
  void validate(Vec3 truck_position, std::vector<std::string>& log);
  void refresh_offset(Vec3 truck_position, std::vector<std::string>& log);
  std::optional<std::vector<ActorPosition>> dynamic_actors(void* scene, std::vector<std::string>& log);
  std::optional<Vec3> actor_position(void* actor);
  void fault(const char* what, unsigned long code, std::uint64_t address);

  State state_ = State::Off;
  std::uint64_t module_base_ = 0;
  std::uint64_t world_key_ = 0;
  void* scene_ = nullptr;
  std::optional<SceneOffset> offset_;
  double offset_timer_ = 0;
  int faults_ = 0;
  std::vector<std::string> pending_log_;  // a lekérdezések naplósorai, a következő update adja tovább
  std::optional<GroundHit> last_ground_;
  std::optional<SweepHit> last_sweep_;

  std::thread thread_;
  std::atomic<bool> stop_requested_{false};
  mutable std::mutex mutex_;  // a háttérszál eredményei és naplósorai
  bool search_done_ = false;
  std::vector<std::uint64_t> candidates_;
  std::vector<std::string> thread_log_;
};

}  // namespace fw
```

- [ ] **Step 3: `src/win/physx_scene.cpp`**

```cpp
#include "win/physx_scene.h"
#include <windows.h>
#include <psapi.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstring>
#include "core/game_layout.h"
#include "core/locator.h"
#include "core/pe_image.h"
#include "core/text.h"
#include "win/physx_types.h"
#include "win/process_memory.h"

namespace fw {
namespace {

// A jelenet vtable-jének címe a DLL saját adatszakaszában (nem a kupacban), így a memóriakeresés nem találja meg a saját másolatunkat.
std::uint64_t g_scene_vtable = 0;

inline constexpr std::size_t kMaxCandidates = 16;
inline constexpr int kSearchAttempts = 8;
inline constexpr auto kSearchRetry = std::chrono::seconds(8);
inline constexpr std::size_t kPagesPerQuery = 1024;
inline constexpr std::uint32_t kMaxDynamicActors = 4096;
inline constexpr int kMaxFaults = 8;
inline constexpr double kOffsetInterval = 0.25;
inline constexpr float kOverlapDistance = 1e-4f;

std::condition_variable g_wake;  // a háttérszál várakozásának megszakítása leállításkor
std::mutex g_wake_mutex;

struct Fault {
  unsigned long code = 0;
  std::uint64_t address = 0;
};

using Thunk = void (*)(void* context);

#if defined(_MSC_VER)
int capture_fault(EXCEPTION_POINTERS* e, Fault* f) {
  f->code = e->ExceptionRecord->ExceptionCode;
  f->address = reinterpret_cast<std::uint64_t>(e->ExceptionRecord->ExceptionAddress);
  return EXCEPTION_EXECUTE_HANDLER;
}

// Egy hívás a játék kódjába kivételvédelemmel: hozzáférési hibánál hamisat ad, a kód és a cím a fault-ba kerül.
bool guarded(Thunk thunk, void* context, Fault* fault) {
  __try {
    thunk(context);
    return true;
  } __except (capture_fault(GetExceptionInformation(), fault)) {
    return false;
  }
}
#else
// SEH nélkül (MinGW) a játék kódját nem hívjuk: a fizika ki van kapcsolva.
bool guarded(Thunk, void*, Fault* fault) {
  fault->code = 0;
  return false;
}
#endif

template <class Fn>
Fn vtable_slot(void* object, std::size_t index) {
  const auto vtable = *static_cast<std::uint64_t* const*>(object);
  return reinterpret_cast<Fn>(vtable[index]);
}

// A vtable címét tartalmazó 8 bájtra igazított helyek a folyamat írható, privát memóriájában. Csak a fizikai memóriában lévő lapokat
// olvassa (QueryWorkingSetEx), hogy semmit ne lapozzon be; a saját szál vermét kihagyja.
std::vector<std::uint64_t> scan_for_vtable(std::uint64_t value, const std::atomic<bool>& stop, std::uint64_t& committed,
                                           std::uint64_t& read) {
  std::vector<std::uint64_t> hits;
  SYSTEM_INFO si{};
  GetSystemInfo(&si);
  const std::uint64_t page = si.dwPageSize;
  ULONG_PTR stack_low = 0;
  ULONG_PTR stack_high = 0;
  GetCurrentThreadStackLimits(&stack_low, &stack_high);
  std::vector<PSAPI_WORKING_SET_EX_INFORMATION> info;
  std::vector<std::uint8_t> buffer(page);
  std::uint64_t address = reinterpret_cast<std::uint64_t>(si.lpMinimumApplicationAddress);
  const std::uint64_t end = reinterpret_cast<std::uint64_t>(si.lpMaximumApplicationAddress);
  while (address < end && hits.size() < kMaxCandidates && !stop) {
    MEMORY_BASIC_INFORMATION mbi{};
    if (VirtualQuery(reinterpret_cast<LPCVOID>(address), &mbi, sizeof mbi) == 0) break;
    const std::uint64_t base = reinterpret_cast<std::uint64_t>(mbi.BaseAddress);
    address = base + mbi.RegionSize;
    if (mbi.State != MEM_COMMIT || mbi.Type != MEM_PRIVATE || mbi.Protect != PAGE_READWRITE) continue;
    if (base < stack_high && base + mbi.RegionSize > stack_low) continue;
    committed += mbi.RegionSize;
    const std::size_t pages = static_cast<std::size_t>(mbi.RegionSize / page);
    for (std::size_t first = 0; first < pages && hits.size() < kMaxCandidates; first += kPagesPerQuery) {
      const std::size_t count = std::min(kPagesPerQuery, pages - first);
      info.assign(count, {});
      for (std::size_t i = 0; i < count; ++i) info[i].VirtualAddress = reinterpret_cast<PVOID>(base + (first + i) * page);
      if (!QueryWorkingSetEx(GetCurrentProcess(), info.data(), static_cast<DWORD>(count * sizeof(info[0])))) continue;
      for (std::size_t i = 0; i < count && hits.size() < kMaxCandidates; ++i) {
        if (!info[i].VirtualAttributes.Valid) continue;
        const std::uint64_t at = base + (first + i) * page;
        if (at == reinterpret_cast<std::uint64_t>(buffer.data())) continue;
        SIZE_T done = 0;
        if (!ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<LPCVOID>(at), buffer.data(), page, &done) || done != page) continue;
        read += page;
        for (std::size_t o = 0; o + 8 <= page; o += 8) {
          std::uint64_t v;
          std::memcpy(&v, buffer.data() + o, 8);
          if (v == value) {
            hits.push_back(at + o);
            if (hits.size() >= kMaxCandidates) break;
          }
        }
      }
    }
  }
  return hits;
}

const char* state_name(int s) {
  static const char* const names[] = {"off", "waiting for a world", "searching", "checking candidates", "ready", "disabled"};
  return names[s];
}

}  // namespace

bool PhysxScene::init(std::vector<std::string>& log) {
  ProcessMemory memory;
  module_base_ = reinterpret_cast<std::uint64_t>(GetModuleHandleW(nullptr));
  const auto image = PeImage::parse(memory, module_base_);
  if (!image) {
    log.push_back("physx: the game executable's headers could not be read; ground and collision are off");
    return false;
  }
  const auto vtable = find_physx_scene_vtable(memory, *image, log);
  if (!vtable) {
    log.push_back("physx: the scene type was not found in this game build; ground and collision are off");
    return false;
  }
  g_scene_vtable = *vtable;
  state_ = State::Idle;
  return true;
}

void PhysxScene::stop() {
  stop_requested_ = true;
  g_wake.notify_all();
  if (thread_.joinable()) thread_.join();
  stop_requested_ = false;
}

void PhysxScene::start_search() {
  stop();
  {
    std::lock_guard<std::mutex> lock(mutex_);
    search_done_ = false;
    candidates_.clear();
  }
  state_ = State::Searching;
  thread_ = std::thread([this] { search_thread(); });
}

void PhysxScene::search_thread() {
  SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
  std::vector<std::uint64_t> hits;
  for (int attempt = 1; attempt <= kSearchAttempts && !stop_requested_; ++attempt) {
    const ULONGLONG start = GetTickCount64();
    std::uint64_t committed = 0;
    std::uint64_t read = 0;
    hits = scan_for_vtable(g_scene_vtable, stop_requested_, committed, read);
    {
      std::lock_guard<std::mutex> lock(mutex_);
      thread_log_.push_back(strf("physx: scene search %d: read %llu MB of %llu MB in %llu ms, %zu candidate(s)", attempt,
                                 static_cast<unsigned long long>(read >> 20), static_cast<unsigned long long>(committed >> 20),
                                 static_cast<unsigned long long>(GetTickCount64() - start), hits.size()));
    }
    if (!hits.empty()) break;
    std::unique_lock<std::mutex> wait(g_wake_mutex);
    g_wake.wait_for(wait, kSearchRetry, [this] { return stop_requested_.load(); });
  }
  std::lock_guard<std::mutex> lock(mutex_);
  candidates_ = hits;
  search_done_ = true;
}

void PhysxScene::update(std::uint64_t world_key, Vec3 truck_position, bool truck_valid, double dt, std::vector<std::string>& log) {
  if (state_ == State::Off) return;
  if (world_key != world_key_) {
    world_key_ = world_key;
    stop();
    scene_ = nullptr;
    offset_.reset();
    faults_ = 0;
    state_ = State::Idle;
    if (world_key != 0) {
      log.push_back("physx: a world was loaded, looking for its scene");
      start_search();
    }
  }
  std::vector<std::uint64_t> found;
  bool done = false;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    log.insert(log.end(), thread_log_.begin(), thread_log_.end());
    thread_log_.clear();
    if (state_ == State::Searching && search_done_) {
      done = true;
      found = candidates_;
    }
  }
  log.insert(log.end(), pending_log_.begin(), pending_log_.end());
  pending_log_.clear();
  if (done) {
    if (thread_.joinable()) thread_.join();
    candidates_ = found;
    state_ = found.empty() ? State::Disabled : State::Validating;
    if (found.empty()) log.push_back("physx: the scene was not found in memory; ground and collision are off until the next world");
  }
  if (state_ == State::Validating && truck_valid) validate(truck_position, log);
  if (state_ == State::Ready && truck_valid) {
    offset_timer_ += dt;
    if (offset_timer_ >= kOffsetInterval || !offset_) {
      offset_timer_ = 0;
      refresh_offset(truck_position, log);
    }
  }
  if (state_ == State::Ready && faults_ >= kMaxFaults) {
    state_ = State::Disabled;
    log.push_back("physx: too many faults; ground and collision are off until the next world");
  }
}

void PhysxScene::validate(Vec3 truck_position, std::vector<std::string>& log) {
  for (const std::uint64_t candidate : candidates_) {
    void* scene = reinterpret_cast<void*>(candidate);
    const auto actors = dynamic_actors(scene, log);
    if (!actors) {
      log.push_back(strf("physx: candidate %p does not answer", scene));
      continue;
    }
    const auto offset = estimate_scene_offset(truck_position, *actors);
    if (!offset) {
      log.push_back(strf("physx: candidate %p has %zu dynamic actor(s), the truck is not among them", scene, actors->size()));
      continue;
    }
    scene_ = scene;
    offset_ = offset;
    state_ = State::Ready;
    log.push_back(strf("physx: using the scene at %p (%zu dynamic actors); origin offset (%.0f, %.0f), residual %.2f m", scene,
                       actors->size(), offset->x, offset->z, offset->residual));
    return;
  }
  state_ = State::Disabled;
  log.push_back("physx: no scene candidate holds the truck; ground and collision are off until the next world");
}

void PhysxScene::refresh_offset(Vec3 truck_position, std::vector<std::string>& log) {
  std::optional<SceneOffset> next;
  if (offset_) {
    if (const auto p = actor_position(reinterpret_cast<void*>(offset_->actor))) next = offset_from_actor(truck_position, {offset_->actor, *p});
  }
  if (!next) {
    if (const auto actors = dynamic_actors(scene_, log)) next = estimate_scene_offset(truck_position, *actors);
  }
  if (!next) {
    if (offset_) log.push_back("physx: the truck's actor is lost; flat ground until it is found again");
    offset_.reset();
    return;
  }
  if (!offset_ || next->x != offset_->x || next->z != offset_->z)
    log.push_back(strf("physx: origin offset (%.0f, %.0f), residual %.2f m", next->x, next->z, next->residual));
  offset_ = next;
}

std::optional<std::vector<ActorPosition>> PhysxScene::dynamic_actors(void* scene, std::vector<std::string>& log) {
  struct CountArgs {
    void* scene;
    std::uint16_t types;
    std::uint32_t count;
  } count_args{scene, physx::kActorRigidDynamic, 0};
  Fault f;
  if (!guarded([](void* c) {
        auto* a = static_cast<CountArgs*>(c);
        a->count = vtable_slot<physx::GetNbActorsFn>(a->scene, layout::kPxSceneGetNbActors)(a->scene, &a->types);
      }, &count_args, &f))
    return std::nullopt;
  const std::uint32_t total = std::min(count_args.count, kMaxDynamicActors);
  std::vector<void*> actors(total);
  struct ListArgs {
    void* scene;
    std::uint16_t types;
    void** buffer;
    std::uint32_t size;
    std::uint32_t got;
  } list_args{scene, physx::kActorRigidDynamic, actors.data(), total, 0};
  if (total > 0 && !guarded([](void* c) {
        auto* a = static_cast<ListArgs*>(c);
        a->got = vtable_slot<physx::GetActorsFn>(a->scene, layout::kPxSceneGetActors)(a->scene, &a->types, a->buffer, a->size, 0);
      }, &list_args, &f))
    return std::nullopt;
  std::vector<ActorPosition> positions;
  for (std::uint32_t i = 0; i < std::min(list_args.got, total); ++i) {
    if (const auto p = actor_position(actors[i])) positions.push_back({reinterpret_cast<std::uint64_t>(actors[i]), *p});
  }
  (void)log;
  return positions;
}

std::optional<Vec3> PhysxScene::actor_position(void* actor) {
  if (!actor) return std::nullopt;
  struct PoseArgs {
    void* actor;
    physx::Transform pose;
  } args{actor, {}};
  Fault f;
  if (!guarded([](void* c) {
        auto* a = static_cast<PoseArgs*>(c);
        vtable_slot<physx::GetGlobalPoseFn>(a->actor, layout::kPxActorGetGlobalPose)(a->actor, &a->pose);
      }, &args, &f))
    return std::nullopt;
  return Vec3{args.pose.p.x, args.pose.p.y, args.pose.p.z};
}

void PhysxScene::fault(const char* what, unsigned long code, std::uint64_t address) {
  ++faults_;
  const unsigned long long where = address >= module_base_ ? address - module_base_ : 0;
  pending_log_.push_back(strf("physx: %s faulted: code 0x%08lX at exe+0x%llX (%d of %d)", what, code, where, faults_, kMaxFaults));
}

World* PhysxScene::world() { return state_ == State::Ready && offset_ ? this : nullptr; }

std::optional<GroundHit> PhysxScene::ground_below(Vec3 from, double max_distance) {
  if (!world()) return std::nullopt;
  const Vec3 origin = world_to_scene(from, *offset_);
  struct Args {
    void* scene;
    physx::Vec3f origin;
    physx::Vec3f direction;
    float distance;
    physx::HitBuffer hit;
    std::uint16_t flags;
    physx::QueryFilterData filter;
    bool result;
  } args{scene_, {static_cast<float>(origin.x), static_cast<float>(origin.y), static_cast<float>(origin.z)}, {0, -1, 0},
         static_cast<float>(max_distance), {}, static_cast<std::uint16_t>(physx::kHitPosition | physx::kHitNormal | physx::kHitMeshBothSides),
         {}, false};
  args.filter.flags = physx::kQueryStatic | physx::kQueryDynamic;
  Fault f;
  if (!guarded([](void* c) {
        auto* a = static_cast<Args*>(c);
        a->result = vtable_slot<physx::RaycastFn>(a->scene, layout::kPxSceneRaycast)(a->scene, &a->origin, &a->direction, a->distance,
                                                                                    &a->hit, &a->flags, &a->filter, nullptr, nullptr);
      }, &args, &f)) {
    fault("ground raycast", f.code, f.address);
    return std::nullopt;
  }
  last_ground_.reset();
  if (!args.hit.has_block) return std::nullopt;
  const auto& b = args.hit.block;
  last_ground_ = GroundHit{b.position.y, {b.normal.x, b.normal.y, b.normal.z}};
  return last_ground_;
}

std::optional<SweepHit> PhysxScene::sweep_capsule(const Capsule& capsule, Vec3 center, Vec3 direction, double distance) {
  if (!world()) return std::nullopt;
  const Vec3 c = world_to_scene(center, *offset_);
  // A PhysX kapszulájának tengelye a saját X tengelye: 90 fokos forgatás a Z tengely körül teszi függőlegessé.
  const float half = static_cast<float>(std::sqrt(0.5));
  struct Args {
    void* scene;
    physx::CapsuleGeometry geometry;
    physx::Transform pose;
    physx::Vec3f direction;
    float distance;
    physx::HitBuffer hit;
    std::uint16_t flags;
    physx::QueryFilterData filter;
    bool result;
  } args{scene_,
         {physx::kGeometryCapsule, static_cast<float>(capsule.radius), static_cast<float>(capsule.half_height)},
         {{0, 0, half, half}, {static_cast<float>(c.x), static_cast<float>(c.y), static_cast<float>(c.z)}},
         {static_cast<float>(direction.x), 0, static_cast<float>(direction.z)},
         static_cast<float>(distance),
         {},
         static_cast<std::uint16_t>(physx::kHitPosition | physx::kHitNormal | physx::kHitMeshBothSides | physx::kHitMtd),
         {},
         false};
  args.filter.flags = physx::kQueryStatic | physx::kQueryDynamic;
  Fault f;
  if (!guarded([](void* p) {
        auto* a = static_cast<Args*>(p);
        a->result = vtable_slot<physx::SweepFn>(a->scene, layout::kPxSceneSweep)(a->scene, &a->geometry, &a->pose, &a->direction,
                                                                                a->distance, &a->hit, &a->flags, &a->filter, nullptr,
                                                                                nullptr, 0.0f);
      }, &args, &f)) {
    fault("capsule sweep", f.code, f.address);
    return std::nullopt;
  }
  last_sweep_.reset();
  if (!args.hit.has_block) return std::nullopt;
  const auto& b = args.hit.block;
  SweepHit hit;
  hit.distance = b.distance;
  hit.normal = {b.normal.x, b.normal.y, b.normal.z};
  hit.initial_overlap = b.distance <= kOverlapDistance;
  hit.depth = hit.initial_overlap ? std::max(0.0, -static_cast<double>(b.distance)) : 0.0;
  last_sweep_ = hit;
  return hit;
}

std::string PhysxScene::status() const { return state_name(static_cast<int>(state_)); }

std::vector<std::string> PhysxScene::describe() const {
  std::vector<std::string> lines;
  lines.push_back(strf("physx: %s, scene %p, offset %s, faults %d", status().c_str(), scene_,
                       offset_ ? strf("(%.0f, %.0f) residual %.2f m", offset_->x, offset_->z, offset_->residual).c_str() : "none",
                       faults_));
  if (last_ground_)
    lines.push_back(strf("physx: last ground %.2f, normal (%.2f, %.2f, %.2f)", last_ground_->height, last_ground_->normal.x,
                         last_ground_->normal.y, last_ground_->normal.z));
  if (last_sweep_)
    lines.push_back(strf("physx: last sweep hit %.3f m, normal (%.2f, %.2f, %.2f)%s", last_sweep_->distance, last_sweep_->normal.x,
                         last_sweep_->normal.y, last_sweep_->normal.z,
                         last_sweep_->initial_overlap ? strf(", overlap %.3f m", last_sweep_->depth).c_str() : ""));
  return lines;
}

}  // namespace fw
```

Megjegyzések a megvalósítóhoz:
- A `describe` két helyén a `strf(...).c_str()` ideiglenes szövege a teljes kifejezés végéig él, ezért a külső `strf` hívásig érvényes.
- Az `std::optional<SweepHit>` és `GroundHit` visszaadása után a `last_*` a diagnosztikához marad meg.
- A `vtable_slot` a játék objektumának első qwordjából olvas; a `guarded` blokkon belül hívva a szemét jelölt hozzáférési hibája is elkapható, ezért a `vtable_slot` hívása mindig a lambdán belül van.
- A `scan_for_vtable` a saját `buffer` lapját kihagyja, a vtable értékét pedig a `g_scene_vtable` (a DLL adatszakasza) tartja; a kupacban maradt régi másolatok szemét jelöltet adhatnak, ezeket a `validate` szűri ki (csak az a jelölt jó, amelyben a kamion aktora megvan).

- [ ] **Step 4: CMake**

A `FreeWalk` könyvtár forrásai közé `src/win/physx_scene.cpp`, a `target_link_libraries(FreeWalk ...)` sorba `psapi`.
Run: `build.cmd` - Expected: figyelmeztetésmentes fordítás, minden teszt zöld. Ha az MSVC figyelmeztet (például a `static_assert(offsetof...)` vagy a lambdák konverziója miatt), javítsd a kódot, ne a figyelmeztetést kapcsold ki.

- [ ] **Step 5: Commit**

```bash
git add src/win/physx_types.h src/win/physx_scene.h src/win/physx_scene.cpp CMakeLists.txt
git commit -m "Query the game's PhysX scene: background search, candidate check by the truck's actor, origin offset, guarded raycast and sweep"
```

---

### Task 9: Bekötés a pluginba, tesztcsomag, élő teszt

**Files:**
- Modify: `src/win/game_camera.h`, `src/win/game_camera.cpp`
- Modify: `src/win/plugin.cpp`
- Modify: `README.md`
- Create: `dist/FreeWalk-0.5.0-physics/TESZT.md` (git-ignorált)

**Interfaces:**
- Consumes: `PhysxScene` (8.), `Walker::step(..., World&)`, `Walker::settle` (5.), `FlatWorld`.

- [ ] **Step 1: A világ kulcsa** (`src/win/game_camera.h`, a nyilvános részbe a `free_index` után)

```cpp
  // A kamerakezelő címe (0, amíg nincs betöltött világ); ha változik, a játék új világot töltött be.
  std::uint64_t world_key() const { return manager(); }
```

- [ ] **Step 2: A plugin bekötése** (`src/win/plugin.cpp`)

- `#include "win/physx_scene.h"` és `#include "core/world.h"`.
- `kVersion` = `"0.5.0"`.
- A `State`-be: `fw::PhysxScene physx;` és `bool walking_on_physics = false;  // a legutóbbi képkocka a játék fizikájával lépett`.
- A `scs_telemetry_init`-ben a kamerakeresés naplója után:

```cpp
  if (g->camera_ready && g->config.collision_enabled) {
    std::vector<std::string> physx_lines;
    const ULONGLONG physx_start = GetTickCount64();
    g->physx.init(physx_lines);
    physx_lines.push_back(fw::strf("physx: the search in the game executable took %llu ms", GetTickCount64() - physx_start));
    log_lines(physx_lines);
  }
```

- Az `on_frame_end`-ben, a `camera_ready` vizsgálata után (a `switch_test` elé):

```cpp
  {
    std::vector<std::string> physx_lines;
    g->physx.update(g->camera.world_key(), g->truck.position, g->truck.valid, dt, physx_lines);
    log_lines(physx_lines);
  }
```

- Új segédfüggvény a `walk` elé:

```cpp
// A gyalogló világa: a játék fizikája, ha kész; különben sík talaj a láb mostani magasságában. A váltás a naplóba kerül.
fw::World& walking_world(fw::FlatWorld& flat) {
  fw::World* physics = g->config.collision_enabled ? g->physx.world() : nullptr;
  if ((physics != nullptr) != g->walking_on_physics) {
    g->walking_on_physics = physics != nullptr;
    fw::log_info(physics ? "ground and collision: from the game's physics"
                         : "ground and collision: flat ground (physx " + g->physx.status() + ")");
  }
  return physics ? *physics : flat;
}
```

- A `walk()`-ban a 5. feladat két sora helyett:

```cpp
  fw::FlatWorld flat(g->walker.feet().y);
  g->walker.step(keys, fw::layout::camera_yaw_to_heading(*yaw), dt, g->config, walking_world(flat));
```

- A `start_walk`-ban a `g->walker.reset(out.start_feet);` után:

```cpp
  {
    fw::FlatWorld flat(out.start_feet.y);
    g->walking_on_physics = false;
    g->walker.settle(walking_world(flat));
  }
```

  és a `walking from` naplósor a `g->walker.feet()`-et írja ki (`out.start_feet` helyett), a becsült magassággal együtt:
  `fw::strf("walking from (%.2f, %.2f, %.2f), estimated ground %.2f", f.x, f.y, f.z, out.start_feet.y)`, ahol `const fw::Vec3 f = g->walker.feet();`.
- A `discovery()`-ben a `describe` után: `for (const auto& line : g->physx.describe()) lines.push_back(line);`.
- A `scs_telemetry_shutdown`-ban a `directinput_filter_stop()` elé: `g->physx.stop();`.

Run: `build.cmd` - Expected: figyelmeztetésmentes fordítás, minden teszt zöld; a `freewalk_probe` mind az öt keresést megtalálja.

- [ ] **Step 3: README** (`README.md`)

A Beállítások után egy rövid `## Talaj és ütközés` szakasz: a gyalogló a játék saját fizikájából követi a talajt és a lépcsőket, nem megy át a falakon, kerítéseken és a kamionon; a játék csak a kamion körül (kb. 35 m-en belül) tart ütközési geometriát, azon kívül a talaj sík marad, ütközés nélkül; a `[collision]` beállítások jelentése (a táblázat a spec szerint); ha a fizika nem érhető el, a napló `physx:` sorai mondják meg, miért, és a séta sík talajjal megy.

- [ ] **Step 4: Commit**

```bash
git add src/win/game_camera.h src/win/plugin.cpp README.md
git commit -m "Walk on the game's physics: search the scene per world, settle on the real ground, fall back to flat ground"
```

- [ ] **Step 5: Tesztcsomag**

`dist/FreeWalk-0.5.0-physics/`: `FreeWalk.dll` (a `build\msvc-Release\`-ből), `FreeWalk.ini` (a gyökérből, `discovery_log = 1`-gyel), `TESZT.md`:

1. Betöltés: a naplóban `physx scene: vtable +0x...`, egy mentés betöltése után `physx: scene search 1: ... candidate(s)` és `physx: using the scene at ...; origin offset (...)`; ha nem, a `physx:` sorok és a keresés ideje.
2. Kiszállás egy parkolóban: a `walking from (...), estimated ground ...` sor; a kamion körül sétálva a `[discovery] physx: last ground` értéke és a látott talaj egyezik; lejtőn, járdaszegélyen, lépcsőn a láb a talajon marad.
3. Fal, kerítés, a kamion és a pótkocsi oldala: nem lehet átmenni, a fal mentén csúszik; sarokban megáll; kiszállás szorosan egy fal vagy egy másik jármű mellett.
4. Leesés egy rakodórámpáról vagy peremről; a kamiontól 40-50 m-re a talaj sík marad (a `flat ground` sor).
5. Új mentés betöltése, majd séta: újrakeresés (`a world was loaded`); `[collision] enabled = 0` és újraindítás: sík talaj, nincs `physx:` keresés.

Visszajelzés: a `game.log.txt` (a FreeWalk sorai), és ha valami furcsa, képernyőkép.

- [ ] **Step 6: Élő teszt**

Kérdezd meg a felhasználót a játék `plugins` mappájának útvonaláról (ha még nincs a végrehajtási naplóban), másold be a DLL-t és az INI-t (a TM Real Walk legyen kivéve), és kérd meg, hogy játssza végig a `TESZT.md` lépéseit.
A naplót közvetlenül olvasd (`Dokumentumok\Euro Truck Simulator 2\game.log.txt`).
Minden eltérést `Ruling:` sorként rögzíts; a javítás teszttel kezdődik (a mag hibáinál), a verzió lép (0.5.1, ...).
Az igazolt tények (a jelenet keresésének ideje, az eltolás, a lekérdezések stabilitása) az `ets2-1.61.md`-be kerülnek.

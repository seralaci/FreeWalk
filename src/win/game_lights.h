#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include "core/flashlight.h"
#include "core/game_layout.h"
#include "core/light_borrow.h"
#include "core/object_check.h"
#include "core/vec.h"
#include "win/process_memory.h"

namespace fw {

// The game's lights: the scene light array, the fields of a light, borrowing and own light (extended by flashlight plan tasks 7 and 8).
class GameLights {
 public:
  // Locates the light_source type, the type entry and the two light distance console variables (at load time).
  bool init(std::vector<std::string>& log);
  // The light_source lamps of the light array (position, whether lit).
  std::vector<LightCandidate> lights();
  // Diagnostics (discovery_log): array size, runtime fields of the type entry, the console variables, fields of the nearest 6 lamps.
  std::vector<std::string> describe(Vec3 walker);

  // Acquires a light: borrows a street lamp (its original fields saved); if none, and own_light, the own light (created once per world
  // with the game's creator function, and never destroyed by us). False if neither succeeded.
  bool acquire(Vec3 walker, double min_distance, double max_distance, bool own_light, std::vector<std::string>& log);
  // Sets the held light; false if the light was lost (the game freed its slot), in which case we no longer hold it.
  bool apply(const LightOutput& out, const std::array<float, 4>& rotation, std::vector<std::string>& log);
  // Returns the borrowed lamp (its original fields written back, if it is still the same lamp).
  void release(std::vector<std::string>& log);
  // On a new world: we do not write to the old addresses, just forget them (the own light too; the game destroys it with the world).
  void forget() {
    held_ = {};
    own_ = 0;
    own_listed_ = false;
    own_missing_reported_ = false;
    own_first_apply_ = 0;
  }
  bool holding() const { return held_.address != 0; }

 private:
  struct Array {
    std::uint64_t data = 0;
    std::uint64_t count = 0;
  };
  Array array() const;
  // Discovery: the offsets in the scene object where a (pointer, count) pair points to an array of light_source objects.
  std::vector<std::string> find_array(std::uint64_t scene);
  bool is_light(std::uint64_t light) const;

  // The borrowed lamp and its original fields: the color, the +0x108..+0x1F8 range and the +0x208 flag.
  struct Held {
    std::uint64_t address = 0;
    bool own = false;
    std::array<std::uint8_t, 0xF8> original{};  // from +0x108 to +0x200
  };
  static constexpr std::uint64_t kSavedFrom = 0x108;

  Held held_;
  bool create_own(std::vector<std::string>& log);
  bool update_own(std::uint64_t light);
  bool in_array(std::uint64_t light);
  // Whether the lamp still belongs to the game: light_source vtable and present in the scene light array (a freed lamp of an unloaded
  // sector may keep the old vtable, so the vtable alone is not enough).
  bool alive(std::uint64_t light) { return is_light(light) && in_array(light); }

  std::uint64_t own_ = 0;           // the own light in this world
  bool own_failed_ = false;         // creation or update reported an error: no own light for this session
  bool own_listed_ = false;         // the own light is already in the scene light array (logged once)
  bool own_missing_reported_ = false;  // the own light is still not in the light array after 2 s: logged once
  std::uint64_t own_first_apply_ = 0;  // GetTickCount64 at the first setting of the own light in this world
  bool creator_ok_ = false;         // the creator function is a function start according to .pdata
  std::uint64_t last_attempt_ = 0;  // GetTickCount64 at the last borrow attempt
  bool reported_no_lamp_ = false;
  bool ready_ = false;
  std::uint64_t base_ = 0;
  const layout::GameBuild* build_ = nullptr;  // the running game build (its RVAs)
  std::uint64_t light_vtable_ = 0;
  bool array_searched_ = false;
  unsigned search_ticks_ = 0;
  std::uint64_t distance_cvar_ = 0;
  std::uint64_t span_cvar_ = 0;
  CodeRanges ranges_;
  ProcessMemory memory_;
};

}  // namespace fw

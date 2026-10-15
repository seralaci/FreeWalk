#include "win/game_lights.h"
// Without the windows.h min and max macros, so that std::min and std::max can be used.
#define NOMINMAX
#include <windows.h>
#include <algorithm>
#include <cstring>
#include "core/coords.h"
#include "core/game_layout.h"
#include "core/locator.h"
#include "core/pe_image.h"
#include "core/text.h"
#include "win/guarded_call.h"

namespace fw {
namespace {

constexpr std::uint64_t kMaxLights = 20000;
constexpr std::size_t kDescribed = 6;

// Light position from the transform block (position within the chunk, then the two int16 chunk indices).
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
  if (const Section* pdata = image->section(".pdata"))
    creator_ok_ = function_start(image->read_section(memory_, *pdata), base_, base_ + layout::kLightCreator) == base_ + layout::kLightCreator;
  if (const auto c = find_cvar(memory_, *image, layout::kLightDistanceCvar, true, log)) distance_cvar_ = *c;
  if (const auto c = find_cvar(memory_, *image, layout::kLightSpanCvar, true, log)) span_cvar_ = *c;
  ready_ = true;
  const auto descriptor_size = memory_.get<std::uint64_t>(base_ + layout::kLightClassDescriptor + 0x30);
  log.push_back(strf("lights: light_source vtable +0x%llX, class descriptor size 0x%llX (expected 0x%llX)",
                     static_cast<unsigned long long>(light_vtable_ - base_), static_cast<unsigned long long>(descriptor_size.value_or(0)),
                     static_cast<unsigned long long>(layout::kLightObjectSize)));
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
  if (a.count == 0) return out;
  std::vector<std::uint64_t> pointers(static_cast<std::size_t>(a.count));
  if (!memory_.read(a.data, pointers.data(), pointers.size() * 8)) return out;
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
  // The search repeats every 10 seconds until it finds something (at load time the array may still be empty).
  if (!array_searched_ && search_ticks_++ % 10 == 0) {
    const auto scene = memory_.get<std::uint64_t>(base_ + layout::kSceneGlobal);
    if (scene && *scene) {
      auto found = find_array(*scene);
      array_searched_ = found.front().find("candidate array") != std::string::npos;
      for (auto& line : found) lines.push_back(std::move(line));
    }
  }
  std::string cvars;
  for (const std::uint64_t c : {distance_cvar_, span_cvar_}) {
    char value[16] = {};
    if (c && memory_.read(c + layout::kCvarValue, value, sizeof value - 1)) cvars += std::string(" '") + value + "'";
  }
  auto list = lights();
  lines.push_back(strf("lights: array %llX, %llu entries, %zu light_source; distance/span cvars%s", static_cast<unsigned long long>(a.data),
                       static_cast<unsigned long long>(a.count), list.size(), cvars.c_str()));
  std::sort(list.begin(), list.end(), [&](const auto& x, const auto& y) {
    return horizontal_distance(walker, x.position) < horizontal_distance(walker, y.position);
  });
  for (std::size_t i = 0; i < list.size() && i < kDescribed; ++i) {
    float f[0x40];
    if (!memory_.read(list[i].id + 0x100, f, sizeof f)) continue;
    const auto at = [&](std::uint64_t offset) { return f[(offset - 0x100) / 4]; };
    lines.push_back(strf("lights:   %llX %.1f m %s pos (%.1f %.1f %.1f) rot (%.3f %.3f %.3f %.3f) colour (%.2f %.2f %.2f / %.2f %.2f %.2f) "
                         "+108 %.3f +168 %.2f +174 %.2f +178 %.2f +1A8 %.2f +1AC %.2f",
                         static_cast<unsigned long long>(list[i].id), horizontal_distance(walker, list[i].position),
                         list[i].active ? "on" : "off", list[i].position.x, list[i].position.y, list[i].position.z, at(0x1C0), at(0x1C4),
                         at(0x1C8), at(0x1CC), at(0x124), at(0x128), at(0x12C), at(0x130), at(0x134), at(0x138), at(0x108), at(0x168),
                         at(0x174), at(0x178), at(0x1A8), at(0x1AC)));
  }
  return lines;
}

bool GameLights::create_own(std::vector<std::string>& log) {
  if (!creator_ok_) {
    log.push_back("lights: the light creator is not a function start in this build; no own light");
    own_failed_ = true;
    return false;
  }
  struct Args {
    void* (*create)(void*);
    void* object;
  } args{reinterpret_cast<void* (*)(void*)>(base_ + layout::kLightCreator), nullptr};
  Fault f;
  if (!guarded_call([](void* c) {
        auto* a = static_cast<Args*>(c);
        a->object = a->create(nullptr);
      }, &args, &f) || !args.object || !is_light(reinterpret_cast<std::uint64_t>(args.object))) {
    log.push_back(strf("lights: the game's light creator failed (code 0x%08lX); no own light this session", f.code));
    own_failed_ = true;
    return false;
  }
  const std::uint64_t light = reinterpret_cast<std::uint64_t>(args.object);
  // Initial values according to TM: the "always on" flag, the fade speed and the enable byte.
  std::uint32_t flags = memory_.get<std::uint32_t>(light + layout::kLightFlags).value_or(0);
  flags = (flags & 0xFE001000u) | 0x1000u;
  const bool ok = memory_.put(light + layout::kLightFlags, flags) && memory_.put(light + layout::kLightFadeSpeed, 0.05f) &&
                  memory_.put(light + layout::kLightOwnEnable, std::uint8_t{1});
  if (!ok) {
    log.push_back("lights: the own light could not be set up; no own light this session");
    own_failed_ = true;
    return false;
  }
  own_ = light;
  log.push_back(strf("lights: own light made at %llX", static_cast<unsigned long long>(light)));
  return true;
}

bool GameLights::update_own(std::uint64_t light) {
  if (checked_vtable(memory_, light, ranges_, {layout::kLightUpdateSlot}) != light_vtable_) return false;
  struct Args {
    void (*update)(void*);
    void* object;
  } args{nullptr, reinterpret_cast<void*>(light)};
  const auto vtable = memory_.get<std::uint64_t>(light);
  const auto slot = vtable ? memory_.get<std::uint64_t>(*vtable + 8 * layout::kLightUpdateSlot) : std::nullopt;
  if (!slot) return false;
  args.update = reinterpret_cast<void (*)(void*)>(*slot);
  Fault f;
  return guarded_call([](void* c) {
    auto* a = static_cast<Args*>(c);
    a->update(a->object);
  }, &args, &f);
}

bool GameLights::in_array(std::uint64_t light) {
  const Array a = array();
  if (a.count == 0) return false;
  std::vector<std::uint64_t> pointers(static_cast<std::size_t>(a.count));
  if (!memory_.read(a.data, pointers.data(), pointers.size() * 8)) return false;
  return std::find(pointers.begin(), pointers.end(), light) != pointers.end();
}

std::vector<std::string> GameLights::find_array(std::uint64_t scene) {
  std::vector<std::string> lines;
  constexpr std::uint64_t kSceneSize = 0x10000;
  std::vector<std::uint64_t> words(kSceneSize / 8);
  if (!memory_.read(scene, words.data(), kSceneSize)) {
    lines.push_back("lights: the scene object could not be read for the array search");
    return lines;
  }
  int found = 0;
  for (std::size_t i = 0; i + 1 < words.size(); ++i) {
    const std::uint64_t data = words[i];
    const std::uint64_t count = words[i + 1];
    if (count == 0 || count > kMaxLights || data < 0x10000 || data % 8 != 0) continue;
    const std::size_t sample = static_cast<std::size_t>(std::min<std::uint64_t>(count, 64));
    std::vector<std::uint64_t> pointers(sample);
    if (!memory_.read(data, pointers.data(), sample * 8)) continue;
    int lights = 0;
    for (const std::uint64_t p : pointers)
      if (is_light(p)) ++lights;
    if (lights == 0) continue;
    lines.push_back(strf("lights: candidate array at scene+0x%llX: %llu entries, %d of the first %zu are light_source",
                         static_cast<unsigned long long>(i * 8), static_cast<unsigned long long>(count), lights, sample));
    if (++found >= 12) break;
  }
  if (found == 0) lines.push_back("lights: no light_source array found in the first 64 KB of the scene object");
  return lines;
}

bool GameLights::acquire(Vec3 walker, double min_distance, double max_distance, bool own_light, std::vector<std::string>& log) {
  if (!ready_ || holding()) return holding();
  // At most one attempt per second; failure is logged only once until a borrow succeeds.
  const ULONGLONG now = GetTickCount64();
  if (last_attempt_ != 0 && now - last_attempt_ < 1000) return false;
  last_attempt_ = now;
  const auto chosen = choose_light(walker, lights(), min_distance, max_distance);
  if (!chosen) {
    if (own_light && !own_failed_ && (own_ != 0 || create_own(log))) {
      held_ = {};
      held_.address = own_;
      held_.own = true;
      reported_no_lamp_ = false;
      log.push_back("lights: no street lamp to borrow; using the own light");
      return true;
    }
    if (!reported_no_lamp_) log.push_back("lights: no street lamp is on to borrow (is it dark enough?)");
    reported_no_lamp_ = true;
    return false;
  }
  reported_no_lamp_ = false;
  Held h;
  h.address = *chosen;
  if (!memory_.read(h.address + kSavedFrom, h.original.data(), h.original.size())) {
    log.push_back("lights: the chosen street lamp could not be read");
    return false;
  }
  held_ = h;
  float block[4];
  memory_.read(h.address + layout::kLightTransform, block, sizeof block);
  float range = 0;
  std::memcpy(&range, h.original.data() + (layout::kLightRange - kSavedFrom), 4);
  log.push_back(strf("lights: borrowed the street lamp %llX %.0f m away (range %.1f m)", static_cast<unsigned long long>(h.address),
                     horizontal_distance(walker, world_of(block)), range));
  return true;
}

bool GameLights::apply(const LightOutput& out, const std::array<float, 4>& rotation, std::vector<std::string>& log) {
  if (!holding()) return false;
  if (held_.own ? !is_light(held_.address) : !alive(held_.address)) {
    log.push_back("lights: the borrowed street lamp is gone");
    held_ = {};
    return false;
  }
  // Position and rotation: position within the chunk and the chunk index, then the camera quaternion (the light shines along its -Z too).
  const CameraPosition c = to_camera(out.position);
  float block[8] = {c.x, c.y, c.z, 0, rotation[0], rotation[1], rotation[2], rotation[3]};
  const std::int16_t chunks[2] = {c.chunk_x, c.chunk_z};
  std::memcpy(block + 3, chunks, 4);
  const float colour[6] = {static_cast<float>(out.r), static_cast<float>(out.g), static_cast<float>(out.b),
                           static_cast<float>(out.r), static_cast<float>(out.g), static_cast<float>(out.b)};
  const auto original = [&](std::uint64_t offset) {
    float v;
    std::memcpy(&v, held_.original.data() + (offset - kSavedFrom), 4);
    return v;
  };
  // +0x1A8/+0x1AC scale with the range: for the borrowed lamp we keep their original ratio, for the own light we use the street lamps'
  // ratio (about 0.365 and 0.378).
  const float range = static_cast<float>(out.range);
  const float original_range = held_.own ? 0.0f : original(layout::kLightRange);
  const float scale = original_range > 0.01f ? range / original_range : 1.0f;
  const float sizes[2] = {held_.own ? range * 0.365f : original(layout::kLightSize) * scale,
                          held_.own ? range * 0.378f : original(layout::kLightSize + 4) * scale};
  const float inner = static_cast<float>(out.inner_angle);
  const float outer = static_cast<float>(out.outer_angle);
  const std::uint64_t a = held_.address;
  const bool ok = memory_.write(a + layout::kLightTransform, block, sizeof block) &&
                  memory_.write(a + layout::kLightTransformCopy, block, sizeof block) &&
                  memory_.write(a + layout::kLightColour, colour, sizeof colour) && memory_.put(a + layout::kLightRange, range) &&
                  memory_.put(a + layout::kLightInnerAngle, inner) && memory_.put(a + layout::kLightOuterAngle, outer) &&
                  memory_.write(a + layout::kLightSize, sizes, sizeof sizes);
  if (!ok) {
    log.push_back("lights: the borrowed street lamp could not be written");
    held_ = {};
    return false;
  }
  if (held_.own) {
    if (!update_own(a)) {
      log.push_back("lights: the game's light update faulted on the own light; own light switched off for this session");
      own_failed_ = true;
      held_ = {};
      return false;
    }
    const ULONGLONG now = GetTickCount64();
    if (own_first_apply_ == 0) own_first_apply_ = now;
    if (!own_listed_ && in_array(a)) {
      own_listed_ = true;
      log.push_back("lights: the own light is in the scene's light list");
    } else if (!own_listed_ && !own_missing_reported_ && now - own_first_apply_ > 2000) {
      own_missing_reported_ = true;
      log.push_back("lights: the own light is not in the scene's light list after 2 s; it will not show");
    }
  }
  return true;
}

void GameLights::release(std::vector<std::string>& log) {
  if (!holding()) return;
  if (held_.own) {
    // We do not destroy the own light: we set it dark and update it, the next switch-on reuses it.
    const float dark[6] = {};
    if (is_light(held_.address)) {
      memory_.write(held_.address + layout::kLightColour, dark, sizeof dark);
      update_own(held_.address);
    }
    held_ = {};
    return;
  }
  if (alive(held_.address) && memory_.write(held_.address + kSavedFrom, held_.original.data(), held_.original.size()))
    log.push_back("lights: the street lamp was given back");
  else
    log.push_back("lights: the borrowed street lamp is gone; nothing to give back");
  held_ = {};
}

}  // namespace fw

#include "win/region_focus.h"
#include <windows.h>
#include <cstring>
#include "core/coords.h"
#include "core/game_layout.h"
#include "core/pe_image.h"
#include "core/text.h"
#include "win/guarded_call.h"
#include "win/process_memory.h"

namespace fw {
namespace {

constexpr double kSlowQueryMs = 250.0;
constexpr int kMaxSlowQueries = 3;

// The box_builder position argument: position within the chunk, then the indices of the two 512 m chunks (per the game file:
// the function multiplies the int16 at +0x0C and +0x0E by 512 and adds them to the floats at +0 and +8).
struct GamePosition {
  float x = 0;
  float y = 0;
  float z = 0;
  std::int16_t chunk_x = 0;
  std::int16_t chunk_z = 0;
};
static_assert(sizeof(GamePosition) == 16);

using BoxBuilderFn = void* (*)(void* unused, void* box, const GamePosition* center, const float* size);
using RegionQueryFn = void (*)(void* scene, const void* box, std::uint8_t flag);

struct QueryArgs {
  BoxBuilderFn build;
  RegionQueryFn query;
  void* scene;
  GamePosition center;
  float size[4];
  float box[16];  // box_builder writes 0x28 bytes into it (with float writes only)
};

}  // namespace

bool RegionFocus::init(std::vector<std::string>& log) {
  ProcessMemory memory;
  base_ = reinterpret_cast<std::uint64_t>(GetModuleHandleW(nullptr));
  const auto image = PeImage::parse(memory, base_);
  const Section* pdata = image ? image->section(".pdata") : nullptr;
  build_ = image ? layout::find_build(image->timestamp(), image->image_size()) : nullptr;
  if (!image || !pdata || !build_) {
    log.push_back("region focus: not the verified game build; collision loads only around vehicles");
    return false;
  }
  const SectionBytes table = image->read_section(memory, *pdata);
  for (const std::uint64_t rva : {build_->box_builder, build_->region_query}) {
    if (function_start(table, base_, base_ + rva) != base_ + rva) {
      log.push_back(strf("region focus: exe+0x%llX is not a function start; collision loads only around vehicles",
                         static_cast<unsigned long long>(rva)));
      return false;
    }
  }
  if (!image->in_section(".data", base_ + build_->scene_global)) {
    log.push_back("region focus: the scene global is not in .data; collision loads only around vehicles");
    return false;
  }
  ready_ = true;
  log.push_back(strf("region focus: ready (box builder exe+0x%llX, region query exe+0x%llX, scene global exe+0x%llX)",
                     static_cast<unsigned long long>(build_->box_builder), static_cast<unsigned long long>(build_->region_query),
                     static_cast<unsigned long long>(build_->scene_global)));
  return true;
}

void RegionFocus::update(Vec3 feet, double dt, std::vector<std::string>& log) {
  if (!ready_) return;
  const auto size = schedule_.step(dt);
  if (!size) return;
  ProcessMemory memory;
  const auto scene = memory.get<std::uint64_t>(base_ + build_->scene_global);
  if (!scene || *scene == 0) return;

  QueryArgs args{};
  args.build = reinterpret_cast<BoxBuilderFn>(base_ + build_->box_builder);
  args.query = reinterpret_cast<RegionQueryFn>(base_ + build_->region_query);
  args.scene = reinterpret_cast<void*>(*scene);
  const CameraPosition c = to_camera(feet);
  args.center = {c.x, c.y, c.z, c.chunk_x, c.chunk_z};
  args.size[0] = args.size[1] = args.size[2] = static_cast<float>(*size);

  LARGE_INTEGER frequency, start, end;
  QueryPerformanceFrequency(&frequency);
  QueryPerformanceCounter(&start);
  Fault f;
  const bool ok = guarded_call(
      [](void* p) {
        auto* a = static_cast<QueryArgs*>(p);
        a->build(nullptr, a->box, &a->center, a->size);
        a->query(a->scene, a->box, 0);
      },
      &args, &f);
  QueryPerformanceCounter(&end);
  const double ms = 1000.0 * static_cast<double>(end.QuadPart - start.QuadPart) / static_cast<double>(frequency.QuadPart);
  ++queries_;
  if (!ok) {
    ready_ = false;
    log.push_back(strf("region focus: the game's region query faulted (code 0x%08lX at exe+0x%llX); switched off", f.code,
                       static_cast<unsigned long long>(f.address >= base_ ? f.address - base_ : 0)));
    return;
  }
  if (queries_ <= 3 || queries_ % 30 == 0)
    log.push_back(strf("region focus: query #%u around (%.1f, %.1f), box %.0f m, %.1f ms", queries_, feet.x, feet.z, *size, ms));
  slow_ = ms > kSlowQueryMs ? slow_ + 1 : 0;
  if (slow_ >= kMaxSlowQueries) {
    ready_ = false;
    log.push_back(strf("region focus: the region query keeps taking more than %.0f ms; switched off", kSlowQueryMs));
  }
}

}  // namespace fw

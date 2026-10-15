#include "win/physx_scene.h"
// Without the windows.h min and max macros, so that std::min and std::max can be used.
#define NOMINMAX
#include <windows.h>
#include <psapi.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstring>
#include "core/game_layout.h"
#include "core/locator.h"
#include "core/object_check.h"
#include "core/pe_image.h"
#include "core/text.h"
#include "win/guarded_call.h"
#include "win/physx_types.h"
#include "win/process_memory.h"

namespace fw {
namespace {

// Scene vtable address in the DLL's own data section (not on the heap), so the memory search does not find our own copy.
std::uint64_t g_scene_vtable = 0;

constexpr std::size_t kMaxCandidates = 16;
constexpr int kSearchAttempts = 8;
constexpr auto kSearchRetry = std::chrono::seconds(8);
constexpr std::size_t kPagesPerQuery = 1024;
constexpr std::uint32_t kMaxDynamicActors = 4096;
constexpr int kMaxFaults = 8;
constexpr double kOffsetInterval = 0.25;
constexpr float kOverlapDistance = 1e-4f;
constexpr int kValidationRounds = 6;
constexpr double kValidationRetry = 5.0;

std::condition_variable g_wake;  // interrupts the background thread's wait on shutdown
std::mutex g_wake_mutex;

template <class Fn>
Fn vtable_slot(void* object, std::size_t index) {
  const auto vtable = *static_cast<std::uint64_t* const*>(object);
  return reinterpret_cast<Fn>(vtable[index]);
}

// 8-byte aligned locations holding the vtable address in the process's writable private memory. Only reads pages resident
// in physical memory (QueryWorkingSetEx), so nothing gets paged in; skips our own thread's stack.
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
  std::vector<std::uint8_t> buffer(static_cast<std::size_t>(page));
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
        // Our own buffer's page (read into itself it could contain the previous page's value).
        const auto own = reinterpret_cast<std::uint64_t>(buffer.data());
        if (at < own + buffer.size() && own < at + page) continue;
        SIZE_T done = 0;
        if (!ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<LPCVOID>(at), buffer.data(), static_cast<SIZE_T>(page), &done) ||
            done != page)
          continue;
        read += page;
        for (std::size_t o = 0; o + 8 <= buffer.size(); o += 8) {
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
  static const char* const names[] = {"off",   "waiting for a world", "searching", "checking candidates", "waiting to search again",
                                      "ready", "disabled"};
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
  const auto ranges = code_ranges(*image);
  if (!ranges) {
    log.push_back("physx: the executable has no .rdata/.text section; ground and collision are off");
    return false;
  }
  ranges_ = *ranges;
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
    validation_rounds_ = 0;
    scene_lost_ = false;
    state_ = State::Idle;
    if (world_key != 0) {
      log.push_back("physx: a world was loaded, looking for its scene");
      start_search();
    }
  }
  bool done = false;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    log.insert(log.end(), thread_log_.begin(), thread_log_.end());
    thread_log_.clear();
    done = state_ == State::Searching && search_done_;
  }
  log.insert(log.end(), pending_log_.begin(), pending_log_.end());
  pending_log_.clear();
  if (done) {
    if (thread_.joinable()) thread_.join();
    state_ = candidates_.empty() ? State::Disabled : State::Validating;
    if (candidates_.empty()) log.push_back("physx: the scene was not found in memory; ground and collision are off until the next world");
  }
  if (state_ == State::Ready && (scene_lost_ || !scene_alive())) {
    // The scene object was freed or replaced (e.g. on loading a save): search again.
    log.push_back("physx: the scene object is gone; looking for the scene again");
    scene_ = nullptr;
    offset_.reset();
    scene_lost_ = false;
    validation_rounds_ = 0;
    start_search();
  }
  if (state_ == State::Retrying) {
    retry_timer_ -= dt;
    if (retry_timer_ <= 0) start_search();
  }
  if (state_ == State::Validating && truck_valid) validate(truck_position, log);
  if (state_ == State::Ready && truck_valid) {
    offset_timer_ += dt;
    if (offset_timer_ >= kOffsetInterval) {
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
    if (checked_vtable(memory_, candidate, ranges_,
                       {layout::kPxSceneGetNbActors, layout::kPxSceneGetActors, layout::kPxSceneRaycast, layout::kPxSceneSweep}) !=
        g_scene_vtable) {
      log.push_back(strf("physx: candidate %p is no longer a scene", scene));
      continue;
    }
    const auto actors = dynamic_actors(scene, false);
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
    truck_actor_vtable_ = actor_vtable(offset->actor).value_or(0);
    offset_timer_ = 0;
    state_ = State::Ready;
    log.push_back(strf("physx: using the scene at %p (%zu dynamic actors); origin offset (%.0f, %.0f), residual %.2f m", scene,
                       actors->size(), offset->x, offset->z, offset->residual));
    return;
  }
  // During loading the scene may not contain the truck yet: we search again a few times.
  if (++validation_rounds_ < kValidationRounds) {
    state_ = State::Retrying;
    retry_timer_ = kValidationRetry;
    log.push_back(strf("physx: no scene candidate holds the truck; searching again in %.0f s (%d of %d)", kValidationRetry,
                       validation_rounds_, kValidationRounds));
    return;
  }
  state_ = State::Disabled;
  log.push_back("physx: no scene candidate holds the truck; ground and collision are off until the next world");
}

void PhysxScene::refresh_offset(Vec3 truck_position, std::vector<std::string>& log) {
  std::optional<SceneOffset> next;
  if (offset_ && truck_actor_vtable_ != 0 && actor_vtable(offset_->actor) == truck_actor_vtable_) {
    if (const auto p = actor_position(reinterpret_cast<void*>(offset_->actor), true))
      next = offset_from_actor(truck_position, {offset_->actor, *p});
  }
  if (!next) {
    if (const auto actors = dynamic_actors(scene_, true)) next = estimate_scene_offset(truck_position, *actors);
    if (next) truck_actor_vtable_ = actor_vtable(next->actor).value_or(0);
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

std::optional<std::vector<ActorPosition>> PhysxScene::dynamic_actors(void* scene, bool count_faults) {
  struct CountArgs {
    void* scene;
    std::uint16_t types;
    std::uint32_t count;
  } count_args{scene, physx::kActorRigidDynamic, 0};
  Fault f;
  if (!guarded_call(
          [](void* c) {
            auto* a = static_cast<CountArgs*>(c);
            a->count = vtable_slot<physx::GetNbActorsFn>(a->scene, layout::kPxSceneGetNbActors)(a->scene, &a->types);
          },
          &count_args, &f)) {
    if (count_faults) fault("actor count", f.code, f.address);
    return std::nullopt;
  }
  const std::uint32_t total = std::min(count_args.count, kMaxDynamicActors);
  std::vector<void*> actors(total);
  struct ListArgs {
    void* scene;
    std::uint16_t types;
    void** buffer;
    std::uint32_t size;
    std::uint32_t got;
  } list_args{scene, physx::kActorRigidDynamic, actors.data(), total, 0};
  if (total > 0 && !guarded_call(
                       [](void* c) {
                         auto* a = static_cast<ListArgs*>(c);
                         a->got = vtable_slot<physx::GetActorsFn>(a->scene, layout::kPxSceneGetActors)(a->scene, &a->types, a->buffer,
                                                                                                     a->size, 0);
                       },
                       &list_args, &f)) {
    if (count_faults) fault("actor list", f.code, f.address);
    return std::nullopt;
  }
  std::vector<ActorPosition> positions;
  for (std::uint32_t i = 0; i < std::min(list_args.got, total); ++i) {
    const auto actor = reinterpret_cast<std::uint64_t>(actors[i]);
    // We only call through an actor whose vtable is in the game file and whose getGlobalPose points to code.
    if (!actor_vtable(actor)) continue;
    const auto p = actor_position(actors[i], count_faults);
    if (!p) break;  // after a failing actor we do not trust the rest of the list either
    positions.push_back({actor, *p});
  }
  return positions;
}

std::optional<Vec3> PhysxScene::actor_position(void* actor, bool count_faults) {
  if (!actor) return std::nullopt;
  struct PoseArgs {
    void* actor;
    physx::Transform pose;
  } args{actor, {}};
  Fault f;
  if (!guarded_call(
          [](void* c) {
            auto* a = static_cast<PoseArgs*>(c);
            vtable_slot<physx::GetGlobalPoseFn>(a->actor, layout::kPxActorGetGlobalPose)(a->actor, &a->pose);
          },
          &args, &f)) {
    if (count_faults) fault("actor pose", f.code, f.address);
    return std::nullopt;
  }
  return Vec3{args.pose.p.x, args.pose.p.y, args.pose.p.z};
}

bool PhysxScene::scene_alive() const {
  return scene_ != nullptr &&
         checked_vtable(memory_, reinterpret_cast<std::uint64_t>(scene_), ranges_,
                        {layout::kPxSceneGetNbActors, layout::kPxSceneGetActors, layout::kPxSceneRaycast, layout::kPxSceneSweep}) ==
             g_scene_vtable;
}

std::optional<std::uint64_t> PhysxScene::actor_vtable(std::uint64_t actor) const {
  return checked_vtable(memory_, actor, ranges_, {layout::kPxActorGetGlobalPose});
}

void PhysxScene::fault(const char* what, unsigned long code, std::uint64_t address) {
  ++faults_;
  const unsigned long long where = address >= module_base_ ? address - module_base_ : 0;
  pending_log_.push_back(strf("physx: %s faulted: code 0x%08lX at exe+0x%llX (%d of %d)", what, code, where, faults_, kMaxFaults));
}

World* PhysxScene::world() { return state_ == State::Ready && offset_ ? this : nullptr; }

std::optional<physx::LocationHit> PhysxScene::raycast(Vec3 origin, Vec3 direction, double distance, std::uint16_t query_flags,
                                                      const char* what) {
  struct Args {
    void* scene;
    physx::Vec3f origin;
    physx::Vec3f direction;
    float distance;
    physx::HitBuffer hit;
    std::uint16_t flags;
    physx::QueryFilterData filter;
    bool result;
  } args{scene_,
         {static_cast<float>(origin.x), static_cast<float>(origin.y), static_cast<float>(origin.z)},
         {static_cast<float>(direction.x), static_cast<float>(direction.y), static_cast<float>(direction.z)},
         static_cast<float>(distance),
         {},
         static_cast<std::uint16_t>(physx::kHitPosition | physx::kHitNormal | physx::kHitMeshBothSides),
         {},
         false};
  args.filter.flags = query_flags;
  Fault f;
  if (!guarded_call(
          [](void* c) {
            auto* a = static_cast<Args*>(c);
            a->result = vtable_slot<physx::RaycastFn>(a->scene, layout::kPxSceneRaycast)(a->scene, &a->origin, &a->direction, a->distance,
                                                                                        &a->hit, &a->flags, &a->filter, nullptr, nullptr);
          },
          &args, &f)) {
    fault(what, f.code, f.address);
    return std::nullopt;
  }
  if (!args.hit.has_block) return std::nullopt;
  return args.hit.block;
}

std::optional<physx::LocationHit> PhysxScene::sweep(const Capsule& capsule, Vec3 center, Vec3 direction, double distance,
                                                    std::uint16_t query_flags, const char* what) {
  // The PhysX capsule axis is its own X axis: a 90 degree rotation around the Z axis makes it vertical.
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
         {{0, 0, half, half}, {static_cast<float>(center.x), static_cast<float>(center.y), static_cast<float>(center.z)}},
         {static_cast<float>(direction.x), 0, static_cast<float>(direction.z)},
         static_cast<float>(distance),
         {},
         static_cast<std::uint16_t>(physx::kHitPosition | physx::kHitNormal | physx::kHitMeshBothSides | physx::kHitMtd),
         {},
         false};
  args.filter.flags = query_flags;
  Fault f;
  if (!guarded_call(
          [](void* p) {
            auto* a = static_cast<Args*>(p);
            a->result = vtable_slot<physx::SweepFn>(a->scene, layout::kPxSceneSweep)(a->scene, &a->geometry, &a->pose, &a->direction,
                                                                                    a->distance, &a->hit, &a->flags, &a->filter, nullptr,
                                                                                    nullptr, 0.0f);
          },
          &args, &f)) {
    fault(what, f.code, f.address);
    return std::nullopt;
  }
  if (!args.hit.has_block) return std::nullopt;
  return args.hit.block;
}

std::optional<std::uint32_t> PhysxScene::actor_count(std::uint16_t types) {
  struct CountArgs {
    void* scene;
    std::uint16_t types;
    std::uint32_t count;
  } args{scene_, types, 0};
  Fault f;
  if (!guarded_call(
          [](void* c) {
            auto* a = static_cast<CountArgs*>(c);
            a->count = vtable_slot<physx::GetNbActorsFn>(a->scene, layout::kPxSceneGetNbActors)(a->scene, &a->types);
          },
          &args, &f)) {
    fault("actor count", f.code, f.address);
    return std::nullopt;
  }
  return args.count;
}

std::optional<GroundHit> PhysxScene::ground_below(Vec3 from, double max_distance) {
  if (!world()) return std::nullopt;
  if (!scene_alive()) {
    scene_lost_ = true;
    return std::nullopt;
  }
  last_ground_.reset();
  const auto b = raycast(world_to_scene(from, *offset_), {0, -1, 0}, max_distance, physx::kQueryStatic | physx::kQueryDynamic,
                         "ground raycast");
  if (!b) return std::nullopt;
  last_ground_ = GroundHit{b->position.y, {b->normal.x, b->normal.y, b->normal.z}, b->distance};
  return last_ground_;
}

std::optional<SweepHit> PhysxScene::sweep_capsule(const Capsule& capsule, Vec3 center, Vec3 direction, double distance) {
  if (!world()) return std::nullopt;
  if (!scene_alive()) {
    scene_lost_ = true;
    return std::nullopt;
  }
  last_sweep_.reset();
  const auto b = sweep(capsule, world_to_scene(center, *offset_), direction, distance, physx::kQueryStatic | physx::kQueryDynamic,
                       "capsule sweep");
  if (!b) return std::nullopt;
  SweepHit hit;
  hit.distance = b->distance;
  hit.normal = {b->normal.x, b->normal.y, b->normal.z};
  hit.initial_overlap = b->distance <= kOverlapDistance;
  hit.depth = hit.initial_overlap ? std::max(0.0, -static_cast<double>(b->distance)) : 0.0;
  last_sweep_ = hit;
  return hit;
}

std::vector<std::string> PhysxScene::probe(Vec3 feet, Vec3 truck) {
  std::vector<std::string> lines;
  if (!world() || !scene_alive()) return lines;
  const auto statics = actor_count(physx::kQueryStatic);  // bit 1 of PxActorTypeFlag: eRIGID_STATIC
  const auto dynamics = actor_count(physx::kActorRigidDynamic);
  lines.push_back(strf("physx probe: walker (%.1f, %.2f, %.1f), %.1f m from the truck; static actors %d, dynamic actors %d", feet.x, feet.y,
                       feet.z, horizontal_distance(feet, truck), statics ? static_cast<int>(*statics) : -1,
                       dynamics ? static_cast<int>(*dynamics) : -1));
  const char* names[8] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
  const Vec3 waist = world_to_scene({feet.x, feet.y + 1.0, feet.z}, *offset_);
  const Capsule capsule{0.40, 0.4};
  const Vec3 center = world_to_scene({feet.x, feet.y + 1.25, feet.z}, *offset_);
  const auto row = [&](const char* label, bool use_sweep, std::uint16_t flags) {
    std::string text = label;
    for (int k = 0; k < 8; ++k) {
      const double a = k * 0.78539816339744830962;  // clockwise from north (-Z)
      const Vec3 dir{std::sin(a), 0, -std::cos(a)};
      const auto hit = use_sweep ? sweep(capsule, center, dir, 8.0, flags, "probe sweep") : raycast(waist, dir, 8.0, flags, "probe ray");
      if (!hit) text += strf(" %s -", names[k]);
      else if (use_sweep && hit->distance <= kOverlapDistance) text += strf(" %s o", names[k]);
      else text += strf(" %s %.2f", names[k], hit->distance);
    }
    lines.push_back(text);
  };
  row("physx probe rays static:", false, physx::kQueryStatic);
  row("physx probe rays all:   ", false, physx::kQueryStatic | physx::kQueryDynamic);
  row("physx probe sweeps static:", true, physx::kQueryStatic);
  return lines;
}

std::string PhysxScene::status() const { return state_name(static_cast<int>(state_)); }

std::vector<std::string> PhysxScene::describe() const {
  std::vector<std::string> lines;
  const std::string offset =
      offset_ ? strf("(%.0f, %.0f) residual %.2f m", offset_->x, offset_->z, offset_->residual) : std::string("none");
  lines.push_back(strf("physx: %s, scene %p, offset %s, faults %d", status().c_str(), scene_, offset.c_str(), faults_));
  if (last_ground_)
    lines.push_back(strf("physx: last ground %.2f, normal (%.2f, %.2f, %.2f)", last_ground_->height, last_ground_->normal.x,
                         last_ground_->normal.y, last_ground_->normal.z));
  if (last_sweep_) {
    const std::string overlap = last_sweep_->initial_overlap ? strf(", overlap %.3f m", last_sweep_->depth) : std::string();
    lines.push_back(strf("physx: last sweep hit %.3f m, normal (%.2f, %.2f, %.2f)%s", last_sweep_->distance, last_sweep_->normal.x,
                         last_sweep_->normal.y, last_sweep_->normal.z, overlap.c_str()));
  }
  return lines;
}

}  // namespace fw

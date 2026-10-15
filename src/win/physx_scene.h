#pragma once
#include <atomic>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>
#include "core/object_check.h"
#include "core/scene_offset.h"
#include "core/vec.h"
#include "core/world.h"
#include "win/physx_types.h"
#include "win/process_memory.h"

namespace fw {

// The game's PhysX scene: finding the instance on a background thread, checking candidates, updating the origin shift,
// and the World interface with the scene queries. Everything outside the background thread runs on the game thread (frame_end).
class PhysxScene final : public World {
 public:
  PhysxScene() = default;
  PhysxScene(const PhysxScene&) = delete;
  PhysxScene& operator=(const PhysxScene&) = delete;
  ~PhysxScene() override { stop(); }

  // Finds the scene vtable in the game file (at load time). False if not found: physics is disabled for this session.
  bool init(std::vector<std::string>& log);

  // Per frame: on a new world (world_key changes; 0 = no world) restarts the search, checks the candidates,
  // updates the shift every 250 ms, and passes the background thread and query log lines to log.
  void update(std::uint64_t world_key, Vec3 truck_position, bool truck_valid, double dt, std::vector<std::string>& log);

  // The walker's world if the scene is ready and there is a valid shift; otherwise nullptr.
  World* world();

  // Short status for the log ("ready", "searching", ...).
  std::string status() const;
  // Diagnostic lines (discovery_log).
  std::vector<std::string> describe() const;
  // Diagnostic probe around the walker (discovery_log, while walking): actor count, horizontal rays and sweeps in 8 directions.
  std::vector<std::string> probe(Vec3 feet, Vec3 truck);

  // Stops the background thread (on plugin unload and on a new world).
  void stop();

  std::optional<GroundHit> ground_below(Vec3 from, double max_distance) override;
  std::optional<SweepHit> sweep_capsule(const Capsule& capsule, Vec3 center, Vec3 direction, double distance) override;

 private:
  enum class State { Off, Idle, Searching, Validating, Retrying, Ready, Disabled };

  void start_search();
  void search_thread();
  void validate(Vec3 truck_position, std::vector<std::string>& log);
  void refresh_offset(Vec3 truck_position, std::vector<std::string>& log);
  std::optional<std::vector<ActorPosition>> dynamic_actors(void* scene, bool count_faults);
  std::optional<Vec3> actor_position(void* actor, bool count_faults);
  bool scene_alive() const;
  std::optional<std::uint64_t> actor_vtable(std::uint64_t actor) const;
  void fault(const char* what, unsigned long code, std::uint64_t address);
  // A ray and a vertical capsule sweep in scene coordinates; the blocking hit, if any.
  std::optional<physx::LocationHit> raycast(Vec3 origin, Vec3 direction, double distance, std::uint16_t query_flags, const char* what);
  std::optional<physx::LocationHit> sweep(const Capsule& capsule, Vec3 center, Vec3 direction, double distance, std::uint16_t query_flags,
                                          const char* what);
  std::optional<std::uint32_t> actor_count(std::uint16_t types);

  State state_ = State::Off;
  std::uint64_t module_base_ = 0;
  ProcessMemory memory_;
  CodeRanges ranges_;
  std::uint64_t world_key_ = 0;
  void* scene_ = nullptr;
  std::optional<SceneOffset> offset_;
  double offset_timer_ = 0;
  int faults_ = 0;
  std::uint64_t truck_actor_vtable_ = 0;  // the truck actor's vtable at acceptance: must be the same before rereading
  int validation_rounds_ = 0;
  double retry_timer_ = 0;
  bool scene_lost_ = false;  // the scene object is no longer the scene (freed): the next update searches again
  std::vector<std::string> pending_log_;  // log lines of the queries, passed on by the next update
  std::optional<GroundHit> last_ground_;
  std::optional<SweepHit> last_sweep_;

  std::thread thread_;
  std::atomic<bool> stop_requested_{false};
  mutable std::mutex mutex_;  // the background thread's results and log lines
  bool search_done_ = false;
  std::vector<std::uint64_t> candidates_;
  std::vector<std::string> thread_log_;
};

}  // namespace fw

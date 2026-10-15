#pragma once
#include <cstddef>
#include <cstdint>

// Data structures needed to call the PhysX 3.4 linked into the game, following the public header layout, written from scratch.
// Contains only what FreeWalk uses; the layout is pinned by static_asserts (docs/game-notes/ets2-1.61.md, "PhysX scene").
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

// PxCapsuleGeometry: the capsule axis is its own X axis.
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

// PxQueryFilterData: four filter words, the query flags and the client index. The scene indexes an array with the client index
// (vtable slot 0x22, getClientBehaviorFlags): if not 0, the query gets an access violation (live test, 0.5.0).
struct QueryFilterData {
  std::uint32_t word[4] = {};
  std::uint16_t flags = 0;
  std::uint8_t client_id = 0;
  std::uint8_t padding = 0;
};
static_assert(offsetof(QueryFilterData, client_id) == 18);
static_assert(sizeof(QueryFilterData) == 20);

// PxRaycastHit and PxSweepHit: PxQueryHit (actor, shape, face index) aligned to 8 bytes, then the PxLocationHit fields;
// u, v at the end for raycast, padding for sweep - the two layouts are identical.
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

// PxHitBuffer<T>: PhysX calls its virtual functions (processTouches, finalizeQuery, destructor - the order was confirmed by the RTTI
// of the TM Real Walk DLL built with PhysX 3.4), so the declaration order and the member layout matter.
// We request no touching hits (touches = nullptr), only the blocking one.
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

// Calls follow the x64 member function convention: this is the first argument; PxFlags and structs passed by reference as pointers,
// the returned PxTransform goes to the location passed as the second argument.
using GetNbActorsFn = std::uint32_t (*)(void* scene, const std::uint16_t* types);
using GetActorsFn = std::uint32_t (*)(void* scene, const std::uint16_t* types, void** buffer, std::uint32_t size, std::uint32_t start);
using RaycastFn = bool (*)(void* scene, const Vec3f* origin, const Vec3f* direction, float distance, HitBuffer* hit,
                           const std::uint16_t* hit_flags, const QueryFilterData* filter, void* filter_call, void* cache);
using SweepFn = bool (*)(void* scene, const CapsuleGeometry* geometry, const Transform* pose, const Vec3f* direction, float distance,
                         HitBuffer* hit, const std::uint16_t* hit_flags, const QueryFilterData* filter, void* filter_call, void* cache,
                         float inflation);
using GetGlobalPoseFn = Transform* (*)(void* actor, Transform* out);

}  // namespace fw::physx

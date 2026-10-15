#pragma once
#include <cstddef>
#include <cstdint>
#include <string_view>

// Facts about the game's internal layout, in one place.
// The values were verified with Ghidra on the ETS2 1.61.1.1s executable; the detailed notes and the addresses of the proving functions:
// docs/game-notes/ets2-1.61.md.
// Whatever only the running game can decide is marked separately ("live test").
namespace fw::layout {

// PE timestamp and image size of the verified build (ETS2 1.61.1.1s). The plugin calls into and writes to the game only on this build;
// after an update the probe re-checks the values, and these must be updated.
inline constexpr std::uint32_t kVerifiedTimestamp = 0x6AB3C451;
inline constexpr std::uint32_t kVerifiedImageSize = 0x398A000;
inline bool is_verified_build(std::uint32_t timestamp, std::uint32_t image_size) {
  return timestamp == kVerifiedTimestamp && image_size == kVerifiedImageSize;
}

// The game's own type system: entry 5 of the vtable returns an object's type name in a "lea rax, [cell]; ret" function;
// the cell points to a type entry whose first field points to the type name.
inline constexpr std::size_t kTypeGetterVtableIndex = 5;

// The camera manager: its type name, and how far from the vtable load the instruction that stores the instance in the global can be.
inline constexpr std::string_view kCameraManagerName = "camera_manager";
inline constexpr std::size_t kManagerStoreWindow = 0x80;

// Camera manager fields (the factory function initially sets all three to the camera count, 14).
inline constexpr std::uint64_t kManagerCurrentIndex = 0x10;    // active camera index (int32); active vs. requested: live test
inline constexpr std::uint64_t kManagerRequestedIndex = 0x14;  // index of the requested camera (int32)
inline constexpr std::uint64_t kManagerSlots = 0x38;           // data pointer of the camera object array
inline constexpr std::uint64_t kManagerSlotCount = 0x40;       // array length (uint64)
inline constexpr std::uint64_t kMaxCameraSlots = 32;

// Type name and fields of the free camera (per the constructor of the cameras' common base class).
inline constexpr std::string_view kFreeCameraClass = "debug_camera";
inline constexpr std::uint64_t kCameraYaw = 0x10;              // yaw (float); the unit and the convention: live test
inline constexpr std::uint64_t kCameraPitch = 0x14;            // pitch (float)
inline constexpr std::uint64_t kCameraRoll = 0x18;             // roll (float)
inline constexpr std::uint64_t kCameraLookScale = 0x1C;        // mouse look multiplier (float, default 1)
inline constexpr std::uint64_t kCameraLocalPosition = 0x40;    // local position, 3 floats, followed by the two int16 chunk indices
inline constexpr std::uint64_t kCameraRotation = 0x50;         // quaternion (w, x, y, z float); whether derived from the angles: live test

// Console variables (cvars): the game copies both the name and the text value into the object; the setter is vtable entry 1.
inline constexpr std::string_view kDeveloperCvar = "g_developer";
inline constexpr std::string_view kFreeCameraSpeedCvar = "g_flyspeed";  // read by the free camera's update function (vtable 30)
inline constexpr std::string_view kAdviserHiddenCvar = "g_adviser_keep_hidden";  // 1 = the route adviser (speedometer, map) is hidden
inline constexpr std::string_view kLanguageCvar = "g_lang";  // the game's language, e.g. "hu_hu"
inline constexpr std::string_view kHudMirrorsHiddenCvar = "g_mirrors_keep_hidden";  // 1 = HUD mirrors hidden; in the free camera they
                                                                                    // would show an invalid image
inline constexpr std::size_t kCvarLeaWindow = 0x40;
inline constexpr std::uint64_t kCvarName = 0x08;               // the name, at most 0x20 bytes
inline constexpr std::size_t kCvarNameSize = 0x20;
inline constexpr std::uint64_t kCvarValue = 0xB1;              // the current value as text, at most 0x65 bytes
inline constexpr std::size_t kCvarValueSize = 0x65;
inline constexpr std::size_t kCvarSetterVtableIndex = 1;       // bool set(this, const char* value)

// Conversion between the camera yaw and the telemetry heading (radians, 0 = north, counter-clockwise).
// Assumption: the two are the same; checked by the live test of task 9.
inline double heading_to_camera_yaw(double heading) { return heading; }
inline double camera_yaw_to_heading(double yaw) { return yaw; }

// The scene (NpScene) of PhysX 3.4, statically linked into the game. The release and shiftOrigin functions reference these error messages;
// the scene vtable is where release sits at kPhysxReleaseSlot and shiftOrigin at kPhysxShiftOriginSlot.
// Entry 0 of the vtable is the destructor: the constructor (RVA 0x1ADE073 and 0x1ADE797) writes the address of the slot before release
// into the object (docs/game-notes/ets2-1.61.md, PhysX scene section).
inline constexpr std::string_view kPhysxReleaseMessage =
    "PxScene::release(): Scene is still being simulated! PxScene::fetchResults() is called implicitly.";
inline constexpr std::string_view kPhysxShiftOriginMessage =
    "PxScene::shiftOrigin() not allowed while simulation is running. Call will be ignored.";
inline constexpr std::size_t kPhysxReleaseSlot = 1;
inline constexpr std::size_t kPhysxShiftOriginSlot = 0x74;

// PhysX 3.4 calls as vtable indices (byte offset / 8), from the start of the scene and the dynamic actor vtables.
// The scene slots were verified on the game file (docs/game-notes/ets2-1.61.md, PhysX scene section); getGlobalPose: live test.
inline constexpr std::size_t kPxSceneGetNbActors = 0x90 / 8;    // PxU32 getNbActors(PxActorTypeFlags) const
inline constexpr std::size_t kPxSceneGetActors = 0x98 / 8;      // PxU32 getActors(PxActorTypeFlags, PxActor**, PxU32, PxU32) const
inline constexpr std::size_t kPxSceneRaycast = 0x2F0 / 8;       // bool raycast(origin, unitDir, distance, hitCall, hitFlags, filterData, filterCall, cache) const
inline constexpr std::size_t kPxSceneSweep = 0x2F8 / 8;         // bool sweep(geometry, pose, unitDir, distance, hitCall, hitFlags, filterData, filterCall, cache, inflation) const
inline constexpr std::size_t kPxActorGetGlobalPose = 0xA0 / 8;  // PxTransform getGlobalPose() const

// Loading the map's collision geometry: the game loads it into physics around vehicles with a region query; we request the same
// around the walker. RVAs valid only for the verified build (kVerifiedTimestamp); source: the TM Real Walk signatures on the game file
// with a single match each, function starts per .pdata, the scene global from the instruction at RVA 0x6E322D in gate_model_item
// (mov r8, [scene]) - docs/game-notes/ets2-1.61.md, region query section.
inline constexpr std::uint64_t kSceneGlobal = 0x36AE6D8;  // pointer to the scene object (it also holds the map item list: +0x18, +0x20)
inline constexpr std::uint64_t kBoxBuilder = 0x46D4C0;    // void* build(void*, box* out, const position*, const float size[3])
inline constexpr std::uint64_t kRegionQuery = 0x489E10;   // void query(scene*, const box*, bool)
inline constexpr std::size_t kRegionBoxSize = 0x28;       // number of bytes box_builder writes

// The game's lights (based on TM Real Walk; confirmed by the 0.6.1 live test, docs/game-notes/ets2-1.61.md, lights section).
inline constexpr std::string_view kLightClass = "light_source";
inline constexpr std::uint64_t kSceneLightArray = 0x3278;    // in the scene object: the array of light pointers (fills up after loading)
inline constexpr std::uint64_t kSceneLightCount = 0x3280;    // ... and the count
inline constexpr std::uint64_t kLightColour = 0x124;         // 6 floats (the same RGB twice); 0 for a switched-off lamp
inline constexpr std::uint64_t kLightIntensity = 0x108;      // float (1.0 or 0.4 for a street lamp)
inline constexpr std::uint64_t kLightRange = 0x168;          // float, m (about 30 for a street lamp)
inline constexpr std::uint64_t kLightInnerAngle = 0x174;     // float, degrees, the full inner cone (1 for a street lamp)
inline constexpr std::uint64_t kLightOuterAngle = 0x178;     // float, degrees, the full outer cone (90 or 130 for a street lamp)
inline constexpr std::uint64_t kLightSize = 0x1A8;           // two floats (+0x1A8, +0x1AC), proportional to the range (about 0.37 times)
inline constexpr std::uint64_t kLightTransform = 0x1B0;      // position (16 bytes, with the chunk index), rotation (w, x, y, z)
                                                             // at +0x1C0; the light points along -Z
inline constexpr std::uint64_t kLightTransformCopy = 0x1D0;  // the same again (TM writes both)
// The light_source class descriptor (.rdata): its first qword points to the .data entry holding the type name, +8 is the creator, +0x30 the
// object size. The creator (RVA 0x6EAF80) allocates with the game's allocator, then jumps to the constructor (RVA 0x6EB290,
// rcx = the object; the second argument is unused); after the base constructor, the constructor writes the light_source vtable and
// zeroes the colour (+0x120..+0x138), the range (+0x168) and the position-rotation (from +0x1B0) - per the game file.
inline constexpr std::uint64_t kLightClassDescriptor = 0x1E17F90;
inline constexpr std::uint64_t kLightConstructor = 0x6EB290;
inline constexpr std::uint64_t kLightObjectSize = 0x230;
inline constexpr std::uint64_t kLightCreator = 0x6EAF80;       // void* create(void* unused): game-allocator alloc, then the constructor
inline constexpr std::size_t kLightUpdateSlot = 0x88 / 8;      // light update (vtable entry 17): moves the +0x108 intensity at the +0x104
                                                               // speed towards whether the light should be lit (vtable +0xE0)
inline constexpr std::uint64_t kLightFlags = 0x08;             // for our own light: (flags & 0xFE001000) | 0x1000 ("always lit" per TM)
inline constexpr std::uint64_t kLightFadeSpeed = 0x104;        // float, 0.05 for our own light
inline constexpr std::uint64_t kLightOwnEnable = 0x198;        // byte, 1 for our own light
inline constexpr std::string_view kLightDistanceCvar = "g_light_distance_factor";
inline constexpr std::string_view kLightSpanCvar = "g_light_span_factor";

}  // namespace fw::layout

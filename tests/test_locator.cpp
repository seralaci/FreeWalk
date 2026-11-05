#include <cstring>
#include <optional>
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

// An entry of the game's type system: the name in .rdata, a type entry in .data points to it, an .rdata cell points to that,
// the cell is returned by a "lea rax, [cell]; ret" function, and that function is the 5th entry of the type's vtable.
struct TypeAt {
  std::uint64_t name, entry, cell, getter, vtable;
};

void put_type(fw::BufferMemory& m, const char* type_name, const TypeAt& at) {
  m.put_bytes(at.name, type_name, std::strlen(type_name) + 1);
  m.put<std::uint64_t>(at.entry, at.name);
  m.put<std::uint64_t>(at.cell, at.entry);
  fwtest::put_rip_insn(m, at.getter, 0x48, 0x8D, 0, at.cell);
  m.put<std::uint8_t>(at.getter + 7, 0xC3);
  m.put<std::uint64_t>(at.vtable + 8 * fw::layout::kTypeGetterVtableIndex, at.getter);
}

const TypeAt kManagerType{kRdata + 0x40, kData + 0x100, kRdata + 0x80, kText + 0x100, kRdata + 0x200};
const TypeAt kFreeCameraType{kRdata + 0x60, kData + 0x110, kRdata + 0x88, kText + 0x110, kRdata + 0x300};

// Factory function of the camera manager: stores the instance in a .data global and loads the camera manager vtable nearby.
fw::BufferMemory image_with_camera_manager(int stores) {
  auto m = fwtest::make_test_image();
  put_type(m, "camera_manager", kManagerType);
  const std::uint64_t factory = kText + 0x400;
  fwtest::put_rip_insn(m, factory, 0x48, 0x89, 0, kData + 0x800);               // mov [global], rax
  fwtest::put_rip_insn(m, factory + 0x10, 0x48, 0x8D, 0, kManagerType.vtable);  // lea rax, [vtable]
  if (stores > 1) fwtest::put_rip_insn(m, factory + 0x20, 0x48, 0x89, 1, kData + 0x808);
  return m;
}

}  // namespace

TEST(finds_type_vtable_through_type_registry) {
  auto m = fwtest::make_test_image();
  put_type(m, "debug_camera", kFreeCameraType);
  const auto img = fw::PeImage::parse(m, fwtest::kBase);
  std::vector<std::string> log;
  const auto vtable = fw::find_type_vtable(m, *img, "debug_camera", log);
  CHECK(vtable && *vtable == kFreeCameraType.vtable);
  CHECK(!fw::find_type_vtable(m, *img, "interior_camera", log).has_value());
}

TEST(type_getter_must_end_with_ret) {
  auto m = fwtest::make_test_image();
  put_type(m, "debug_camera", kFreeCameraType);
  m.put<std::uint8_t>(kFreeCameraType.getter + 7, 0x90);
  const auto img = fw::PeImage::parse(m, fwtest::kBase);
  std::vector<std::string> log;
  CHECK(!fw::find_type_vtable(m, *img, "debug_camera", log).has_value());
}

TEST(finds_camera_manager_global) {
  const auto m = image_with_camera_manager(1);
  const auto img = fw::PeImage::parse(m, fwtest::kBase);
  std::vector<std::string> log;
  const auto global = fw::find_camera_manager_global(m, *img, log);
  CHECK(global && *global == kData + 0x800);
  CHECK(!log.empty());
}

TEST(ambiguous_manager_store_is_rejected) {
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
  const std::uint64_t buffer = cvar + fw::layout::kCvarName;
  // lea rax, [name buffer]; mov edx, 0x20; lea r8, [name text] - this is how the game copies the name into the cvar object.
  fwtest::put_rip_insn(m, kText + 0x300, 0x48, 0x8D, 0, buffer);
  const std::uint8_t mov_edx[5] = {0xBA, 0x20, 0, 0, 0};
  m.put_bytes(kText + 0x307, mov_edx, 5);
  fwtest::put_rip_insn(m, kText + 0x30C, 0x4C, 0x8D, 0, kRdata + 0x80);
  const auto img = fw::PeImage::parse(m, fwtest::kBase);

  std::vector<std::string> log;
  const auto unverified = fw::find_cvar(m, *img, "g_developer", false, log);
  CHECK(unverified && *unverified == cvar);

  CHECK(!fw::find_cvar(m, *img, "g_developer", true, log).has_value());
  m.put_bytes(buffer, name, sizeof name);
  const auto verified = fw::find_cvar(m, *img, "g_developer", true, log);
  CHECK(verified && *verified == cvar);
}

TEST(lists_cameras_and_finds_free_camera_by_vtable) {
  auto m = fwtest::make_test_image();
  const std::uint64_t manager = kHeap;
  const std::uint64_t slots = kHeap + 0x100;
  m.put<std::uint64_t>(manager + fw::layout::kManagerSlots, slots);
  m.put<std::uint64_t>(manager + fw::layout::kManagerSlotCount, 3);
  m.put<std::uint64_t>(kHeap + 0x400, kRdata + 0x500);
  m.put<std::uint64_t>(kHeap + 0x500, kFreeCameraType.vtable);
  m.put<std::uint64_t>(slots, kHeap + 0x400);
  m.put<std::uint64_t>(slots + 8, 0);
  m.put<std::uint64_t>(slots + 16, kHeap + 0x500);

  std::vector<std::string> log;
  const auto cameras = fw::list_cameras(m, manager, log);
  CHECK(cameras.size() == 3);
  CHECK(cameras[0].vtable == kRdata + 0x500);
  CHECK(cameras[1].object == 0 && cameras[1].vtable == 0);
  const auto free = fw::find_camera(cameras, kFreeCameraType.vtable);
  CHECK(free && free->index == 2 && free->object == kHeap + 0x500);
  CHECK(!fw::find_camera(cameras, kRdata + 0x999).has_value());
}

TEST(implausible_camera_count_gives_empty_list) {
  auto m = fwtest::make_test_image();
  m.put<std::uint64_t>(kHeap + fw::layout::kManagerSlots, kHeap + 0x100);
  m.put<std::uint64_t>(kHeap + fw::layout::kManagerSlotCount, 5000);
  std::vector<std::string> log;
  CHECK(fw::list_cameras(m, kHeap, log).empty());
}

// Code review (point 6): calls into the game's code and writes are only allowed on the verified build.
TEST(only_the_verified_build_is_accepted) {
  CHECK(fw::layout::is_verified_build(0x6AB3C451u, 0x398A000u));
  CHECK(!fw::layout::is_verified_build(0x6AB3C452u, 0x398A000u));
  CHECK(!fw::layout::is_verified_build(0x6AB3C451u, 0x398B000u));
}

TEST(each_verified_build_has_its_own_addresses) {
  const fw::layout::GameBuild* ets2 = fw::layout::find_build(0x6AB3C451u, 0x398A000u);
  const fw::layout::GameBuild* ats = fw::layout::find_build(0x6ABDFBF2u, 0x3973000u);
  CHECK(ets2 && ats && ets2 != ats);
  CHECK(ets2->scene_global == 0x36AE6D8u);
  CHECK(ats->scene_global == 0x36B8D70u);
  CHECK(ats->region_query == 0x48B2B0u);
  CHECK(ats->light_creator == 0x6EC520u);
  CHECK(fw::layout::is_verified_build(0x6ABDFBF2u, 0x3973000u));
  CHECK(!fw::layout::find_build(0x6ABDFBF2u, 0x398A000u));
  // The 2026-10-15 updates.
  const fw::layout::GameBuild* ets2_9 = fw::layout::find_build(0x6ACDF049u, 0x398B000u);
  const fw::layout::GameBuild* ats_18 = fw::layout::find_build(0x6ACDECACu, 0x3975000u);
  CHECK(ets2_9 && ets2_9->scene_global == 0x36AF7C8u && ets2_9->light_creator == 0x6EB390u);
  CHECK(ats_18 && ats_18->scene_global == 0x36BAD90u && ats_18->region_query == 0x48B290u);
  CHECK(ets2_9->driver_object == 0x1DF0u && ats_18->driver_object == 0x1DF0u);
}

TEST(ets2_1_62_has_its_addresses_and_the_moved_driver) {
  const fw::layout::GameBuild* ets2 = fw::layout::find_build(0x6AEB1934u, 0x39F8000u);
  CHECK(ets2 && ets2->scene_global == 0x3714F30u && ets2->box_builder == 0x478400u && ets2->region_query == 0x494D30u);
  CHECK(ets2 && ets2->light_class_descriptor == 0x1E4D290u && ets2->light_constructor == 0x6D47D0u && ets2->light_creator == 0x6D44C0u);
  CHECK(ets2 && ets2->driver_object == 0x1DF8u);
}

TEST(ats_1_62_has_its_addresses_and_the_moved_driver) {
  const fw::layout::GameBuild* ats = fw::layout::find_build(0x6AE990FBu, 0x39DB000u);
  CHECK(ats && ats->scene_global == 0x3719CA8u && ats->box_builder == 0x478040u && ats->region_query == 0x4949E0u);
  CHECK(ats && ats->light_class_descriptor == 0x1E4D3D0u && ats->light_constructor == 0x6D4460u && ats->light_creator == 0x6D4150u);
  // In 1.62 the driver object in the vehicle moved by 8 bytes (the game calls the driver draw with +0x1DF8).
  CHECK(ats && ats->driver_object == 0x1DF8u);
}

namespace {

// Pattern of a PhysX scene in the test image: the two messages in .rdata, a lea referencing them in each of the two functions,
// the two functions in .pdata, and the vtable with release and shiftOrigin at the expected slots.
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
  fwtest::put_rip_insn(m, f.release + 0x10, 0x48, 0x8D, 2, f.release_text);  // lea rdx, [message]
  fwtest::put_rip_insn(m, f.shift + 0x20, 0x4C, 0x8D, 8, f.shift_text);      // lea r8, [message]
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

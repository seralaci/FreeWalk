#include <vector>
#include "check.h"
#include "core/object_check.h"
#include "pe_builder.h"

namespace {

// Ranges of the test image: the vtables in .rdata, the functions in .text.
const fw::CodeRanges kRanges{fwtest::kRdata, fwtest::kRdata + 0x1000, fwtest::kText, fwtest::kText + 0x1000};

// An object on the heap whose vtable is in .rdata and whose two slots point into .text.
fw::BufferMemory object_image() {
  auto m = fwtest::make_test_image();
  m.put<std::uint64_t>(fwtest::kHeap, fwtest::kRdata + 0x100);
  m.put<std::uint64_t>(fwtest::kRdata + 0x100 + 8 * 2, fwtest::kText + 0x40);
  m.put<std::uint64_t>(fwtest::kRdata + 0x100 + 8 * 5, fwtest::kText + 0x80);
  return m;
}

}  // namespace

TEST(checked_vtable_accepts_a_live_object) {
  const auto m = object_image();
  CHECK(fw::checked_vtable(m, fwtest::kHeap, kRanges, {2, 5}) == fwtest::kRdata + 0x100);
}

TEST(checked_vtable_rejects_a_vtable_outside_the_constants) {
  auto m = object_image();
  m.put<std::uint64_t>(fwtest::kHeap, fwtest::kHeap + 0x200);
  CHECK(!fw::checked_vtable(m, fwtest::kHeap, kRanges, {2, 5}));
}

TEST(checked_vtable_rejects_a_slot_outside_the_code) {
  auto m = object_image();
  m.put<std::uint64_t>(fwtest::kRdata + 0x100 + 8 * 5, fwtest::kHeap);
  CHECK(!fw::checked_vtable(m, fwtest::kHeap, kRanges, {2, 5}));
}

TEST(checked_vtable_rejects_unreadable_objects) {
  const auto m = object_image();
  CHECK(!fw::checked_vtable(m, 0x10, kRanges, {2}));
  CHECK(!fw::checked_vtable(m, 0, kRanges, {2}));
}

TEST(code_ranges_come_from_the_sections) {
  const auto m = fwtest::make_test_image();
  const auto img = fw::PeImage::parse(m, fwtest::kBase);
  CHECK(img.has_value());
  if (!img) return;
  const auto r = fw::code_ranges(*img);
  CHECK(r.has_value());
  if (!r) return;
  CHECK(r->vtables_begin == fwtest::kRdata && r->vtables_end == fwtest::kRdata + 0x1000);
  CHECK(r->code_begin == fwtest::kText && r->code_end == fwtest::kText + 0x1000);
}

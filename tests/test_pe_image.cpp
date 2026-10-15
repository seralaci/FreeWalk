#include <cstring>
#include "check.h"
#include "core/pe_image.h"
#include "pe_builder.h"

TEST(parses_headers_and_sections) {
  const auto m = fwtest::make_test_image();
  const auto img = fw::PeImage::parse(m, fwtest::kBase);
  CHECK(img.has_value());
  CHECK(img->timestamp() == fwtest::kTimestamp);
  CHECK(img->image_size() == fwtest::kImageSize);
  CHECK(img->preferred_base() == fwtest::kBase);
  CHECK(img->sections().size() == 3);
  CHECK(img->section(".rdata") != nullptr);
  CHECK(img->section(".rdata")->rva == 0x2000);
  CHECK(img->in_section(".text", fwtest::kText + 0x10));
  CHECK(!img->in_section(".text", fwtest::kRdata));
  CHECK(!img->in_section(".data", fwtest::kHeap));
}

TEST(rejects_non_pe_memory) {
  fw::BufferMemory m(0x1000, std::vector<std::uint8_t>(0x200, 0));
  CHECK(!fw::PeImage::parse(m, 0x1000).has_value());
}

TEST(find_cstring_requires_exact_terminated_string) {
  auto m = fwtest::make_test_image();
  const char longer[] = "my_camera_manager";
  const char exact[] = "camera_manager";
  m.put_bytes(fwtest::kRdata + 0x10, longer, sizeof longer);
  m.put_bytes(fwtest::kRdata + 0x40, exact, sizeof exact);
  const auto img = fw::PeImage::parse(m, fwtest::kBase);
  const auto rdata = img->read_section(m, *img->section(".rdata"));
  const auto found = fw::find_cstring(rdata, "camera_manager");
  CHECK(found.has_value());
  CHECK(*found == fwtest::kRdata + 0x40);
  CHECK(!fw::find_cstring(rdata, "debug_camera").has_value());
}

TEST(find_qword_finds_aligned_values) {
  auto m = fwtest::make_test_image();
  m.put<std::uint64_t>(fwtest::kData + 0x18, 0x1122334455667788ull);
  m.put<std::uint64_t>(fwtest::kData + 0x80, 0x1122334455667788ull);
  const auto img = fw::PeImage::parse(m, fwtest::kBase);
  const auto data = img->read_section(m, *img->section(".data"));
  const auto hits = fw::find_qword(data, 0x1122334455667788ull);
  CHECK(hits.size() == 2);
  CHECK(hits[0] == fwtest::kData + 0x18);
}

TEST(decode_rip_ref_variants) {
  auto m = fwtest::make_test_image();
  fwtest::put_rip_insn(m, fwtest::kText, 0x48, 0x89, 0, fwtest::kData + 0x100);       // mov [rip+x], rax
  fwtest::put_rip_insn(m, fwtest::kText + 0x10, 0x4C, 0x8D, 0, fwtest::kRdata);       // lea r8, [rip+x]
  fwtest::put_rip_insn(m, fwtest::kText + 0x20, 0x48, 0x8B, 1, fwtest::kData);        // mov rcx, [rip+x]
  const std::uint8_t not_rip[7] = {0x48, 0x89, 0x45, 0x10, 0, 0, 0};                  // mov [rbp+0x10], rax
  m.put_bytes(fwtest::kText + 0x30, not_rip, 7);
  const auto img = fw::PeImage::parse(m, fwtest::kBase);
  const auto text = img->read_section(m, *img->section(".text"));

  const auto a = fw::decode_rip_ref(text.bytes.data(), text.bytes.size(), fwtest::kText);
  CHECK(a && a->opcode == 0x89 && a->reg == 0 && a->target == fwtest::kData + 0x100);
  const auto b = fw::decode_rip_ref(text.bytes.data() + 0x10, text.bytes.size() - 0x10, fwtest::kText + 0x10);
  CHECK(b && b->opcode == 0x8D && b->reg == 8 && b->target == fwtest::kRdata);
  const auto c = fw::decode_rip_ref(text.bytes.data() + 0x20, text.bytes.size() - 0x20, fwtest::kText + 0x20);
  CHECK(c && c->opcode == 0x8B && c->reg == 1);
  CHECK(!fw::decode_rip_ref(text.bytes.data() + 0x30, text.bytes.size() - 0x30, fwtest::kText + 0x30));
  CHECK(!fw::decode_rip_ref(text.bytes.data(), 6, fwtest::kText));

  const auto refs = fw::find_rip_refs_to(text, fwtest::kRdata);
  CHECK(refs.size() == 1);
  CHECK(refs[0].address == fwtest::kText + 0x10);
}

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

#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "core/memory.h"

namespace fw {

struct Section {
  std::string name;
  std::uint32_t rva = 0;
  std::uint32_t virtual_size = 0;
  std::uint32_t raw_offset = 0;
  std::uint32_t raw_size = 0;
};

// Copy of a section's bytes and their start address.
struct SectionBytes {
  std::uint64_t address = 0;
  std::vector<std::uint8_t> bytes;
};

// Headers and sections of a 64-bit PE image mapped into memory.
class PeImage {
 public:
  static std::optional<PeImage> parse(const Memory& memory, std::uint64_t base);

  std::uint64_t base() const { return base_; }
  std::uint64_t preferred_base() const { return preferred_base_; }
  std::uint32_t timestamp() const { return timestamp_; }
  std::uint32_t image_size() const { return image_size_; }
  std::uint32_t headers_size() const { return headers_size_; }
  const std::vector<Section>& sections() const { return sections_; }

  const Section* section(std::string_view name) const;
  bool in_section(std::string_view name, std::uint64_t address) const;
  SectionBytes read_section(const Memory& memory, const Section& section) const;

 private:
  std::uint64_t base_ = 0;
  std::uint64_t preferred_base_ = 0;
  std::uint32_t timestamp_ = 0;
  std::uint32_t image_size_ = 0;
  std::uint32_t headers_size_ = 0;
  std::vector<Section> sections_;
};

// Finds exactly this null-terminated string, preceded by a zero byte so it is not the end of a longer one; returns its address.
std::optional<std::uint64_t> find_cstring(const SectionBytes& data, std::string_view text);

// Addresses of 8-byte aligned places holding this 64-bit value.
std::vector<std::uint64_t> find_qword(const SectionBytes& data, std::uint64_t value);

// A REX.W-prefixed mov/lea instruction whose memory operand is relative to the instruction end (RIP-relative).
struct RipRef {
  std::uint64_t address = 0;  // address of the instruction
  std::uint8_t opcode = 0;    // 0x89 store, 0x8B load, 0x8D load address (lea)
  int reg = 0;                // register number (0 rax, 1 rcx, 2 rdx, 8 r8, ...)
  std::uint64_t target = 0;   // the referenced memory address
};

// Decodes the bytes at a place as a 48/4C + 89/8B/8D + modrm(00 reg 101) + disp32 instruction.
std::optional<RipRef> decode_rip_ref(const std::uint8_t* p, std::size_t available, std::uint64_t address);

// Tries decoding at every byte position and returns those whose target is the given address.
std::vector<RipRef> find_rip_refs_to(const SectionBytes& code, std::uint64_t target);

// From the .pdata function table (RUNTIME_FUNCTION: start and end RVA, unwind data), the start of the function containing the address.
// The linker sorts the table by start address; zero entries at the end of the table do not count.
std::optional<std::uint64_t> function_start(const SectionBytes& pdata, std::uint64_t image_base, std::uint64_t address);

}  // namespace fw

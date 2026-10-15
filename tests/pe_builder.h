#pragma once
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include "core/memory.h"

namespace fwtest {

inline constexpr std::uint64_t kBase = 0x140000000ull;
inline constexpr std::uint64_t kText = kBase + 0x1000;
inline constexpr std::uint64_t kRdata = kBase + 0x2000;
inline constexpr std::uint64_t kData = kBase + 0x3000;
inline constexpr std::uint64_t kHeap = kBase + 0x4000;  // area outside the sections, for the objects
inline constexpr std::uint32_t kImageSize = 0x6000;
inline constexpr std::uint32_t kTimestamp = 0x6AAA8369;

// A memory-mapped 64-bit PE image with three sections, modeled on the game executable.
inline fw::BufferMemory make_test_image() {
  fw::BufferMemory m(kBase, std::vector<std::uint8_t>(kImageSize, 0));
  m.put<std::uint16_t>(kBase, 0x5A4D);
  m.put<std::uint32_t>(kBase + 0x3C, 0x80);
  const std::uint64_t nt = kBase + 0x80;
  m.put<std::uint32_t>(nt, 0x00004550);
  m.put<std::uint16_t>(nt + 6, 3);
  m.put<std::uint32_t>(nt + 8, kTimestamp);
  m.put<std::uint16_t>(nt + 20, 0xF0);
  const std::uint64_t opt = nt + 24;
  m.put<std::uint16_t>(opt, 0x20B);
  m.put<std::uint64_t>(opt + 24, kBase);
  m.put<std::uint32_t>(opt + 56, kImageSize);
  m.put<std::uint32_t>(opt + 60, 0x400);
  const char* names[] = {".text", ".rdata", ".data"};
  for (int i = 0; i < 3; ++i) {
    const std::uint64_t s = opt + 0xF0 + 40 * i;
    char name[8] = {};
    std::memcpy(name, names[i], std::strlen(names[i]));
    m.put_bytes(s, name, 8);
    m.put<std::uint32_t>(s + 8, 0x1000);                  // VirtualSize
    m.put<std::uint32_t>(s + 12, 0x1000 * (i + 1));       // VirtualAddress
    m.put<std::uint32_t>(s + 16, 0x1000);                 // SizeOfRawData
    m.put<std::uint32_t>(s + 20, 0x400 + 0x1000 * i);     // PointerToRawData
  }
  return m;
}

// Bytes of a RIP-relative REX.W instruction: rex, opcode, modrm (reg field is the low 3 bits of reg), disp32.
inline void put_rip_insn(fw::BufferMemory& m, std::uint64_t at, std::uint8_t rex, std::uint8_t opcode, int reg, std::uint64_t target) {
  const std::uint8_t bytes[3] = {rex, opcode, static_cast<std::uint8_t>(0x05 | ((reg & 7) << 3))};
  m.put_bytes(at, bytes, 3);
  m.put<std::int32_t>(at + 3, static_cast<std::int32_t>(static_cast<std::int64_t>(target) - static_cast<std::int64_t>(at + 7)));
}

inline constexpr std::uint64_t kPdata = kBase + 0x5000;

// Adds a fourth, .pdata section to the test image at RVA 0x5000, for the function table.
inline void add_pdata_section(fw::BufferMemory& m) {
  const std::uint64_t nt = kBase + 0x80;
  m.put<std::uint16_t>(nt + 6, 4);
  const std::uint64_t s = nt + 24 + 0xF0 + 40 * 3;
  const char name[8] = {'.', 'p', 'd', 'a', 't', 'a', 0, 0};
  m.put_bytes(s, name, 8);
  m.put<std::uint32_t>(s + 8, 0x1000);
  m.put<std::uint32_t>(s + 12, 0x5000);
  m.put<std::uint32_t>(s + 16, 0x1000);
  m.put<std::uint32_t>(s + 20, 0x3400);
}

// A function in the .pdata table (RUNTIME_FUNCTION): start and end given as absolute addresses.
inline void put_function_entry(fw::BufferMemory& m, int index, std::uint64_t begin, std::uint64_t end) {
  const std::uint64_t e = kPdata + 12ull * static_cast<std::uint64_t>(index);
  m.put<std::uint32_t>(e, static_cast<std::uint32_t>(begin - kBase));
  m.put<std::uint32_t>(e + 4, static_cast<std::uint32_t>(end - kBase));
  m.put<std::uint32_t>(e + 8, 0);
}

}  // namespace fwtest

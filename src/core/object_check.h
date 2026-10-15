#pragma once
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include "core/memory.h"
#include "core/pe_image.h"

namespace fw {

// Two ranges of the game file: where a live object's vtable (.rdata) and its virtual functions (.text) are.
struct CodeRanges {
  std::uint64_t vtables_begin = 0;
  std::uint64_t vtables_end = 0;
  std::uint64_t code_begin = 0;
  std::uint64_t code_end = 0;
};

std::optional<CodeRanges> code_ranges(const PeImage& image);

// The object's vtable, if its first qword points into the vtable range and the given slots all point to code.
// Before calling through a game object this filters out freed or foreign memory, so the call cannot jump to an unknown place.
std::optional<std::uint64_t> checked_vtable(const Memory& memory, std::uint64_t object, const CodeRanges& ranges,
                                            std::initializer_list<std::size_t> slots);

}  // namespace fw

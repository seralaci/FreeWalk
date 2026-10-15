#include "core/object_check.h"

namespace fw {

std::optional<CodeRanges> code_ranges(const PeImage& image) {
  const Section* rdata = image.section(".rdata");
  const Section* text = image.section(".text");
  if (!rdata || !text) return std::nullopt;
  const auto size = [](const Section& s) { return static_cast<std::uint64_t>(s.virtual_size != 0 ? s.virtual_size : s.raw_size); };
  CodeRanges r;
  r.vtables_begin = image.base() + rdata->rva;
  r.vtables_end = r.vtables_begin + size(*rdata);
  r.code_begin = image.base() + text->rva;
  r.code_end = r.code_begin + size(*text);
  return r;
}

std::optional<std::uint64_t> checked_vtable(const Memory& memory, std::uint64_t object, const CodeRanges& ranges,
                                            std::initializer_list<std::size_t> slots) {
  if (object == 0) return std::nullopt;
  const auto vtable = memory.get<std::uint64_t>(object);
  if (!vtable || *vtable < ranges.vtables_begin || *vtable >= ranges.vtables_end) return std::nullopt;
  for (const std::size_t slot : slots) {
    const auto target = memory.get<std::uint64_t>(*vtable + 8 * slot);
    if (!target || *target < ranges.code_begin || *target >= ranges.code_end) return std::nullopt;
  }
  return vtable;
}

}  // namespace fw

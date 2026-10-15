#include "core/pe_image.h"
#include <algorithm>
#include <cstring>

namespace fw {

std::optional<PeImage> PeImage::parse(const Memory& m, std::uint64_t base) {
  const auto mz = m.get<std::uint16_t>(base);
  const auto lfanew = m.get<std::uint32_t>(base + 0x3C);
  if (!mz || *mz != 0x5A4D || !lfanew) return std::nullopt;
  const std::uint64_t nt = base + *lfanew;
  const auto signature = m.get<std::uint32_t>(nt);
  const auto count = m.get<std::uint16_t>(nt + 6);
  const auto timestamp = m.get<std::uint32_t>(nt + 8);
  const auto optional_size = m.get<std::uint16_t>(nt + 20);
  const std::uint64_t opt = nt + 24;
  const auto magic = m.get<std::uint16_t>(opt);
  const auto preferred = m.get<std::uint64_t>(opt + 24);
  const auto image_size = m.get<std::uint32_t>(opt + 56);
  const auto headers_size = m.get<std::uint32_t>(opt + 60);
  if (!signature || *signature != 0x00004550 || !count || !timestamp || !optional_size || !magic || *magic != 0x20B ||
      !preferred || !image_size || !headers_size)
    return std::nullopt;

  PeImage img;
  img.base_ = base;
  img.preferred_base_ = *preferred;
  img.timestamp_ = *timestamp;
  img.image_size_ = *image_size;
  img.headers_size_ = *headers_size;
  const std::uint64_t table = opt + *optional_size;
  for (std::uint16_t i = 0; i < *count; ++i) {
    const std::uint64_t entry = table + 40ull * i;
    char name[9] = {};
    const auto virtual_size = m.get<std::uint32_t>(entry + 8);
    const auto rva = m.get<std::uint32_t>(entry + 12);
    const auto raw_size = m.get<std::uint32_t>(entry + 16);
    const auto raw_offset = m.get<std::uint32_t>(entry + 20);
    if (!m.read(entry, name, 8) || !virtual_size || !rva || !raw_size || !raw_offset) return std::nullopt;
    img.sections_.push_back({name, *rva, *virtual_size, *raw_offset, *raw_size});
  }
  return img;
}

const Section* PeImage::section(std::string_view name) const {
  for (const auto& s : sections_)
    if (s.name == name) return &s;
  return nullptr;
}

bool PeImage::in_section(std::string_view name, std::uint64_t address) const {
  const Section* s = section(name);
  if (!s) return false;
  const std::uint64_t start = base_ + s->rva;
  const std::uint64_t size = s->virtual_size != 0 ? s->virtual_size : s->raw_size;
  return address >= start && address < start + size;
}

SectionBytes PeImage::read_section(const Memory& memory, const Section& s) const {
  SectionBytes result;
  result.address = base_ + s.rva;
  result.bytes.resize(s.virtual_size != 0 ? s.virtual_size : s.raw_size);
  if (!memory.read(result.address, result.bytes.data(), result.bytes.size())) result.bytes.clear();
  return result;
}

std::optional<std::uint64_t> find_cstring(const SectionBytes& data, std::string_view text) {
  if (text.empty()) return std::nullopt;
  std::string needle(1, '\0');
  needle.append(text);
  needle.push_back('\0');
  const auto& b = data.bytes;
  if (b.size() > text.size() && std::memcmp(b.data(), text.data(), text.size()) == 0 && b[text.size()] == 0)
    return data.address;
  const auto it = std::search(b.begin(), b.end(), needle.begin(), needle.end());
  if (it == b.end()) return std::nullopt;
  return data.address + static_cast<std::uint64_t>(it - b.begin()) + 1;
}

std::vector<std::uint64_t> find_qword(const SectionBytes& data, std::uint64_t value) {
  std::vector<std::uint64_t> hits;
  for (std::size_t i = 0; i + 8 <= data.bytes.size(); i += 8) {
    std::uint64_t v;
    std::memcpy(&v, data.bytes.data() + i, 8);
    if (v == value) hits.push_back(data.address + i);
  }
  return hits;
}

std::optional<RipRef> decode_rip_ref(const std::uint8_t* p, std::size_t available, std::uint64_t address) {
  if (available < 7) return std::nullopt;
  const std::uint8_t rex = p[0];
  const std::uint8_t opcode = p[1];
  const std::uint8_t modrm = p[2];
  if (rex != 0x48 && rex != 0x4C) return std::nullopt;
  if (opcode != 0x89 && opcode != 0x8B && opcode != 0x8D) return std::nullopt;
  if ((modrm & 0xC7) != 0x05) return std::nullopt;
  std::int32_t displacement;
  std::memcpy(&displacement, p + 3, 4);
  RipRef r;
  r.address = address;
  r.opcode = opcode;
  r.reg = ((rex & 0x04) != 0 ? 8 : 0) | ((modrm >> 3) & 7);
  r.target = address + 7 + static_cast<std::int64_t>(displacement);
  return r;
}

std::vector<RipRef> find_rip_refs_to(const SectionBytes& code, std::uint64_t target) {
  std::vector<RipRef> hits;
  const auto& b = code.bytes;
  for (std::size_t i = 0; i + 7 <= b.size(); ++i) {
    const auto r = decode_rip_ref(b.data() + i, b.size() - i, code.address + i);
    if (r && r->target == target) hits.push_back(*r);
  }
  return hits;
}

std::optional<std::uint64_t> function_start(const SectionBytes& pdata, std::uint64_t image_base, std::uint64_t address) {
  if (address < image_base) return std::nullopt;
  const auto field = [&](std::size_t entry, std::size_t offset) {
    std::uint32_t v;
    std::memcpy(&v, pdata.bytes.data() + entry * 12 + offset, 4);
    return v;
  };
  std::size_t count = pdata.bytes.size() / 12;
  while (count > 0 && field(count - 1, 0) == 0 && field(count - 1, 4) == 0) --count;
  const std::uint64_t target = address - image_base;
  std::size_t lo = 0;
  std::size_t hi = count;
  while (lo < hi) {
    const std::size_t mid = (lo + hi) / 2;
    if (target < field(mid, 0)) hi = mid;
    else if (target >= field(mid, 4)) lo = mid + 1;
    else return image_base + field(mid, 0);
  }
  return std::nullopt;
}

}  // namespace fw

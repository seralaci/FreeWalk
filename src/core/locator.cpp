#include "core/locator.h"
#include <algorithm>
#include <cstring>
#include <set>
#include "core/game_layout.h"
#include "core/text.h"

namespace fw {
namespace {

unsigned long long rva(const PeImage& image, std::uint64_t address) {
  return static_cast<unsigned long long>(address - image.base());
}

// Copies of the three sections the searches work on; within one search they are read only once.
struct Sections {
  SectionBytes text;
  SectionBytes rdata;
  SectionBytes data;
};

std::optional<Sections> read_sections(const Memory& m, const PeImage& image, std::vector<std::string>& log) {
  const Section* text = image.section(".text");
  const Section* rdata = image.section(".rdata");
  const Section* data = image.section(".data");
  if (!text || !rdata || !data) {
    log.push_back("the executable has no .text/.rdata/.data section");
    return std::nullopt;
  }
  Sections s{image.read_section(m, *text), image.read_section(m, *rdata), image.read_section(m, *data)};
  if (s.text.bytes.empty() || s.rdata.bytes.empty() || s.data.bytes.empty()) {
    log.push_back("the executable's sections could not be read");
    return std::nullopt;
  }
  return s;
}

std::optional<std::uint64_t> type_vtable(const PeImage& image, const Sections& s, std::string_view type_name, std::vector<std::string>& log) {
  const std::string label = "type " + std::string(type_name) + ": ";
  const auto name_address = find_cstring(s.rdata, type_name);
  if (!name_address) {
    log.push_back(label + "the name string was not found");
    return std::nullopt;
  }
  std::set<std::uint64_t> vtables;
  for (const std::uint64_t entry : find_qword(s.data, *name_address)) {
    std::vector<std::uint64_t> cells = find_qword(s.rdata, entry);
    const std::vector<std::uint64_t> data_cells = find_qword(s.data, entry);
    cells.insert(cells.end(), data_cells.begin(), data_cells.end());
    for (const std::uint64_t cell : cells) {
      for (const RipRef& ref : find_rip_refs_to(s.text, cell)) {
        // The type name getter is exactly "lea rax, [cell]; ret".
        const std::size_t at = static_cast<std::size_t>(ref.address - s.text.address);
        if (ref.opcode != 0x8D || ref.reg != 0 || at + 7 >= s.text.bytes.size() || s.text.bytes[at + 7] != 0xC3) continue;
        for (const std::uint64_t slot : find_qword(s.rdata, ref.address)) vtables.insert(slot - 8 * layout::kTypeGetterVtableIndex);
      }
    }
  }
  log.push_back(strf("%sname at +0x%llX, %zu vtable(s)", label.c_str(), rva(image, *name_address), vtables.size()));
  for (const std::uint64_t v : vtables) log.push_back(strf("%s  vtable +0x%llX", label.c_str(), rva(image, v)));
  if (vtables.size() != 1) return std::nullopt;
  return *vtables.begin();
}

}  // namespace

std::optional<std::uint64_t> find_type_vtable(const Memory& m, const PeImage& image, std::string_view type_name,
                                              std::vector<std::string>& log) {
  const auto s = read_sections(m, image, log);
  if (!s) return std::nullopt;
  return type_vtable(image, *s, type_name, log);
}

std::optional<std::uint64_t> find_camera_manager_global(const Memory& m, const PeImage& image, std::vector<std::string>& log) {
  const auto s = read_sections(m, image, log);
  if (!s) return std::nullopt;
  const auto vtable = type_vtable(image, *s, layout::kCameraManagerName, log);
  if (!vtable) return std::nullopt;

  // Near the vtable load, the factory function stores the instance in a global variable in .data.
  std::set<std::uint64_t> globals;
  const auto& code = s->text.bytes;
  for (const RipRef& ref : find_rip_refs_to(s->text, *vtable)) {
    if (ref.opcode != 0x8D) continue;
    const std::size_t at = static_cast<std::size_t>(ref.address - s->text.address);
    const std::size_t from = at > layout::kManagerStoreWindow ? at - layout::kManagerStoreWindow : 0;
    const std::size_t to = std::min(code.size(), at + layout::kManagerStoreWindow);
    for (std::size_t i = from; i < to; ++i) {
      const auto r = decode_rip_ref(code.data() + i, code.size() - i, s->text.address + i);
      if (r && r->opcode == 0x89 && image.in_section(".data", r->target)) globals.insert(r->target);
    }
  }
  log.push_back(strf("camera manager: %zu global store(s) near the vtable", globals.size()));
  for (const std::uint64_t g : globals) log.push_back(strf("camera manager:   global +0x%llX", rva(image, g)));
  if (globals.size() != 1) return std::nullopt;
  return *globals.begin();
}

std::optional<std::uint64_t> find_cvar(const Memory& m, const PeImage& image, std::string_view name, bool verify_name,
                                       std::vector<std::string>& log) {
  const std::string label = "cvar " + std::string(name) + ": ";
  const auto s = read_sections(m, image, log);
  if (!s) return std::nullopt;
  const auto name_address = find_cstring(s->rdata, name);
  if (!name_address) {
    log.push_back(label + "the name string was not found");
    return std::nullopt;
  }
  // The game copies the name into the cvar object in a static initializer: first it loads the name buffer address, then the name text's.
  const auto& code = s->text.bytes;
  std::set<std::uint64_t> candidates;
  for (const RipRef& ref : find_rip_refs_to(s->text, *name_address)) {
    if (ref.opcode != 0x8D) continue;
    const std::size_t at = static_cast<std::size_t>(ref.address - s->text.address);
    for (std::size_t back = 7; back <= layout::kCvarLeaWindow && back <= at; ++back) {
      const std::size_t start = at - back;
      const auto r = decode_rip_ref(code.data() + start, code.size() - start, s->text.address + start);
      if (!r || r->opcode != 0x8D || !image.in_section(".data", r->target)) continue;
      if (verify_name) {
        char stored[layout::kCvarNameSize + 1] = {};
        if (!m.read(r->target, stored, layout::kCvarNameSize) || std::string_view(stored) != name) continue;
      }
      candidates.insert(r->target - layout::kCvarName);
    }
  }
  log.push_back(strf("%sname at +0x%llX, %zu candidate object(s)%s", label.c_str(), rva(image, *name_address), candidates.size(),
                     verify_name ? " (copied name checked)" : ""));
  for (const std::uint64_t c : candidates) log.push_back(strf("%s  candidate +0x%llX", label.c_str(), rva(image, c)));
  if (candidates.size() != 1) return std::nullopt;
  return *candidates.begin();
}

std::vector<CameraSlot> list_cameras(const Memory& m, std::uint64_t manager, std::vector<std::string>& log) {
  const auto slots = m.get<std::uint64_t>(manager + layout::kManagerSlots);
  const auto count = m.get<std::uint64_t>(manager + layout::kManagerSlotCount);
  if (!slots || !count || *count == 0 || *count > layout::kMaxCameraSlots) {
    log.push_back(strf("cameras: implausible camera list (count %llu)", static_cast<unsigned long long>(count.value_or(0))));
    return {};
  }
  std::vector<CameraSlot> cameras;
  for (std::uint64_t i = 0; i < *count; ++i) {
    CameraSlot c;
    c.index = static_cast<int>(i);
    c.object = m.get<std::uint64_t>(*slots + 8 * i).value_or(0);
    if (c.object != 0) c.vtable = m.get<std::uint64_t>(c.object).value_or(0);
    cameras.push_back(c);
  }
  return cameras;
}

std::optional<CameraSlot> find_camera(const std::vector<CameraSlot>& cameras, std::uint64_t vtable) {
  for (const auto& c : cameras)
    if (c.vtable == vtable) return c;
  return std::nullopt;
}

std::optional<std::uint64_t> find_physx_scene_vtable(const Memory& m, const PeImage& image, std::vector<std::string>& log) {
  const std::string label = "physx scene: ";
  const Section* text = image.section(".text");
  const Section* pdata_section = image.section(".pdata");
  if (!text || !pdata_section) {
    log.push_back(label + "the executable has no .text/.pdata section");
    return std::nullopt;
  }
  const SectionBytes code = image.read_section(m, *text);
  const SectionBytes pdata = image.read_section(m, *pdata_section);
  std::vector<SectionBytes> constants;
  for (const char* name : {".rdata", "_RDATA"})
    if (const Section* s = image.section(name)) constants.push_back(image.read_section(m, *s));
  if (code.bytes.empty() || pdata.bytes.empty() || constants.empty() || constants.front().bytes.empty()) {
    log.push_back(label + "the executable's sections could not be read");
    return std::nullopt;
  }

  // Start addresses of the functions in which a lea points to the message.
  const auto functions_using = [&](std::string_view message, const char* what) {
    std::set<std::uint64_t> starts;
    bool text_found = false;
    for (const auto& c : constants) {
      const auto at = find_cstring(c, message);
      if (!at) continue;
      text_found = true;
      for (const RipRef& ref : find_rip_refs_to(code, *at))
        if (ref.opcode == 0x8D)
          if (const auto start = function_start(pdata, image.base(), ref.address)) starts.insert(*start);
    }
    log.push_back(strf("%s%s: message %s, %zu function(s)", label.c_str(), what, text_found ? "found" : "NOT found", starts.size()));
    return starts;
  };
  const auto releases = functions_using(layout::kPhysxReleaseMessage, "release");
  const auto shifts = functions_using(layout::kPhysxShiftOriginMessage, "shiftOrigin");

  std::set<std::uint64_t> vtables;
  for (const auto& c : constants)
    for (const std::uint64_t release : releases)
      for (const std::uint64_t slot : find_qword(c, release)) {
        const std::uint64_t vtable = slot - 8 * layout::kPhysxReleaseSlot;
        const auto shift = m.get<std::uint64_t>(vtable + 8 * layout::kPhysxShiftOriginSlot);
        if (shift && shifts.count(*shift) != 0) vtables.insert(vtable);
      }
  if (vtables.size() != 1) {
    log.push_back(strf("%s%zu vtable candidate(s), expected exactly one", label.c_str(), vtables.size()));
    return std::nullopt;
  }
  log.push_back(strf("%svtable +0x%llX", label.c_str(), rva(image, *vtables.begin())));
  return *vtables.begin();
}

}  // namespace fw

#include "win/driver_hider.h"
#include <windows.h>
#include <cstring>
#include "core/pe_image.h"
#include "core/text.h"

namespace fw {
namespace {

// A plausible user-mode object address.
bool pointer_ok(std::uint64_t p) { return p >= 0x10001 && p < 0x7FFFFFFD0000ull && (p & 7) == 0; }

}  // namespace

bool DriverHider::init(std::vector<std::string>& log) {
  base_ = reinterpret_cast<std::uint64_t>(GetModuleHandleW(nullptr));
  const auto image = PeImage::parse(memory_, base_);
  build_ = image ? layout::find_build(image->timestamp(), image->image_size()) : nullptr;
  if (!build_) {
    log.push_back("driver: not a verified game build; the driver stays visible while walking");
    return false;
  }
  image_end_ = base_ + image->image_size();
  return true;
}

std::string DriverHider::type_of(std::uint64_t object) const {
  const auto vtable = memory_.get<std::uint64_t>(object);
  if (!vtable || *vtable < base_ || *vtable >= image_end_) return "?";
  const auto getter = memory_.get<std::uint64_t>(*vtable + 8 * layout::kTypeGetterVtableIndex);
  std::uint8_t code[8] = {};
  if (!getter || !memory_.read(*getter, code, sizeof code) || code[0] != 0x48 || code[1] != 0x8D || code[2] != 0x05 || code[7] != 0xC3)
    return strf("vtable exe+0x%llX", static_cast<unsigned long long>(*vtable - base_));
  std::int32_t rel = 0;
  std::memcpy(&rel, code + 3, 4);
  const auto entry = memory_.get<std::uint64_t>(*getter + 7 + static_cast<std::int64_t>(rel));
  const auto name = entry ? memory_.get<std::uint64_t>(*entry) : std::nullopt;
  char text[48] = {};
  if (!name || !memory_.read(*name, text, sizeof text - 1)) return "?";
  return strf("%s, vtable exe+0x%llX", text, static_cast<unsigned long long>(*vtable - base_));
}

std::uint64_t DriverHider::find_slot(std::string* reason) const {
  const auto why = [&](const std::string& text) {
    if (reason) *reason = text;
    return std::uint64_t{0};
  };
  if (!build_) return why("not a verified build");
  const auto scene = memory_.get<std::uint64_t>(base_ + build_->scene_global);
  if (!scene || !pointer_ok(*scene)) return why(strf("scene %llX", static_cast<unsigned long long>(scene.value_or(0))));
  const auto actor = memory_.get<std::uint64_t>(*scene + layout::kScenePlayerActor);
  if (!actor || !pointer_ok(*actor))
    return why(strf("scene %llX, vehicle (+0x31B0) %llX", static_cast<unsigned long long>(*scene),
                    static_cast<unsigned long long>(actor.value_or(0))));
  // Layout check: the two embedded objects start with the same pointer.
  const auto first = memory_.get<std::uint64_t>(*actor + layout::kActorEmbedded);
  const auto second = memory_.get<std::uint64_t>(*actor + layout::kActorDriverObject);
  const auto model = memory_.get<std::uint64_t>(*actor + layout::kActorDriverModel);
  if (!first || !second || *first != *second || !pointer_ok(*first))
    return why(strf("vehicle %llX (%s): +0x18 %llX (%s), +0x1DF0 %llX (%s), +0x1DF8 %llX (%s)", static_cast<unsigned long long>(*actor),
                    type_of(*actor).c_str(), static_cast<unsigned long long>(first.value_or(0)), type_of(*actor + layout::kActorEmbedded).c_str(),
                    static_cast<unsigned long long>(second.value_or(0)), type_of(*actor + layout::kActorDriverObject).c_str(),
                    static_cast<unsigned long long>(model.value_or(0)), model && *model ? type_of(*model).c_str() : "-"));
  return *actor + layout::kActorDriverModel;
}

void DriverHider::hide(std::vector<std::string>& log) {
  if (slot_ != 0) return;
  std::string reason;
  const std::uint64_t slot = find_slot(&reason);
  if (slot == 0) {
    if (!reported_) log.push_back("driver: not found, left visible (" + reason + ")");
    reported_ = true;
    return;
  }
  const auto model = memory_.get<std::uint64_t>(slot);
  if (!model || *model == 0) {
    if (!reported_) log.push_back(strf("driver: no driver model in the slot (vehicle %s)", type_of(slot - layout::kActorDriverModel).c_str()));
    reported_ = true;
    return;
  }
  const auto vtable = memory_.get<std::uint64_t>(*model);
  if (!pointer_ok(*model) || !vtable || *vtable < base_ || *vtable >= image_end_) {
    log.push_back("driver: the driver model does not look right; left visible");
    return;
  }
  if (!memory_.put<std::uint64_t>(slot, 0)) {
    log.push_back("driver: the driver model slot could not be written; left visible");
    return;
  }
  slot_ = slot;
  model_ = *model;
  check_timer_ = 0;
  log.push_back(strf("driver: hidden (model %llX: %s)", static_cast<unsigned long long>(model_), type_of(model_).c_str()));
}

void DriverHider::tick(double dt, std::vector<std::string>& log) {
  if (slot_ == 0) return;
  check_timer_ += dt;
  if (check_timer_ < 1.0) return;
  check_timer_ = 0;
  const auto value = memory_.get<std::uint64_t>(slot_);
  if (!value || *value == 0) return;
  // The game set the driver up again: the earlier model cannot be given back any more.
  ++lost_;
  log.push_back(strf("driver: the game set the driver up again while it was hidden (%d reference(s) not given back)", lost_));
  slot_ = 0;
  model_ = 0;
  hide(log);
}

void DriverHider::show(std::vector<std::string>& log) {
  reported_ = false;
  if (slot_ == 0) return;
  const std::uint64_t slot = find_slot();
  const auto value = slot == slot_ ? memory_.get<std::uint64_t>(slot) : std::nullopt;
  if (value && *value == 0 && memory_.put<std::uint64_t>(slot, model_)) {
    log.push_back("driver: shown again");
  } else {
    ++lost_;
    log.push_back(strf("driver: the vehicle changed while the driver was hidden; not put back (%d reference(s) not given back)", lost_));
  }
  slot_ = 0;
  model_ = 0;
}

}  // namespace fw

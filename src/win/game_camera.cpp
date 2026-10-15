#include "win/game_camera.h"
#include <windows.h>
#include "core/game_layout.h"
#include "core/locator.h"
#include "core/text.h"

namespace fw {
namespace {

// Free camera position in memory: 3 floats, then the two int16 chunk indices, 16 bytes in total.
#pragma pack(push, 1)
struct RawPosition {
  float x, y, z;
  std::int16_t chunk_x, chunk_z;
};
#pragma pack(pop)
static_assert(sizeof(RawPosition) == 16);

}  // namespace

bool GameCamera::init(std::vector<std::string>& log) {
  const auto base = reinterpret_cast<std::uint64_t>(GetModuleHandleW(nullptr));
  image_ = PeImage::parse(memory_, base);
  if (!image_) {
    log.push_back("the game executable's headers could not be read");
    return false;
  }
  log.push_back(strf("game executable: timestamp 0x%08X, image size 0x%X", image_->timestamp(), image_->image_size()));
  if (!layout::is_verified_build(image_->timestamp(), image_->image_size())) {
    // Searches may still hit after an update, but the setter function and field locations may differ: no writes on an unknown build.
    log.push_back(strf("this is not the verified game build (timestamp 0x%08X, image size 0x%X); after a game update the plugin must be checked again",
                       layout::kVerifiedTimestamp, layout::kVerifiedImageSize));
    return false;
  }
  if (const auto g = find_camera_manager_global(memory_, *image_, log)) manager_global_ = *g;
  if (const auto v = find_type_vtable(memory_, *image_, layout::kFreeCameraClass, log)) free_vtable_ = *v;
  if (const auto c = find_cvar(memory_, *image_, layout::kDeveloperCvar, true, log)) developer_cvar_ = *c;
  if (const auto c = find_cvar(memory_, *image_, layout::kFreeCameraSpeedCvar, true, log)) speed_cvar_ = *c;
  // Hiding the HUD mirrors is not required for walking: if not found, the mirrors stay visible.
  if (const auto c = find_cvar(memory_, *image_, layout::kHudMirrorsHiddenCvar, true, log)) mirrors_cvar_ = *c;
  // Hiding the speedometer and the map, and the hint language, are not required for walking either.
  if (const auto c = find_cvar(memory_, *image_, layout::kAdviserHiddenCvar, true, log)) adviser_cvar_ = *c;
  if (const auto c = find_cvar(memory_, *image_, layout::kLanguageCvar, true, log)) language_cvar_ = *c;
  return manager_global_ != 0 && free_vtable_ != 0 && developer_cvar_ != 0 && speed_cvar_ != 0;
}

std::uint64_t GameCamera::manager() const {
  if (manager_global_ == 0) return 0;
  return memory_.get<std::uint64_t>(manager_global_).value_or(0);
}

bool GameCamera::resolve_cameras(std::vector<std::string>& log) {
  free_index_ = -1;
  free_camera_ = 0;
  camera_count_ = 0;
  const std::uint64_t m = manager();
  if (m == 0) {
    log.push_back("cameras: the camera manager does not exist yet");
    return false;
  }
  const auto cameras = list_cameras(memory_, m, log);
  camera_count_ = static_cast<int>(cameras.size());
  const auto free = find_camera(cameras, free_vtable_);
  if (!free) {
    log.push_back(strf("cameras: %d camera(s), the free camera is not among them", camera_count_));
    return false;
  }
  free_index_ = free->index;
  free_camera_ = free->object;
  return true;
}

bool GameCamera::free_camera_alive() const {
  const std::uint64_t m = manager();
  if (m == 0 || free_camera_ == 0 || free_index_ < 0) return false;
  const auto slots = memory_.get<std::uint64_t>(m + layout::kManagerSlots);
  if (!slots || *slots == 0) return false;
  const auto object = memory_.get<std::uint64_t>(*slots + 8ull * static_cast<std::uint64_t>(free_index_));
  const auto vtable = memory_.get<std::uint64_t>(free_camera_);
  return object && *object == free_camera_ && vtable && *vtable == free_vtable_;
}

int GameCamera::active_index() const {
  const std::uint64_t m = manager();
  if (m == 0) return -1;
  const auto index = memory_.get<std::int32_t>(m + layout::kManagerCurrentIndex);
  if (!index || *index < 0 || *index >= camera_count_) return -1;
  return *index;
}

bool GameCamera::request(int index) {
  // The camera manager being alive is confirmed by the free camera's array slot; on exit the manager may already be a destroyed object.
  const std::uint64_t m = manager();
  if (m == 0 || index < 0 || index >= camera_count_ || !free_camera_alive()) return false;
  return memory_.put<std::int32_t>(m + layout::kManagerRequestedIndex, index);
}

std::optional<std::array<float, 4>> GameCamera::rotation() const {
  if (!free_camera_alive()) return std::nullopt;
  return memory_.get<std::array<float, 4>>(free_camera_ + layout::kCameraRotation);
}

std::optional<double> GameCamera::yaw() const {
  if (!free_camera_alive()) return std::nullopt;
  const auto v = memory_.get<float>(free_camera_ + layout::kCameraYaw);
  if (!v) return std::nullopt;
  return *v;
}

bool GameCamera::set_yaw(double yaw) {
  return free_camera_alive() && memory_.put<float>(free_camera_ + layout::kCameraYaw, static_cast<float>(yaw));
}

bool GameCamera::set_pitch(double pitch) {
  return free_camera_alive() && memory_.put<float>(free_camera_ + layout::kCameraPitch, static_cast<float>(pitch));
}

std::optional<CameraPosition> GameCamera::read_position() const {
  if (free_camera_ == 0) return std::nullopt;
  const auto raw = memory_.get<RawPosition>(free_camera_ + layout::kCameraLocalPosition);
  if (!raw) return std::nullopt;
  return CameraPosition{raw->chunk_x, raw->chunk_z, raw->x, raw->y, raw->z};
}

bool GameCamera::write_position(const CameraPosition& p) {
  if (!free_camera_alive()) return false;
  const RawPosition raw{p.x, p.y, p.z, p.chunk_x, p.chunk_z};
  return memory_.put(free_camera_ + layout::kCameraLocalPosition, raw);
}

std::optional<std::string> GameCamera::cvar_value(std::uint64_t cvar) const {
  char text[layout::kCvarValueSize + 1] = {};
  if (cvar == 0 || !memory_.read(cvar + layout::kCvarValue, text, layout::kCvarValueSize)) return std::nullopt;
  return std::string(text);
}

bool GameCamera::set_cvar(std::uint64_t cvar, const std::string& value) {
  // We call the console variable's own set-from-string virtual function, so the game updates the string and the cached number together.
  // The function address is only called if it points into the game's code section.
  const auto vtable = memory_.get<std::uint64_t>(cvar);
  if (cvar == 0 || !vtable) return false;
  const auto function = memory_.get<std::uint64_t>(*vtable + 8ull * layout::kCvarSetterVtableIndex);
  if (!function || !image_ || !image_->in_section(".text", *function)) return false;
  using Setter = bool (*)(std::uint64_t self, const char* text);
  return reinterpret_cast<Setter>(*function)(cvar, value.c_str());
}

bool GameCamera::developer_enable() {
  const auto value = cvar_value(developer_cvar_);
  if (!value) return false;
  saved_developer_ = *value;
  return *value == "1" || set_cvar(developer_cvar_, "1");
}

void GameCamera::developer_restore() {
  if (saved_developer_ && *saved_developer_ != "1") set_cvar(developer_cvar_, *saved_developer_);
  saved_developer_.reset();
}

bool GameCamera::speed_freeze() {
  const auto value = cvar_value(speed_cvar_);
  if (!value) return false;
  saved_speed_ = *value;
  return set_cvar(speed_cvar_, "0");
}

void GameCamera::speed_restore() {
  if (saved_speed_) set_cvar(speed_cvar_, *saved_speed_);
  saved_speed_.reset();
}

bool GameCamera::mirrors_hide() {
  const auto value = cvar_value(mirrors_cvar_);
  if (!value) return false;
  saved_mirrors_ = *value;
  return *value == "1" || set_cvar(mirrors_cvar_, "1");
}

void GameCamera::mirrors_restore() {
  if (saved_mirrors_ && *saved_mirrors_ != "1") set_cvar(mirrors_cvar_, *saved_mirrors_);
  saved_mirrors_.reset();
}

bool GameCamera::adviser_hide() {
  const auto value = cvar_value(adviser_cvar_);
  if (!value) return false;
  saved_adviser_ = *value;
  return *value == "1" || set_cvar(adviser_cvar_, "1");
}

void GameCamera::adviser_restore() {
  if (saved_adviser_ && *saved_adviser_ != "1") set_cvar(adviser_cvar_, *saved_adviser_);
  saved_adviser_.reset();
}

std::string GameCamera::language() const { return cvar_value(language_cvar_).value_or(""); }

bool GameCamera::look_scale_set(double scale) {
  if (!free_camera_alive()) return false;
  const auto current = memory_.get<float>(free_camera_ + layout::kCameraLookScale);
  if (!current) return false;
  if (!saved_look_scale_) saved_look_scale_ = *current;
  return memory_.put<float>(free_camera_ + layout::kCameraLookScale, static_cast<float>(scale));
}

void GameCamera::look_scale_restore() {
  if (saved_look_scale_ && free_camera_alive()) memory_.put<float>(free_camera_ + layout::kCameraLookScale, *saved_look_scale_);
  saved_look_scale_.reset();
}

std::vector<SavedSetting> GameCamera::saved_settings() const {
  std::vector<SavedSetting> settings;
  if (saved_developer_) settings.push_back({std::string(layout::kDeveloperCvar), *saved_developer_});
  if (saved_speed_) settings.push_back({std::string(layout::kFreeCameraSpeedCvar), *saved_speed_});
  if (saved_mirrors_) settings.push_back({std::string(layout::kHudMirrorsHiddenCvar), *saved_mirrors_});
  if (saved_adviser_) settings.push_back({std::string(layout::kAdviserHiddenCvar), *saved_adviser_});
  return settings;
}

bool GameCamera::restore_setting(const SavedSetting& setting) {
  std::uint64_t cvar = 0;
  if (setting.name == layout::kDeveloperCvar) cvar = developer_cvar_;
  else if (setting.name == layout::kFreeCameraSpeedCvar) cvar = speed_cvar_;
  else if (setting.name == layout::kHudMirrorsHiddenCvar) cvar = mirrors_cvar_;
  else if (setting.name == layout::kAdviserHiddenCvar) cvar = adviser_cvar_;
  return cvar != 0 && set_cvar(cvar, setting.value);
}

void GameCamera::describe(const Vec3& truck, std::vector<std::string>& lines) {
  const std::uint64_t m = manager();
  if (m == 0) {
    lines.push_back("camera manager: not created yet");
    return;
  }
  const auto current = memory_.get<std::int32_t>(m + layout::kManagerCurrentIndex);
  const auto requested = memory_.get<std::int32_t>(m + layout::kManagerRequestedIndex);
  lines.push_back(strf("camera manager: current %d, requested %d, cameras %d, free camera index %d", current.value_or(-999),
                       requested.value_or(-999), camera_count_, free_index_));
  if (camera_count_ != described_count_) {
    // The camera list is only printed when it changed, to keep the log readable.
    described_count_ = camera_count_;
    std::vector<std::string> ignored;
    for (const auto& c : list_cameras(memory_, m, ignored))
      lines.push_back(strf("camera slot %d: vtable %s", c.index,
                           c.vtable != 0 && image_ ? strf("+0x%llX", static_cast<unsigned long long>(c.vtable - image_->base())).c_str() : "-"));
  }
  if (free_camera_ != 0) {
    const auto yaw_value = memory_.get<float>(free_camera_ + layout::kCameraYaw);
    const auto pitch = memory_.get<float>(free_camera_ + layout::kCameraPitch);
    const auto roll = memory_.get<float>(free_camera_ + layout::kCameraRoll);
    lines.push_back(strf("free camera angles: yaw %.4f pitch %.4f roll %.4f", yaw_value.value_or(0.0f), pitch.value_or(0.0f),
                         roll.value_or(0.0f)));
    float q[4] = {};
    if (memory_.read(free_camera_ + layout::kCameraRotation, q, sizeof q))
      lines.push_back(strf("free camera rotation: w %.4f x %.4f y %.4f z %.4f", q[0], q[1], q[2], q[3]));
    if (const auto p = read_position()) {
      const Vec3 w = to_world(*p);
      lines.push_back(strf("free camera position: chunk (%d, %d) local (%.2f, %.2f, %.2f) = world (%.2f, %.2f, %.2f), %.1f m from the truck",
                           p->chunk_x, p->chunk_z, p->x, p->y, p->z, w.x, w.y, w.z, horizontal_distance(w, truck)));
    }
  }
  lines.push_back(strf("cvars: g_developer '%s', g_flyspeed '%s'", cvar_value(developer_cvar_).value_or("?").c_str(),
                       cvar_value(speed_cvar_).value_or("?").c_str()));
}

}  // namespace fw

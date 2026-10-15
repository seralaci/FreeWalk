#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include "core/coords.h"
#include "core/pe_image.h"
#include "core/recovery.h"
#include "core/vec.h"
#include "win/process_memory.h"

namespace fw {

// Access to the game's camera manager, the free camera and its two console variables (cvars).
class GameCamera {
 public:
  // Locates in the game executable the camera manager global, the free camera type and the two console variables.
  // True if all are found; the camera list is only readable after a world is loaded, resolve_cameras loads it.
  bool init(std::vector<std::string>& log);

  // Rereads the camera list and finds the free camera in it; the game may recreate the cameras for each world.
  bool resolve_cameras(std::vector<std::string>& log);

  int active_index() const;  // -1 if unreadable or implausible

  // Whether the last found free camera is still alive: the same object, with the same vtable, still sits in the camera array slot.
  // After loading a save or the main menu the game may recreate the cameras; writing to the old object is forbidden.
  bool free_camera_alive() const;
  int free_index() const { return free_index_; }
  // Address of the camera manager (0 while no world is loaded); if it changes, the game has loaded a new world.
  std::uint64_t world_key() const { return manager(); }
  bool request(int index);

  std::optional<double> yaw() const;
  // Free camera rotation as a quaternion (w, x, y, z); the camera looks along its own -Z axis.
  std::optional<std::array<float, 4>> rotation() const;
  bool set_yaw(double yaw);
  bool set_pitch(double pitch);
  std::optional<CameraPosition> read_position() const;

  // The interior (seat) camera: the active camera if it is the game's interior camera and its values are plausible.
  struct SeatCamera {
    std::uint64_t object = 0;
    std::array<float, 4> offset{};  // [1] lateral, [2] up, [3] forward
    float yaw = 0;
    float pitch = 0;
  };
  std::optional<SeatCamera> seat_camera() const;
  // Writes the seat offset and the look angles, if that object is still the active interior camera.
  bool write_seat(const SeatCamera& seat);
  bool write_position(const CameraPosition& position);

  bool developer_enable();
  void developer_restore();
  bool speed_freeze();
  void speed_restore();
  bool mirrors_hide();
  void mirrors_restore();
  bool adviser_hide();
  void adviser_restore();
  // The game language (g_lang, e.g. "hu_hu"); empty if unreadable.
  std::string language() const;
  bool look_scale_set(double scale);
  void look_scale_restore();

  // Original values of the console variables changed for the duration of the walk (for the restore file).
  std::vector<SavedSetting> saved_settings() const;
  // Writes back an original value left over from an earlier session; only accepts console variables managed by FreeWalk.
  bool restore_setting(const SavedSetting& setting);

  // Diagnostic lines for discovery: camera manager fields, raw free camera fields, console variable values.
  void describe(const Vec3& truck, std::vector<std::string>& lines);

 private:
  std::uint64_t manager() const;
  std::optional<std::string> cvar_value(std::uint64_t cvar) const;
  bool set_cvar(std::uint64_t cvar, const std::string& value);

  ProcessMemory memory_;
  std::optional<PeImage> image_;
  std::uint64_t manager_global_ = 0;
  std::uint64_t free_vtable_ = 0;
  std::uint64_t interior_vtable_ = 0;  // 0: not found (no in-cab moves)
  std::uint64_t developer_cvar_ = 0;
  std::uint64_t speed_cvar_ = 0;
  std::uint64_t mirrors_cvar_ = 0;
  std::uint64_t adviser_cvar_ = 0;
  std::uint64_t language_cvar_ = 0;
  std::uint64_t free_camera_ = 0;
  int free_index_ = -1;
  int camera_count_ = 0;
  int described_count_ = -1;
  std::optional<std::string> saved_developer_;
  std::optional<std::string> saved_speed_;
  std::optional<std::string> saved_mirrors_;
  std::optional<std::string> saved_adviser_;
  std::optional<float> saved_look_scale_;
};

}  // namespace fw

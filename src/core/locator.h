#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "core/memory.h"
#include "core/pe_image.h"

namespace fw {

// Address of a type's vtable via the game's own type system, by type name.
std::optional<std::uint64_t> find_type_vtable(const Memory& memory, const PeImage& image, std::string_view type_name,
                                              std::vector<std::string>& log);

// Address of the global pointing to the camera manager; the manager itself is its content and exists only after a world loads.
std::optional<std::uint64_t> find_camera_manager_global(const Memory& memory, const PeImage& image, std::vector<std::string>& log);

// Address of a console variable (cvar) object by its name.
// Checking the copied name works only in the running game, since the game copies it at startup; turn it off when analysing the file.
std::optional<std::uint64_t> find_cvar(const Memory& memory, const PeImage& image, std::string_view name, bool verify_name,
                                       std::vector<std::string>& log);

struct CameraSlot {
  int index = -1;
  std::uint64_t object = 0;
  std::uint64_t vtable = 0;
};

// The camera manager's cameras with their vtables.
std::vector<CameraSlot> list_cameras(const Memory& memory, std::uint64_t manager, std::vector<std::string>& log);

// The first camera with this vtable.
std::optional<CameraSlot> find_camera(const std::vector<CameraSlot>& cameras, std::uint64_t vtable);

// The vtable of the PhysX scene class in the game: release and shiftOrigin are found from the code referencing their error messages
// (the .pdata function table gives the function start), and the vtable is the only place where both sit in the expected slots.
std::optional<std::uint64_t> find_physx_scene_vtable(const Memory& memory, const PeImage& image, std::vector<std::string>& log);

}  // namespace fw

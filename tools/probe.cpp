// Runs the same searches on the game executable that the plugin performs in the game, and prints the results.
// Usage: freewalk_probe <eurotrucks2.exe>
#include <cstdio>
#include <string>
#include <vector>
#include "core/file_image.h"
#include "core/game_layout.h"
#include "core/locator.h"
#include "core/pe_image.h"

int main(int argc, char** argv) {
  if (argc != 2) {
    std::fprintf(stderr, "usage: freewalk_probe <eurotrucks2.exe>\n");
    return 2;
  }
  std::string error;
  const auto memory = fw::load_pe_file(argv[1], error);
  if (!memory) {
    std::fprintf(stderr, "%s\n", error.c_str());
    return 1;
  }
  const auto image = fw::PeImage::parse(*memory, memory->base());
  if (!image) {
    std::fprintf(stderr, "the mapped image could not be parsed\n");
    return 1;
  }
  std::printf("timestamp 0x%08X, image size 0x%X, preferred base 0x%llX\n", image->timestamp(), image->image_size(),
              static_cast<unsigned long long>(image->preferred_base()));
  const auto* build = fw::layout::find_build(image->timestamp(), image->image_size());
  std::printf("verified build: %s\n", build ? build->name : "NO - check the layout again, then add the build to kBuilds in game_layout.h");
  for (const auto& s : image->sections())
    std::printf("  section %-8s rva 0x%08X size 0x%08X\n", s.name.c_str(), s.rva, s.virtual_size);

  std::vector<std::string> log;
  const auto manager = fw::find_camera_manager_global(*memory, *image, log);
  const auto free_camera = fw::find_type_vtable(*memory, *image, fw::layout::kFreeCameraClass, log);
  const auto developer = fw::find_cvar(*memory, *image, fw::layout::kDeveloperCvar, false, log);
  const auto speed = fw::find_cvar(*memory, *image, fw::layout::kFreeCameraSpeedCvar, false, log);
  const auto physx = fw::find_physx_scene_vtable(*memory, *image, log);
  for (const auto& line : log) std::printf("%s\n", line.c_str());

  std::printf("result: camera manager %s, free camera %s, g_developer %s, free camera speed %s, physx scene %s\n",
              manager ? "found" : "NOT FOUND", free_camera ? "found" : "NOT FOUND", developer ? "found" : "NOT FOUND",
              speed ? "found" : "NOT FOUND", physx ? "found" : "NOT FOUND");
  return manager && free_camera && developer && speed && physx ? 0 : 1;
}

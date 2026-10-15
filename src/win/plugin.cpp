// SCS telemetry SDK entry points and telemetry reception.
#include <windows.h>
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <optional>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include "scssdk_telemetry.h"
#include "common/scssdk_telemetry_common_configs.h"
#include "common/scssdk_telemetry_truck_common_channels.h"
#include "eurotrucks2/scssdk_eut2.h"
#include "core/config.h"
#include "core/coords.h"
#include "core/door_animation.h"
#include "core/door_choice.h"
#include "core/door_path.h"
#include "core/game_layout.h"
#include "core/hint.h"
#include "core/keys.h"
#include "core/recovery.h"
#include "core/settings.h"
#include "core/text.h"
#include "core/walk_session.h"
#include "core/flashlight.h"
#include "core/walker.h"
#include "core/world.h"
#include "win/fade_window.h"
#include "win/game_camera.h"
#include "win/game_lights.h"
#include "win/hint_window.h"
#include "win/input.h"
#include "win/log.h"
#include "win/physx_scene.h"
#include "win/region_focus.h"
#include "win/settings_window.h"

namespace {

constexpr const char* kVersion = "0.8.3";
constexpr double kTwoPi = 6.28318530717958647692;

struct State {
  fw::Config config;
  fw::TruckState truck;
  fw::Vec3 cabin_position;
  fw::Vec3 head_position;
  bool paused = true;
  fw::GameCamera camera;
  bool camera_ready = false;
  bool layout_confirmed = false;  // a successful walk confirmed the free camera position field
  std::vector<fw::SavedSetting> pending_recovery;  // values changed during a walk in an earlier session, written back once the world loads
  LARGE_INTEGER frequency{};
  LARGE_INTEGER last_frame{};
  double discovery_timer = 0;
  double input_check_timer = 0;
  fw::EdgeDetector toggle;
  fw::EdgeDetector settings_key;
  fw::Config wanted;                // settings requested in the settings window; config is derived from it by the apply_settings rule
  bool ini_had_unknown_keys = false;  // the loaded INI had an unknown key (the settings window warns that Save drops it)
  int saved_camera = -1;
  fw::WalkSession session;
  fw::Walker walker;
  fw::PhysxScene physx;
  fw::RegionFocus region_focus;  // the game's region query around the walker
  fw::GameLights lights;  // the game's lights (flashlight)
  fw::Flashlight flashlight;
  fw::EdgeDetector flashlight_key;
  fw::EdgeDetector wider_key;
  fw::EdgeDetector narrower_key;
  bool flashlight_resume = false;      // it was on before the pause: back on when the game resumes
  std::uint64_t lights_world = 0;      // the world in which the lamp addresses are valid
  std::vector<ULONGLONG> light_losses;  // times the light was lost (three times within 10 s: the lamp switches off)
  bool walking_on_physics = false;  // the last frame stepped with the game's physics
  bool have_eye = false;
  fw::Vec3 eye;  // the last computed eye position, written in the selected event
  fw::DoorAnimation door;       // the exit or entry animation
  fw::DoorPath door_path;       // the animation's door
  double door_final_yaw = 0;    // initial walk heading on exit
  std::string door_reason;      // reason for returning on entry
  bool door_switching = false;  // the entry is restoring the camera in the dark right now: return_to_truck must not interrupt it
};

State* g = nullptr;

void start_walk(const fw::SessionOutput& out);

// The plugin DLL folder, with a trailing slash.
std::wstring plugin_directory() {
  HMODULE module = nullptr;
  GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                     reinterpret_cast<LPCWSTR>(&plugin_directory), &module);
  wchar_t path[MAX_PATH] = {};
  GetModuleFileNameW(module, path, MAX_PATH);
  std::wstring result(path);
  const auto slash = result.find_last_of(L"\\/");
  return result.substr(0, slash == std::wstring::npos ? 0 : slash + 1);
}

std::wstring ini_path() { return plugin_directory() + L"FreeWalk.ini"; }

// Code review (item 7), live test: the game saves the console variables on Esc and on exit, so their in-walk values
// (hidden HUD mirrors, 0 free camera speed) could stay in the profile. So the original values go to a file for the duration of the walk,
// the file is deleted on return to the truck; if the game exited during a walk, they are written back from here on the next start.
// (Restoring them for the pause is not possible: after resuming the game shows the mirrors, and the value 1 no longer hides them.)
std::wstring recovery_path() { return plugin_directory() + L"FreeWalk.recovery"; }

void remember_for_recovery(const std::vector<fw::SavedSetting>& settings) {
  std::ofstream out(std::filesystem::path(recovery_path()), std::ios::binary | std::ios::trunc);
  out << fw::format_recovery(settings);
  if (!out) fw::log_warning("FreeWalk.recovery could not be written: if the game quits while walking, the walking settings may stay saved");
}

void forget_recovery() {
  std::error_code ignored;
  std::filesystem::remove(std::filesystem::path(recovery_path()), ignored);
}

void load_recovery() {
  std::ifstream in(std::filesystem::path(recovery_path()), std::ios::binary);
  if (!in) return;
  std::stringstream text;
  text << in.rdbuf();
  g->pending_recovery = fw::parse_recovery(text.str());
}

// After the world loads (the profile has already set the saved values) we write back the values from before the earlier walk.
void apply_pending_recovery() {
  if (g->pending_recovery.empty() || !g->camera_ready || g->paused || !g->truck.valid || g->session.phase() != fw::Phase::Driving) return;
  for (const auto& s : g->pending_recovery) {
    const bool ok = g->camera.restore_setting(s);
    fw::log_info(fw::strf("setting from a walk the game quit during: %s = \"%s\" %s", s.name.c_str(), s.value.c_str(),
                          ok ? "restored" : "could NOT be restored"));
  }
  g->pending_recovery.clear();
  forget_recovery();
}

void load_config() {
  std::ifstream in(std::filesystem::path(ini_path()), std::ios::binary);
  if (!in) {
    fw::log_warning("FreeWalk.ini was not found next to the DLL: the defaults are used");
    g->wanted = g->config;
    return;
  }
  std::stringstream text;
  text << in.rdbuf();
  std::vector<std::string> warnings;
  g->config = fw::parse_config(text.str(), warnings);
  for (const auto& w : warnings) {
    fw::log_warning("FreeWalk.ini " + w);
    if (w.find("unknown setting") != std::string::npos) g->ini_had_unknown_keys = true;
  }
  g->wanted = g->config;
  fw::log_info(fw::strf("settings: toggle key 0x%02X, discovery_log %d, switch_test %d, write_on_frame_start %d", g->config.key_toggle,
                        g->config.discovery_log ? 1 : 0, g->config.switch_test ? 1 : 0, g->config.write_on_frame_start ? 1 : 0));
}

SCSAPI_VOID on_placement(const scs_string_t, const scs_u32_t, const scs_value_t* const value, const scs_context_t) {
  if (!value || value->type != SCS_VALUE_TYPE_dplacement) return;
  const auto& p = value->value_dplacement;
  g->truck.position = {p.position.x, p.position.y, p.position.z};
  g->truck.heading = p.orientation.heading * kTwoPi;
  g->truck.valid = true;
}

SCSAPI_VOID on_speed(const scs_string_t, const scs_u32_t, const scs_value_t* const value, const scs_context_t) {
  if (value && value->type == SCS_VALUE_TYPE_float) g->truck.speed = value->value_float.value;
}

SCSAPI_VOID on_parking_brake(const scs_string_t, const scs_u32_t, const scs_value_t* const value, const scs_context_t) {
  if (value && value->type == SCS_VALUE_TYPE_bool) g->truck.parking_brake = value->value_bool.value != 0;
}

// The driver's head position in truck coordinates from the truck configuration: the cab position plus the head position in the cab.
SCSAPI_VOID on_configuration(const scs_event_t, const void* const event_info, const scs_context_t) {
  const auto* info = static_cast<const scs_telemetry_configuration_t*>(event_info);
  if (std::strcmp(info->id, SCS_TELEMETRY_CONFIG_truck) != 0) return;
  g->cabin_position = {};
  bool head_found = false;
  for (const scs_named_value_t* a = info->attributes; a->name; ++a) {
    if (a->value.type != SCS_VALUE_TYPE_fvector) continue;
    const auto& v = a->value.value_fvector;
    if (std::strcmp(a->name, SCS_TELEMETRY_CONFIG_ATTRIBUTE_cabin_position) == 0) g->cabin_position = {v.x, v.y, v.z};
    if (std::strcmp(a->name, SCS_TELEMETRY_CONFIG_ATTRIBUTE_head_position) == 0) {
      g->head_position = {v.x, v.y, v.z};
      head_found = true;
    }
  }
  g->truck.head_known = head_found;
  g->truck.head_in_truck = g->cabin_position + g->head_position;
  fw::log_info(fw::strf("truck configuration: driver's head at (%.2f, %.2f, %.2f) in truck space%s", g->truck.head_in_truck.x,
                        g->truck.head_in_truck.y, g->truck.head_in_truck.z, head_found ? "" : " (head position missing)"));
}

SCSAPI_VOID on_paused(const scs_event_t, const void* const, const scs_context_t) { g->paused = true; }
SCSAPI_VOID on_started(const scs_event_t, const void* const, const scs_context_t) { g->paused = false; }

double frame_seconds() {
  LARGE_INTEGER now;
  QueryPerformanceCounter(&now);
  const double dt = static_cast<double>(now.QuadPart - g->last_frame.QuadPart) / static_cast<double>(g->frequency.QuadPart);
  g->last_frame = now;
  return dt;
}

void log_lines(const std::vector<std::string>& lines, const std::string& prefix = "") {
  for (const auto& line : lines) fw::log_info(prefix + line);
}

// Prints the raw camera and truck data every second; the live test checks the game_layout.h assumptions from these.
void discovery(double dt) {
  g->discovery_timer += dt;
  if (g->discovery_timer < 1.0) return;
  g->discovery_timer = 0;
  std::vector<std::string> lines;
  g->camera.resolve_cameras(lines);
  g->camera.describe(g->truck.position, lines);
  for (const auto& line : g->physx.describe()) lines.push_back(line);
  for (const auto& line : g->lights.describe(g->session.phase() == fw::Phase::Walking ? g->walker.feet() : g->truck.position))
    lines.push_back(line);
  if (g->session.phase() == fw::Phase::Walking)
    for (const auto& line : g->physx.probe(g->walker.feet(), g->truck.position)) lines.push_back(line);
  lines.push_back(fw::strf("truck: (%.2f, %.2f, %.2f) heading %.4f rad, speed %.2f m/s, parking brake %d, paused %d", g->truck.position.x,
                           g->truck.position.y, g->truck.position.z, g->truck.heading, g->truck.speed, g->truck.parking_brake ? 1 : 0,
                           g->paused ? 1 : 0));
  log_lines(lines, "[discovery] ");
}

// Discovery toggle: the key requests the free camera, pressed again the previous camera; does not touch speed or position.
void switch_test() {
  const bool down = fw::combo_down(g->config.key_toggle, fw::key_down);
  const bool focused = fw::game_window_focused();
  if (!g->toggle.update(down)) return;
  std::vector<std::string> lines;
  lines.push_back(fw::strf("toggle key pressed, game window focused %d", focused ? 1 : 0));
  if (!focused) {
    log_lines(lines, "[switch-test] ");
    return;
  }
  g->camera.resolve_cameras(lines);
  const int active = g->camera.active_index();
  const int free = g->camera.free_index();
  if (active != free) {
    g->saved_camera = active;
    lines.push_back(fw::strf("requesting the free camera (%d), the active one was %d: %s", free, active,
                             g->camera.request(free) ? "written" : "FAILED"));
  } else {
    lines.push_back(fw::strf("requesting the previous camera (%d): %s", g->saved_camera,
                             g->camera.request(g->saved_camera) ? "written" : "FAILED"));
  }
  log_lines(lines, "[switch-test] ");
}

// Switches the flashlight off immediately, returning the borrowed lamp.
void flashlight_off() {
  g->flashlight.switch_off();
  std::vector<std::string> lines;
  g->lights.release(lines);
  log_lines(lines);
}

// Per frame while walking: keys and wheel, lamp state, acquiring and setting the light.
void flashlight_step(double dt, bool focused, const std::function<bool(int)>& down) {
  if (g->paused) {
    if (g->flashlight.on()) {
      g->flashlight_resume = true;
      flashlight_off();
    }
    fw::mouse_wheel_take();
    return;
  }
  const auto edge = [&](fw::EdgeDetector& e, int key) { return e.update(focused && key != 0 && fw::combo_down(key, down)); };
  fw::FlashlightInput in;
  in.toggle = edge(g->flashlight_key, g->config.key_flashlight);
  if (g->flashlight_resume) {
    in.toggle = !g->flashlight.on();
    g->flashlight_resume = false;
  }
  const int wheel = fw::mouse_wheel_take();
  in.wider = (edge(g->wider_key, g->config.key_flashlight_wider) ? 1 : 0) + (wheel > 0 ? wheel : 0);
  in.narrower = (edge(g->narrower_key, g->config.key_flashlight_narrower) ? 1 : 0) + (wheel < 0 ? -wheel : 0);
  in.eye = g->eye;
  in.yaw = fw::layout::camera_yaw_to_heading(g->camera.yaw().value_or(0.0));
  in.dt = dt;
  const fw::LightOutput out = g->flashlight.step(in, fw::light_settings(g->config));
  std::vector<std::string> lines;
  if (!out.lit) {
    g->lights.release(lines);
  } else {
    if (!g->lights.holding())
      g->lights.acquire(g->walker.feet(), g->config.light_borrow_min, g->config.light_borrow_max, g->config.light_own, lines);
    const auto rotation = g->camera.rotation();
    if (g->lights.holding() && rotation && !g->lights.apply(out, *rotation, lines)) {
      const ULONGLONG now = GetTickCount64();
      g->light_losses.push_back(now);
      while (!g->light_losses.empty() && now - g->light_losses.front() > 10000) g->light_losses.erase(g->light_losses.begin());
      if (g->light_losses.size() >= 3) {
        g->flashlight.switch_off();
        g->light_losses.clear();
        lines.push_back("lights: the flashlight's light was lost three times in 10 s: switched off (switch it on again to retry)");
      }
    }
  }
  log_lines(lines);
}

// Restores everything the exit changed; may be called more than once.
void return_to_truck(bool restore_camera, const std::string& reason) {
  if (!g->door_switching) {
    g->door.cancel();
    fw::fade_window_set(0);
  }
  g->session.force_return(reason);
  flashlight_off();
  g->flashlight_resume = false;
  g->have_eye = false;
  if (restore_camera && g->saved_camera >= 0 && !g->camera.request(g->saved_camera))
    fw::log_warning("the previous camera could not be requested back");
  g->camera.speed_restore();
  g->camera.look_scale_restore();
  g->camera.mirrors_restore();
  g->camera.adviser_restore();
  g->camera.developer_restore();
  forget_recovery();
  g->saved_camera = -1;
  fw::log_info("back in the truck: " + reason);
}

bool free_camera_position_plausible() {
  return fw::free_camera_position_plausible(g->camera.read_position(), g->truck.position, g->layout_confirmed);
}

void layout_check_failed() {
  return_to_truck(true, "the free camera's position does not look right");
  fw::log_error("walking is switched off for this session: the free camera layout check failed");
  g->camera_ready = false;
}

// Exit by requesting the free camera. feet is the walker's initial foot point, eye is the pre-written free camera position.
void begin_exit(const std::string& reason, fw::Vec3 feet, fw::Vec3 eye) {
  fw::log_info(reason);
  g->saved_camera = g->camera.active_index();
  // Live test (0.3.2): on switching, the game loads the map at the free camera's current position ("Forcing sector data"); on the first
  // exit this was the untouched origin, and it took 2.2 s. So we write the exit point in advance, and it requests the already loaded
  // area.
  if (!free_camera_position_plausible()) {
    layout_check_failed();
    return;
  }
  g->walker.reset(feet);
  if (!g->camera.write_position(fw::to_camera(eye)))
    fw::log_warning("the exit point could not be written before the switch: the game may load the map at the old camera position");
  if (!g->camera.developer_enable()) fw::log_warning("g_developer could not be switched on: the free camera may not activate");
  if (!g->camera.speed_freeze()) fw::log_warning("the free camera speed could not be set to 0: the view may drift while walking");
  if (!g->camera.mirrors_hide()) fw::log_warning("the HUD mirrors could not be hidden");
  if (g->config.hide_hud && !g->camera.adviser_hide()) fw::log_warning("the speedometer and the map could not be hidden");
  remember_for_recovery(g->camera.saved_settings());
  if (!g->camera.request(g->camera.free_index())) return_to_truck(true, "the free camera could not be requested");
}

void begin_exit(const std::string& reason) {
  const fw::Vec3 feet = fw::exit_point(g->truck, g->config);
  fw::Walker w;
  w.reset(feet);
  begin_exit(reason, feet, w.eye(g->config));
}

fw::World* physics_world() { return g->config.collision_enabled ? g->physx.world() : nullptr; }

// Door geometry. Ground height is the given value if any; otherwise from physics below the exit point if ready, or the estimate.
fw::DoorPath door_geometry(int side, std::optional<double> ground) {
  const fw::DoorSettings settings = fw::door_settings(g->config);
  const double estimate = ground.value_or(g->truck.position.y + g->config.ground_offset);
  fw::DoorPath path = fw::door_path(g->truck, g->config, side, estimate, settings);
  if (ground) return path;
  if (fw::World* world = physics_world()) {
    if (const auto hit = world->ground_below({path.ground.x, path.sill.y, path.ground.z}, 6.0))
      path = fw::door_path(g->truck, g->config, side, hit->height, settings);
  }
  return path;
}

// Free space in front of the door (m): a thin capsule swept outward from below the door sill; nullopt if physics is not ready.
std::optional<double> door_free_space(const fw::DoorPath& path) {
  fw::World* world = physics_world();
  if (!world) return std::nullopt;
  constexpr double kReach = 1.5;
  const fw::Capsule capsule{0.1, 0.6};
  const fw::Vec3 center{path.sill.x, path.ground.y + 1.1, path.sill.z};
  const auto hit = world->sweep_capsule(capsule, center, fw::forward_from_yaw(path.outward_yaw), kReach);
  if (!hit) return kReach;
  return hit->initial_overlap ? 0.0 : hit->distance;
}

std::string free_text(std::optional<double> free) { return free ? fw::strf("%.2f m", *free) : std::string("not measured"); }

fw::DoorTiming door_timing() {
  fw::DoorTiming timing = fw::door_timing(g->config);
  timing.switch_timeout = fw::kSwitchTimeout;
  return timing;
}

// Animated exit: door choice by free space, then the blink; the free camera is requested in the dark (door_frame).
void begin_door_exit() {
  const int driver = fw::driver_side(g->truck);
  const fw::DoorPath driver_path = door_geometry(driver, std::nullopt);
  const auto driver_free = door_free_space(driver_path);
  std::optional<double> passenger_free;
  fw::DoorPath passenger_path;
  if (driver_free && *driver_free < g->config.door_min_free) {
    passenger_path = door_geometry(-driver, std::nullopt);
    passenger_free = door_free_space(passenger_path);
  }
  const fw::DoorSide choice = fw::choose_door(driver_free, passenger_free, g->config.door_min_free);
  if (choice == fw::DoorSide::None) {
    g->session.force_return("both doors are squeezed");
    fw::log_info("not leaving the truck: both doors are squeezed (driver side " + free_text(driver_free) + ", passenger side " +
                 free_text(passenger_free) + ")");
    return;
  }
  g->door_path = choice == fw::DoorSide::Driver ? driver_path : passenger_path;
  g->door_final_yaw = fw::exit_yaw_on(g->truck, g->config, g->door_path.side);
  g->door.start_exit(g->door_path, g->door_final_yaw, door_timing());
  fw::log_info(fw::strf("leaving by the %s door (driver side %s, passenger side %s): sill %.2f m above the ground, %d step(s)",
                        choice == fw::DoorSide::Driver ? "driver" : "passenger", free_text(driver_free).c_str(),
                        free_text(passenger_free).c_str(), g->door_path.sill.y - g->door_path.ground.y, g->door_path.steps));
}

// Animated entry: full animation from next to the door (if the key was the reason), a blink from farther away.
void begin_door_enter(const std::string& reason, bool by_key) {
  flashlight_off();
  g->flashlight_resume = false;
  g->have_eye = false;
  const fw::Vec3 feet = g->walker.feet();
  // Through the door nearer the walker; climbing in is only possible from in front of the door (not through the cab from the far side).
  const int side = fw::nearer_door(g->truck, g->config, feet, fw::door_settings(g->config));
  g->door_path = door_geometry(side, feet.y);
  const double distance = fw::horizontal_distance(feet, g->door_path.sill);
  const bool close = by_key && fw::near_door(g->door_path, feet, g->config.enter_near_door);
  const double yaw = fw::layout::camera_yaw_to_heading(g->camera.yaw().value_or(0.0));
  g->door_reason = reason;
  g->door.start_enter(g->door_path, feet, yaw, close, door_timing());
  fw::log_info(fw::strf("entering the truck (%s, %.1f m from the door): %s", close ? "near: steps up" : "far: blink", distance,
                        reason.c_str()));
}

// One frame of the animation: the fade, the camera, the switches in the dark.
void door_frame(double dt) {
  const fw::DoorFrame f = g->door.step(g->paused ? 0.0 : (std::min)(dt, fw::kMaxStepSeconds));
  // When paused (menu) the fade does not stay over the picture; it comes back when the game resumes.
  fw::fade_window_set(g->paused ? 0.0 : f.blackout);
  const bool entering = g->door.entering();
  if (f.switch_now) {
    if (entering) {
      g->door_switching = true;
      return_to_truck(true, g->door_reason);
      g->door_switching = false;
      g->door.switched();
    } else {
      begin_exit("leaving the truck", g->door_path.ground, f.eye);
    }
    return;
  }
  if (f.camera && (entering || g->session.phase() == fw::Phase::Walking)) {
    if (!g->camera.write_position(fw::to_camera(f.eye))) {
      return_to_truck(true, "the free camera's position could not be written");
      return;
    }
    g->camera.set_yaw(fw::layout::heading_to_camera_yaw(f.yaw));
    g->camera.set_pitch(0.0);
  }
  if (!f.done) return;
  if (f.failed) {
    return_to_truck(true, "the game did not switch to the free camera");
  } else if (!entering) {
    fw::SessionOutput out;
    out.command = fw::Command::StartWalk;
    out.start_feet = g->door_path.ground;
    out.start_yaw = g->door_final_yaw;
    start_walk(out);
  }
}

// The free camera is active but the exit animation still runs: layout confirmation and settings; the walk starts at the animation end.
void door_switched() {
  if (!free_camera_position_plausible()) {
    layout_check_failed();
    return;
  }
  g->layout_confirmed = true;
  if (!g->camera.look_scale_set(g->config.mouse_sensitivity)) fw::log_warning("the mouse sensitivity could not be set");
  g->door.switched();
}

// Key hint per frame: exit in the cab (truck stopped, parking brake on), entry in front of the door; in the game's language.
void hint_frame(bool focused) {
  fw::HintInput in;
  in.enabled = g->config.key_hints && g->config.key_toggle != 0;
  in.focused = focused;
  in.paused = g->paused;
  in.animating = g->door.active();
  in.driving = g->session.phase() == fw::Phase::Driving;
  in.walking = g->session.phase() == fw::Phase::Walking && g->have_eye;
  in.can_leave = fw::truck_parked(g->truck);
  if (in.enabled && in.walking) {
    const fw::Vec3 feet = g->walker.feet();
    const int side = fw::nearer_door(g->truck, g->config, feet, fw::door_settings(g->config));
    const fw::DoorPath path = fw::door_path(g->truck, g->config, side, feet.y, fw::door_settings(g->config));
    in.at_door = fw::near_door(path, feet, fw::kDoorHintDistance);
  }
  const fw::Hint hint = fw::choose_hint(in);
  fw::hint_window_show(fw::key_name(g->config.key_toggle), fw::hint_text(hint, g->camera.language()));
}

// The walker's world: the game's physics if ready; otherwise flat ground at the current foot height. The switch is logged.
fw::World& walking_world(fw::FlatWorld& flat) {
  fw::World* physics = g->config.collision_enabled ? g->physx.world() : nullptr;
  if ((physics != nullptr) != g->walking_on_physics) {
    g->walking_on_physics = physics != nullptr;
    // If physics becomes available mid-walk, the feet move from the flat ground height to the real ground (else they could stay below it).
    if (physics) g->walker.settle(*physics);
    fw::log_info(physics ? std::string("ground and collision: from the game's physics")
                         : "ground and collision: flat ground (physx " + g->physx.status() + ")");
  }
  return physics ? *physics : flat;
}

void start_walk(const fw::SessionOutput& out) {
  if (!free_camera_position_plausible()) {
    layout_check_failed();
    return;
  }
  g->layout_confirmed = true;
  g->walker.reset(out.start_feet);
  g->region_focus.reset();
  {
    fw::FlatWorld flat(out.start_feet.y);
    g->walking_on_physics = false;
    // Switching from flat ground to physics itself puts the feet on the real ground (once); a second search would start 2 m above
    // the already raised feet, and could put them on top of an eave or truck bed.
    walking_world(flat);
  }
  // We look out horizontally instead of keeping the pitch left over from the previous walk.
  if (!g->camera.set_pitch(0.0)) fw::log_warning("the free camera's pitch could not be set");
  if (!g->camera.set_yaw(fw::layout::heading_to_camera_yaw(out.start_yaw))) fw::log_warning("the free camera's direction could not be set");
  if (!g->camera.look_scale_set(g->config.mouse_sensitivity)) fw::log_warning("the mouse sensitivity could not be set");
  g->eye = g->walker.eye(g->config);
  g->have_eye = true;
  const fw::Vec3 f = g->walker.feet();
  fw::log_info(fw::strf("walking from (%.2f, %.2f, %.2f), estimated ground %.2f", f.x, f.y, f.z, out.start_feet.y));
}

// Movement for one frame while walking: the walker steps by the keys and camera direction, and the eye position to write updates.
void walk(double dt, bool focused) {
  const auto yaw = g->camera.yaw();
  if (!yaw) {
    return_to_truck(true, "the free camera's direction could not be read");
    return;
  }
  // Besides the hook state the Windows key state also counts: if Windows silently removes the hook (LowLevelHooksTimeout),
  // the walker still does not stop.
  const auto down = [](int key) { return fw::keyboard_hook_down(key) || fw::key_down(key); };
  const fw::WalkKeys keys = fw::map_keys(focused, g->paused, down, g->config);
  if (g->config.collision_enabled) {
    std::vector<std::string> focus_lines;
    g->region_focus.update(g->walker.feet(), dt, focus_lines);
    log_lines(focus_lines);
  }
  fw::FlatWorld flat(g->walker.feet().y);
  g->walker.step(keys, fw::layout::camera_yaw_to_heading(*yaw), dt, g->config, walking_world(flat));
  g->eye = g->walker.eye(g->config);
  flashlight_step(dt, focused, down);
}

void write_eye() {
  if (!g->have_eye || g->session.phase() != fw::Phase::Walking) return;
  if (!g->camera.write_position(fw::to_camera(g->eye))) return_to_truck(true, "the free camera's position could not be written");
}

// Every two seconds: whether the DirectInput filter entries are in place (if not, rewrites them), and while walking
// logs how many times the game read the keyboard and how much the filter hid.
void check_input(double dt) {
  if (!g->config.directinput_filter) return;
  g->input_check_timer += dt;
  if (g->input_check_timer < 2.0) return;
  g->input_check_timer = 0;
  const std::string report = fw::directinput_filter_check();
  if (report.empty()) return;
  if ((g->config.discovery_log && g->session.phase() != fw::Phase::Driving) || report.find("overwritten") != std::string::npos)
    fw::log_info("DirectInput reads in the last 2 s: " + report);
}

// Settings window log lines and changes. While walking, keys take effect from the next exit, settings needing a restart
// after the game restarts (apply_settings).
void take_settings() {
  for (const auto& line : fw::settings_window_take_log()) {
    fw::log_info(line);
    if (line == "settings saved to FreeWalk.ini") g->ini_had_unknown_keys = false;
  }
  if (auto wanted = fw::settings_window_take_wanted()) g->wanted = *wanted;
  const bool walking = g->session.phase() != fw::Phase::Driving;
  const fw::Config next = fw::apply_settings(g->config, g->wanted, walking);
  if (fw::same_settings(next, g->config)) return;
  const bool keys_changed = next.key_forward != g->config.key_forward || next.key_back != g->config.key_back ||
                            next.key_left != g->config.key_left || next.key_right != g->config.key_right || next.key_run != g->config.key_run ||
                            next.key_flashlight != g->config.key_flashlight || next.key_flashlight_wider != g->config.key_flashlight_wider ||
                            next.key_flashlight_narrower != g->config.key_flashlight_narrower;
  const bool sensitivity_changed = next.mouse_sensitivity != g->config.mouse_sensitivity;
  g->config = next;
  if (keys_changed) {
    const std::vector<int> keys{g->config.key_forward, g->config.key_back, g->config.key_left, g->config.key_right, g->config.key_run,
                                g->config.key_flashlight, g->config.key_flashlight_wider, g->config.key_flashlight_narrower};
    fw::keyboard_hook_watch(keys);
    if (g->config.directinput_filter) fw::directinput_filter_watch(keys);
  }
  if (walking && sensitivity_changed && g->camera_ready && !g->camera.look_scale_set(g->config.mouse_sensitivity))
    fw::log_warning("the mouse sensitivity could not be set");
}

// The settings window key: opens it, or if it is open, closes it (same as Cancel).
void settings_hotkey(bool focused) {
  if (!g->settings_key.update(focused && fw::combo_down(g->config.key_settings, fw::key_down))) return;
  if (fw::settings_window_is_open()) {
    fw::settings_window_close();
    return;
  }
  fw::SettingsWindowSetup setup;
  setup.current = g->wanted;
  setup.ini_path = ini_path();
  setup.ini_had_unknown_keys = g->ini_had_unknown_keys;
  setup.game_window = GetForegroundWindow();
  setup.version = kVersion;
  if (!fw::settings_window_open(setup)) fw::log_warning("the settings window could not be opened");
}

SCSAPI_VOID on_frame_start(const scs_event_t, const void* const, const scs_context_t) {
  if (g->camera_ready && g->config.write_on_frame_start) write_eye();
}

SCSAPI_VOID on_frame_end(const scs_event_t, const void* const, const scs_context_t) {
  const double dt = frame_seconds();
  if (g->config.discovery_log) discovery(dt);
  apply_pending_recovery();
  take_settings();
  settings_hotkey(fw::game_window_focused());
  if (!g->camera_ready) {
    // If the walk was switched off mid-walk, key swallowing must not stay on either.
    fw::keyboard_hook_block(false);
    fw::mouse_wheel_capture(false);
    fw::hint_window_show("", "");
    return;
  }
  {
    std::vector<std::string> physx_lines;
    g->physx.update(g->camera.world_key(), g->truck.position, g->truck.valid, dt, physx_lines);
    // In a new world the old lamp addresses are invalid: we do not write to them.
    if (g->camera.world_key() != g->lights_world) {
      g->lights_world = g->camera.world_key();
      g->lights.forget();
      g->flashlight.switch_off();
    }
    log_lines(physx_lines);
  }
  if (g->config.switch_test) {
    switch_test();
    return;
  }

  const bool focused = fw::game_window_focused();
  // During the animation the exit/entry key has no effect.
  const bool toggle = g->toggle.update(focused && fw::combo_down(g->config.key_toggle, fw::key_down)) && !g->door.active();
  if (toggle && g->session.phase() == fw::Phase::Driving) {
    std::vector<std::string> lines;
    if (!g->camera.resolve_cameras(lines)) log_lines(lines);
  }

  fw::SessionInput in;
  in.truck = g->truck;
  in.toggle_pressed = toggle;
  in.active_camera = g->camera.active_index();
  in.free_camera = g->camera.free_index();
  // When paused the animation stops: the switch deadline must not run either.
  in.dt = g->paused && g->door.active() ? 0.0 : dt;
  in.walker_feet = g->walker.feet();
  const fw::SessionOutput out = g->session.update(in, g->config);

  switch (out.command) {
    case fw::Command::None:
      if (!out.reason.empty()) fw::log_info("not leaving the truck: " + out.reason);
      break;
    case fw::Command::RequestFreeCamera:
      if (g->config.door_animation) begin_door_exit();
      else begin_exit(out.reason);
      break;
    case fw::Command::StartWalk:
      if (g->door.active()) door_switched();
      else start_walk(out);
      break;
    case fw::Command::Walk:
      if (!g->door.active()) walk(dt, focused);
      break;
    case fw::Command::Return:
      // While walking (key, distance, the truck moved): animated entry; during exit (the switch did not happen) immediate.
      if (g->config.door_animation && !g->door.active() && g->have_eye) begin_door_enter(out.reason, toggle);
      else return_to_truck(true, out.reason);
      break;
    case fw::Command::ReturnKeepCamera: return_to_truck(false, out.reason); break;
  }
  if (g->door.active()) door_frame(dt);
  hint_frame(focused);
  if (g->camera_ready && !g->config.write_on_frame_start) write_eye();
  fw::keyboard_hook_block(g->camera_ready && (g->session.phase() != fw::Phase::Driving || g->door.active()));
  fw::mouse_wheel_capture(g->camera_ready && g->session.phase() == fw::Phase::Walking && !g->paused && !g->door.active());
  check_input(dt);
}

bool register_all(const scs_telemetry_init_params_v101_t* p) {
  bool ok = true;
  ok &= p->register_for_event(SCS_TELEMETRY_EVENT_frame_start, on_frame_start, nullptr) == SCS_RESULT_ok;
  ok &= p->register_for_event(SCS_TELEMETRY_EVENT_frame_end, on_frame_end, nullptr) == SCS_RESULT_ok;
  ok &= p->register_for_event(SCS_TELEMETRY_EVENT_paused, on_paused, nullptr) == SCS_RESULT_ok;
  ok &= p->register_for_event(SCS_TELEMETRY_EVENT_started, on_started, nullptr) == SCS_RESULT_ok;
  ok &= p->register_for_event(SCS_TELEMETRY_EVENT_configuration, on_configuration, nullptr) == SCS_RESULT_ok;
  ok &= p->register_for_channel(SCS_TELEMETRY_TRUCK_CHANNEL_world_placement, SCS_U32_NIL, SCS_VALUE_TYPE_dplacement,
                                SCS_TELEMETRY_CHANNEL_FLAG_none, on_placement, nullptr) == SCS_RESULT_ok;
  ok &= p->register_for_channel(SCS_TELEMETRY_TRUCK_CHANNEL_speed, SCS_U32_NIL, SCS_VALUE_TYPE_float, SCS_TELEMETRY_CHANNEL_FLAG_none,
                                on_speed, nullptr) == SCS_RESULT_ok;
  ok &= p->register_for_channel(SCS_TELEMETRY_TRUCK_CHANNEL_parking_brake, SCS_U32_NIL, SCS_VALUE_TYPE_bool,
                                SCS_TELEMETRY_CHANNEL_FLAG_none, on_parking_brake, nullptr) == SCS_RESULT_ok;
  return ok;
}

}  // namespace

extern "C" {

SCSAPI_RESULT scs_telemetry_init(const scs_u32_t version, const scs_telemetry_init_params_t* const params) {
  if (version != SCS_TELEMETRY_VERSION_1_01) return SCS_RESULT_unsupported;
  const auto* p = static_cast<const scs_telemetry_init_params_v101_t*>(params);
  fw::log_init(p->common.log);
  if (std::strcmp(p->common.game_id, SCS_GAME_ID_EUT2) != 0) {
    fw::log_warning(std::string("only Euro Truck Simulator 2 is supported, not ") + p->common.game_id);
    fw::log_init(nullptr);
    return SCS_RESULT_unsupported;
  }
  g = new State();
  fw::log_info(fw::strf("loading %s (%s, telemetry game version %u.%u)", kVersion, p->common.game_name,
                        SCS_GET_MAJOR_VERSION(p->common.game_version), SCS_GET_MINOR_VERSION(p->common.game_version)));
  load_config();
  load_recovery();
  QueryPerformanceFrequency(&g->frequency);
  QueryPerformanceCounter(&g->last_frame);
  std::vector<std::string> lines;
  const ULONGLONG search_start = GetTickCount64();
  g->camera_ready = g->camera.init(lines);
  lines.push_back(fw::strf("the search in the game executable took %llu ms", GetTickCount64() - search_start));
  log_lines(lines);
  if (g->camera_ready) {
    std::vector<std::string> light_lines;
    g->lights.init(light_lines);
    log_lines(light_lines);
  }
  if (g->camera_ready && g->config.collision_enabled) {
    std::vector<std::string> physx_lines;
    const ULONGLONG physx_start = GetTickCount64();
    g->physx.init(physx_lines);
    g->region_focus.init(physx_lines);
    physx_lines.push_back(fw::strf("physx: the search in the game executable took %llu ms", GetTickCount64() - physx_start));
    log_lines(physx_lines);
  }
  if (!g->camera_ready) fw::log_warning("walking is off: not everything was found in this game build (see the lines above)");
  if (!register_all(p)) {
    fw::log_error("the telemetry callbacks could not be registered");
    delete g;
    g = nullptr;
    fw::log_init(nullptr);
    return SCS_RESULT_generic_error;
  }
  // The hook and the DirectInput filter install only after successful registration: on failure the game unloads the DLL without shutdown,
  // and a leftover redirection would point to unloaded code.
  if (g->camera_ready && !fw::keyboard_hook_start({g->config.key_forward, g->config.key_back, g->config.key_left, g->config.key_right,
                                                   g->config.key_flashlight, g->config.key_flashlight_wider, g->config.key_flashlight_narrower,
                                                   g->config.key_run}))
    fw::log_warning("the keyboard hook could not be installed: the walking keys also reach the truck");
  // The fade window thread also starts only after successful registration (on failure it would keep running in the unloaded DLL's code).
  if (g->camera_ready && !fw::fade_window_start())
    fw::log_warning("the blink overlay window could not be created: the door blink is skipped (the steps remain)");
  if (g->camera_ready && !fw::hint_window_start()) fw::log_warning("the key hint window could not be created: no key hints");
  if (g->camera_ready && g->config.directinput_filter) {
    std::vector<std::string> input_lines;
    // The flashlight keys too: F is the hazard lights in the game by default.
    const int patched = fw::directinput_filter_start({g->config.key_forward, g->config.key_back, g->config.key_left, g->config.key_right,
                                                      g->config.key_run, g->config.key_flashlight, g->config.key_flashlight_wider,
                                                      g->config.key_flashlight_narrower},
                                                     input_lines);
    log_lines(input_lines);
    if (patched == 0) fw::log_warning("the DirectInput keyboard filter could not be installed: the walking keys also reach the truck");
    else fw::log_info(fw::strf("DirectInput keyboard filter installed (%d interface variant(s))", patched));
  }
  fw::log_info("loaded");
  return SCS_RESULT_ok;
}

SCSAPI_VOID scs_telemetry_shutdown(void) {
  if (!g) return;
  fw::settings_window_shutdown();
  for (const auto& line : fw::settings_window_take_log()) fw::log_info(line);
  if (g->camera_ready && g->session.phase() != fw::Phase::Driving) {
    // The game may already have saved the in-walk values before exiting (on Esc): the file stays, the next start writes them back.
    const auto settings = g->camera.saved_settings();
    return_to_truck(true, "the game is unloading the plugin");
    remember_for_recovery(settings);
  }
  fw::fade_window_set(0);
  fw::fade_window_stop();
  fw::hint_window_show("", "");
  fw::hint_window_stop();
  g->physx.stop();
  fw::directinput_filter_stop();
  fw::keyboard_hook_stop();
  fw::log_info("unloaded");
  fw::log_init(nullptr);
  delete g;
  g = nullptr;
}

}  // extern "C"

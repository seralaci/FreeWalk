#pragma once
#include <string>
#include <string_view>
#include <vector>

namespace fw {

// Added to a key code: the key together with Ctrl (only for the command keys: leaving the truck, settings window).
inline constexpr int kKeyCtrl = 0x100;

struct Config {
  // [keys] - Windows virtual key codes
  int key_toggle = 0x79;  // F10
  int key_settings = kKeyCtrl | 0x79;  // Ctrl+F10: the settings window, 0 = disabled
  int key_forward = 'W';
  int key_back = 'S';
  int key_left = 'A';
  int key_right = 'D';
  int key_run = 0x10;  // Shift
  int key_flashlight = 'F';            // flashlight on/off, 0 = disabled
  int key_flashlight_wider = 0x6B;     // NUMPAD+: wider beam, 0 = mouse wheel only
  int key_flashlight_narrower = 0x6D;  // NUMPAD-: narrower beam, 0 = mouse wheel only

  // [walker]
  double walk_speed = 1.4;
  double run_speed = 4.0;
  double eye_height = 1.7;
  bool head_bob = true;
  double head_bob_amplitude = 0.03;
  double exit_side_distance = 1.5;  // m beyond the driver's head position, sideways
  bool exit_face_door = true;       // face the door when leaving (otherwise the truck's front)
  double exit_face_angle = 22.0;    // degrees, turn this much from the door towards the truck's front (negative: towards the rear)
  double ground_offset = 0.0;
  bool door_animation = true;        // animated leaving and entering (blink, steps)
  double door_step_time = 1.3;       // s, on the steps
  double door_fade_time = 0.25;      // s, blink fade to black and back
  double door_out = 0.7;             // m, door sill sideways from the head
  double door_sill_below_head = 1.0; // m, door sill below the head
  double door_min_free = 0.8;        // m, free space needed in front of the door
  double enter_near_door = 3.0;      // m, full entry animation within this
  bool hide_hud = true;              // speedometer and map hidden while walking (g_adviser_keep_hidden)
  bool key_hints = true;             // key hints at the bottom of the screen (get out in the cab, get in at the door)
  double max_distance = 150.0;
  double mouse_sensitivity = 0.25;  // free camera mouse look multiplier while walking (the game's default is 1, which is too sensitive)

  // [collision]
  bool collision_enabled = true;    // ground and collision from the game's physics (applies after a restart)
  double collision_radius = 0.40;   // m, body radius
  double step_height = 0.45;        // m, the walker steps up onto anything lower than this
  double step_down_max = 0.6;       // m, a step down bigger than this is a fall
  double ground_follow_time = 0.12; // s, ground following smoothing

  // [light]
  double light_angle = 28;          // degrees, outer beam angle at the middle setting
  double light_inner_angle = 12;    // degrees, the sharp inner part
  double light_range = 40;          // m
  double light_brightness = 0.3;    // colour multiplier
  double light_r = 1.0;
  double light_g = 0.96;
  double light_b = 0.88;
  double light_fade_time = 0.18;    // s, fade when switching on and off
  double light_hand_right = 0.5;    // m, hand to the right of the eye
  double light_hand_down = 0.21;    // m, hand below the eye
  double light_flicker_chance = 0;  // times per hour on average the bulb plays up
  bool light_own = true;            // own light if no street lamp can be borrowed
  double light_borrow_min = 300;    // m, street lamps closer than this are never taken
  double light_borrow_max = 900;    // m, search within this first

  // [debug]
  bool discovery_log = false;
  bool write_on_frame_start = false;
  bool directinput_filter = false;  // DirectInput keyboard filter (no effect in the live test; for diagnostics)
  bool switch_test = false;  // instead of walking, the key only switches to the free camera and back (for discovery)
};

// Windows virtual key code from a name ("F10", "W", "SHIFT", "0x79"); -1 if the name is unknown.
// "NONE" = 0 (disabled), "CTRL+<name>" = kKeyCtrl | code.
int parse_key(std::string_view name);

// Name of a key code for the INI and the UI ("W", "F10", "SHIFT", "CTRL+F10", "NONE", "0x6A"); the counterpart of parse_key.
std::string key_name(int code);

// Reads the settings from INI text.
// Every missing or invalid value keeps its default; each invalid value adds one line to warnings.
Config parse_config(std::string_view text, std::vector<std::string>& warnings);

}  // namespace fw

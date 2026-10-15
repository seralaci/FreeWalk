#pragma once
#include <initializer_list>
#include <string>
#include <vector>

namespace fw {

// True if the foreground window belongs to this process (the game).
bool game_window_focused();

// True if the key is currently pressed.
bool key_down(int virtual_key);

// Low-level keyboard hook on its own message thread: tracks the given keys,
// and while blocking is on and the game is in the foreground, swallows them from the game.
bool keyboard_hook_start(std::initializer_list<int> keys);
void keyboard_hook_stop();
void keyboard_hook_block(bool blocking);
// Replaces the watched keys (swallowed while walking) if they changed in the settings window.
void keyboard_hook_watch(const std::vector<int>& keys);

// State of a key tracked by the hook; the Windows key state does not see swallowed keys, hence this is needed.
bool keyboard_hook_down(int virtual_key);

// Wheel steps since the last call (up is positive, one notch = 1). The mouse hook starts together with the keyboard hook,
// and while walking (keyboard_hook_block) swallows the wheel from the game.
int mouse_wheel_take();
// Wheel capture: only while walking and not paused (in the game's menu and map the wheel stays with the game).
void mouse_wheel_capture(bool capture);

// The game reads the keyboard through DirectInput, which the low-level hook does not block.
// This filter redirects the two read functions of the DirectInput keyboard device (in the dinput8.dll vtable),
// and hides the walk keys from the game while walking. Returns the number of patched vtables, writes the details to lines.
int directinput_filter_start(std::initializer_list<int> keys, std::vector<std::string>& lines);
void directinput_filter_stop();
// Replaces the hidden keys if they changed in the settings window.
void directinput_filter_watch(const std::vector<int>& keys);

// Diagnostics: the number of game reads since the last call; if someone overwrote our entries, writes them back.
std::string directinput_filter_check();

}  // namespace fw

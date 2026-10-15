# FreeWalk

Get out of your truck and walk around it in Euro Truck Simulator 2.

FreeWalk is a telemetry plugin for Euro Truck Simulator 2 (version 1.61.1.1s, Steam, 64-bit). When your truck is parked, press one key
and you climb down the cab steps and walk around on foot; press it again and you climb back into the cab.

The full user guide (settings, troubleshooting, known limitations) is in [docs/release/README.md](docs/release/README.md).

## Install

1. Close the game.
2. Copy `FreeWalk.dll` and `FreeWalk.ini` to `...\Steam\steamapps\common\Euro Truck Simulator 2\bin\win_x64\plugins\` (if there is no
   `plugins` folder, create it).
3. Start the game and load a profile.
   On startup the game asks whether to allow the telemetry plugin; choose OK.

## Usage

- `F10` in a parked truck with the parking brake on: get out.
- `W A S D` walk, `Shift` run, mouse to look around.
- `F10` again: get back in.

When you get out, during a blink the view moves to the door and you walk down the steps to the ground; if there is not enough room in
front of the door (a wall, another truck), you get out on the other side, and if neither side is free, you do not get out. Next to the
door (within 3 m), `F10` takes you up the steps and after a blink you are in the cab; from farther away there is just the blink. In
exclusive fullscreen mode the blink is not visible, but the steps remain; with `door_animation` switched off on the Leaving the truck
tab, getting out and back in is instant.

While walking, the speedometer and the map are hidden (`hide_hud`). In a stopped truck with the parking brake on, the key for getting
out is shown at the bottom of the screen, and while walking, the key for getting back in is shown in front of the door (`key_hints`), in
the game's language (15 languages, English otherwise); in exclusive fullscreen mode this is not visible either.

The game also receives the walking keys, so in the game's settings (Keys & Buttons) they must not be bound to truck controls.
In the default controls, W, S, A, D are the secondary keys for throttle, brake and steering: remove these (the primary arrow keys stay),
or choose walking keys in the `[keys]` section of `FreeWalk.ini` that are free in the game.

`F10` is the game's default screenshot key: if you do not want a screenshot every time you get out, move the screenshot to another key,
or choose a different get out key with `[keys] toggle` in `FreeWalk.ini`.

## Settings

`Ctrl+F10` (while driving or walking) opens the settings window: a dark panel over the game that can be moved by its title bar.
The tabs: Keys, Walking, Leaving the truck, Collision, Light, Debug.

- Changes apply immediately; Save writes them to `FreeWalk.ini`, while Cancel, `Esc`, the X in the top right corner or `Ctrl+F10` again
  restores the values from when the window was opened.
- Numbers can be set with the slider, step by step with the mouse wheel, or by clicking the number and typing
  (`Enter`: done, `Esc`: cancel).
- To set a key: click its button, then press the new key (`Esc`: keep the old one, `Backspace`: NONE, that is no key, where allowed).
  If two functions would get the same key, a red line at the bottom of the window says so, and Save is disabled until it is resolved.
- Keys changed while walking take effect from the next time you get out; Debug settings marked with an asterisk take effect after the
  game is restarted. The DirectInput filter (Debug) also picks up changed walking keys only after a restart.
- Save rewrites the whole `FreeWalk.ini`; if the file contains a line the plugin does not know, the window warns about it in advance.
- If the game runs in exclusive fullscreen mode, opening the window may minimize the game to the taskbar; in windowed or borderless mode
  the window appears over the game.

The plugin writes to `game.log.txt` (`Documents\Euro Truck Simulator 2\game.log.txt`); every line starts with `[FreeWalk]`.

## Ground and collision

The walker follows the ground, slopes and stairs using the game's own physics, and does not pass through walls, fences or the truck,
but slides along them; it falls off ledges.
On its own, the game loads the collision geometry of the surroundings only near vehicles; while walking, FreeWalk also requests it
around the walker (using the game's own function), so buildings, fences and poles stop you even far from the truck.
Known limitations: cars standing in parking spots have no collision in the game at all (the game gives them none), so you can walk
through them; some props (for example benches, small props, parked trailers) far from the truck can be walked through, because the game
creates their collision only near the player's truck.
Where there is no collision geometry, the ground stays flat at the last known height.

The `[collision]` section of `FreeWalk.ini` (also on the Collision tab of the settings window):

| Key | Default | Meaning |
|---|---|---|
| `enabled` | 1 | ground and collision from the game's physics (a change takes effect after a restart) |
| `radius` | 0.4 | radius of the body (m) |
| `step_height` | 0.45 | the walker steps up onto steps lower than this (m) |
| `step_down_max` | 0.6 | a step down larger than this is a fall (m) |
| `ground_follow_time` | 0.12 | smoothing of ground following (s) |

If the physics is not available, walking uses flat ground, and the `physx:` lines in the log say why.

## Flashlight

While walking, `F` switches the flashlight on and off; the mouse wheel or the numeric keypad `+`/`-` makes the beam wider or narrower.
At night the flashlight "borrows" a distant street light of the game (by default at least 300 m away) and puts it in your hand; when
you switch it off or get back in, the light is returned to its place. On the Light tab of the settings window you can set the beam
angle, range, colour, brightness, hand position, fading and flicker frequency; the keys are on the Keys tab.

## Building on Windows

Requirements: Visual Studio or Build Tools with the "Desktop development with C++" workload, CMake 3.20+ and Ninja
(`winget install Kitware.CMake` and `winget install Ninja-build.Ninja`).

    build.cmd

The script sets up the MSVC environment, builds the core tests, the DLL and the probe tool in one build, then runs the tests.
The finished DLL: `build\msvc-Release\FreeWalk.dll` (the runtime library is linked statically; next to it is `FreeWalk.pdb` for
debugging).
Debug build: `build.cmd Debug`.

The probe tool checks on the game executable whether the plugin's searches find their targets:

    build\msvc-Release\freewalk_probe.exe "<game folder>\bin\win_x64\eurotrucks2.exe"

## Release package

    package.cmd [name] [-Zip]

After `build.cmd`, this copies the DLL, the INI, the English user guide (`docs\release\README.md`, as `README.md` in the package),
`LICENSE` (MIT) and `THIRD-PARTY-NOTICES.txt` (the SCS SDK license) to the `dist\FreeWalk-<version>[-<name>]\` folder.

The PDB is not included in the package; `-Zip` also creates `dist\FreeWalk-<version>.zip` next to it.

### Releasing on GitHub

`.github/workflows/release.yml` runs when a tag starting with `v` is pushed: it builds, runs the tests, checks that the tag matches
`kVersion`, creates the ZIP, and creates the GitHub Release with automatically generated release notes (a version with a hyphen is a
pre-release). The PDB is stored among the run's artifacts (for 90 days).

    git tag v0.8.3
    git push origin main v0.8.3

## Cross-compiling from macOS

    brew install cmake ninja mingw-w64
    cmake -S . -B build/host -G Ninja && cmake --build build/host && ctest --test-dir build/host --output-on-failure
    cmake -S . -B build/win -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake -DCMAKE_BUILD_TYPE=Release && cmake --build build/win

The finished DLL: `build/win/FreeWalk.dll`.

## License

FreeWalk is released under the MIT License (see `LICENSE`). It includes the SCS Telemetry SDK headers, which are also under the MIT
License (see `THIRD-PARTY-NOTICES.txt`).

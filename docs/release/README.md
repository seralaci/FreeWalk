# FreeWalk

**Get out of your truck and walk around in Euro Truck Simulator 2 and American Truck Simulator.**

FreeWalk is a telemetry plugin for Euro Truck Simulator 2 and American Truck Simulator. When your truck is parked, press one key and you climb down the cab steps
and walk around on foot: around your truck, across the yard, up to the buildings. Press the key again and you climb back into the cab.

This document covers FreeWalk 0.9.x.

---

## Features

- **Get out and walk.** Leave the cab of a parked truck, walk and run with the keyboard, look around with the mouse.
- **Animated doors.** You turn toward the door, a short blink, then you step down the cab steps. If something blocks the driver's
  door (a wall, another truck), you get out on the passenger side. Getting back in from the door: a blink, and you climb into the
  seat in the cab view, turning to look ahead.
- **No driver left behind.** While you walk, the driver no longer sits in the cab.
- **Real ground and walls.** The walker follows the ground, slopes and stairs from the game's own physics. Buildings, fences, poles and
  your truck stop you, and you slide along them instead of walking through.
- **Flashlight.** At night, switch on a flashlight and change the width of its beam with the mouse wheel.
- **Clean view.** The speedometer, the map and the mirrors are hidden while you walk.
- **Key hints.** A small hint at the bottom of the screen shows which key gets you out (parked, parking brake on) or back in (at the
  door). The hint uses the game's language for 15 languages, and English for all others.
- **Settings window.** Change every key and setting in the game with `Ctrl+F10`. Changes apply at once.

---

## Requirements

- Euro Truck Simulator 2, version **1.61.1.9s** (or 1.61.1.1s), or American Truck Simulator, version **1.62.1.1** (or 1.61.3.18s,
  1.61.3.1s); Steam, 64-bit.
  FreeWalk reads the game's memory and only switches itself on for the exact game builds it was made for. On any other version the
  game still runs normally, but walking stays off, and the log says why.
- Windows 10 or 11, 64-bit.
- Recommended: **windowed** or **borderless** display mode. In exclusive fullscreen the blink and the key hints cannot be drawn over
  the game. Walking still works without them.

---

## Installation

1. Close the game.
2. Open the game's plugin folder:
   `...\Steam\steamapps\common\Euro Truck Simulator 2\bin\win_x64\plugins\`
   (American Truck Simulator: `...\Steam\steamapps\common\American Truck Simulator\bin\win_x64\plugins\`)
   If there is no `plugins` folder, create it.
3. Copy `FreeWalk.dll` and `FreeWalk.ini` into that folder (the other files of the package are documentation).
4. Start the game and load a profile.
   The first time, the game asks whether to allow advanced SDK (telemetry) plugins. Choose **OK**.

**Updating:** close the game and replace `FreeWalk.dll`. Keep your own `FreeWalk.ini`. Settings that are
missing from it use their default values.

---

## Usage

1. Stop the truck and apply the **parking brake**. The hint `F10 Get out` appears at the bottom of the screen.
2. Press **F10** to get out.
3. Walk around. Walk up to the door and press **F10** again to climb back in.

| Key | Action |
|---|---|
| `F10` | Get out / get back in |
| `W` `A` `S` `D` | Walk |
| `Shift` | Run |
| Mouse | Look around |
| `F` | Flashlight on / off |
| Mouse wheel, `Num +` / `Num -` | Wider / narrower flashlight beam |
| `Ctrl+F10` | Settings window |

All keys can be changed in the settings window.

**Getting back in**
- From up to 3 m in front of the door, you walk to the steps and climb up. From farther away, there is just a short blink.
- You are also put back in the cab automatically if you walk too far (150 m by default), if the truck is moved, or if you select
  another camera.

**Important: free the walking keys in the game.** The game also receives the walking keys. In the game's default controls, `W`, `S`,
`A`, `D` are secondary keys for throttle, brake and steering, and `F` is the hazard lights. Either remove these bindings in the game
(*Options → Keys & Buttons*), or choose free keys for FreeWalk in its settings window.

`F10` is the game's default screenshot key. To avoid a screenshot every time you get out, move the screenshot to another key in the
game, or choose a different key for FreeWalk.

---

## Settings

Press **`Ctrl+F10`** (while driving or walking) to open the settings window over the game. It has these tabs:

| Tab | What you can change |
|---|---|
| **Keys** | Every key, including the get out key and the settings window key |
| **Walking** | Walking and running speed, eye height, head bob, mouse sensitivity, how far you may walk |
| **Leaving the truck** | Door animation and its timing, which way you face, hiding the speedometer, the map and the driver, key hints |
| **Collision** | Ground and walls from the game's physics, body radius, step height |
| **Light** | Flashlight beam, range, colour, brightness, flicker |
| **Debug** | Diagnostic options (normally leave these off) |

- Changes apply immediately. **Save** writes them to `FreeWalk.ini`. **Cancel**, `Esc` or the X restores the values you had when you
  opened the window.
- To set a key, click its button, then press the new key. `Esc` keeps the old key, and `Backspace` clears it where that is allowed.
- Settings marked with `*` take effect after the game is restarted.

You can also edit `FreeWalk.ini` with a text editor while the game is closed. Every line is optional, and every setting has a short
comment.

---

## Known limitations

- **Parked cars in car parks:** you can walk through the cars that stand in parking spots. The game gives them no collision at all.
  Your own truck, buildings, fences and poles do stop you.
- **Props far from your truck:** the game only creates collision for some objects (benches, small props, parked trailers) near the
  player's truck, so far away you can walk through them.
- **Exclusive fullscreen:** no blink and no key hints (see *Requirements*).
- **Game version:** works only with the game builds listed above. A game update needs a new FreeWalk version.

---

## Troubleshooting

FreeWalk writes its own log, `FreeWalk.log`, next to `FreeWalk.dll` in the `plugins` folder (if that folder cannot be written:
`Documents\Euro Truck Simulator 2\FreeWalk.log` or `Documents\American Truck Simulator\FreeWalk.log`). It starts anew at every game start. To also get the lines in the game's own log
(`game.log.txt`), switch on *Debug → Also log to the game's game.log.txt*.

| Problem | What to check |
|---|---|
| Nothing happens on `F10` | The truck must be stopped with the parking brake on. The log line `not leaving the truck: ...` gives the reason. |
| "walking is off" in the log | Your game version differs from the supported one. |
| The truck moves or a light goes on while walking | The game also receives the key. Free it in the game's controls (see *Usage*). |
| Walking through walls | Look for `physx:` lines in the log. If the game's physics is not available, FreeWalk falls back to flat ground. |
| The game was closed while walking | Nothing to do. On the next start FreeWalk restores the game settings it changed (developer mode, camera speed, hidden mirrors and HUD), using its `FreeWalk.recovery` file. |

If you report a problem, please attach `FreeWalk.log` (it also shows the version, in the line `loading <version>`).

---

## Uninstalling

Close the game. Delete `FreeWalk.dll`, `FreeWalk.ini`, `FreeWalk.log` and, if present, `FreeWalk.recovery` from the `plugins` folder.

---

## Notes

- FreeWalk does not change any game files. While the game runs, it reads the game's memory, calls some of the game's own functions,
  and temporarily sets a few console variables, which it restores when you get back in.
- FreeWalk is a fan-made plugin. It is not affiliated with or endorsed by SCS Software.
- Use at your own risk. Back up your profile before trying any new plugin.

---

## License

FreeWalk is released under the MIT License (see `LICENSE`). It includes the SCS Telemetry SDK headers, which are also under the MIT
License (see `THIRD-PARTY-NOTICES.txt`).

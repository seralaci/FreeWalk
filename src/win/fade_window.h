#pragma once

namespace fw {

// Black, transparent, click-through, non-activating window above the game window, on its own message thread; for the exit and entry blink.
// Not visible in (exclusive) fullscreen mode.
bool fade_window_start();
// 0 = hidden, 1 = fully black; callable from any thread (posts a message to the window's thread).
void fade_window_set(double blackout);
void fade_window_stop();

}  // namespace fw

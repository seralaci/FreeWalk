#pragma once
#include <string>

namespace fw {

// Key hint at the bottom center of the game window (a key badge with the text beside it), on its own message thread; click-through,
// does not take focus, and hides when the game is not in the foreground. Not visible in (exclusive) fullscreen mode.
bool hint_window_start();
// Shows the hint (UTF-8 strings); hides it on empty text. Callable from any thread; only posts a message on change.
void hint_window_show(const std::string& key, const std::string& text);
void hint_window_stop();

}  // namespace fw

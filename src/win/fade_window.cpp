#include "win/fade_window.h"
#define NOMINMAX
#include <windows.h>
#include <algorithm>
#include <atomic>

namespace fw {
namespace {

constexpr UINT kSetAlpha = WM_APP + 1;
HANDLE g_thread = nullptr;
DWORD g_thread_id = 0;
std::atomic<HWND> g_window{nullptr};
std::atomic<int> g_last_alpha{-1};

// The game's main window: the largest visible top-level window of the process (other than the fade window).
HWND game_window() {
  struct Search {
    HWND best = nullptr;
    long area = 0;
  } s;
  EnumWindows(
      [](HWND w, LPARAM p) -> BOOL {
        auto* s = reinterpret_cast<Search*>(p);
        DWORD pid = 0;
        GetWindowThreadProcessId(w, &pid);
        if (pid != GetCurrentProcessId() || w == g_window.load() || !IsWindowVisible(w) || GetWindow(w, GW_OWNER)) return TRUE;
        RECT r{};
        GetClientRect(w, &r);
        const long area = (r.right - r.left) * (r.bottom - r.top);
        if (area > s->area) {
          s->area = area;
          s->best = w;
        }
        return TRUE;
      },
      reinterpret_cast<LPARAM>(&s));
  return s.best;
}

// Whether the game (or another of its windows) is in the foreground: the topmost black layer must not stay above another application.
bool game_in_foreground() {
  DWORD pid = 0;
  GetWindowThreadProcessId(GetForegroundWindow(), &pid);
  return pid == GetCurrentProcessId();
}

void hide(HWND w) {
  KillTimer(w, 1);
  ShowWindow(w, SW_HIDE);
  g_last_alpha = -1;  // the next set shows it again
}

LRESULT CALLBACK window_proc(HWND w, UINT message, WPARAM wp, LPARAM lp) {
  if (message == WM_TIMER) {
    if (!game_in_foreground()) hide(w);
    return 0;
  }
  if (message == kSetAlpha) {
    const int alpha = static_cast<int>(wp);
    if (alpha <= 0 || !game_in_foreground()) {
      hide(w);
      return 0;
    }
    // While visible, we check every tenth of a second whether the game is in the foreground (during Alt+Tab the game sends no frames).
    SetTimer(w, 1, 100, nullptr);
    if (HWND game = game_window()) {
      RECT r{};
      GetClientRect(game, &r);
      POINT origin{0, 0};
      ClientToScreen(game, &origin);
      SetWindowPos(w, HWND_TOPMOST, origin.x, origin.y, r.right - r.left, r.bottom - r.top, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }
    SetLayeredWindowAttributes(w, 0, static_cast<BYTE>(std::min(alpha, 255)), LWA_ALPHA);
    return 0;
  }
  if (message == WM_PAINT) {
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(w, &ps);
    FillRect(dc, &ps.rcPaint, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
    EndPaint(w, &ps);
    return 0;
  }
  return DefWindowProcW(w, message, wp, lp);
}

DWORD WINAPI fade_thread(LPVOID started) {
  WNDCLASSW wc{};
  wc.lpfnWndProc = window_proc;
  wc.hInstance = GetModuleHandleW(nullptr);
  wc.lpszClassName = L"FreeWalkFade";
  wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
  RegisterClassW(&wc);
  HWND w = CreateWindowExW(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW, wc.lpszClassName, L"",
                           WS_POPUP, 0, 0, 1, 1, nullptr, nullptr, wc.hInstance, nullptr);
  g_window = w;
  *static_cast<std::atomic<bool>*>(started) = w != nullptr;
  if (!w) return 1;
  MSG m;
  while (GetMessageW(&m, nullptr, 0, 0) > 0) DispatchMessageW(&m);
  DestroyWindow(w);
  g_window = nullptr;
  UnregisterClassW(wc.lpszClassName, wc.hInstance);
  return 0;
}

}  // namespace

bool fade_window_start() {
  if (g_thread) return g_window.load() != nullptr;
  static std::atomic<bool> started{false};
  started = false;
  g_thread = CreateThread(nullptr, 0, fade_thread, &started, 0, &g_thread_id);
  if (!g_thread) return false;
  for (int i = 0; i < 200 && !started && WaitForSingleObject(g_thread, 10) == WAIT_TIMEOUT; ++i) {
  }
  return started;
}

void fade_window_set(double blackout) {
  HWND w = g_window.load();
  if (!w) return;
  const int alpha = static_cast<int>(std::clamp(blackout, 0.0, 1.0) * 255.0 + 0.5);
  if (alpha == g_last_alpha.load()) return;
  if (PostMessageW(w, kSetAlpha, static_cast<WPARAM>(alpha), 0)) g_last_alpha = alpha;
}

void fade_window_stop() {
  if (!g_thread) return;
  PostThreadMessageW(g_thread_id, WM_QUIT, 0, 0);
  WaitForSingleObject(g_thread, 2000);
  CloseHandle(g_thread);
  g_thread = nullptr;
  g_thread_id = 0;
  g_last_alpha = -1;
}

}  // namespace fw

#include "win/hint_window.h"
#define NOMINMAX
#include <windows.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <mutex>
// The GDI+ headers use min/max unqualified; we give them the std versions instead of the windows.h macros.
namespace Gdiplus {
using std::max;
using std::min;
}  // namespace Gdiplus
// The Windows SDK GDI+ headers warn under /W4 about parameter names shadowing members; this is not our code.
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4458)
#endif
#include <gdiplus.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

namespace fw {
namespace {

constexpr UINT kUpdate = WM_APP + 1;
constexpr UINT_PTR kAnimationTimer = 1;
constexpr UINT_PTR kForegroundTimer = 2;
constexpr double kFadeSeconds = 0.16;  // fade-in and fade-out time

HANDLE g_thread = nullptr;
DWORD g_thread_id = 0;
std::atomic<HWND> g_window{nullptr};
std::mutex g_mutex;
std::wstring g_key;   // under g_mutex
std::wstring g_text;  // under g_mutex; empty: no hint
std::string g_last_key, g_last_text;  // on the caller side: the last sent

// State of the window thread (used only there).
struct View {
  std::wstring key, text;  // the drawn content
  int width = 0, height = 0;
  int game_x = 0, game_y = 0, game_w = 0, game_h = 0;
  double scale = 1;
  double alpha = 0;       // 0..1, fade-in/fade-out progress
  bool wanted = false;    // should be visible (there is text, and the game is in the foreground)
  ULONGLONG last_tick = 0;
  HDC memory_dc = nullptr;
  HBITMAP bitmap = nullptr;
  HGDIOBJ old_bitmap = nullptr;
} g_view;

std::wstring wide(const std::string& utf8) {
  if (utf8.empty()) return {};
  const int n = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
  std::wstring out(static_cast<std::size_t>(n), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), out.data(), n);
  return out;
}

bool game_in_foreground() {
  DWORD pid = 0;
  GetWindowThreadProcessId(GetForegroundWindow(), &pid);
  return pid == GetCurrentProcessId();
}

// The game's main window: the largest visible top-level, non-layered window of the process.
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
        if (pid != GetCurrentProcessId() || !IsWindowVisible(w) || GetWindow(w, GW_OWNER)) return TRUE;
        if (GetWindowLongW(w, GWL_EXSTYLE) & WS_EX_LAYERED) return TRUE;
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

void add_round_rect(Gdiplus::GraphicsPath& path, float x, float y, float w, float h, float r) {
  r = std::min(r, std::min(w, h) / 2);
  const float d = 2 * r;
  path.AddArc(x, y, d, d, 180, 90);
  path.AddArc(x + w - d, y, d, d, 270, 90);
  path.AddArc(x + w - d, y + h - d, d, d, 0, 90);
  path.AddArc(x, y + h - d, d, d, 90, 90);
  path.CloseFigure();
}

void release_bitmap() {
  if (g_view.memory_dc) {
    SelectObject(g_view.memory_dc, g_view.old_bitmap);
    DeleteObject(g_view.bitmap);
    DeleteDC(g_view.memory_dc);
  }
  g_view.memory_dc = nullptr;
  g_view.bitmap = nullptr;
}

// Draws the hint into a premultiplied-alpha image, in the style of the game's own key hints: small, nearly black, translucent box
// with a thin light border, slight rounding and a colored stripe at the bottom, inside a small white key square, the text beside it.
void render() {
  const float s = static_cast<float>(g_view.scale);
  Gdiplus::FontFamily family(L"Segoe UI");
  Gdiplus::Font text_font(&family, 17.0f * s, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
  Gdiplus::Font key_font(&family, 13.0f * s, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
  Gdiplus::StringFormat format(Gdiplus::StringFormat::GenericTypographic());
  format.SetFormatFlags(format.GetFormatFlags() | Gdiplus::StringFormatFlagsMeasureTrailingSpaces);

  // Sizes: text widths measured on a scratch image.
  Gdiplus::RectF key_box, text_box;
  {
    Gdiplus::Bitmap probe(1, 1, PixelFormat32bppPARGB);
    Gdiplus::Graphics m(&probe);
    m.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAlias);
    m.MeasureString(g_view.key.c_str(), -1, &key_font, Gdiplus::PointF(0, 0), &format, &key_box);
    m.MeasureString(g_view.text.c_str(), -1, &text_font, Gdiplus::PointF(0, 0), &format, &text_box);
  }
  const float margin = 4.0f * s;  // for the faint shadow
  const float box_h = 34.0f * s;
  const float cap_h = 20.0f * s;
  const float cap_w = std::max(cap_h, key_box.Width + 10.0f * s);
  const float inset = (box_h - cap_h) / 2;
  const float gap = 10.0f * s;
  const float box_w = inset + cap_w + gap + text_box.Width + 14.0f * s;
  g_view.width = static_cast<int>(std::ceil(box_w + 2 * margin));
  g_view.height = static_cast<int>(std::ceil(box_h + 2 * margin));

  release_bitmap();
  BITMAPINFO info{};
  info.bmiHeader.biSize = sizeof info.bmiHeader;
  info.bmiHeader.biWidth = g_view.width;
  info.bmiHeader.biHeight = -g_view.height;  // top-down
  info.bmiHeader.biPlanes = 1;
  info.bmiHeader.biBitCount = 32;
  info.bmiHeader.biCompression = BI_RGB;
  void* bits = nullptr;
  HDC screen = GetDC(nullptr);
  g_view.memory_dc = CreateCompatibleDC(screen);
  ReleaseDC(nullptr, screen);
  g_view.bitmap = CreateDIBSection(g_view.memory_dc, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
  if (!g_view.bitmap || !bits) {
    release_bitmap();
    return;
  }
  g_view.old_bitmap = SelectObject(g_view.memory_dc, g_view.bitmap);
  std::memset(bits, 0, static_cast<std::size_t>(g_view.width) * g_view.height * 4);

  Gdiplus::Bitmap canvas(g_view.width, g_view.height, g_view.width * 4, PixelFormat32bppPARGB, static_cast<BYTE*>(bits));
  Gdiplus::Graphics g(&canvas);
  g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
  g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
  g.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAlias);
  g.SetCompositingQuality(Gdiplus::CompositingQualityHighQuality);

  const float x = margin, y = margin;
  const float radius = 4.0f * s;
  // Faint shadow around the box so it stands out on light backgrounds too.
  for (int i = 3; i >= 1; --i) {
    const float spread = static_cast<float>(i) * s;
    Gdiplus::GraphicsPath shadow;
    add_round_rect(shadow, x - spread, y - spread, box_w + 2 * spread, box_h + 2 * spread, radius + spread);
    Gdiplus::SolidBrush brush(Gdiplus::Color(static_cast<BYTE>(14 * (4 - i)), 0, 0, 0));
    g.FillPath(&brush, &shadow);
  }
  // The box: nearly black, translucent, with a thin light gray border.
  Gdiplus::GraphicsPath box;
  add_round_rect(box, x, y, box_w, box_h, radius);
  Gdiplus::SolidBrush box_brush(Gdiplus::Color(215, 20, 20, 22));
  g.FillPath(&box_brush, &box);
  // Thin colored stripe at the bottom (as in the game's hints; with the settings window accent color), clipped to the box shape.
  g.SetClip(&box);
  Gdiplus::SolidBrush stripe(Gdiplus::Color(255, 0x2E, 0xC4, 0xB6));  // the settings window accent color
  g.FillRectangle(&stripe, x, y + box_h - 2.0f * s, box_w, 2.0f * s);
  g.ResetClip();
  Gdiplus::Pen frame(Gdiplus::Color(120, 150, 150, 155), std::max(1.0f, 1.0f * s));
  Gdiplus::GraphicsPath frame_path;
  add_round_rect(frame_path, x + 0.5f, y + 0.5f, box_w - 1, box_h - 1, radius);
  g.DrawPath(&frame, &frame_path);

  // The key: small white square with a dark letter.
  const float cx = x + inset, cy = y + inset;
  Gdiplus::GraphicsPath cap;
  add_round_rect(cap, cx, cy, cap_w, cap_h, 2.5f * s);
  Gdiplus::SolidBrush cap_brush(Gdiplus::Color(255, 236, 236, 238));
  g.FillPath(&cap_brush, &cap);
  Gdiplus::SolidBrush key_brush(Gdiplus::Color(255, 24, 24, 26));
  g.DrawString(g_view.key.c_str(), -1, &key_font,
               Gdiplus::PointF(cx + (cap_w - key_box.Width) / 2, cy + (cap_h - key_box.Height) / 2), &format, &key_brush);

  // The text.
  Gdiplus::SolidBrush text_brush(Gdiplus::Color(255, 240, 240, 240));
  g.DrawString(g_view.text.c_str(), -1, &text_font, Gdiplus::PointF(cx + cap_w + gap, y + (box_h - text_box.Height) / 2), &format,
               &text_brush);
}

// The window is at the bottom center of the game window; during fade-in/out it fades and slides up slightly.
void present(HWND w) {
  if (!g_view.memory_dc) return;
  const double eased = 1 - std::pow(1 - g_view.alpha, 3);
  const int lift = static_cast<int>((1 - eased) * 6.0 * g_view.scale);
  POINT position{g_view.game_x + (g_view.game_w - g_view.width) / 2, g_view.game_y + g_view.game_h * 84 / 100 - g_view.height / 2 + lift};
  SIZE size{g_view.width, g_view.height};
  POINT source{0, 0};
  BLENDFUNCTION blend{AC_SRC_OVER, 0, static_cast<BYTE>(std::clamp(eased, 0.0, 1.0) * 255.0), AC_SRC_ALPHA};
  UpdateLayeredWindow(w, nullptr, &position, &size, g_view.memory_dc, &source, 0, &blend, ULW_ALPHA);
}

void step_animation(HWND w) {
  const ULONGLONG now = GetTickCount64();
  const double dt = g_view.last_tick ? static_cast<double>(now - g_view.last_tick) / 1000.0 : 0.0;
  g_view.last_tick = now;
  const double target = g_view.wanted ? 1.0 : 0.0;
  const double change = dt / kFadeSeconds;
  g_view.alpha = target > g_view.alpha ? std::min(target, g_view.alpha + change) : std::max(target, g_view.alpha - change);
  if (g_view.alpha <= 0) {
    ShowWindow(w, SW_HIDE);
    KillTimer(w, kAnimationTimer);
    g_view.last_tick = 0;
    return;
  }
  present(w);
  if (g_view.alpha == target) {
    KillTimer(w, kAnimationTimer);
    g_view.last_tick = 0;
  }
}

void start_animation(HWND w) {
  if (g_view.wanted && !IsWindowVisible(w)) {
    present(w);
    SetWindowPos(w, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
  }
  g_view.last_tick = GetTickCount64();
  SetTimer(w, kAnimationTimer, 15, nullptr);
}

// Updates the content and position; redraws on new text or window size.
void update(HWND w) {
  std::wstring key, text;
  {
    std::lock_guard lock(g_mutex);
    key = g_key;
    text = g_text;
  }
  HWND game = game_window();
  const bool have_text = !text.empty() && game;
  if (have_text) {
    RECT gr{};
    GetClientRect(game, &gr);
    POINT origin{0, 0};
    ClientToScreen(game, &origin);
    const int gw = gr.right - gr.left, gh = gr.bottom - gr.top;
    const double scale = std::clamp(gh / 1080.0, 0.6, 2.5);
    const bool redraw = key != g_view.key || text != g_view.text || std::fabs(scale - g_view.scale) > 1e-6 || !g_view.memory_dc;
    g_view.game_x = origin.x;
    g_view.game_y = origin.y;
    g_view.game_w = gw;
    g_view.game_h = gh;
    g_view.scale = scale;
    // While fading out the old text stays until it disappears.
    if (redraw) {
      g_view.key = key;
      g_view.text = text;
      render();
    }
    SetTimer(w, kForegroundTimer, 100, nullptr);
  } else {
    KillTimer(w, kForegroundTimer);
  }
  g_view.wanted = have_text && game_in_foreground();
  start_animation(w);
}

LRESULT CALLBACK window_proc(HWND w, UINT message, WPARAM wp, LPARAM lp) {
  switch (message) {
    case kUpdate: update(w); return 0;
    case WM_TIMER:
      if (wp == kAnimationTimer) step_animation(w);
      else if (wp == kForegroundTimer && g_view.wanted != game_in_foreground()) update(w);
      return 0;
    default: return DefWindowProcW(w, message, wp, lp);
  }
}

DWORD WINAPI hint_thread(LPVOID started) {
  ULONG_PTR gdiplus = 0;
  Gdiplus::GdiplusStartupInput input;
  if (Gdiplus::GdiplusStartup(&gdiplus, &input, nullptr) != Gdiplus::Ok) {
    *static_cast<std::atomic<bool>*>(started) = false;
    return 1;
  }
  WNDCLASSW wc{};
  wc.lpfnWndProc = window_proc;
  wc.hInstance = GetModuleHandleW(nullptr);
  wc.lpszClassName = L"FreeWalkHint";
  RegisterClassW(&wc);
  HWND w = CreateWindowExW(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW, wc.lpszClassName, L"",
                           WS_POPUP, 0, 0, 1, 1, nullptr, nullptr, wc.hInstance, nullptr);
  g_window = w;
  *static_cast<std::atomic<bool>*>(started) = w != nullptr;
  if (w) {
    MSG m;
    while (GetMessageW(&m, nullptr, 0, 0) > 0) DispatchMessageW(&m);
    DestroyWindow(w);
  }
  g_window = nullptr;
  release_bitmap();
  g_view = View{};
  UnregisterClassW(wc.lpszClassName, wc.hInstance);
  Gdiplus::GdiplusShutdown(gdiplus);
  return w ? 0 : 1;
}

}  // namespace

bool hint_window_start() {
  if (g_thread) return g_window.load() != nullptr;
  static std::atomic<bool> started{false};
  started = false;
  g_thread = CreateThread(nullptr, 0, hint_thread, &started, 0, &g_thread_id);
  if (!g_thread) return false;
  for (int i = 0; i < 200 && !started && WaitForSingleObject(g_thread, 10) == WAIT_TIMEOUT; ++i) {
  }
  return started;
}

void hint_window_show(const std::string& key, const std::string& text) {
  HWND w = g_window.load();
  if (!w || (key == g_last_key && text == g_last_text)) return;
  {
    std::lock_guard lock(g_mutex);
    g_key = wide(key);
    g_text = wide(text);
  }
  if (PostMessageW(w, kUpdate, 0, 0)) {
    g_last_key = key;
    g_last_text = text;
  }
}

void hint_window_stop() {
  if (!g_thread) return;
  PostThreadMessageW(g_thread_id, WM_QUIT, 0, 0);
  WaitForSingleObject(g_thread, 2000);
  CloseHandle(g_thread);
  g_thread = nullptr;
  g_thread_id = 0;
  g_last_key.clear();
  g_last_text.clear();
}

}  // namespace fw

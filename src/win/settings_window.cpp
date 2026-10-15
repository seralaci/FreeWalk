#include "win/settings_window.h"
#include <windowsx.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <utility>
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
#include "core/settings.h"
#include "core/text.h"

namespace fw {
namespace {

constexpr wchar_t kClassName[] = L"FreeWalkSettingsWindow";

// Between the game thread and the window thread: the requested settings and the lines meant for the log.
std::mutex g_mutex;
std::optional<Config> g_wanted;
std::vector<std::string> g_log;
HANDLE g_thread = nullptr;
std::atomic<bool> g_open{false};
std::atomic<bool> g_stop{false};
std::atomic<HWND> g_window{nullptr};

void push_wanted(const Config& c) {
  std::lock_guard lock(g_mutex);
  g_wanted = c;
}

void push_log(const std::string& line) {
  std::lock_guard lock(g_mutex);
  g_log.push_back(line);
}

// Labels and values are ASCII, so they can be converted character by character.
std::wstring widen(std::string_view s) {
  std::wstring w;
  for (const char c : s) w.push_back(static_cast<wchar_t>(static_cast<unsigned char>(c)));
  return w;
}

std::string narrow(const std::wstring& w) {
  std::string s;
  for (const wchar_t c : w) s.push_back(c < 0x80 ? static_cast<char>(c) : '?');
  return s;
}

HMODULE this_module() {
  HMODULE module = nullptr;
  GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                     reinterpret_cast<LPCWSTR>(&this_module), &module);
  return module;
}

const char* tab_note(std::string_view tab) {
  if (tab == "Keys")
    return "Remove these keys from the truck controls in the game (Options, Keys & Buttons), or the truck also gets them. "
           "Click a key, then press the new one (Esc: keep the old one, Backspace: none where allowed). "
           "Keys changed during a walk take effect at the next exit.";
  if (tab == "Collision")
    return "* takes effect after restarting the game. Without the game's physics (or far from the truck) the ground stays flat.";
  if (tab == "Light")
    return "The flashlight borrows a distant street lamp at night, or makes a light of its own. Changes show at once while it is on.";
  if (tab == "Debug") return "* takes effect after restarting the game. The DirectInput filter also takes changed walking keys only after a restart.";
  return nullptr;
}

double round_to(double v, int decimals) {
  const double scale = std::pow(10.0, decimals);
  return std::round(v * scale) / scale;
}

// Color scheme: dark panel, teal accent.
namespace color {
const Gdiplus::Color background(255, 0x1B, 0x1D, 0x21);
const Gdiplus::Color header(255, 0x16, 0x18, 0x1B);
const Gdiplus::Color row(255, 0x23, 0x26, 0x2B);
const Gdiplus::Color row_hover(255, 0x29, 0x2D, 0x33);
const Gdiplus::Color line(255, 0x33, 0x37, 0x3D);
const Gdiplus::Color text(255, 0xE6, 0xE8, 0xEB);
const Gdiplus::Color dim(255, 0x9A, 0xA0, 0xA6);
const Gdiplus::Color accent(255, 0x2E, 0xC4, 0xB6);
const Gdiplus::Color accent_hover(255, 0x45, 0xD6, 0xC9);
const Gdiplus::Color accent_dark(255, 0x10, 0x2A, 0x28);
const Gdiplus::Color on_accent(255, 0x0E, 0x1B, 0x1A);
const Gdiplus::Color button(255, 0x2C, 0x30, 0x36);
const Gdiplus::Color button_hover(255, 0x36, 0x3B, 0x42);
const Gdiplus::Color off(255, 0x3A, 0x3F, 0x46);
const Gdiplus::Color knob_off(255, 0xC8, 0xCC, 0xD0);
const Gdiplus::Color white(255, 0xFF, 0xFF, 0xFF);
const Gdiplus::Color error(255, 0xFF, 0x6B, 0x6B);
constexpr COLORREF edit_background = RGB(0x2C, 0x30, 0x36);
constexpr COLORREF edit_text = RGB(0xE6, 0xE8, 0xEB);
}  // namespace color

struct Box {
  int x = 0;
  int y = 0;
  int w = 0;
  int h = 0;
  bool contains(POINT p) const { return p.x >= x && p.x < x + w && p.y >= y && p.y < y + h; }
  Gdiplus::RectF f() const {
    return {static_cast<Gdiplus::REAL>(x), static_cast<Gdiplus::REAL>(y), static_cast<Gdiplus::REAL>(w), static_cast<Gdiplus::REAL>(h)};
  }
};

struct Row {
  const SettingInfo* info = nullptr;
  int tab = 0;
  bool invalid = false;
  Box box;
  Box track;
  Box value;
  Box toggle;
  Box key;
};

// IDs of the mouse-reachable elements (for highlighting and clicking).
enum Element : int { kNothing = -1, kClose = 1, kDefaults = 2, kCancel = 3, kSave = 4, kFirstTab = 10, kFirstRow = 100 };
enum RowPart : int { kTrack = 0, kValue = 1, kToggle = 2, kKey = 3 };
int row_element(std::size_t row, RowPart part) { return kFirstRow + 4 * static_cast<int>(row) + part; }

void rounded(Gdiplus::GraphicsPath& path, const Gdiplus::RectF& r, float radius) {
  const float d = radius * 2;
  path.AddArc(r.X, r.Y, d, d, 180, 90);
  path.AddArc(r.X + r.Width - d, r.Y, d, d, 270, 90);
  path.AddArc(r.X + r.Width - d, r.Y + r.Height - d, d, d, 0, 90);
  path.AddArc(r.X, r.Y + r.Height - d, d, d, 90, 90);
  path.CloseFigure();
}

void fill_rounded(Gdiplus::Graphics& g, const Gdiplus::RectF& r, float radius, const Gdiplus::Color& c) {
  Gdiplus::GraphicsPath path;
  rounded(path, r, radius);
  Gdiplus::SolidBrush brush(c);
  g.FillPath(&brush, &path);
}

void outline_rounded(Gdiplus::Graphics& g, const Gdiplus::RectF& r, float radius, const Gdiplus::Color& c, float width) {
  Gdiplus::GraphicsPath path;
  rounded(path, r, radius);
  Gdiplus::Pen pen(c, width);
  g.DrawPath(&pen, &path);
}

class Window {
 public:
  explicit Window(const SettingsWindowSetup& setup) : setup_(setup), config_(setup.current) {}
  void run();

 private:
  static LRESULT CALLBACK proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
  LRESULT handle(UINT message, WPARAM wparam, LPARAM lparam);
  bool create();
  void layout();
  void loop();
  float s(double v) const { return static_cast<float>(v * scale_); }
  int si(double v) const { return static_cast<int>(std::lround(v * scale_)); }

  void paint();
  void draw_text(Gdiplus::Graphics& g, const std::wstring& text, const Gdiplus::Font& font, const Gdiplus::Color& c, const Box& box,
                 Gdiplus::StringAlignment align, bool wrap = false) const;
  void draw_button(Gdiplus::Graphics& g, const Box& box, const wchar_t* text, bool hover, bool primary, bool enabled) const;
  void draw_row(Gdiplus::Graphics& g, std::size_t index);

  int hit(POINT p) const;
  void on_mouse_down(POINT p);
  void on_mouse_move(POINT p);
  void set_from_track(std::size_t row, int x);
  void step_row(std::size_t row, int steps);

  void open_edit(std::size_t row);
  void commit_edit();
  void close_edit();
  void begin_capture(int row);
  void finish_capture(int code, bool cancelled);
  void on_capture_key(int vk);
  void changed();
  void refresh_status();
  void redraw() const { InvalidateRect(hwnd_, nullptr, FALSE); }
  void save();
  void cancel();
  void close();

  SettingsWindowSetup setup_;
  Config config_;
  double scale_ = 1.0;
  ULONG_PTR gdiplus_ = 0;
  HFONT edit_font_ = nullptr;
  HBRUSH edit_brush_ = nullptr;
  HWND hwnd_ = nullptr;
  HWND edit_ = nullptr;
  int width_ = 0;
  int height_ = 0;
  std::vector<std::string_view> tab_names_;
  std::vector<Box> tab_boxes_;
  std::vector<Row> rows_;
  Box header_;
  Box close_;
  Box content_;
  Box note_;
  Box status_box_;
  Box defaults_;
  Box cancel_;
  Box save_;
  int tab_ = 0;
  int hover_ = kNothing;
  int capturing_ = -1;
  int editing_ = -1;
  int dragging_ = -1;
  bool tracking_leave_ = false;
  bool save_enabled_ = true;
  std::string status_;
  bool closed_ = false;
};

void Window::run() {
  Gdiplus::GdiplusStartupInput input;
  if (Gdiplus::GdiplusStartup(&gdiplus_, &input, nullptr) != Gdiplus::Ok) {
    push_log("the settings window could not start its drawing library (GDI+)");
    return;
  }
  if (create()) {
    if (g_stop) close();
    loop();
  } else {
    push_log(strf("the settings window could not be created (error %lu)", GetLastError()));
  }
  if (edit_font_) DeleteObject(edit_font_);
  if (edit_brush_) DeleteObject(edit_brush_);
  Gdiplus::GdiplusShutdown(gdiplus_);
}

bool Window::create() {
  if (const HDC screen = GetDC(nullptr)) {
    scale_ = GetDeviceCaps(screen, LOGPIXELSY) / 96.0;
    ReleaseDC(nullptr, screen);
  }
  WNDCLASSW window_class{};
  window_class.style = CS_DROPSHADOW;
  window_class.lpfnWndProc = &Window::proc;
  window_class.hInstance = this_module();
  window_class.hCursor = LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_ARROW));
  window_class.lpszClassName = kClassName;
  if (!RegisterClassW(&window_class) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;

  for (const auto& info : settings_schema()) {
    if (std::find(tab_names_.begin(), tab_names_.end(), info.tab) == tab_names_.end()) tab_names_.push_back(info.tab);
    Row r;
    r.info = &info;
    r.tab = static_cast<int>(std::find(tab_names_.begin(), tab_names_.end(), info.tab) - tab_names_.begin());
    rows_.push_back(r);
  }
  layout();

  RECT around{};
  if (!setup_.game_window || !GetWindowRect(setup_.game_window, &around)) SystemParametersInfoW(SPI_GETWORKAREA, 0, &around, 0);
  const int x = around.left + (around.right - around.left - width_) / 2;
  const int y = around.top + (around.bottom - around.top - height_) / 2;
  hwnd_ = CreateWindowExW(WS_EX_TOPMOST | WS_EX_LAYERED, kClassName, L"FreeWalk settings", WS_POPUP | WS_CLIPCHILDREN, x, y, width_, height_,
                          nullptr, nullptr, this_module(), this);
  if (!hwnd_) return false;
  g_window = hwnd_;
  // Slightly translucent, rounded panel above the game.
  SetLayeredWindowAttributes(hwnd_, 0, 250, LWA_ALPHA);
  SetWindowRgn(hwnd_, CreateRoundRectRgn(0, 0, width_ + 1, height_ + 1, si(18), si(18)), TRUE);

  LOGFONTW font{};
  font.lfHeight = -si(14);
  font.lfWeight = FW_NORMAL;
  font.lfQuality = CLEARTYPE_QUALITY;
  wcscpy_s(font.lfFaceName, L"Segoe UI");
  edit_font_ = CreateFontIndirectW(&font);
  edit_brush_ = CreateSolidBrush(color::edit_background);

  refresh_status();
  ShowWindow(hwnd_, SW_SHOW);
  SetForegroundWindow(hwnd_);
  return true;
}

// Position of every element; drawing and the mouse use the same.
void Window::layout() {
  width_ = si(680);
  const int margin = si(20);
  const int row_height = si(36);
  const int row_gap = si(4);
  std::vector<int> per_tab(tab_names_.size(), 0);
  for (const auto& r : rows_) ++per_tab[static_cast<std::size_t>(r.tab)];
  const int most = *std::max_element(per_tab.begin(), per_tab.end());

  header_ = {0, 0, width_, si(58)};
  close_ = {width_ - si(48), si(13), si(32), si(32)};
  int x = margin;
  tab_boxes_.clear();
  for (const auto name : tab_names_) {
    const int w = si(26 + 8.2 * static_cast<double>(name.size()));
    tab_boxes_.push_back({x, header_.h + si(6), w, si(34)});
    x += w + si(6);
  }
  content_ = {margin, header_.h + si(52), width_ - 2 * margin, most * (row_height + row_gap)};
  std::fill(per_tab.begin(), per_tab.end(), 0);
  for (auto& r : rows_) {
    const int y = content_.y + per_tab[static_cast<std::size_t>(r.tab)]++ * (row_height + row_gap);
    r.box = {content_.x, y, content_.w, row_height};
    const int right = r.box.x + r.box.w - si(14);
    r.value = {right - si(70), y + si(6), si(70), row_height - si(12)};
    r.track = {right - si(70) - si(14) - si(230), y + si(8), si(230), row_height - si(16)};
    r.toggle = {right - si(42), y + (row_height - si(22)) / 2, si(42), si(22)};
    r.key = {right - si(150), y + si(5), si(150), row_height - si(10)};
  }
  note_ = {margin, content_.y + content_.h + si(8), width_ - 2 * margin, si(58)};
  status_box_ = {margin, note_.y + note_.h + si(6), width_ - 2 * margin, si(22)};
  const int button_y = status_box_.y + status_box_.h + si(10);
  const int button_w = si(104);
  const int button_h = si(34);
  save_ = {width_ - margin - button_w, button_y, button_w, button_h};
  cancel_ = {save_.x - button_w - si(10), button_y, button_w, button_h};
  defaults_ = {margin, button_y, button_w, button_h};
  height_ = button_y + button_h + margin;
}

void Window::draw_text(Gdiplus::Graphics& g, const std::wstring& text, const Gdiplus::Font& font, const Gdiplus::Color& c, const Box& box,
                       Gdiplus::StringAlignment align, bool wrap) const {
  Gdiplus::StringFormat format;
  format.SetAlignment(align);
  format.SetLineAlignment(wrap ? Gdiplus::StringAlignmentNear : Gdiplus::StringAlignmentCenter);
  if (!wrap) format.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);
  format.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
  Gdiplus::SolidBrush brush(c);
  g.DrawString(text.c_str(), -1, &font, box.f(), &format, &brush);
}

void Window::draw_button(Gdiplus::Graphics& g, const Box& box, const wchar_t* text, bool hover, bool primary, bool enabled) const {
  const Gdiplus::Font font(L"Segoe UI", s(14), primary ? Gdiplus::FontStyleBold : Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
  Gdiplus::Color fill = primary ? (hover ? color::accent_hover : color::accent) : (hover ? color::button_hover : color::button);
  Gdiplus::Color ink = primary ? color::on_accent : color::text;
  if (!enabled) {
    fill = color::button;
    ink = color::dim;
  }
  fill_rounded(g, box.f(), s(8), fill);
  draw_text(g, text, font, ink, box, Gdiplus::StringAlignmentCenter);
}

void Window::draw_row(Gdiplus::Graphics& g, std::size_t index) {
  Row& r = rows_[index];
  const SettingInfo& info = *r.info;
  const Gdiplus::Font font(L"Segoe UI", s(14), Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
  const bool row_hover = hover_ >= row_element(index, kTrack) && hover_ <= row_element(index, kKey);
  fill_rounded(g, r.box.f(), s(8), row_hover ? color::row_hover : color::row);
  const std::wstring label = widen(info.label) + (info.applies == Applies::Restart ? L"  *" : L"");
  draw_text(g, label, font, color::text, {r.box.x + si(14), r.box.y, si(300), r.box.h}, Gdiplus::StringAlignmentNear);

  switch (info.kind) {
    case SettingKind::Number: {
      const double v = config_.*info.number_field;
      const float t = std::clamp(static_cast<float>((v - info.min) / (info.max - info.min)), 0.0f, 1.0f);
      const float cy = static_cast<float>(r.track.y) + static_cast<float>(r.track.h) / 2;
      const float x0 = static_cast<float>(r.track.x) + s(8);
      const float x1 = static_cast<float>(r.track.x + r.track.w) - s(8);
      const float knob = x0 + (x1 - x0) * t;
      fill_rounded(g, {x0, cy - s(2), x1 - x0, s(4)}, s(2), color::off);
      if (knob > x0 + 1) fill_rounded(g, {x0, cy - s(2), knob - x0, s(4)}, s(2), color::accent);
      const float radius = dragging_ == static_cast<int>(index) || hover_ == row_element(index, kTrack) ? s(8) : s(7);
      Gdiplus::SolidBrush knob_brush(color::accent);
      g.FillEllipse(&knob_brush, knob - radius, cy - radius, radius * 2, radius * 2);
      Gdiplus::SolidBrush centre(color::background);
      g.FillEllipse(&centre, knob - s(3), cy - s(3), s(6), s(6));
      if (editing_ != static_cast<int>(index)) {
        const bool hot = hover_ == row_element(index, kValue);
        fill_rounded(g, r.value.f(), s(6), hot ? color::button_hover : color::button);
        draw_text(g, widen(format_setting_value(info, config_)), font, color::text, r.value, Gdiplus::StringAlignmentCenter);
      } else {
        fill_rounded(g, r.value.f(), s(6), color::button);
      }
      if (r.invalid) outline_rounded(g, r.value.f(), s(6), color::error, s(1.5));
      break;
    }
    case SettingKind::Flag: {
      const bool on = config_.*info.flag_field;
      const Gdiplus::RectF track = r.toggle.f();
      fill_rounded(g, track, track.Height / 2, on ? color::accent : color::off);
      const float d = track.Height - s(6);
      const float kx = on ? track.X + track.Width - d - s(3) : track.X + s(3);
      Gdiplus::SolidBrush knob(on ? color::white : color::knob_off);
      g.FillEllipse(&knob, kx, track.Y + s(3), d, d);
      break;
    }
    case SettingKind::Key: {
      const bool capturing = capturing_ == static_cast<int>(index);
      const bool hot = hover_ == row_element(index, kKey);
      fill_rounded(g, r.key.f(), s(6), capturing ? color::accent_dark : (hot ? color::button_hover : color::button));
      if (capturing) outline_rounded(g, r.key.f(), s(6), color::accent, s(1.5));
      const std::wstring text = capturing ? L"Press a key..." : widen(key_name(config_.*info.key_field));
      draw_text(g, text, font, capturing ? color::accent : color::text, r.key, Gdiplus::StringAlignmentCenter);
      break;
    }
  }
}

void Window::paint() {
  PAINTSTRUCT ps;
  const HDC target = BeginPaint(hwnd_, &ps);
  // Double buffering: we draw into an in-memory image and copy it out at once, so it does not flicker.
  const HDC memory = CreateCompatibleDC(target);
  const HBITMAP bitmap = CreateCompatibleBitmap(target, width_, height_);
  const HGDIOBJ old = SelectObject(memory, bitmap);
  {
    Gdiplus::Graphics g(memory);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    g.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);
    g.Clear(color::background);

    Gdiplus::SolidBrush header_brush(color::header);
    g.FillRectangle(&header_brush, header_.f());
    const Gdiplus::Font title(L"Segoe UI", s(20), Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
    const Gdiplus::Font small_font(L"Segoe UI", s(12), Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
    const Gdiplus::Font normal(L"Segoe UI", s(14), Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
    Gdiplus::SolidBrush accent(color::accent);
    g.FillEllipse(&accent, s(20), s(22), s(14), s(14));
    draw_text(g, L"FreeWalk", title, color::text, {si(42), 0, si(140), header_.h}, Gdiplus::StringAlignmentNear);
    draw_text(g, L"walk around your truck  ·  settings  ·  v" + widen(setup_.version), small_font, color::dim,
              {si(150), 0, si(400), header_.h}, Gdiplus::StringAlignmentNear);
    if (hover_ == kClose) fill_rounded(g, close_.f(), s(6), color::button_hover);
    Gdiplus::Pen cross(hover_ == kClose ? color::text : color::dim, s(1.6));
    const float cx = static_cast<float>(close_.x) + static_cast<float>(close_.w) / 2;
    const float cy = static_cast<float>(close_.y) + static_cast<float>(close_.h) / 2;
    g.DrawLine(&cross, cx - s(6), cy - s(6), cx + s(6), cy + s(6));
    g.DrawLine(&cross, cx - s(6), cy + s(6), cx + s(6), cy - s(6));

    for (std::size_t t = 0; t < tab_boxes_.size(); ++t) {
      const bool selected = static_cast<int>(t) == tab_;
      const bool hot = hover_ == kFirstTab + static_cast<int>(t);
      const Gdiplus::Font tab_font(L"Segoe UI", s(14), selected ? Gdiplus::FontStyleBold : Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
      draw_text(g, widen(tab_names_[t]), tab_font, selected || hot ? color::text : color::dim, tab_boxes_[t], Gdiplus::StringAlignmentCenter);
      if (selected) {
        const Box& b = tab_boxes_[t];
        fill_rounded(g, {static_cast<float>(b.x) + s(8), static_cast<float>(b.y + b.h) - s(3), static_cast<float>(b.w) - s(16), s(3)}, s(1.5),
                     color::accent);
      }
    }
    Gdiplus::Pen divider(color::line, s(1));
    const float line_y = static_cast<float>(tab_boxes_.front().y + tab_boxes_.front().h) + s(1);
    g.DrawLine(&divider, s(20), line_y, static_cast<float>(width_) - s(20), line_y);

    for (std::size_t i = 0; i < rows_.size(); ++i)
      if (rows_[i].tab == tab_) draw_row(g, i);

    if (const char* note = tab_note(tab_names_[static_cast<std::size_t>(tab_)]))
      draw_text(g, widen(note), small_font, color::dim, note_, Gdiplus::StringAlignmentNear, true);
    if (!status_.empty()) draw_text(g, widen(status_), normal, color::error, status_box_, Gdiplus::StringAlignmentNear);

    draw_button(g, defaults_, L"Defaults", hover_ == kDefaults, false, true);
    draw_button(g, cancel_, L"Cancel", hover_ == kCancel, false, true);
    draw_button(g, save_, L"Save", hover_ == kSave, true, save_enabled_);
  }
  BitBlt(target, 0, 0, width_, height_, memory, 0, 0, SRCCOPY);
  SelectObject(memory, old);
  DeleteObject(bitmap);
  DeleteDC(memory);
  EndPaint(hwnd_, &ps);
}

int Window::hit(POINT p) const {
  if (close_.contains(p)) return kClose;
  for (std::size_t t = 0; t < tab_boxes_.size(); ++t)
    if (tab_boxes_[t].contains(p)) return kFirstTab + static_cast<int>(t);
  for (std::size_t i = 0; i < rows_.size(); ++i) {
    const Row& r = rows_[i];
    if (r.tab != tab_ || !r.box.contains(p)) continue;
    switch (r.info->kind) {
      case SettingKind::Number:
        if (r.value.contains(p)) return row_element(i, kValue);
        if (p.x >= r.track.x - si(10)) return row_element(i, kTrack);
        return kNothing;
      case SettingKind::Flag: return r.toggle.contains(p) ? row_element(i, kToggle) : kNothing;
      case SettingKind::Key: return r.key.contains(p) ? row_element(i, kKey) : kNothing;
    }
  }
  if (defaults_.contains(p)) return kDefaults;
  if (cancel_.contains(p)) return kCancel;
  if (save_.contains(p)) return kSave;
  return kNothing;
}

void Window::set_from_track(std::size_t row, int x) {
  const Row& r = rows_[row];
  const SettingInfo& info = *r.info;
  const double x0 = r.track.x + s(8);
  const double x1 = r.track.x + r.track.w - s(8);
  const double t = std::clamp((x - x0) / (x1 - x0), 0.0, 1.0);
  const long steps = std::lround((info.max - info.min) / info.step);
  const double v = round_to(info.min + static_cast<double>(std::lround(t * static_cast<double>(steps))) * info.step, info.decimals);
  if (v == config_.*info.number_field) return;
  config_.*info.number_field = v;
  rows_[row].invalid = false;
  changed();
}

void Window::step_row(std::size_t row, int steps) {
  const SettingInfo& info = *rows_[row].info;
  const double v = std::clamp(round_to(config_.*info.number_field + steps * info.step, info.decimals), info.min, info.max);
  if (v == config_.*info.number_field) return;
  config_.*info.number_field = v;
  changed();
}

void Window::on_mouse_down(POINT p) {
  if (editing_ >= 0) commit_edit();
  const int element = hit(p);
  if (capturing_ >= 0 && element != row_element(static_cast<std::size_t>(capturing_), kKey)) finish_capture(0, true);
  if (element == kClose || element == kCancel) return cancel();
  if (element == kDefaults) {
    config_ = Config{};
    for (auto& r : rows_) r.invalid = false;
    return changed();
  }
  if (element == kSave) {
    if (save_enabled_) save();
    return;
  }
  if (element >= kFirstTab && element < kFirstRow) {
    tab_ = element - kFirstTab;
    return redraw();
  }
  if (element < kFirstRow) return;
  const auto row = static_cast<std::size_t>((element - kFirstRow) / 4);
  switch ((element - kFirstRow) % 4) {
    case kTrack:
      dragging_ = static_cast<int>(row);
      SetCapture(hwnd_);
      set_from_track(row, p.x);
      redraw();
      break;
    case kValue: open_edit(row); break;
    case kToggle:
      config_.*rows_[row].info->flag_field = !(config_.*rows_[row].info->flag_field);
      changed();
      break;
    case kKey: begin_capture(static_cast<int>(row)); break;
    default: break;
  }
}

void Window::on_mouse_move(POINT p) {
  if (dragging_ >= 0) return set_from_track(static_cast<std::size_t>(dragging_), p.x);
  const int element = hit(p);
  if (element != hover_) {
    hover_ = element;
    redraw();
  }
  if (!tracking_leave_) {
    TRACKMOUSEEVENT track{sizeof track, TME_LEAVE, hwnd_, 0};
    tracking_leave_ = TrackMouseEvent(&track) != FALSE;
  }
}

void Window::open_edit(std::size_t row) {
  const Row& r = rows_[row];
  editing_ = static_cast<int>(row);
  edit_ = CreateWindowExW(0, L"EDIT", widen(format_setting_value(*r.info, config_)).c_str(), WS_CHILD | WS_VISIBLE | ES_CENTER | ES_AUTOHSCROLL,
                          r.value.x + si(4), r.value.y + (r.value.h - si(18)) / 2, r.value.w - si(8), si(18), hwnd_, nullptr, this_module(), nullptr);
  if (!edit_) {
    editing_ = -1;
    return;
  }
  SendMessageW(edit_, WM_SETFONT, reinterpret_cast<WPARAM>(edit_font_), TRUE);
  SendMessageW(edit_, EM_SETSEL, 0, -1);
  SetFocus(edit_);
  redraw();
}

void Window::commit_edit() {
  if (editing_ < 0 || !edit_) return;
  Row& r = rows_[static_cast<std::size_t>(editing_)];
  const SettingInfo& info = *r.info;
  wchar_t text[64] = {};
  GetWindowTextW(edit_, text, 64);
  const std::string value = narrow(text);
  char* end = nullptr;
  const double v = std::strtod(value.c_str(), &end);
  if (value.empty() || end != value.c_str() + value.size() || !std::isfinite(v) || v < info.min || v > info.max) {
    // Invalid value: not applied, the field stays open marked in red.
    r.invalid = true;
    refresh_status();
    return redraw();
  }
  r.invalid = false;
  // Rounded to as many decimals as the save writes, so the live and saved values do not differ.
  config_.*info.number_field = round_to(v, info.decimals);
  close_edit();
  changed();
}

void Window::close_edit() {
  if (editing_ >= 0) rows_[static_cast<std::size_t>(editing_)].invalid = false;
  editing_ = -1;
  if (edit_) {
    const HWND edit = edit_;
    edit_ = nullptr;
    DestroyWindow(edit);
  }
  SetFocus(hwnd_);
  refresh_status();
  redraw();
}

void Window::begin_capture(int row) {
  if (capturing_ >= 0) finish_capture(0, true);
  capturing_ = row;
  redraw();
}

void Window::finish_capture(int code, bool cancelled) {
  if (capturing_ < 0) return;
  const Row& r = rows_[static_cast<std::size_t>(capturing_)];
  capturing_ = -1;
  if (cancelled) return redraw();
  config_.*r.info->key_field = code;
  changed();
}

void Window::on_capture_key(int vk) {
  const SettingInfo& info = *rows_[static_cast<std::size_t>(capturing_)].info;
  if (vk == VK_ESCAPE) return finish_capture(0, true);
  // For a command key Ctrl is only a modifier: together with the next key it forms the combination.
  if (info.allow_ctrl && (vk == VK_CONTROL || vk == VK_LCONTROL || vk == VK_RCONTROL)) return;
  if (vk == VK_BACK && info.allow_none) return finish_capture(0, false);
  if (vk <= 0 || vk >= 255) return;
  int code = vk;
  if (info.allow_ctrl && (GetKeyState(VK_CONTROL) & 0x8000) != 0) code |= kKeyCtrl;
  finish_capture(code, false);
}

void Window::changed() {
  push_wanted(config_);
  refresh_status();
  redraw();
}

void Window::refresh_status() {
  status_.clear();
  bool blocked = false;
  for (const auto& r : rows_) {
    if (!r.invalid) continue;
    status_ = strf("Not a valid value for \"%.*s\": use a number from %g to %g.", static_cast<int>(r.info->label.size()), r.info->label.data(),
                   r.info->min, r.info->max);
    blocked = true;
    break;
  }
  if (status_.empty()) {
    const auto conflicts = key_conflicts(config_);
    if (!conflicts.empty()) {
      status_ = "The same key is set for \"" + std::string(conflicts[0].first) + "\" and \"" + std::string(conflicts[0].second) + "\".";
      blocked = true;
    }
  }
  if (status_.empty() && setup_.ini_had_unknown_keys) status_ = "FreeWalk.ini has settings this version does not know: Save leaves them out.";
  save_enabled_ = !blocked;
}

void Window::save() {
  const std::string text = format_config(config_);
  const std::wstring temp = setup_.ini_path + L".tmp";
  // First we write to a temporary file and only replace the old one with the complete file: an interrupted save leaves no broken INI.
  bool ok = false;
  DWORD error = 0;
  const HANDLE file = CreateFileW(temp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file != INVALID_HANDLE_VALUE) {
    DWORD written = 0;
    ok = WriteFile(file, text.data(), static_cast<DWORD>(text.size()), &written, nullptr) && written == text.size() && FlushFileBuffers(file);
    if (!ok) error = GetLastError();
    CloseHandle(file);
  } else {
    error = GetLastError();
  }
  if (ok && !MoveFileExW(temp.c_str(), setup_.ini_path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
    ok = false;
    error = GetLastError();
  }
  if (!ok) {
    DeleteFileW(temp.c_str());
    push_log(strf("the settings could not be saved to FreeWalk.ini (error %lu)", error));
    status_ = strf("FreeWalk.ini could not be saved (error %lu): is the plugins folder write-protected?", error);
    return redraw();
  }
  push_log("settings saved to FreeWalk.ini");
  push_wanted(config_);
  close();
}

void Window::cancel() {
  push_wanted(setup_.current);
  close();
}

void Window::close() {
  if (closed_) return;
  closed_ = true;
  g_window = nullptr;
  DestroyWindow(hwnd_);
  if (setup_.game_window) SetForegroundWindow(setup_.game_window);
}

void Window::loop() {
  MSG message;
  while (GetMessageW(&message, nullptr, 0, 0) > 0) {
    // Repeated keystrokes (held key, e.g. the opening Ctrl+F10) are ignored.
    const bool repeat = (message.lParam & (1 << 30)) != 0;
    if (!closed_ && !repeat && (message.message == WM_KEYDOWN || message.message == WM_SYSKEYDOWN)) {
      const int vk = static_cast<int>(message.wParam);
      if (edit_ && message.hwnd == edit_) {
        if (vk == VK_RETURN) {
          commit_edit();
          continue;
        }
        if (vk == VK_ESCAPE) {
          close_edit();
          continue;
        }
      } else if (capturing_ >= 0) {
        on_capture_key(vk);
        continue;
      } else {
        // The settings window key and Esc in the window are the same as Cancel.
        const int hotkey = setup_.current.key_settings;
        const bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        if (vk == VK_ESCAPE || (hotkey != 0 && vk == (hotkey & 0xFF) && ctrl == ((hotkey & kKeyCtrl) != 0))) {
          cancel();
          continue;
        }
      }
    }
    TranslateMessage(&message);
    DispatchMessageW(&message);
  }
}

LRESULT CALLBACK Window::proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
  if (message == WM_NCCREATE) {
    const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lparam);
    SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
  }
  auto* self = reinterpret_cast<Window*>(GetWindowLongPtrW(window, GWLP_USERDATA));
  if (!self) return DefWindowProcW(window, message, wparam, lparam);
  if (!self->hwnd_) self->hwnd_ = window;
  return self->handle(message, wparam, lparam);
}

LRESULT Window::handle(UINT message, WPARAM wparam, LPARAM lparam) {
  switch (message) {
    case WM_NCHITTEST: {
      // The header (except the close button) is the title bar: the window can be dragged by it.
      POINT p{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
      ScreenToClient(hwnd_, &p);
      if (header_.contains(p) && !close_.contains(p)) return HTCAPTION;
      return HTCLIENT;
    }
    case WM_SYSCOMMAND:
      // Releasing F10 and Alt must not put the window into system menu mode (it has no menu).
      if ((wparam & 0xFFF0) == SC_KEYMENU) return 0;
      break;
    case WM_ERASEBKGND:
      return 1;
    case WM_PAINT:
      paint();
      return 0;
    case WM_LBUTTONDOWN:
      on_mouse_down({GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)});
      return 0;
    case WM_MOUSEMOVE:
      on_mouse_move({GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)});
      return 0;
    case WM_LBUTTONUP:
      if (dragging_ >= 0) {
        dragging_ = -1;
        ReleaseCapture();
        redraw();
      }
      return 0;
    case WM_MOUSELEAVE:
      tracking_leave_ = false;
      if (hover_ != kNothing && dragging_ < 0) {
        hover_ = kNothing;
        redraw();
      }
      return 0;
    case WM_MOUSEWHEEL: {
      POINT p{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
      ScreenToClient(hwnd_, &p);
      const int element = hit(p);
      if (element >= kFirstRow && (element - kFirstRow) % 4 == kTrack)
        step_row(static_cast<std::size_t>((element - kFirstRow) / 4), GET_WHEEL_DELTA_WPARAM(wparam) > 0 ? 1 : -1);
      return 0;
    }
    case WM_COMMAND:
      if (edit_ && reinterpret_cast<HWND>(lparam) == edit_ && HIWORD(wparam) == EN_KILLFOCUS) commit_edit();
      return 0;
    case WM_CTLCOLOREDIT: {
      const auto dc = reinterpret_cast<HDC>(wparam);
      SetTextColor(dc, color::edit_text);
      SetBkColor(dc, color::edit_background);
      return reinterpret_cast<LRESULT>(edit_brush_);
    }
    case WM_CLOSE:
      cancel();
      return 0;
    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;
    default:
      break;
  }
  return DefWindowProcW(hwnd_, message, wparam, lparam);
}

struct ThreadStart {
  SettingsWindowSetup setup;
  HMODULE module = nullptr;  // the thread's own reference to the DLL: the game cannot unload it while the thread runs
};

DWORD WINAPI thread_main(LPVOID parameter) {
  std::unique_ptr<ThreadStart> start(static_cast<ThreadStart*>(parameter));
  {
    Window window(start->setup);
    window.run();
  }
  g_window = nullptr;
  g_open = false;
  const HMODULE module = start->module;
  start.reset();
  if (module) FreeLibraryAndExitThread(module, 0);
  return 0;
}

}  // namespace

bool settings_window_open(const SettingsWindowSetup& setup) {
  if (g_open) return false;
  if (g_thread) {
    CloseHandle(g_thread);
    g_thread = nullptr;
  }
  g_stop = false;
  g_open = true;
  auto* start = new ThreadStart{setup, nullptr};
  // Own reference to the DLL, so the thread's code does not vanish under it during a lengthy shutdown; the thread releases it at the end.
  GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, reinterpret_cast<LPCWSTR>(&settings_window_open), &start->module);
  g_thread = CreateThread(nullptr, 0, thread_main, start, 0, nullptr);
  if (!g_thread) {
    if (start->module) FreeLibrary(start->module);
    delete start;
    g_open = false;
    return false;
  }
  return true;
}

bool settings_window_is_open() { return g_open; }

bool settings_window_is_foreground() {
  const HWND window = g_window;
  return window && GetForegroundWindow() == window;
}

void settings_window_close() {
  if (const HWND window = g_window) PostMessageW(window, WM_CLOSE, 0, 0);
}

void settings_window_shutdown() {
  if (!g_thread) return;
  g_stop = true;
  if (const HWND window = g_window) PostMessageW(window, WM_CLOSE, 0, 0);
  WaitForSingleObject(g_thread, 2000);
  CloseHandle(g_thread);
  g_thread = nullptr;
}

std::optional<Config> settings_window_take_wanted() {
  std::lock_guard lock(g_mutex);
  return std::exchange(g_wanted, std::nullopt);
}

std::vector<std::string> settings_window_take_log() {
  std::lock_guard lock(g_mutex);
  return std::exchange(g_log, {});
}

}  // namespace fw

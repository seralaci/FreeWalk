#include "win/input.h"
#define DIRECTINPUT_VERSION 0x0800
#include <windows.h>
#include <dinput.h>
#include <array>
#include <atomic>
#include <cstring>
#include <string>
#include <vector>
#include "core/key_filter.h"
#include "core/text.h"
#include "win/settings_window.h"

namespace fw {
namespace {

KeyFilter g_filter;
std::atomic<bool> g_blocking{false};
HANDLE g_thread = nullptr;
DWORD g_thread_id = 0;
std::atomic<int> g_wheel{0};  // accumulated wheel value in WHEEL_DELTA units
std::atomic<bool> g_wheel_capture{false};  // while walking, not paused

LRESULT CALLBACK hook_proc(int code, WPARAM message, LPARAM data) {
  if (code == HC_ACTION) {
    const auto* key = reinterpret_cast<const KBDLLHOOKSTRUCT*>(data);
    const bool down = message == WM_KEYDOWN || message == WM_SYSKEYDOWN;
    if (g_filter.on_key(static_cast<int>(key->vkCode), down, g_blocking && game_window_focused())) return 1;
  }
  return CallNextHookEx(nullptr, code, message, data);
}

// While walking (blocking) and with the game focused, the wheel belongs to the flashlight: we accumulate it and the game does not get it.
LRESULT CALLBACK mouse_proc(int code, WPARAM message, LPARAM data) {
  if (code == HC_ACTION && message == WM_MOUSEWHEEL && g_wheel_capture && game_window_focused()) {
    const auto* mouse = reinterpret_cast<const MSLLHOOKSTRUCT*>(data);
    g_wheel += static_cast<short>(HIWORD(mouse->mouseData));
    return 1;
  }
  return CallNextHookEx(nullptr, code, message, data);
}

// The hook receives events through the installing thread's message queue, so it needs its own message-processing thread.
DWORD WINAPI hook_thread(LPVOID started) {
  MSG message;
  PeekMessageW(&message, nullptr, 0, 0, PM_NOREMOVE);
  const HHOOK hook = SetWindowsHookExW(WH_KEYBOARD_LL, hook_proc, GetModuleHandleW(nullptr), 0);
  *static_cast<std::atomic<bool>*>(started) = hook != nullptr;
  if (!hook) return 1;
  // Missing mouse hook (wheel only) does not prevent the keyboard hook.
  const HHOOK mouse = SetWindowsHookExW(WH_MOUSE_LL, mouse_proc, GetModuleHandleW(nullptr), 0);
  while (GetMessageW(&message, nullptr, 0, 0) > 0) {
  }
  if (mouse) UnhookWindowsHookEx(mouse);
  UnhookWindowsHookEx(hook);
  return 0;
}

// --- DirectInput filter ---

using GetDeviceStateFn = HRESULT(STDMETHODCALLTYPE*)(void* self, DWORD size, LPVOID data);
using GetDeviceDataFn = HRESULT(STDMETHODCALLTYPE*)(void* self, DWORD object_size, LPDIDEVICEOBJECTDATA objects, LPDWORD count, DWORD flags);

// In the IDirectInputDevice8 vtable GetDeviceState is entry 9, GetDeviceData entry 10.
constexpr int kGetDeviceStateSlot = 9;
constexpr int kGetDeviceDataSlot = 10;

// The keyboard snapshot is 256 bytes (c_dfDIKeyboard); other devices (mouse, wheel, joystick) have different state sizes.
constexpr DWORD kKeyboardStateSize = 256;

// A patched vtable: the W (Unicode) and A (ANSI) device variants have separate vtables (their functions are shared).
struct PatchedVtable {
  void** vtable = nullptr;
  GetDeviceStateFn original_state = nullptr;
  GetDeviceDataFn original_data = nullptr;
};
PatchedVtable g_patched[2];

// Diagnostic counters of the game's reads (incremented on the game thread, read and reset by the plugin).
struct ReadCounters {
  std::atomic<unsigned> state_reads{0};           // GetDeviceState calls
  std::atomic<unsigned> keyboard_state_reads{0};  // of which 256-byte (keyboard)
  std::atomic<unsigned> state_keys_hidden{0};     // pressed walk key hidden from the snapshot
  std::atomic<unsigned> data_reads{0};            // GetDeviceData calls
  std::atomic<unsigned> keyboard_data_reads{0};   // of which on a keyboard device
  std::atomic<unsigned> data_events{0};           // keyboard events
  std::atomic<unsigned> data_events_dropped{0};   // of which hidden
};
ReadCounters g_counters[2];

// DirectInput key code (scan code, +0x80 for extended keys) -> the watched Windows virtual key code, 0 if not watched.
std::array<int, 256> g_dik_to_vk{};

void map_key(int code) {
  const int virtual_key = code & 0xFF;  // without the Ctrl flag
  if (virtual_key == 0) return;
  switch (virtual_key) {
    case 0x10: g_dik_to_vk[0x2A] = 0xA0; g_dik_to_vk[0x36] = 0xA1; return;  // left and right Shift
    case 0x11: g_dik_to_vk[0x1D] = 0xA2; g_dik_to_vk[0x9D] = 0xA3; return;  // left and right Ctrl
    case 0x12: g_dik_to_vk[0x38] = 0xA4; g_dik_to_vk[0xB8] = 0xA5; return;  // left and right Alt
    default: break;
  }
  const UINT scan = MapVirtualKeyW(static_cast<UINT>(virtual_key), MAPVK_VK_TO_VSC_EX);
  if (scan == 0) return;
  const int dik = static_cast<int>(scan & 0xFF) | ((scan & 0xFF00) != 0 ? 0x80 : 0);
  g_dik_to_vk[dik & 0xFF] = virtual_key;
}

// The patched vtable is shared by every dinput8.dll device (mouse, wheel, joystick), and their event offsets
// may overlap key codes; so the event queue is filtered only for devices that report as keyboards.
// GetCapabilities is vtable entry 3.
bool is_keyboard(void* self) {
  using GetCapabilitiesFn = HRESULT(STDMETHODCALLTYPE*)(void* self, LPDIDEVCAPS caps);
  DIDEVCAPS caps{};
  caps.dwSize = sizeof caps;
  const auto get_capabilities = reinterpret_cast<GetCapabilitiesFn>((*reinterpret_cast<void***>(self))[3]);
  return SUCCEEDED(get_capabilities(self, &caps)) && GET_DIDEVICE_TYPE(caps.dwDevType) == DI8DEVTYPE_KEYBOARD;
}

template <int Variant>
HRESULT STDMETHODCALLTYPE filtered_state(void* self, DWORD size, LPVOID data) {
  const HRESULT result = g_patched[Variant].original_state(self, size, data);
  auto& counters = g_counters[Variant];
  ++counters.state_reads;
  if (SUCCEEDED(result) && data && size == kKeyboardStateSize) {
    ++counters.keyboard_state_reads;
    const bool blocking = g_blocking;
    auto* keys = static_cast<BYTE*>(data);
    for (int dik = 0; dik < 256; ++dik) {
      if (g_dik_to_vk[dik] == 0 || !g_filter.hide_in_state(g_dik_to_vk[dik], blocking)) continue;
      if ((keys[dik] & 0x80) != 0) ++counters.state_keys_hidden;
      keys[dik] = 0;
    }
  }
  return result;
}

template <int Variant>
HRESULT STDMETHODCALLTYPE filtered_data(void* self, DWORD object_size, LPDIDEVICEOBJECTDATA objects, LPDWORD count, DWORD flags) {
  const HRESULT result = g_patched[Variant].original_data(self, object_size, objects, count, flags);
  auto& counters = g_counters[Variant];
  ++counters.data_reads;
  // A peek read does not remove events from the queue, so it is not filtered: the real read filters them anyway.
  if (FAILED(result) || !objects || !count || (flags & DIGDD_PEEK) != 0 || object_size < sizeof(DIDEVICEOBJECTDATA) || !is_keyboard(self))
    return result;
  ++counters.keyboard_data_reads;
  auto* bytes = reinterpret_cast<BYTE*>(objects);
  DWORD kept = 0;
  for (DWORD i = 0; i < *count; ++i) {
    auto* event = reinterpret_cast<DIDEVICEOBJECTDATA*>(bytes + i * object_size);
    ++counters.data_events;
    const int vk = event->dwOfs < 256 ? g_dik_to_vk[event->dwOfs] : 0;
    if (vk != 0 && !g_filter.pass_device_event(vk, (event->dwData & 0x80) != 0, g_blocking)) {
      ++counters.data_events_dropped;
      continue;
    }
    if (kept != i) std::memmove(bytes + kept * object_size, event, object_size);
    ++kept;
  }
  *count = kept;
  return result;
}

void* own_state(int variant) {
  return variant == 0 ? reinterpret_cast<void*>(&filtered_state<0>) : reinterpret_cast<void*>(&filtered_state<1>);
}

void* own_data(int variant) {
  return variant == 0 ? reinterpret_cast<void*>(&filtered_data<0>) : reinterpret_cast<void*>(&filtered_data<1>);
}

// Writes the two vtable entries; the vtable is in the read-only data of dinput8.dll.
template <class Write>
bool write_slots(void** vtable, Write write) {
  DWORD protection = 0;
  if (!VirtualProtect(&vtable[kGetDeviceStateSlot], 2 * sizeof(void*), PAGE_READWRITE, &protection)) return false;
  write();
  VirtualProtect(&vtable[kGetDeviceStateSlot], 2 * sizeof(void*), protection, &protection);
  return true;
}

// Which module the address belongs to (for the log).
std::string module_of(const void* address) {
  HMODULE module = nullptr;
  if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                          static_cast<LPCWSTR>(address), &module))
    return "no module";
  char path[MAX_PATH] = {};
  if (GetModuleFileNameA(module, path, MAX_PATH) == 0) return "?";
  const char* name = std::strrchr(path, '\\');
  return name ? name + 1 : path;
}

// The dinput8.dll's own DirectInput object via the COM class factory, bypassing DirectInput8Create.
// Live test (0.3.2): the Steam overlay intercepts DirectInput8Create and returns its own wrapper instead of the keyboard device,
// whose read functions ReShade also redirects; the game does not read through the wrapper vtable, so the filter is useless there.
// Every wrapper ultimately reads through the vtable of the real dinput8.dll device, so that is the one to patch.
template <class DirectInput>
DirectInput* create_real_direct_input(REFIID interface_id, std::vector<std::string>& lines) {
  using GetClassObjectFn = HRESULT(STDAPICALLTYPE*)(REFCLSID clsid, REFIID iid, LPVOID* object);
  const HMODULE dinput8 = GetModuleHandleW(L"dinput8.dll");
  const auto get_class_object = dinput8 ? reinterpret_cast<GetClassObjectFn>(GetProcAddress(dinput8, "DllGetClassObject")) : nullptr;
  if (!get_class_object) {
    lines.push_back("DirectInput: dinput8.dll has no DllGetClassObject");
    return nullptr;
  }
  IClassFactory* factory = nullptr;
  HRESULT result = get_class_object(CLSID_DirectInput8, IID_IClassFactory, reinterpret_cast<LPVOID*>(&factory));
  DirectInput* input = nullptr;
  if (SUCCEEDED(result)) {
    result = factory->CreateInstance(nullptr, interface_id, reinterpret_cast<void**>(&input));
    factory->Release();
  }
  if (SUCCEEDED(result)) {
    result = input->Initialize(GetModuleHandleW(nullptr), DIRECTINPUT_VERSION);
    if (FAILED(result)) {
      input->Release();
      input = nullptr;
    }
  }
  if (FAILED(result)) lines.push_back(strf("DirectInput: dinput8.dll's own object could not be made (0x%08lX)", static_cast<unsigned long>(result)));
  return input;
}

bool in_dinput8(const void* address) {
  HMODULE module = nullptr;
  return GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            static_cast<LPCWSTR>(address), &module) &&
         module == GetModuleHandleW(L"dinput8.dll");
}

// Creates a temporary keyboard device to read its vtable; the vtable is in dinput8.dll and is shared by every device.
template <class DirectInput, class Device, int Variant>
bool patch_keyboard_vtable(REFIID interface_id, const char* name, std::vector<std::string>& lines) {
  DirectInput* input = create_real_direct_input<DirectInput>(interface_id, lines);
  if (!input) return false;
  Device* device = nullptr;
  bool patched = false;
  const HRESULT device_created = input->CreateDevice(GUID_SysKeyboard, &device, nullptr);
  if (FAILED(device_created)) {
    lines.push_back(strf("DirectInput %s: CreateDevice(keyboard) failed (0x%08lX)", name, static_cast<unsigned long>(device_created)));
  } else {
    void** vtable = *reinterpret_cast<void***>(device);
    const std::string where = strf("vtable in %s, GetDeviceState in %s, GetDeviceData in %s", module_of(vtable).c_str(),
                                   module_of(vtable[kGetDeviceStateSlot]).c_str(), module_of(vtable[kGetDeviceDataSlot]).c_str());
    bool shared = false;
    for (const auto& p : g_patched) shared |= p.vtable == vtable;
    if (shared) {
      lines.push_back(strf("DirectInput %s: same vtable as the other variant (%s)", name, where.c_str()));
    } else if (!in_dinput8(vtable)) {
      // A wrapper's vtable: the game does not read through it (see above), patching it would only mislead.
      lines.push_back(strf("DirectInput %s: NOT patched, the device is a wrapper, not dinput8.dll's own (%s)", name, where.c_str()));
    } else {
      const auto state = reinterpret_cast<GetDeviceStateFn>(vtable[kGetDeviceStateSlot]);
      const auto data = reinterpret_cast<GetDeviceDataFn>(vtable[kGetDeviceDataSlot]);
      // Record the original first, because the game thread may call ours right after the write.
      g_patched[Variant] = {vtable, state, data};
      patched = write_slots(vtable, [&] {
        vtable[kGetDeviceStateSlot] = own_state(Variant);
        vtable[kGetDeviceDataSlot] = own_data(Variant);
      });
      if (!patched) g_patched[Variant] = {};
      lines.push_back(strf("DirectInput %s: %s (%s)", name, patched ? "patched" : "VirtualProtect failed", where.c_str()));
    }
    device->Release();
  }
  input->Release();
  return patched;
}

}  // namespace

bool game_window_focused() {
  const HWND window = GetForegroundWindow();
  if (!window) return false;
  DWORD process = 0;
  GetWindowThreadProcessId(window, &process);
  // The settings window runs in the same process but is not the game: while it has focus, keys are not meant for the game.
  return process == GetCurrentProcessId() && !settings_window_is_foreground();
}

bool key_down(int virtual_key) { return (GetAsyncKeyState(virtual_key) & 0x8000) != 0; }

bool keyboard_hook_start(std::initializer_list<int> keys) {
  if (g_thread) return true;
  g_filter.watch(keys);
  static std::atomic<bool> started{false};
  started = false;
  g_thread = CreateThread(nullptr, 0, hook_thread, &started, 0, &g_thread_id);
  if (!g_thread) return false;
  // We wait for the thread to start, to learn whether the hook installed.
  for (int i = 0; i < 200 && !started && WaitForSingleObject(g_thread, 10) == WAIT_TIMEOUT; ++i) {
  }
  if (!started) keyboard_hook_stop();
  return started;
}

void keyboard_hook_stop() {
  if (!g_thread) return;
  PostThreadMessageW(g_thread_id, WM_QUIT, 0, 0);
  WaitForSingleObject(g_thread, 2000);
  CloseHandle(g_thread);
  g_thread = nullptr;
  g_thread_id = 0;
  g_blocking = false;
}

void keyboard_hook_block(bool blocking) { g_blocking = blocking; }

void keyboard_hook_watch(const std::vector<int>& keys) { g_filter.watch_only(keys); }

bool keyboard_hook_down(int virtual_key) { return g_filter.is_down(virtual_key); }

void mouse_wheel_capture(bool capture) { g_wheel_capture = capture; }

int mouse_wheel_take() {
  // Only whole notches: the remainder keeps accumulating the sub-120 steps of smooth wheels.
  const int steps = g_wheel.load() / WHEEL_DELTA;
  g_wheel -= steps * WHEEL_DELTA;
  return steps;
}

int directinput_filter_start(std::initializer_list<int> keys, std::vector<std::string>& lines) {
  for (const int k : keys) map_key(k);
  int patched = 0;
  if (patch_keyboard_vtable<IDirectInput8W, IDirectInputDevice8W, 0>(IID_IDirectInput8W, "W", lines)) ++patched;
  if (patch_keyboard_vtable<IDirectInput8A, IDirectInputDevice8A, 1>(IID_IDirectInput8A, "A", lines)) ++patched;
  return patched;
}

void directinput_filter_watch(const std::vector<int>& keys) {
  g_dik_to_vk.fill(0);
  for (const int k : keys) map_key(k);
}

std::string directinput_filter_check() {
  std::string report;
  for (int v = 0; v < 2; ++v) {
    auto& p = g_patched[v];
    if (!p.vtable) continue;
    auto& c = g_counters[v];
    report += strf("%s%s: state reads %u (keyboard %u, keys hidden %u), data reads %u (keyboard %u, events %u, dropped %u)",
                   report.empty() ? "" : "; ", v == 0 ? "W" : "A", c.state_reads.exchange(0), c.keyboard_state_reads.exchange(0),
                   c.state_keys_hidden.exchange(0), c.data_reads.exchange(0), c.keyboard_data_reads.exchange(0), c.data_events.exchange(0),
                   c.data_events_dropped.exchange(0));
    // If someone (e.g. another program's hook) overwrote our entries, they become ours again; our own original stays in the call chain.
    void* const state = p.vtable[kGetDeviceStateSlot];
    void* const data = p.vtable[kGetDeviceDataSlot];
    if (state != own_state(v) || data != own_data(v)) {
      const bool repaired = write_slots(p.vtable, [&] {
        p.vtable[kGetDeviceStateSlot] = own_state(v);
        p.vtable[kGetDeviceDataSlot] = own_data(v);
      });
      report += strf(" - its entries had been overwritten (GetDeviceState now in %s, GetDeviceData now in %s), %s",
                     module_of(state).c_str(), module_of(data).c_str(), repaired ? "patched again" : "could NOT be patched again");
    }
  }
  return report;
}

void directinput_filter_stop() {
  // Only restored if the entry is still ours; if someone else redirected it since, we do not overwrite theirs.
  for (int v = 0; v < 2; ++v) {
    auto& p = g_patched[v];
    if (!p.vtable) continue;
    write_slots(p.vtable, [&] {
      if (p.vtable[kGetDeviceStateSlot] == own_state(v)) p.vtable[kGetDeviceStateSlot] = reinterpret_cast<void*>(p.original_state);
      if (p.vtable[kGetDeviceDataSlot] == own_data(v)) p.vtable[kGetDeviceDataSlot] = reinterpret_cast<void*>(p.original_data);
    });
    // We only forget the vtable; the original function addresses stay, since a call of ours running on another thread may still use them.
    p.vtable = nullptr;
  }
}

}  // namespace fw

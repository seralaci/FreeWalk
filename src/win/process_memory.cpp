#include "win/process_memory.h"
#include <windows.h>

namespace fw {

bool ProcessMemory::read(std::uint64_t address, void* out, std::size_t size) const {
  SIZE_T done = 0;
  return ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<LPCVOID>(address), out, size, &done) && done == size;
}

bool ProcessMemory::write(std::uint64_t address, const void* data, std::size_t size) const {
  // We only write to committed memory; WriteProcessMemory temporarily makes read-only pages writable.
  MEMORY_BASIC_INFORMATION info{};
  if (VirtualQuery(reinterpret_cast<LPCVOID>(address), &info, sizeof info) == 0 || info.State != MEM_COMMIT) return false;
  SIZE_T done = 0;
  return WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<LPVOID>(address), data, size, &done) && done == size;
}

}  // namespace fw

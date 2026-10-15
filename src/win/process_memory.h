#pragma once
#include <cstddef>
#include <cstdint>
#include "core/memory.h"

namespace fw {

// Memory of the game (our own process).
// Reads and writes return an error on an invalid address instead of crashing.
class ProcessMemory final : public Memory {
 public:
  bool read(std::uint64_t address, void* out, std::size_t size) const override;
  bool write(std::uint64_t address, const void* data, std::size_t size) const;

  template <class T>
  bool put(std::uint64_t address, const T& value) const {
    return write(address, &value, sizeof value);
  }
};

}  // namespace fw

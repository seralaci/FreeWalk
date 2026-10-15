#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <utility>
#include <vector>

namespace fw {

// Reading an address space: in the game the own process memory, in tests and the analysis tool an image loaded into a buffer.
class Memory {
 public:
  virtual ~Memory() = default;

  // Returns true only if all size bytes were read; on an invalid address it returns false instead of crashing.
  virtual bool read(std::uint64_t address, void* out, std::size_t size) const = 0;

  template <class T>
  std::optional<T> get(std::uint64_t address) const {
    T value{};
    if (!read(address, &value, sizeof value)) return std::nullopt;
    return value;
  }
};

// A byte array starting at a given address, as memory.
class BufferMemory final : public Memory {
 public:
  BufferMemory(std::uint64_t base, std::vector<std::uint8_t> bytes) : base_(base), bytes_(std::move(bytes)) {}

  bool read(std::uint64_t address, void* out, std::size_t size) const override {
    if (address < base_) return false;
    const std::uint64_t offset = address - base_;
    if (offset > bytes_.size() || size > bytes_.size() - offset) return false;
    std::memcpy(out, bytes_.data() + offset, size);
    return true;
  }

  template <class T>
  void put(std::uint64_t address, const T& value) {
    put_bytes(address, &value, sizeof value);
  }

  void put_bytes(std::uint64_t address, const void* data, std::size_t size) {
    std::memcpy(bytes_.data() + (address - base_), data, size);
  }

  std::uint64_t base() const { return base_; }

 private:
  std::uint64_t base_;
  std::vector<std::uint8_t> bytes_;
};

}  // namespace fw

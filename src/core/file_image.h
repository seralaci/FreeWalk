#pragma once
#include <optional>
#include <string>
#include "core/memory.h"

namespace fw {

// Loads a PE file into a buffer the way Windows would: copies the sections into place, at the preferred base address.
// It applies no relocations, so absolute pointers stored in the file are valid for this base address.
std::optional<BufferMemory> load_pe_file(const std::string& path, std::string& error);

}  // namespace fw

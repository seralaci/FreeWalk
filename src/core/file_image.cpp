#include "core/file_image.h"
#include <algorithm>
#include <fstream>
#include <iterator>
#include "core/pe_image.h"

namespace fw {

std::optional<BufferMemory> load_pe_file(const std::string& path, std::string& error) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    error = "cannot open " + path;
    return std::nullopt;
  }
  std::vector<std::uint8_t> file((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  const BufferMemory raw(0, file);
  const auto headers = PeImage::parse(raw, 0);
  if (!headers) {
    error = path + " is not a 64-bit PE file";
    return std::nullopt;
  }
  std::vector<std::uint8_t> mapped(headers->image_size(), 0);
  const std::size_t header_bytes = std::min<std::size_t>({headers->headers_size(), file.size(), mapped.size()});
  std::copy_n(file.begin(), header_bytes, mapped.begin());
  for (const auto& s : headers->sections()) {
    const std::size_t size = std::min<std::size_t>(s.raw_size, s.virtual_size != 0 ? s.virtual_size : s.raw_size);
    if (s.raw_offset + size > file.size() || s.rva + size > mapped.size()) {
      error = "section " + s.name + " does not fit";
      return std::nullopt;
    }
    std::copy_n(file.begin() + s.raw_offset, size, mapped.begin() + s.rva);
  }
  return BufferMemory(headers->preferred_base(), std::move(mapped));
}

}  // namespace fw

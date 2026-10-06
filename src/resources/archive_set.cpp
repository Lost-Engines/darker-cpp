#include "archive_set.h"
#include <algorithm>
#include <format>
#include <fstream>
#include <stdexcept>
#include "decoder.h"
#include "directory.h"

namespace darker::resources {

std::span<directory_entry const> resource_directory() noexcept {
  /// Expose the immutable directory of the supported executable edition
  return supported_directory;
}

std::vector<std::byte> read_binary_file(std::filesystem::path const &path, std::size_t const size_limit) {
  /// Read a complete bounded file and diagnose missing, oversized or short reads
  std::ifstream stream{path, std::ios::binary | std::ios::ate};
  if(!stream) throw std::runtime_error{std::format("cannot open {}", path.string())};
  auto const end{stream.tellg()};
  if(end < 0 || static_cast<std::uintmax_t>(end) > size_limit) {
    throw std::runtime_error{std::format("invalid or excessive file size: {}", path.string())};
  }
  std::vector<std::byte> result(static_cast<std::size_t>(end));
  stream.seekg(0);
  if(!stream.read(reinterpret_cast<char*>(result.data()), static_cast<std::streamsize>(result.size()))) {
    throw std::runtime_error{std::format("cannot read complete file: {}", path.string())};
  }
  return result;
}

archive_set::archive_set(std::filesystem::path const &install_directory) {
  /// Load the five original packs; this implementation does not reproduce the DOS memory cache
  for(unsigned int archive{0}; archive != archives.size(); ++archive) {
    auto const path{install_directory / std::format("DARKER.{:02}", archive)};
    archives[archive] = read_binary_file(path, 0x3f'ff'ff);
    if(archives[archive].size() != supported_archive_sizes[archive]) {
      throw std::runtime_error{std::format("unsupported archive size: {} (expected {}, got {})", path.string(), supported_archive_sizes[archive], archives[archive].size())};
    }
  }
}

std::vector<std::byte> archive_set::load(resource_id const id) const {
  /// Return independently owned decoded bytes, preserving archive/slot identity at the boundary
  auto const entry{std::ranges::find(supported_directory, id, &directory_entry::id)};
  if(entry == supported_directory.end()) {
    throw std::out_of_range{std::format("unknown resource {} / {}", id.archive, id.slot)};
  }
  auto const packed{std::span{archives[id.archive]}.subspan(entry->offset, entry->compressed_size)};
  try {
    return decompress(packed);
  } catch(std::exception const &error) {
    throw std::runtime_error{std::format("resource {} / {}: {}", id.archive, id.slot, error.what())};
  }
}

} // namespace darker::resources

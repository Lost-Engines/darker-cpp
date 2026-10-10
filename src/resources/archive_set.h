#pragma once

#include <array>
#include <cstddef>
#include <filesystem>
#include <span>
#include <vector>

namespace darker::resources {

struct resource_id {
  unsigned int archive;
  unsigned int slot;
  bool operator==(resource_id const&) const = default;
};

struct directory_entry {
  resource_id id;
  size_t offset;
  size_t compressed_size;
};

std::span<directory_entry const> resource_directory() noexcept;
std::vector<std::byte> read_binary_file(std::filesystem::path const &path, size_t size_limit);

class archive_set {
private:
  std::array<std::vector<std::byte>, 5> archives;

public:
  archive_set(std::filesystem::path const &install_directory);
  std::vector<std::byte> load(resource_id id) const;
};

} // namespace darker::resources

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>

namespace darker::resources {

struct pilot_record {
  uint8_t stage{0};
  std::array<char,29> name{};
  uint16_t weapons{0}, return_site{0};
  std::array<std::byte,953> delphi{};
  std::array<std::byte,468> halon{};
  std::array<std::byte,194> reserved{};
  std::string display_name() const;
  void set_name(std::string_view value);
};

struct save_file {
  std::array<pilot_record,4> pilots{};
  std::array<std::byte,2> trailer{};
};

inline constexpr size_t save_file_size{6600};
uint16_t save_checksum(std::span<std::byte const> bytes) noexcept;
save_file decode_save(std::span<std::byte const> bytes);
std::array<std::byte,save_file_size> encode_save(save_file const &save);
void write_save(std::filesystem::path const &path, save_file const &save);

} // namespace darker::resources

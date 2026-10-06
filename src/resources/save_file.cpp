#include "resources/save_file.h"
#include <algorithm>
#include <fstream>
#include <stdexcept>

namespace darker::resources {

std::string pilot_record::display_name() const {
  /// Bound even an unterminated imported name to its original 29-byte field
  return {name.begin(),std::find(name.begin(),name.end(),'\0')};
}

void pilot_record::set_name(std::string_view const value) {
  /// Leave space for a terminator without disturbing the other saved fields
  if(value.size() >= name.size() || value.find('\0') != std::string_view::npos) throw std::invalid_argument{"Invalid pilot name"};
  name.fill('\0');
  std::ranges::copy(value,name.begin());
}

uint16_t save_checksum(std::span<std::byte const> const bytes) noexcept {
  /// Native 9325 uses CRC-16/XMODEM, stored little-endian by A110
  uint16_t crc{0};
  for(auto const byte : bytes) {
    crc ^= static_cast<uint16_t>(std::to_integer<uint8_t>(byte) << 8);
    for(unsigned int bit{0}; bit < 8; ++bit) crc = static_cast<uint16_t>((crc << 1) ^ (crc & 0x8000 ? 0x1021 : 0));
  }
  return crc;
}

save_file decode_save(std::span<std::byte const> const bytes) {
  /// Decode all four records without discarding unknown fields or the Nightmare trailer
  if(bytes.size() != save_file_size) throw std::invalid_argument{"Save file must contain exactly 6600 bytes"};
  auto const stored{std::to_integer<uint8_t>(bytes[6598]) | (std::to_integer<uint8_t>(bytes[6599]) << 8)};
  if(save_checksum(bytes.first(6598)) != stored) throw std::invalid_argument{"Save file checksum mismatch"};
  size_t cursor{0};
  auto const byte{[&]{ return std::to_integer<uint8_t>(bytes[cursor++]); }};
  auto const word{[&]{ auto const low{byte()}; return static_cast<uint16_t>(low | (byte() << 8)); }};
  auto const block{[&](auto &destination){
    std::ranges::copy(bytes.subspan(cursor,destination.size()),destination.begin());
    cursor += destination.size();
  }};
  save_file result;
  for(auto &pilot : result.pilots) {
    pilot.stage = byte();
    for(auto &character : pilot.name) character = static_cast<char>(byte());
    pilot.weapons = word();
    pilot.return_site = word();
    block(pilot.delphi);
    block(pilot.halon);
    block(pilot.reserved);
  }
  block(result.trailer);
  return result;
}

std::array<std::byte,save_file_size> encode_save(save_file const &save) {
  /// Serialise explicit DOS offsets independently of host alignment and structure padding
  std::array<std::byte,save_file_size> result{};
  size_t cursor{0};
  auto const byte{[&](uint8_t const value){ result[cursor++] = static_cast<std::byte>(value); }};
  auto const word{[&](uint16_t const value){ byte(static_cast<uint8_t>(value)); byte(static_cast<uint8_t>(value >> 8)); }};
  auto const block{[&](auto const &source){
    std::ranges::copy(source,result.begin() + static_cast<ptrdiff_t>(cursor));
    cursor += source.size();
  }};
  for(auto const &pilot : save.pilots) {
    byte(pilot.stage);
    for(auto const character : pilot.name) byte(static_cast<uint8_t>(character));
    word(pilot.weapons);
    word(pilot.return_site);
    block(pilot.delphi);
    block(pilot.halon);
    block(pilot.reserved);
  }
  block(save.trailer);
  word(save_checksum(std::span{result}.first(6598)));
  return result;
}

void write_save(std::filesystem::path const &path, save_file const &save) {
  /// Replace the previous save only after the complete new file has been written and closed
  auto temporary{path};
  temporary += ".tmp";
  auto const bytes{encode_save(save)};
  try {
    std::ofstream stream;
    stream.exceptions(std::ios::failbit | std::ios::badbit);
    stream.open(temporary,std::ios::binary | std::ios::trunc);
    stream.write(reinterpret_cast<char const*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
    stream.close();
    std::filesystem::rename(temporary,path);
  } catch(...) {
    std::error_code ignored;
    std::filesystem::remove(temporary,ignored);
    throw;
  }
}

} // namespace darker::resources

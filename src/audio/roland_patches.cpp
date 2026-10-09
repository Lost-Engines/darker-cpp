#include "audio/roland_patches.h"
#include <array>
#include <stdexcept>
#include <utility>

namespace darker::audio {

auto roland_setup_messages(std::span<std::byte const> const file)->sysex_messages {
  /// Setup banks contain metadata and complete SysEx events, not a playable note sequence
  size_t cursor{0};
  auto const read{[&]()->uint8_t {
    if(cursor >= file.size()) throw std::invalid_argument{"Truncated Roland setup MIDI file"};
    return std::to_integer<uint8_t>(file[cursor++]);
  }};
  auto const big_endian{[&](unsigned int const count) {
    uint32_t value{0};
    for(unsigned int i{0}; i < count; ++i) value = (value << 8) | read();
    return value;
  }};
  auto const variable{[&] {
    uint32_t value{0};
    for(unsigned int i{0}; i < 4; ++i) {
      auto const byte{read()};
      value = (value << 7) | (byte & 127);
      if(!(byte & 128)) return value;
    }
    throw std::invalid_argument{"Invalid Roland setup MIDI event length"};
  }};
  if(big_endian(4) != 0x4d546864 || big_endian(4) != 6 || big_endian(2) != 0 || big_endian(2) != 1)
    throw std::invalid_argument{"Roland setup requires a format-0 single-track MIDI file"};
  big_endian(2); // Timing is irrelevant: upload the complete bank before playback.
  if(big_endian(4) != 0x4d54726b) throw std::invalid_argument{"Missing Roland setup MIDI track"};
  auto const track_size{big_endian(4)};
  if(track_size != file.size() - cursor) throw std::invalid_argument{"Invalid Roland setup MIDI track size"};
  sysex_messages messages;
  while(cursor < file.size()) {
    variable(); // Delta time.
    auto const status{read()};
    if(status == 0xff) {
      read(); // Metadata type.
      auto const length{variable()};
      if(length > file.size() - cursor) throw std::invalid_argument{"Truncated Roland setup metadata"};
      cursor += length;
      continue;
    }
    if(status != 0xf0) throw std::invalid_argument{"Roland setup must contain complete SysEx events only"};
    auto const length{variable()};
    if(length < 9 || length > file.size() - cursor) throw std::invalid_argument{"Truncated Roland setup SysEx"};
    std::vector<uint8_t> message{0xf0};
    for(uint32_t i{0}; i < length; ++i) message.push_back(read());
    if(message[1] != 0x41 || message[2] != 0x10 || message[3] != 0x16 || message[4] != 0x12 || message.back() != 0xf7)
      throw std::invalid_argument{"Roland setup requires MT-32 device-17 DT1 messages"};
    unsigned int checksum{0};
    for(size_t i{5}; i + 1 < message.size(); ++i) {
      if(message[i] > 127) throw std::invalid_argument{"Invalid Roland setup SysEx data"};
      checksum += message[i];
    }
    if(checksum & 127) throw std::invalid_argument{"Invalid Roland setup SysEx checksum"};
    messages.push_back(std::move(message));
  }
  if(messages.empty()) throw std::invalid_argument{"Roland setup contains no instruments"};
  return messages;
}

auto lapc_initialisation(std::span<std::byte const> const driver)->sysex_messages {
  /// Native 07DF loads the directory at 1A94; 080E expands timbres and 07F7 assigns patch slots
  auto const read{[&](size_t const offset)->uint8_t {
    if(offset >= driver.size()) throw std::invalid_argument{"Truncated LAPC-I instrument table"};
    return std::to_integer<uint8_t>(driver[offset]);
  }};
  if(driver.size() != 0x1d13) throw std::invalid_argument{"Unexpected LAPC-I driver size"};
  size_t cursor{static_cast<size_t>(read(0x1a94) | (read(0x1a95) << 8))};
  sysex_messages messages;
  auto const append{[&](uint32_t const address, std::span<uint8_t const> const data) {
    // 0823 emits Roland device 10h, model 16h, DT1; 0782 accumulates the seven-bit checksum
    std::vector<uint8_t> message{0xf0, 0x41, 0x10, 0x16, 0x12,
      static_cast<uint8_t>((address >> 14) & 127), static_cast<uint8_t>((address >> 7) & 127), static_cast<uint8_t>(address & 127)};
    message.insert(message.end(), data.begin(), data.end());
    unsigned int checksum{0};
    for(size_t i{5}; i < message.size(); ++i) checksum += message[i];
    message.push_back(static_cast<uint8_t>(-checksum & 127));
    message.push_back(0xf7);
    messages.push_back(std::move(message));
  }};
  auto const count{read(cursor++)};
  for(unsigned int i{0}; i < count; ++i) {
    auto const slot{read(cursor++)};
    if(slot > 63) throw std::invalid_argument{"LAPC-I timbre index exceeds writable memory"};
    auto remaining{static_cast<unsigned int>(read(cursor++))};
    std::vector<uint8_t> data;
    while(remaining) {
      auto value{read(cursor++)};
      --remaining;
      unsigned int repeats{1};
      if(value & 128) {
        if(!remaining) throw std::invalid_argument{"Truncated LAPC-I timbre repeat"};
        value &= 127;
        repeats = read(cursor++);
        --remaining;
        if(!repeats) throw std::invalid_argument{"Invalid LAPC-I timbre repeat count"};
      }
      data.insert(data.end(), repeats, value);
    }
    // Skip the ten-byte name: the original deliberately retains it and overwrites only synthesis parameters
    append(0x20000 + static_cast<uint32_t>(slot) * 256 + 10, data);
  }
  for(;;) {
    auto const slot{read(cursor++)};
    if(slot & 128) break;
    std::array<uint8_t,4> packed{};
    for(auto &value : packed) value = read(cursor++);
    std::array<uint8_t,7> const data{static_cast<uint8_t>(packed[0] >> 6), static_cast<uint8_t>(packed[0] & 63),
      static_cast<uint8_t>(packed[1] & 63), static_cast<uint8_t>(packed[2] & 127), static_cast<uint8_t>(packed[3] & 127),
      static_cast<uint8_t>(packed[1] >> 6), static_cast<uint8_t>(packed[2] >> 7)};
    append(0x14000 + static_cast<uint32_t>(slot) * 8, data);
  }
  return messages;
}

} // namespace darker::audio

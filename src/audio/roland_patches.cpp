#include "audio/roland_patches.h"
#include <array>
#include <stdexcept>
#include <utility>

namespace darker::audio {

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

#include "audio/fm_driver.h"
#include <algorithm>
#include <stdexcept>
#include "audio/fm_patches.h"

namespace darker::audio {
namespace {

std::array<std::uint8_t, 9> constexpr operators{0, 1, 2, 8, 9, 10, 16, 17, 18};

} // namespace

fm_program fm_driver::stop(std::uint8_t const channel) {
  /// 0F0A silences the carrier and releases the key unless that release is already cached
  if(channel >= voices.size()) throw std::out_of_range{"FM channel exceeds nine voices"};
  if(voices[channel].block == 31) return {};
  voices[channel].block = 31;
  return {
    .writes{{{static_cast<std::uint8_t>(0x43 + operators[channel]), 63}, {static_cast<std::uint8_t>(0xb0 + channel), 31}}},
    .count{2}
  };
}

fm_program fm_driver::program(std::uint8_t const channel, std::uint8_t const patch, std::uint16_t pitch,
  std::uint16_t const level, bool const key_on, bool const retrigger) {
  /// 1121/10F4/39A9 program the original nonspatial OPL voice, retaining patch and block caches
  if(channel >= voices.size() || patch >= fm_patches.size()) throw std::out_of_range{"FM voice or patch index exceeds the original bank"};
  auto &voice{voices[channel]};
  auto const &p{fm_patches[patch]};
  auto const op{operators[channel]};
  fm_program result;
  auto const write{[&](int const address, int const value){
    result.writes[result.count++] = {
      .address{static_cast<std::uint8_t>(address)},
      .value{static_cast<std::uint8_t>(value)}
    };
  }};
  if(voice.patch != patch) {
    voice.patch = patch;
    write(0xc0 + channel, p[1] & 15);
    write(0xe0 + op, (p[1] >> 6) & 3);
    write(0x20 + op, p[2]);
    write(0x80 + op, p[4]);
    write(0x60 + op, p[5]);
    write(0x40 + op, p[3]);
    write(0xe3 + op, (p[1] >> 4) & 3);
    write(0x23 + op, p[6]);
    write(0x83 + op, p[8]);
    write(0x63 + op, p[9]);
  }
  if(retrigger) {
    auto const release{stop(channel)};
    for(auto const command : release.view()) write(command.address, command.value);
  }
  auto block{static_cast<std::uint8_t>((key_on ? 32 : 0) + ((p[0] & 192) >> 4) - 4)};
  while(pitch >= 768) {
    pitch >>= 1;
    block = static_cast<std::uint8_t>(block + 4);
  }
  block = static_cast<std::uint8_t>(block + (pitch >> 8));
  write(0xa0 + channel, pitch & 255);
  if(voice.block != block) {
    voice.block = block;
    write(0xb0 + channel, block);
  }
  auto const scaled{std::min(65535u, static_cast<unsigned int>(level) + (level >> 4))};
  write(0x43 + op, 63 ^ (scaled >> 10));
  return result;
}

} // namespace darker::audio

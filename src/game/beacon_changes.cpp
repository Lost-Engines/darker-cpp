#include "game/beacon_changes.h"
#include <algorithm>
#include <stdexcept>

namespace darker::game {

void beacon_changes::command(uint8_t const opcode, uint8_t origin, uint8_t length, uint16_t const clock, std::span<std::byte const> const queue) {
  /// C442/C467/C470 configure a retained fade; C1A0 reverses its direction without starting a new run
  if(opcode == 0x14) {
    dimming = !dimming;
    return;
  }
  if(opcode == 0x11) {
    if(queue_cursor >= queue.size()) throw std::out_of_range{"Beacon command exhausted the scenario queue"};
    origin = std::to_integer<uint8_t>(queue[queue_cursor++]);
    length = 1;
    dimming = true;
  } else if(opcode == 0x12) stride = 18;
  else if(opcode == 0x13) stride = 0x900;
  else throw std::invalid_argument{"Unknown beacon change command"};
  position = static_cast<uint16_t>(((origin & 15) * 256 + (origin >> 4) * 2) * 9);
  deadline = static_cast<uint16_t>(clock + 256);
  count = length;
  active = true;
}

void beacon_changes::advance(city_map &cells, uint16_t const clock) {
  /// C510 fades over 256 ticks; dimming only lowers output, while restoration writes each intermediate value
  if(!active) return;
  auto const remaining{static_cast<uint16_t>(clock - deadline)};
  uint8_t level{static_cast<uint8_t>(remaining)};
  if(!(remaining & 0x8000)) {
    level = 255;
    active = false;
  }
  if(dimming) level = static_cast<uint8_t>(~level);
  auto offset{position};
  for(unsigned int i{0}; i < (count ? count : 65536u); ++i) {
    auto &state{cells.at(offset / 2).state};
    state = dimming ? std::min(state, level) : level;
    offset = static_cast<uint16_t>(offset + stride);
  }
}

} // namespace darker::game

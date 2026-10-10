#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include "game/city_map.h"
#include "game/time.h"

namespace darker::game {

class beacon_changes {
private:
  size_t queue_cursor{0};
  uint16_t position{0};
  clock_tick deadline{0};
  uint16_t stride{18};
  uint8_t count{0};
  bool dimming{true};
  bool active{false};

public:
  void command(uint8_t opcode, uint8_t origin, uint8_t length, clock_tick clock, std::span<std::byte const> queue);
  void advance(city_map &cells, clock_tick clock);
};

} // namespace darker::game

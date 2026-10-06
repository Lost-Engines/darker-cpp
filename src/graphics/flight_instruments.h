#pragma once

#include <cstdint>
#include "game/caero_flight.h"

namespace darker::graphics {

struct caero_instruments {
  std::uint8_t altitude{0};
  std::uint8_t impact{0};
  std::uint8_t damage_lights{0};
  std::uint8_t power_cells{0};
  std::uint8_t charging{0};
};

caero_instruments measure_caero_instruments(game::caero_flight_state const &state, std::uint16_t clock) noexcept;
std::uint8_t skimma_speed_instrument(std::uint16_t speed, bool upgraded) noexcept;

} // namespace darker::graphics

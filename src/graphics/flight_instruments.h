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

uint8_t skimma_mission_bearing(uint16_t site, uint16_t column, uint16_t row, uint16_t heading);

uint8_t caero_engine_indicator(uint8_t previous, bool enabled, uint16_t speed) noexcept;

caero_instruments measure_caero_instruments(game::caero_flight_state const &state, std::uint16_t clock) noexcept;
struct shield_strip_range {
  std::uint8_t first{0};
  std::uint8_t end{0};
};

shield_strip_range skimma_shield_strips(std::uint8_t strength) noexcept;

struct skimma_instruments {
  std::uint8_t low_altitude{0};
  std::uint8_t shield{0};
  std::uint8_t shield_startup{0};
  bool shield_ready_sound{false};
};

skimma_instruments measure_skimma_instruments(std::uint16_t height, std::uint16_t shield_charge, bool shield_enabled,
  bool warning_flash, std::uint16_t clock, std::uint16_t &shield_deadline) noexcept;
std::uint8_t skimma_speed_instrument(std::uint16_t speed, bool upgraded) noexcept;

} // namespace darker::graphics

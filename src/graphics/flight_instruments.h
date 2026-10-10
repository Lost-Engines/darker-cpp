#pragma once

#include <cstdint>
#include "game/caero_flight.h"

namespace darker::graphics {

struct caero_instruments {
  uint8_t altitude{0};
  uint8_t impact{0};
  uint8_t damage_lights{0};
  uint8_t power_cells{0};
  uint8_t charging{0};
};

uint8_t skimma_mission_bearing(uint16_t site, uint16_t column, uint16_t row, uint16_t heading);

uint8_t caero_receiver_indicator(uint16_t clock, bool message, bool script_stopped, bool objectives_complete, uint8_t player_flags) noexcept;

uint8_t caero_engine_indicator(uint8_t previous, bool enabled, uint16_t speed) noexcept;

caero_instruments measure_caero_instruments(game::caero_flight_state const &state, uint16_t clock) noexcept;
struct shield_strip_range {
  uint8_t first{0};
  uint8_t end{0};
};

shield_strip_range skimma_shield_strips(uint8_t strength) noexcept;

struct skimma_instruments {
  static uint8_t constexpr engine_output_limit{16};
  static uint8_t constexpr upgraded_engine_output_limit{20};
  static uint8_t constexpr shield_startup_limit{23};

  uint8_t low_altitude{0};
  uint8_t shield{0};
  uint8_t shield_startup{0};
  bool shield_ready_sound{false};
};

skimma_instruments measure_skimma_instruments(uint16_t height, uint16_t shield_charge, bool shield_enabled,
  bool warning_flash, uint16_t clock, uint16_t &shield_deadline) noexcept;
uint8_t skimma_speed_instrument(uint16_t speed, bool upgraded) noexcept;

} // namespace darker::graphics

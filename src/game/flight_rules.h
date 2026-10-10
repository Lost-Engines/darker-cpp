#pragma once

#include <cstdint>

namespace darker::game {

// native drive settings, not world-distance or host-time units
struct skimma_flight_rules {
  static uint16_t constexpr low_drive{248};
  static uint16_t constexpr high_drive{500};
  static uint16_t constexpr upgraded_boost_drive{640};
  static unsigned int constexpr steering_assist_speed_limit{2047};
  static int8_t constexpr default_vertical_bias{-106};
};

struct caero_flight_rules {
  static uint16_t constexpr startup_energy_limit{0x5000};                     // high byte 80 yields five complete pips when doubled and masked
  static unsigned int constexpr startup_charge_rate{7};
  static uint16_t constexpr active_boost_duration{0x3800};                    // native drive-accounting units, not milliseconds
};

} // namespace darker::game

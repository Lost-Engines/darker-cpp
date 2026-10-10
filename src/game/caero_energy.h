#pragma once

#include <cstdint>
#include "game/time.h"

namespace darker::game {

struct caero_energy_state {
  static uint16_t constexpr buffer_capacity{0xffff};
  static uint16_t constexpr reserve_capacity{0xcfff};                          // twelve complete display units plus the fractional remainder
  static uint16_t constexpr boost_capacity{0xbfff};                            // five complete 0x2000 boost pips plus a partial sixth
  static uint16_t constexpr boost_pip_energy{0x2000};
  uint16_t buffer{0};
  uint16_t reserve{0};
  uint16_t boost{0};
  uint8_t incoming_display{0};
  uint8_t reserve_display{0};

  void charge(uint16_t source, uint8_t engine_flags, game_duration accounting_step, bool boost_cheat) noexcept;
};

} // namespace darker::game

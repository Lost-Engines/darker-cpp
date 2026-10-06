#pragma once

#include <cstdint>

namespace darker::game {

struct caero_energy_state {
  std::uint16_t buffer{0};
  std::uint16_t reserve{0};
  std::uint16_t boost{0};
  std::uint8_t incoming_display{0};
  std::uint8_t reserve_display{0};
};

void charge_caero_energy(caero_energy_state &state, std::uint16_t source, std::uint8_t engine_flags,
  std::uint16_t accounting_step, bool boost_cheat) noexcept;

} // namespace darker::game

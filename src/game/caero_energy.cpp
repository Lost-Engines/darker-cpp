#include "game/caero_energy.h"
#include <algorithm>

namespace darker::game {

void charge_caero_energy(caero_energy_state &state, std::uint16_t const source, std::uint8_t const engine_flags,
  std::uint16_t const accounting_step, bool const boost_cheat) noexcept {
  /// 7F38's gate/display followed by 7F6C's accounting and 8529's reserve caps; the caller supplies the transformed timestep
  unsigned int const energy{engine_flags == 1 ? source : 0u};
  state.incoming_display = static_cast<std::uint8_t>(std::min(24u, energy >> 8) >> 1);
  auto gain{energy > 250 ? (boost_cheat ? accounting_step >> 1 : ((energy - 250) * accounting_step) >> 17) : 0u};
  if(gain != 0) gain = static_cast<std::uint16_t>(gain + (accounting_step >> 8));
  auto const incoming{(std::min(energy, 250u) * 257 * accounting_step) >> 18};
  auto buffer{std::min(65535u, state.buffer + incoming)};
  unsigned int transfer{0};
  if(energy >= 256) {
    transfer = std::min<unsigned int>(static_cast<std::uint16_t>(0xcfff - state.reserve), accounting_step >> 2);
    transfer = std::min(transfer, buffer);
    buffer -= transfer;
  }
  state.buffer = static_cast<std::uint16_t>(buffer);
  state.reserve = std::min<std::uint16_t>(static_cast<std::uint16_t>(state.reserve + transfer), 0xcfff);
  state.reserve_display = static_cast<std::uint8_t>(state.reserve >> 12);
  state.boost = std::min<std::uint16_t>(static_cast<std::uint16_t>(state.boost + gain), 0xbfff);
}

} // namespace darker::game

#include "game/caero_energy.h"
#include <algorithm>

namespace darker::game {

void caero_energy_state::charge(uint16_t const source, uint8_t const engine_flags,
  game_duration const accounting_step, bool const boost_cheat) noexcept {
  /// 7F38's gate/display followed by 7F6C's accounting and 8529's reserve caps; the caller supplies the transformed timestep
  unsigned int constexpr incoming_display_limit{24};
  unsigned int constexpr drive_source_limit{250};                              // light beyond the drive budget charges boost cells
  unsigned int constexpr reserve_source_threshold{256};
  unsigned int constexpr byte_replication_scale{257};                          // reproduce the native byte-to-word expansion before fixed-point division
  unsigned int const energy{engine_flags == 1 ? source : 0u};
  incoming_display = static_cast<uint8_t>(std::min(incoming_display_limit, energy >> 8) >> 1);
  auto gain{energy > drive_source_limit ? (boost_cheat ? accounting_step >> 1 : ((energy - drive_source_limit) * accounting_step) >> 17) : 0u};
  if(gain != 0) gain = static_cast<uint16_t>(gain + (accounting_step >> 8));
  auto const incoming{(std::min(energy, drive_source_limit) * byte_replication_scale * accounting_step) >> 18};
  auto charged_buffer{std::min<unsigned int>(caero_energy_state::buffer_capacity, buffer + incoming)};
  unsigned int transfer{0};
  if(energy >= reserve_source_threshold) {
    transfer = std::min<unsigned int>(static_cast<uint16_t>(caero_energy_state::reserve_capacity - reserve), accounting_step >> 2);
    transfer = std::min(transfer, charged_buffer);
    charged_buffer -= transfer;
  }
  buffer = static_cast<uint16_t>(charged_buffer);
  reserve = std::min<uint16_t>(static_cast<uint16_t>(reserve + transfer), caero_energy_state::reserve_capacity);
  reserve_display = static_cast<uint8_t>(reserve >> 12);
  boost = std::min<uint16_t>(static_cast<uint16_t>(boost + gain), caero_energy_state::boost_capacity);
}

} // namespace darker::game

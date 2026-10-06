#include "graphics/flight_instruments.h"
#include <algorithm>
#include <bit>

namespace darker::graphics {

caero_instruments measure_caero_instruments(game::caero_flight_state const &state, std::uint16_t const clock) noexcept {
  /// 56D3–573B derives damage, boost and altitude strips; charging sounds are a separate event consumer
  unsigned int impact{state.damage.damage & 255u};
  unsigned int lights{state.damage.damage >> 8};
  if(lights >= 3) {
    if(lights != 3) impact = 67;
    lights = impact >= 36 && (clock & 128) ? 0 : 3;
  }
  auto const altitude{std::bit_cast<std::int16_t>(static_cast<std::uint16_t>((std::bit_cast<std::int16_t>(state.pose.position[2]) >> 2) + 224))};
  return {
    .altitude{static_cast<std::uint8_t>(std::min(8, std::max(0, static_cast<int>(altitude)) >> 8))},
    .impact{static_cast<std::uint8_t>(impact >> 2)}, .damage_lights{static_cast<std::uint8_t>(lights)},
    .power_cells{static_cast<std::uint8_t>(state.energy.boost >> 13)},
    .charging{static_cast<std::uint8_t>((13 * ((state.energy.boost >> 5) & 255)) >> 8)},
  };
}

std::uint8_t skimma_speed_instrument(std::uint16_t const speed, bool const upgraded) noexcept {
  /// 582B retains the low byte of the unsigned product's high word before selecting the craft-specific strip limit
  auto const value{static_cast<std::uint8_t>((static_cast<std::uint32_t>(speed) * 0x760) >> 16)};
  return std::min<std::uint8_t>(value, upgraded ? 20 : 16);
}

} // namespace darker::graphics

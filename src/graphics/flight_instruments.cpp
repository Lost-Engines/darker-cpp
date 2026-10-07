#include "graphics/flight_instruments.h"
#include <algorithm>
#include <bit>
#include "maths/direction.h"

namespace darker::graphics {

uint8_t skimma_mission_bearing(uint16_t const site, uint16_t const column, uint16_t const row, uint16_t const heading) {
  /// 576E projects the scripted cell centre into seven forward bearing sectors, hiding absent and rearward targets
  if(site & 0x8000) return 0;
  auto const dx{static_cast<uint16_t>((site & 255)*256+128-column)};
  auto const dy{static_cast<uint16_t>((site & 0xff00)+128-row)};
  auto const direction{static_cast<uint16_t>(maths::direction_index(dx,dy) << 5)};
  auto const difference{static_cast<uint16_t>(heading-direction)};
  auto const sector{static_cast<uint8_t>((difference >> 8)+0xb8)};
  return sector < 0x70 ? static_cast<uint8_t>((sector >> 4)+1) : 0;
}

uint8_t caero_engine_indicator(uint8_t const previous, bool const enabled, uint16_t const speed) noexcept {
  /// 56B2–56C5 retains dimming between the stall and recovery speed thresholds
  auto dim{static_cast<uint8_t>(previous & 128)};
  if(speed < 200) dim = 128;
  if(speed >= 410) dim = 0;
  return static_cast<uint8_t>(dim | (enabled ? 1 : 0));
}

caero_instruments measure_caero_instruments(game::caero_flight_state const &state, std::uint16_t const clock) noexcept {
  /// 56D3–573B derives damage, boost and altitude strips; charging sounds are a separate event consumer
  unsigned int impact{state.damage.damage & 255u};
  unsigned int lights{static_cast<unsigned int>(state.damage.damage >> 8)};
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

shield_strip_range skimma_shield_strips(std::uint8_t const strength) noexcept {
  /// 5845 supplies both the shield count and alternating first strip used during startup
  unsigned int count{strength};
  std::uint8_t first{0};
  if(count >= 8) {
    count -= 2;
    first = static_cast<std::uint8_t>((count & 1) == 0 ? 1 : 0);
    count += count >> 1;
  }
  return {.first{first}, .end{static_cast<std::uint8_t>(count <= 2 ? (count == 0 ? 0 : 1) : count - 2)}};
}

skimma_instruments measure_skimma_instruments(std::uint16_t const height, std::uint16_t const shield_charge, bool const shield_enabled,
  bool const warning_flash, std::uint16_t const clock, std::uint16_t &shield_deadline) noexcept {
  /// 579C–5828 separates the height warning, shield startup animation and available shield-strength strip
  skimma_instruments result{.low_altitude{static_cast<std::uint8_t>(height < 1024 && !(warning_flash && (clock & 256)) ? 1 : 0)}};
  if(!shield_enabled) return result;
  auto remaining{static_cast<std::uint16_t>(shield_deadline - clock)};
  auto const charge{static_cast<std::uint8_t>(shield_charge >> 8)};
  std::uint8_t strength{charge};
  if(remaining & 0x8000) {
    shield_deadline = clock;
  } else if(remaining >> 8) {
    auto const high{remaining >> 8};
    if(high <= 2) result.shield_startup = high == 1 ? 1 : 0;
    else {
      auto const phase{static_cast<std::uint16_t>(remaining - 768) >> 2};
      result.shield_startup = static_cast<std::uint8_t>(4 + ((20 * static_cast<std::uint8_t>(~phase)) >> 8));
    }
    return result;
  } else {
    remaining = static_cast<std::uint16_t>((remaining << 2) + (remaining >> 1));
    remaining = static_cast<std::uint16_t>((remaining & 255) | (((remaining & 0xff00) << 4) & 0xff00));
    strength = static_cast<std::uint8_t>(191 - (remaining >> 8));
    if(strength > charge) {
      shield_deadline = static_cast<std::uint16_t>(shield_deadline - remaining);
      strength = charge;
    }
  }
  result.shield = skimma_shield_strips(static_cast<std::uint8_t>(strength >> 3)).end;
  result.shield_ready_sound = true;
  return result;
}

std::uint8_t skimma_speed_instrument(std::uint16_t const speed, bool const upgraded) noexcept {
  /// 582B retains the low byte of the unsigned product's high word before selecting the craft-specific strip limit
  auto const value{static_cast<std::uint8_t>((static_cast<std::uint32_t>(speed) * 0x760) >> 16)};
  return std::min<std::uint8_t>(value, upgraded ? 20 : 16);
}

} // namespace darker::graphics

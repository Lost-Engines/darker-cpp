#include "game/caero_weapons.h"
#include <algorithm>
#include <stdexcept>
#include "game/object_definitions.h"

namespace darker::game {

uint8_t pinner_direct_strength(bool const underground) noexcept {
  /// BCBF patches definition zero from world profile BDEC: 34h in Delphi and 3Bh underground
  return underground ? 0x3b : 0x34;
}

std::optional<uint8_t> chargeable_impact_strength(uint16_t const deadline, uint16_t const clock) noexcept {
  /// CF37 ignores expired and last-page impacts; otherwise AH after the wrapped shift supplies strength
  auto const remaining{static_cast<uint16_t>(deadline - clock)};
  if(remaining < 256 || (remaining & 0x8000)) return std::nullopt;
  return static_cast<uint8_t>(remaining >> 5);
}

caero_fire_result fire_caero_weapon(projectile_pool &pool, caero_energy_state &energy, uint16_t &charge, caero_fire_request const request) {
  /// C9C2 gates shots; CA7D accumulates Chargeable energy while held and releases it as projectile lifetime
  auto const selection{request.selection};
  if(selection != 1 && selection != 2 && selection != 6 && selection != 9 && selection != 10) throw std::invalid_argument{"Caero firing branch is not implemented"};
  auto const &definition{original_object_definitions[selection - 1]};
  auto const cost{static_cast<uint16_t>((request.underground ? 0x80 : definition.role_data[0]) * 256 + 255)};
  if((request.player_flags & 0x30) || !pool.objects().free) return {};
  auto lifetime{static_cast<uint16_t>(definition.role_data[1] * 256)};
  if(selection == 9) {
    if(request.pressed) {
      if(energy.reserve < cost) return {};
      energy.reserve -= cost;
      charge = static_cast<uint16_t>((cost & 0x8000) ? 0xffff : cost);
      return {.ready{true}};
    }
    if(request.held) {
      if(charge) {
        auto const drain{static_cast<uint16_t>(request.frame_step * (charge == 0xffff ? 2 : 64))};
        auto const spent{std::min(energy.reserve,drain)};
        energy.reserve -= spent;
        charge = static_cast<uint16_t>(std::min(unsigned{charge} + spent,65535u));
      }
      return {.ready{true}};
    }
    lifetime = charge >> 4;
    charge = 0;
    if(!lifetime || !(static_cast<uint16_t>(request.target + 1) & 0x8000)) return {.ready{true}};
  } else {
    if(energy.reserve < cost) return {};
    if(selection == 10 && !(static_cast<uint16_t>(request.target + 1) & 0x8000)) return {};
    if(selection == 6 && (request.target & 0x8000)) return {};
    if(!request.pressed) return {.ready{true}};
    energy.reserve -= cost;
  }
  return {
    .shot{pool.launch({.definition{definition}, .emitter{request.emitter}, .model_token{request.model}, .clock{request.clock},
      .lifetime{lifetime}, .target_token{request.target}})},
    .ready{true},
  };
}

} // namespace darker::game

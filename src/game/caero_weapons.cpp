#include "game/caero_weapons.h"
#include <algorithm>
#include <stdexcept>
#include "game/beacon_light.h"
#include "game/object_definitions.h"
#include "maths/world_coordinates.h"

namespace darker::game {

diffuser_impact diffuser_state::hit(bool const gas, uint8_t const category, uint8_t const state, uint16_t const target, uint16_t const clock) noexcept {
  /// CE84 shares one gas target and accepts its trigger only in the final 1000h ticks before the 2800h deadline
  if(category != 3 || !(state & 0x40)) return diffuser_impact::rejected;
  if(gas) {
    cell = target;
    deadline = static_cast<uint16_t>(clock + 0x2800);
    return diffuser_impact::gas;
  }
  if(target != cell || static_cast<uint16_t>(clock - deadline) < 0xf000) return diffuser_impact::rejected;
  sound_deadline = clock;
  deadline = clock;
  return diffuser_impact::destroyed;
}

uint8_t caero_weapon_strength(city_map const &cells, maths::world_position const position, uint16_t const victim) {
  /// CF21 leaves the victim pointer in AX, so 8450 uses its nibbles as sub-coordinate fractions
  auto const light{beacon_light(cells,position,{static_cast<uint8_t>(victim),static_cast<uint8_t>(victim >> 8)})};
  return static_cast<uint8_t>((light >> 6) + 45);
}

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
  if(selection < 1 || selection > 10) throw std::invalid_argument{"Caero weapon selection is outside 1–10"};
  auto const &definition{original_object_definitions[selection - 1]};
  auto const cost{static_cast<uint16_t>((request.underground ? 0x80 : definition.role_data.projectile().launch_cost) * 256 + 255)};
  if((request.player_flags & 0x30) || !pool.objects().free) return {};
  auto lifetime{static_cast<uint16_t>(definition.role_data.projectile().lifetime * 256)};
  auto target{request.target};
  uint8_t next_selection{0};
  if(selection == 9) {
    if(request.pressed) {
      if(energy.reserve < cost) return {};
      energy.reserve -= cost;
      charge = static_cast<uint16_t>((cost & 0x8000) ? 0xffff : cost);
      return {
        .ready{true}
      };
    }
    if(request.held) {
      if(charge) {
        auto const drain{static_cast<uint16_t>(request.frame_step * (charge == 0xffff ? 2 : 64))};
        auto const spent{std::min(energy.reserve,drain)};
        energy.reserve -= spent;
        charge = static_cast<uint16_t>(std::min(unsigned{charge} + spent,65535u));
      }
      return {
        .ready{true}
      };
    }
    lifetime = charge >> 4;
    charge = 0;
    if(!lifetime || !(static_cast<uint16_t>(request.target + 1) & 0x8000)) return {
      .ready{true}
    };
  } else if(selection == 4 || selection == 5) {
    if(energy.reserve < cost || (request.target & 0x8000)) return {
      .next_selection{request.pressed ? uint8_t{5} : uint8_t{0}}
    };
    if(!request.pressed) return {
      .ready{true}
    };
    energy.reserve -= cost;
    next_selection = selection ^ 1;
  } else {
    if(energy.reserve < cost) return {};
    if(selection == 7) {
      auto const *capsule{pool.objects().tail};
      if(!capsule || capsule->parameters.definition != &original_object_definitions[2]) return {
        .ready{true},
        .next_selection{3}
      };
      if(!request.released) return {
        .ready{true}
      };
      target = capsule->native_id;
    }
    if((selection == 8 || selection == 10) && !(static_cast<uint16_t>(request.target + 1) & 0x8000)) return {};
    if(selection == 6 && (request.target & 0x8000)) return {};
    if(selection != 7 && !request.pressed) return {
      .ready{true}
    };
    energy.reserve -= cost;
    if(selection == 3) next_selection = 7;
    else if(selection == 7) next_selection = 3;
  }
  return {
    .shot{pool.launch({
      .definition{definition},
      .emitter{request.emitter},
      .model_token{request.model},
      .clock{request.clock},
      .lifetime{lifetime},
      .target_token{target}
    })},
    .ready{true},
    .next_selection{next_selection},
  };
}

} // namespace darker::game

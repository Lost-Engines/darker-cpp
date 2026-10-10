#include "game/caero_weapons.h"
#include "game/weapon_selection.h"
#include <algorithm>
#include <stdexcept>
#include "game/beacon_light.h"
#include "game/object_definitions.h"
#include "maths/world_coordinates.h"
#include "game/target_reference.h"
#include "game/player_flags.h"

namespace darker::game {

diffuser_impact diffuser_state::hit(bool const gas, collision_category const category, uint8_t const state, uint16_t const target, clock_tick const clock) noexcept {
  /// CE84 shares one gas target and accepts its trigger only in the final 1000h ticks before the 2800h deadline
  int constexpr gas_lifetime_ticks{0x2800};
  uint16_t constexpr trigger_window_start{0xf000};                             // wrapped -0x1000: the final 4096 ticks
  if(category != collision_category::diffuser || !(state & city_cell::permitted_target)) return diffuser_impact::rejected;
  if(gas) {
    cell = target;
    deadline = static_cast<uint16_t>(clock + gas_lifetime_ticks);
    return diffuser_impact::gas;
  }
  if(target != cell || static_cast<uint16_t>(clock - deadline) < trigger_window_start) return diffuser_impact::rejected;
  sound_deadline = clock;
  deadline = clock;
  return diffuser_impact::destroyed;
}

uint8_t caero_weapon_strength(city_map const &cells, maths::world_position const position, uint16_t const victim) {
  /// CF21 leaves the victim pointer in AX, so 8450 uses its nibbles as sub-coordinate fractions
  auto const light{beacon_light(cells, position, {static_cast<uint8_t>(victim), static_cast<uint8_t>(victim >> 8)})};
  int constexpr beacon_strength_shift{6};
  int constexpr unpowered_weapon_strength{45};
  return static_cast<uint8_t>((light >> beacon_strength_shift) + unpowered_weapon_strength);
}

uint8_t pinner_direct_strength(bool const underground) noexcept {
  /// BCBF patches definition zero from world profile BDEC: 34h in Delphi and 3Bh underground
  uint8_t constexpr underground_strength{0x3b};
  uint8_t constexpr delphi_strength{0x34};
  return underground ? underground_strength : delphi_strength;
}

std::optional<impact_strength> chargeable_impact_strength(clock_tick const deadline, clock_tick const clock) noexcept {
  /// CF37 ignores expired and last-page impacts; otherwise AH after the wrapped shift supplies strength
  auto const remaining{static_cast<uint16_t>(deadline - clock)};
  int constexpr minimum_damaging_lifetime_ticks{256};
  int constexpr lifetime_to_strength_shift{5};
  if(remaining < minimum_damaging_lifetime_ticks || (remaining & 0x8000)) return std::nullopt;
  return static_cast<uint8_t>(remaining >> lifetime_to_strength_shift);
}

caero_fire_result fire_caero_weapon(projectile_pool &pool, caero_energy_state &energy, uint16_t &charge, caero_fire_request const request) {
  /// C9C2 gates shots; CA7D accumulates Chargeable energy while held and releases it as projectile lifetime
  auto const selection{static_cast<caero_weapon>(request.selection)};
  if(selection < caero_weapon::pinner_direct || selection > caero_weapon::brent_hunter) throw std::invalid_argument{"Caero weapon selection is outside 1–10"};
  auto const &definition{original_object_definitions[caero_definition_slot(selection)]};
  auto const cost{static_cast<uint16_t>((request.underground ? 0x80 : definition.role_data.projectile().launch_cost) * 256 + 255)};
  if(player_actions_blocked(request.player_flags) || !pool.objects().free_head()) return {};
  auto lifetime{static_cast<uint16_t>(definition.role_data.projectile().lifetime * 256)};
  auto target{request.target};
  caero_weapon next_selection{caero_weapon::none};
  if(selection == caero_weapon::chargeable) {
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
        auto const spent{std::min(energy.reserve, drain)};
        energy.reserve -= spent;
        charge = static_cast<uint16_t>(std::min(unsigned{charge} + spent, 65535u));
      }
      return {
        .ready{true}
      };
    }
    lifetime = charge >> 4;
    charge = 0;
    if(!lifetime || !target_reference{request.target}.permits_air_weapon()) return {
      .ready{true}
    };
  } else if(selection == caero_weapon::diffuser_gas || selection == caero_weapon::diffuser_trigger) {
    if(energy.reserve < cost || target_reference{request.target}.is_object_encoded()) return {
      .next_selection{std::to_underlying(request.pressed ? caero_weapon::diffuser_trigger : caero_weapon::none)}
    };
    if(!request.pressed) return {
      .ready{true}
    };
    energy.reserve -= cost;
    next_selection = selection == caero_weapon::diffuser_gas ? caero_weapon::diffuser_trigger : caero_weapon::diffuser_gas;
  } else {
    if(energy.reserve < cost) return {};
    if(selection == caero_weapon::dual_launch_follow_up) {
      auto const *capsule{pool.objects().tail()};
      if(!capsule || capsule->parameters.definition != &original_object_definitions[caero_definition_slot(caero_weapon::dual_launch_capsule)]) return {
        .ready{true},
        .next_selection{std::to_underlying(caero_weapon::dual_launch_capsule)}
      };
      if(!request.released) return {
        .ready{true}
      };
      target = capsule->native_id;
    }
    if((selection == caero_weapon::caero_weapon || selection == caero_weapon::brent_hunter) && !target_reference{request.target}.permits_air_weapon()) return {};
    if(selection == caero_weapon::brent_ground && target_reference{request.target}.is_object_encoded()) return {};
    if(selection != caero_weapon::dual_launch_follow_up && !request.pressed) return {
      .ready{true}
    };
    energy.reserve -= cost;
    if(selection == caero_weapon::dual_launch_capsule) next_selection = caero_weapon::dual_launch_follow_up;
    else if(selection == caero_weapon::dual_launch_follow_up) next_selection = caero_weapon::dual_launch_capsule;
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
    .next_selection{std::to_underlying(next_selection)},
  };
}

} // namespace darker::game

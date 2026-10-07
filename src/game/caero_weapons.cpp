#include "game/caero_weapons.h"
#include <stdexcept>
#include "game/object_definitions.h"

namespace darker::game {

caero_fire_result fire_caero_weapon(projectile_pool &pool, caero_energy_state &energy, launch_emitter const &emitter,
  uint8_t const selection, uint8_t const player_flags, bool const trigger_pressed, uint16_t const model, uint16_t const clock, uint16_t const target, bool const underground) {
  /// C9C2 gates Pinner and Brent shots, including target kind and the underground energy override
  if(selection != 1 && selection != 2 && selection != 6 && selection != 10) throw std::invalid_argument{"Caero firing branch is not implemented"};
  auto const &definition{original_object_definitions[selection - 1]};
  auto const cost{static_cast<uint16_t>((underground ? 0x80 : definition.role_data[0]) * 256 + 255)};
  if((player_flags & 0x30) || !pool.objects().free || energy.reserve < cost) return {};
  if(selection == 10 && !(static_cast<uint16_t>(target + 1) & 0x8000)) return {};
  if(selection == 6 && (target & 0x8000)) return {};
  if(!trigger_pressed) return {.ready{true}};
  energy.reserve -= cost;
  return {
    .shot{pool.launch({.definition{definition}, .emitter{emitter}, .model_token{model}, .clock{clock},
      .lifetime{static_cast<uint16_t>(definition.role_data[1] * 256)}, .target_token{target}})},
    .ready{true},
  };
}

} // namespace darker::game

#include "game/caero_weapons.h"
#include "game/object_definitions.h"

namespace darker::game {

caero_fire_result fire_pinner_direct(projectile_pool &pool, caero_energy_state &energy, launch_emitter const &emitter,
  uint8_t const player_flags, bool const trigger_pressed, uint16_t const model, uint16_t const clock) {
  /// C9C2/CAC0 gate the Pinner Direct, spend weapon reserve and launch with definition-specific lifetime
  auto const &definition{original_object_definitions[0]};
  auto const cost{static_cast<uint16_t>(definition.role_data[0] * 256 + 255)};
  if((player_flags & 0x30) || !pool.objects().free || energy.reserve < cost) return {};
  if(!trigger_pressed) return {.ready{true}};
  energy.reserve -= cost;
  return {
    .shot{pool.launch({.definition{definition}, .emitter{emitter}, .model_token{model}, .clock{clock},
      .lifetime{static_cast<uint16_t>(definition.role_data[1] * 256)}})},
    .ready{true},
  };
}

} // namespace darker::game

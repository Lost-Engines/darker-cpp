#include "game/caero_weapons.h"
#include <stdexcept>
#include "game/object_definitions.h"

namespace darker::game {

caero_fire_result fire_pinner(projectile_pool &pool, caero_energy_state &energy, launch_emitter const &emitter,
  uint8_t const selection, uint8_t const player_flags, bool const trigger_pressed, uint16_t const model, uint16_t const clock) {
  /// C9C2/CAC0 gate Direct and Mimic shots, spend weapon reserve and launch with definition-specific lifetime
  if(selection < 1 || selection > 2) throw std::invalid_argument{"Pinner selection must be Direct or Mimic"};
  auto const &definition{original_object_definitions[selection - 1]};
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

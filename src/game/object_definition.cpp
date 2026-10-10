#include "game/object_definition.h"

namespace darker::game {

void apply_object_definition(object_parameters &parameters, object_definition const &definition, std::uint16_t const model_token) {
  /// BF41 expands four unsigned seeds and resets the common flags without clearing other object state
  parameters = {
    .definition{&definition},
    .model_token{model_token},
    .update_entry{definition.update_entry},
    .flags_4c{0xc0c0},
    .angular_response{static_cast<std::uint16_t>(definition.angular_seed * 8)},
    .motion{
      static_cast<std::uint16_t>(definition.motion_seeds.bank_response * 256),
      static_cast<std::uint16_t>(definition.motion_seeds.bank_limit * 64),
      static_cast<std::uint16_t>(definition.motion_seeds.turn_response * 128),
    },
  };
}

} // namespace darker::game

#include <catch2/catch_test_macros.hpp>
#include "game/caero_weapons.h"
#include "game/object_definitions.h"
#include "reference/caero_weapon_samples.h"

TEST_CASE("Pinner Direct and Mimic firing matches original guards and reserve accounting", "[game][weapons]") {
  /// Exercise native cost boundaries, blocked player states, trigger edges and an exhausted projectile pool
  for(auto const &sample : darker::test_reference::caero_weapon_samples) {
    CAPTURE(sample);
    darker::game::projectile_pool pool;
    darker::game::launch_emitter const emitter{.position{1000, 2000, 3000}, .definition_strength{40}};
    if(!sample[3]) {
      for(size_t i{0}; i < pool.capacity; ++i) REQUIRE(pool.launch({.definition{darker::game::original_object_definitions[0]}, .emitter{emitter}}));
    }
    darker::game::caero_energy_state energy{.reserve{static_cast<uint16_t>(sample[0])}};
    auto const result{darker::game::fire_pinner(pool, energy, emitter, static_cast<uint8_t>(sample[8]), static_cast<uint8_t>(sample[1]), sample[2] != 0, 0x400, 65000)};
    CHECK(energy.reserve == sample[4]);
    CHECK(result.ready == (sample[5] != 0));
    CHECK((result.shot != nullptr) == (sample[6] != 0));
    if(result.shot) {
      CHECK(result.shot->deadline == static_cast<uint16_t>(65000 + sample[7]));
      CHECK(result.shot->parameters.model_token == 0x400);
      CHECK(result.shot->parameters.definition == &darker::game::original_object_definitions[static_cast<size_t>(sample[8] - 1)]);
    }
  }
}

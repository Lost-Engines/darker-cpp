#include <catch2/catch_test_macros.hpp>
#include "audio/flight_sounds.h"
#include "game/caero_weapons.h"
#include "game/object_definitions.h"
#include "reference/caero_weapon_samples.h"
#include "reference/chargeable_samples.h"

TEST_CASE("Caero Pinner and Brent firing matches original guards, targets and reserve accounting", "[game][weapons]") {
  /// Exercise native cost boundaries, blocked player states, trigger edges and an exhausted projectile pool
  for(auto const &sample : darker::test_reference::caero_weapon_samples) {
    CAPTURE(sample);
    darker::game::projectile_pool pool;
    darker::game::launch_emitter const emitter{.position{1000, 2000, 3000}, .definition_strength{40}};
    if(!sample[3]) {
      for(size_t i{0}; i < pool.capacity; ++i) REQUIRE(pool.launch({.definition{darker::game::original_object_definitions[0]}, .emitter{emitter}}));
    }
    darker::game::caero_energy_state energy{.reserve{static_cast<uint16_t>(sample[0])}};
    uint16_t charge{0};
    auto const result{darker::game::fire_caero_weapon(pool,energy,charge,{.emitter{emitter},.selection{static_cast<uint8_t>(sample[8])},
      .player_flags{static_cast<uint8_t>(sample[1])},.pressed{sample[2] != 0},.model{0x400},.clock{65000},
      .target{static_cast<uint16_t>(sample[9])},.underground{sample[10] == 2}})};
    CHECK(energy.reserve == sample[4]);
    CHECK(result.ready == (sample[5] != 0));
    CHECK((result.shot != nullptr) == (sample[6] != 0));
    if(result.shot) {
      CHECK(result.shot->deadline == static_cast<uint16_t>(65000 + sample[7]));
      CHECK(result.shot->parameters.model_token == 0x400);
      CHECK(result.shot->target_token == sample[9]);
      CHECK(result.shot->parameters.definition == &darker::game::original_object_definitions[static_cast<size_t>(sample[8] - 1)]);
    }
  }
}

TEST_CASE("Chargeable charging and release match the native firing handler", "[game][weapons]") {
  /// Compare edge, held and released inputs with native reserve, charge, readiness and lifetime results
  for(auto const &sample : darker::test_reference::chargeable_samples) {
    CAPTURE(sample);
    darker::game::projectile_pool pool;
    darker::game::launch_emitter const emitter{.position{1000,2000,3000},.definition_strength{40}};
    if(!sample[3]) {
      for(size_t i{0}; i < pool.capacity; ++i) REQUIRE(pool.launch({.definition{darker::game::original_object_definitions[0]},.emitter{emitter}}));
    }
    darker::game::caero_energy_state energy{.reserve{static_cast<uint16_t>(sample[0])}};
    auto charge{static_cast<uint16_t>(sample[1])};
    auto const result{darker::game::fire_caero_weapon(pool,energy,charge,{.emitter{emitter},.selection{9},
      .player_flags{static_cast<uint8_t>(sample[2])},.pressed{sample[4] != 0},.held{sample[5] != 0},.model{0x400},.clock{65000},
      .frame_step{static_cast<uint16_t>(sample[8])},.target{static_cast<uint16_t>(sample[6])},.underground{sample[7] == 2}})};
    CHECK(energy.reserve == sample[9]);
    CHECK(charge == sample[10]);
    CHECK(result.ready == (sample[11] != 0));
    CHECK((result.shot != nullptr) == (sample[12] != 0));
    if(result.shot) {
      CHECK(result.shot->deadline == static_cast<uint16_t>(65000 + sample[13]));
      CHECK(result.shot->target_token == sample[6]);
      CHECK(result.shot->parameters.definition == &darker::game::original_object_definitions[8]);
    }
  }
}

TEST_CASE("Chargeable impact strength matches native remaining-lifetime dispatch", "[game][weapons]") {
  /// Preserve the signed high-byte cutoff and wrapped strength at every timer page boundary
  for(auto const &sample : darker::test_reference::chargeable_impact_samples) {
    CAPTURE(sample);
    auto const strength{darker::game::chargeable_impact_strength(static_cast<uint16_t>(sample[0]),static_cast<uint16_t>(sample[1]))};
    CHECK(strength.has_value() == (sample[2] != 0));
    if(strength) CHECK(*strength == sample[3]);
  }
}

TEST_CASE("Chargeable sound follows the native charge and timer modulation", "[audio][weapons]") {
  /// Check the rising continuous tone and its silent zero-charge state against callback 370F
  for(auto const &sample : darker::test_reference::chargeable_tone_samples) {
    auto const pitch{darker::audio::chargeable_sound_pitch(static_cast<uint16_t>(sample[0]),static_cast<uint16_t>(sample[1]))};
    CHECK(pitch.has_value() == (sample[2] != 0));
    if(pitch) CHECK(*pitch == sample[3]);
  }
}

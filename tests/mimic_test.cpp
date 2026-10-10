#include <catch2/catch_test_macros.hpp>
#include "game/object_definitions.h"
#include "game/projectile_steering.h"
#include "reference/mimic_samples.h"

TEST_CASE("Pinner Mimic steering and displacement match complete native updates", "[weapons]") {
  /// Cover player-linked pitch, folded midpoint bank, remaining-life turn response and fractional movement
  for(auto const &sample : darker::test_reference::mimic_samples) {
    CAPTURE(sample);
    darker::game::projectile shot;
    shot.parameters.definition = &darker::game::original_object_definitions[1];
    shot.placement.angles.heading = static_cast<uint16_t>(sample[6 + 0]);
    shot.placement.angles.pitch = static_cast<uint16_t>(sample[6 + 1]);
    shot.placement.angles.roll = static_cast<uint16_t>(sample[6 + 2]);
    for(size_t axis{0}; axis < 3; ++axis) {
      shot.placement.position[axis] = static_cast<uint16_t>(sample[axis]);
      shot.placement.fractions[axis] = static_cast<uint8_t>(sample[3 + axis]);
    }
    shot.placement.speed = static_cast<uint16_t>(sample[9]);
    darker::game::object_pose const player{
      .angles{
        .heading{0},
        .pitch{static_cast<uint16_t>(sample[10])},
        .roll{static_cast<uint16_t>(sample[11])}
      }
    };
    darker::game::advance_mimic_projectile(shot,player,static_cast<uint16_t>(sample[12]),static_cast<uint16_t>(sample[13]));
    CHECK(shot.placement.angles.heading == sample[20 + 0]);
    CHECK(shot.placement.angles.pitch == sample[20 + 1]);
    CHECK(shot.placement.angles.roll == sample[20 + 2]);
    for(size_t axis{0}; axis < 3; ++axis) {
      CHECK(shot.placement.position[axis] == sample[14 + axis]);
      CHECK(shot.placement.fractions[axis] == sample[17 + axis]);
    }
    CHECK(shot.placement.speed == sample[23]);
    CHECK(shot.parameters.motion.turn_response == sample[24]);
  }
}

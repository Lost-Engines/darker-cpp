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
    for(size_t axis{0}; axis < 3; ++axis) {
      shot.placement.position[axis] = static_cast<uint16_t>(sample[axis]);
      shot.placement.fractions[axis] = static_cast<uint8_t>(sample[3 + axis]);
      shot.placement.angles[axis] = static_cast<uint16_t>(sample[6 + axis]);
    }
    shot.placement.speed = static_cast<uint16_t>(sample[9]);
    darker::game::object_pose const player{.angles{0,static_cast<uint16_t>(sample[10]),static_cast<uint16_t>(sample[11])}};
    darker::game::advance_mimic_projectile(shot,player,static_cast<uint16_t>(sample[12]),static_cast<uint16_t>(sample[13]));
    for(size_t axis{0}; axis < 3; ++axis) {
      CHECK(shot.placement.position[axis] == sample[14 + axis]);
      CHECK(shot.placement.fractions[axis] == sample[17 + axis]);
      CHECK(shot.placement.angles[axis] == sample[20 + axis]);
    }
    CHECK(shot.placement.speed == sample[23]);
    CHECK(shot.parameters.motion[2] == sample[24]);
  }
}

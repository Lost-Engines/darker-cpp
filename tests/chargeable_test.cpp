#include <catch2/catch_test_macros.hpp>
#include "game/object_definitions.h"
#include "game/projectile_steering.h"
#include "reference/chargeable_motion_samples.h"

TEST_CASE("Chargeable steering, spin and displacement match native updates", "[weapons]") {
  /// Exercise charge-dependent speed and spin alongside object tracking, self-targeting and word wrapping
  for(auto const &sample : darker::test_reference::chargeable_motion_samples) {
    CAPTURE(sample);
    darker::game::projectile shot;
    shot.parameters.definition = &darker::game::original_object_definitions[8];
    shot.parameters.angular_response = 480;
    darker::game::object_pose target;
    for(size_t axis{0}; axis < 3; ++axis) {
      shot.placement.position[axis] = static_cast<uint16_t>(sample[axis]);
      shot.placement.fractions[axis] = static_cast<uint8_t>(sample[3 + axis]);
      shot.placement.angles[axis] = static_cast<uint16_t>(sample[6 + axis]);
      target.position[axis] = static_cast<uint16_t>(sample[13 + axis]);
    }
    shot.placement.speed = static_cast<uint16_t>(sample[9]);
    shot.inherited_roll = static_cast<uint16_t>(sample[10]);
    shot.angular_motion[1] = static_cast<uint16_t>(sample[11]);
    shot.angular_motion[2] = static_cast<uint16_t>(sample[12]);
    darker::game::advance_chargeable_projectile(shot,sample[18] ? shot.placement : target,
      static_cast<uint16_t>(sample[16]),static_cast<uint16_t>(sample[17]));
    for(size_t axis{0}; axis < 3; ++axis) {
      CHECK(shot.placement.position[axis] == sample[19 + axis]);
      CHECK(shot.placement.fractions[axis] == sample[22 + axis]);
      CHECK(shot.placement.angles[axis] == sample[27 + axis]);
    }
    CHECK(shot.angular_motion[1] == sample[25]);
    CHECK(shot.angular_motion[2] == sample[26]);
    CHECK(shot.placement.speed == sample[30]);
    CHECK(shot.inherited_roll == sample[31]);
  }
}

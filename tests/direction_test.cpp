#include <catch2/catch_test_macros.hpp>
#include <array>
#include <cstdint>
#include "game/object_definitions.h"
#include "game/projectile_steering.h"
#include "maths/direction.h"
#include "reference/direction_samples.h"

TEST_CASE("Object target direction matches native quadrants, axes and signed word boundaries") {
  for(auto const &sample : darker::test_reference::direction_samples) {
    CAPTURE(sample.x, sample.y, sample.z);
    auto const result{darker::maths::object_target_direction({65535, 2000, 32768},
      {static_cast<std::uint16_t>(sample.x), static_cast<std::uint16_t>(sample.y), static_cast<std::uint16_t>(sample.z)})};
    CHECK(result.heading == sample.heading);
    CHECK(result.pitch == sample.pitch);
  }
}

TEST_CASE("Full object-homing trajectories follow native moving, coincident and self targets") {
  darker::game::projectile record;
  for(auto const &sample : darker::test_reference::trajectory_samples) {
    CAPTURE(sample.scenario, sample.tick);
    if(sample.tick == 0) {
      record = {};
      record.parameters.definition = &darker::game::original_object_definitions[0];
      record.parameters.angular_response = 480;
      record.placement = {
        .position{
          .column{0},
          .row{65535},
          .height{8192}
        },
        .fractions{
          .column{255},
          .row{127},
          .height{1}
        },
        .angles{
          .heading{0},
          .pitch{0},
          .roll{1234}
        },
        .speed{1000}
      };
    }
    darker::game::object_pose const target{
      .position{
        .column{static_cast<std::uint16_t>(sample.target[0])},
        .row{static_cast<std::uint16_t>(sample.target[1])},
        .height{static_cast<std::uint16_t>(sample.target[2])}
      }
    };
    darker::game::advance_object_homing_projectile(record, sample.scenario == 5 ? record.placement : target,
      static_cast<std::uint16_t>(sample.step));
    auto const &p{record.placement};
    std::array<int, 12> const actual{p.position.column, p.position.row, p.position.height, p.fractions.column, p.fractions.row, p.fractions.height,
      record.angular_motion.pitch, record.angular_motion.turn, p.angles.heading, p.angles.pitch, p.angles.roll, p.speed};
    CHECK(actual == sample.result);
  }
}

#include <catch2/catch_test_macros.hpp>
#include <array>
#include <cstdint>
#include "game/projectile_placement.h"
#include "maths/world_coordinates.h"
#include "reference/placement_samples.h"

TEST_CASE("Projectile placement matches both native launch paths including fractional carries") {
  for(auto const &sample : darker::test_reference::placement_samples) {
    CAPTURE(sample.strength, sample.heading, sample.pitch, sample.roll, sample.edge);
    darker::game::launch_emitter const emitter{
      .position{sample.edge ? darker::maths::world_position{
        .column{0},
        .row{65535},
        .height{0}
      } : darker::maths::world_position{
        .column{1000},
        .row{2000},
        .height{3000}
      }},
      .fractions{sample.edge ? darker::maths::position_fractions{
        .column{255},
        .row{1},
        .height{128}
      } : darker::maths::position_fractions{
        .column{0},
        .row{127},
        .height{255}
      }},
      .angles{
        .heading{static_cast<uint16_t>(sample.heading)},
        .pitch{static_cast<uint16_t>(sample.pitch)},
        .roll{static_cast<uint16_t>(sample.roll)}
      },
      .speed{0x9876},
      .side_flags{static_cast<uint8_t>(sample.edge ? 0x80 : 0)},
      .definition_strength{static_cast<uint8_t>(sample.strength)},
    };
    auto const result{darker::game::place_projectile(emitter)};
    std::array<int, 10> const actual{
      result.position.column, result.position.row, result.position.height, result.fractions.column, result.fractions.row, result.fractions.height,
      result.angles.heading, result.angles.pitch, result.angles.roll, result.speed,
    };
    CHECK(actual == sample.result);
  }
}

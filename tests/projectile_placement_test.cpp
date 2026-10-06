#include <catch2/catch_test_macros.hpp>
#include <array>
#include <cstdint>
#include "game/projectile_placement.h"
#include "reference/placement_samples.h"

TEST_CASE("Projectile placement matches both native launch paths including fractional carries") {
  for(auto const &sample : darker::test_reference::placement_samples) {
    CAPTURE(sample.strength, sample.heading, sample.pitch, sample.roll, sample.edge);
    darker::game::launch_emitter const emitter{
      .position{sample.edge ? std::array<std::uint16_t, 3>{0, 65535, 0} : std::array<std::uint16_t, 3>{1000, 2000, 3000}},
      .fractions{sample.edge ? std::array<std::uint8_t, 3>{255, 1, 128} : std::array<std::uint8_t, 3>{0, 127, 255}},
      .angles{static_cast<std::uint16_t>(sample.heading), static_cast<std::uint16_t>(sample.pitch), static_cast<std::uint16_t>(sample.roll)},
      .speed{0x9876}, .side_flags{static_cast<std::uint8_t>(sample.edge ? 0x80 : 0)},
      .definition_strength{static_cast<std::uint8_t>(sample.strength)},
    };
    auto const result{darker::game::place_projectile(emitter)};
    std::array<int, 10> const actual{
      result.position[0], result.position[1], result.position[2], result.fractions[0], result.fractions[1], result.fractions[2],
      result.angles[0], result.angles[1], result.angles[2], result.speed,
    };
    CHECK(actual == sample.result);
  }
}

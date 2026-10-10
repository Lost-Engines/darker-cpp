#include <catch2/catch_test_macros.hpp>
#include <array>
#include <cstdint>
#include "game/projectile_motion.h"
#include "reference/motion_samples.h"

TEST_CASE("Straight projectile integration matches native speed smoothing and fractional displacement") {
  for(auto const &sample : darker::test_reference::motion_samples) {
    CAPTURE(sample.heading, sample.pitch, sample.step, sample.speed, sample.base);
    darker::game::object_pose state{
      .position{
        .column{0},
        .row{65535},
        .height{0}
      },
      .fractions{
        .column{255},
        .row{127},
        .height{1}
      },
      .angles{
        .heading{static_cast<std::uint16_t>(sample.heading)},
        .pitch{static_cast<std::uint16_t>(sample.pitch)},
        .roll{1234}
      },
      .speed{static_cast<std::uint16_t>(sample.speed)},
    };
    darker::game::object_definition const definition{
      .base_speed{static_cast<std::uint8_t>(sample.base)}
    };
    auto const angles{state.angles};
    darker::game::advance_direct_projectile(state, definition, static_cast<std::uint16_t>(sample.step));
    std::array<int, 7> const actual{state.position.column, state.position.row, state.position.height,
      state.fractions.column, state.fractions.row, state.fractions.height, state.speed};
    CHECK(actual == sample.result);
    CHECK(state.angles == angles);
  }
}

TEST_CASE("Projectile deadline and fade update matches native signed clocks and altitude branches") {
  for(auto const &sample : darker::test_reference::deadline_samples) {
    CAPTURE(sample.flags, sample.delta, sample.altitude);
    darker::game::projectile record;
    record.flags = static_cast<std::uint8_t>(sample.flags);
    record.fade = 123;
    record.deadline = static_cast<std::uint16_t>(65000 + sample.delta);
    record.placement.position.height = static_cast<std::uint16_t>(sample.altitude);
    CHECK(darker::game::update_projectile_deadline(record, 65000) == static_cast<bool>(sample.expired));
    CHECK(record.flags == sample.next_flags);
    CHECK(record.deadline == sample.deadline);
    CHECK(record.fade == sample.fade);
  }
}

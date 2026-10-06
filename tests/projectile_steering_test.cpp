#include <catch2/catch_test_macros.hpp>
#include <array>
#include <cstdint>
#include "game/object_definitions.h"
#include "game/projectile_steering.h"
#include "game/projectile_update.h"
#include "reference/map_guidance_samples.h"
#include "reference/steering_samples.h"

TEST_CASE("Angular response preserves native clamps, rounding and step wrapping") {
  for(auto const &sample : darker::test_reference::response_samples) {
    CAPTURE(sample.error, sample.rate, sample.step, sample.response);
    auto const result{darker::game::calculate_angular_response(static_cast<std::uint16_t>(sample.error),
      static_cast<std::uint16_t>(sample.rate), static_cast<std::uint16_t>(sample.response), static_cast<std::uint16_t>(sample.step))};
    CHECK(result.rate == sample.next_rate);
    CHECK(result.angle_delta == sample.delta);
    CHECK(result.frame_step == sample.next_step);
  }
}

TEST_CASE("Homing steering and motion match native updates with supplied target angles") {
  for(auto const &sample : darker::test_reference::homing_samples) {
    CAPTURE(sample.heading, sample.pitch, sample.step, sample.rate);
    darker::game::projectile record;
    record.parameters.definition = &darker::game::original_object_definitions[0];
    record.parameters.angular_response = 480;
    record.placement = {.position{0, 65535, 0}, .fractions{255, 127, 1}, .angles{0, 0, 0}, .speed{1000}};
    record.angular_motion = {0, static_cast<std::uint16_t>(sample.rate), static_cast<std::uint16_t>(sample.rate)};
    darker::game::advance_homing_projectile(record, static_cast<std::uint16_t>(sample.heading),
      static_cast<std::uint16_t>(sample.pitch), static_cast<std::uint16_t>(sample.step));
    auto const &p{record.placement};
    std::array<int, 12> const actual{p.position[0], p.position[1], p.position[2], p.fractions[0], p.fractions[1], p.fractions[2],
      record.angular_motion[1], record.angular_motion[2], p.angles[0], p.angles[1], p.angles[2], p.speed};
    CHECK(actual == sample.result);
  }
}

TEST_CASE("Map homing preserves native banked steering and near-target rejection") {
  darker::game::projectile record;
  auto definition{darker::game::original_object_definitions[5]};
  for(auto const &sample : darker::test_reference::map_guidance_samples) {
    CAPTURE(sample.heading, sample.x, sample.y, sample.shift, sample.tick, sample.step);
    if(sample.tick == 0) {
      record = {};
      definition.role_data[6] = static_cast<std::uint8_t>(sample.shift);
      darker::game::apply_object_definition(record.parameters, definition, 0);
      record.placement = {.position{0, 0, 8192}, .fractions{255, 127, 1},
        .angles{static_cast<std::uint16_t>(sample.heading), 4096, 1234}, .speed{1000}};
      record.angular_motion = {0, 100, 65535};
    }
    auto const status{darker::game::update_projectile(record, 0, static_cast<std::uint16_t>(sample.step),
      darker::game::map_guidance_target{.position{static_cast<std::uint16_t>(sample.x), static_cast<std::uint16_t>(sample.y)},
        .height{12000}, .height_extent{4096}})};
    CHECK(status == darker::game::projectile_update_result::advanced);
    auto const &p{record.placement};
    std::array<int, 12> const actual{p.position[0], p.position[1], p.position[2], p.fractions[0], p.fractions[1], p.fractions[2],
      record.angular_motion[1], record.angular_motion[2], p.angles[0], p.angles[1], p.angles[2], p.speed};
    REQUIRE(actual == sample.result);
  }
}

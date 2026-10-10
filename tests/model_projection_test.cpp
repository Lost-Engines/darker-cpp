#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include "graphics/model_projection.h"
#include "reference/model_projection_samples.h"

TEST_CASE("Stateful model projection matches native component updates and fractional carries", "[graphics][projection]") {
  /// Retain the projection cache across set/zero/negate instructions, including the shared vertical fraction
  for(auto const &sample : darker::test_reference::projection_sequences) {
    darker::graphics::projection_parameters parameters{};
    for(std::size_t axis{0}; axis < 3; ++axis) {
      parameters.axes[axis] = {
        .horizontal{static_cast<std::int16_t>(sample.axes[axis * 3])},
        .vertical{static_cast<std::int16_t>(sample.axes[axis * 3 + 1])},
        .depth{static_cast<std::int16_t>(sample.axes[axis * 3 + 2])},
      };
    }
    parameters.horizontal = {
      .whole{static_cast<std::uint16_t>(sample.base[0])},
      .fraction{static_cast<std::uint8_t>(sample.base[1])}
    };
    parameters.vertical = {
      .whole{static_cast<std::uint16_t>(sample.base[2])},
      .fraction{static_cast<std::uint8_t>(sample.base[3])}
    };
    parameters.depth = {
      .whole{static_cast<std::uint16_t>(sample.base[4])},
      .fraction{static_cast<std::uint8_t>(sample.base[5])}
    };
    parameters.origin = {static_cast<std::int16_t>(sample.origin[0]), static_cast<std::int16_t>(sample.origin[1])};
    darker::graphics::model_projection projection{parameters};
    for(auto const &step : sample.steps) {
      CAPTURE(step.operation, step.axis, step.value);
      if(step.operation == 0) projection.set_component(step.axis, static_cast<std::int16_t>(step.value));
      else if(step.operation == 1) projection.zero_component(step.axis);
      else projection.negate_component(step.axis);
      auto const actual{projection.project()};
      REQUIRE(actual.screen.x == step.x);
      REQUIRE(actual.screen.y == step.y);
      REQUIRE(actual.depth == step.depth);
    }
  }
}

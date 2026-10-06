#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include "game/object_definition.h"
#include "game/object_definitions.h"
#include "reference/definition_samples.h"

TEST_CASE("Object definition expansion matches all native records and unsigned byte boundaries") {
  for(auto const &sample : darker::test_reference::definition_samples) {
    CAPTURE(sample.slot, sample.field, sample.value, sample.model);
    darker::game::object_definition definition;
    if(sample.slot >= 0) definition = darker::game::original_object_definitions[static_cast<std::size_t>(sample.slot)];
    else {
      definition.update_entry = 0xbeef;
      if(sample.field == 0) definition.angular_seed = static_cast<std::uint8_t>(sample.value);
      else definition.motion_seeds[static_cast<std::size_t>(sample.field - 1)] = static_cast<std::uint8_t>(sample.value);
    }
    darker::game::object_parameters parameters{
      .definition{nullptr}, .model_token{0xa5a5}, .update_entry{0xa5a5}, .flags_4c{0xa5a5},
      .angular_response{0xa5a5}, .motion{0xa5a5, 0xa5a5, 0xa5a5},
    };
    darker::game::apply_object_definition(parameters, definition, static_cast<std::uint16_t>(sample.model));
    CHECK(parameters.definition == &definition);
    CHECK(parameters.model_token == sample.expected_model);
    CHECK(parameters.update_entry == sample.update);
    CHECK(parameters.flags_4c == sample.flags);
    CHECK(parameters.angular_response == sample.angular);
    CHECK(parameters.motion[0] == sample.motion0);
    CHECK(parameters.motion[1] == sample.motion1);
    CHECK(parameters.motion[2] == sample.motion2);
    CHECK(definition.model_token == 0);
  }
}

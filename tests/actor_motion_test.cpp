#include <catch2/catch_test_macros.hpp>
#include <array>
#include "game/actor_motion.h"
#include "reference/actor_motion_samples.h"

TEST_CASE("Actor steering matches the original pitch, bank and heading coupling", "[game][actors]") {
  /// Retain the two response-rate fields and any timestep truncation across both angular updates
  for(auto const &sample : darker::test_reference::actor_steering_samples) {
    auto const &v{sample.input};
    darker::game::object_pose pose{.angles{static_cast<std::uint16_t>(v[0]), static_cast<std::uint16_t>(v[1]), static_cast<std::uint16_t>(v[2])}};
    darker::game::actor_attitude state{.pitch_rate{static_cast<std::uint16_t>(v[3])}, .bank_rate{static_cast<std::uint16_t>(v[4])}};
    auto const step{darker::game::steer_actor(pose, state,
      {.response{static_cast<std::uint16_t>(v[5])}, .bank_response{static_cast<std::uint16_t>(v[6])},
        .bank_limit{static_cast<std::uint16_t>(v[7])}, .turn_response{static_cast<std::uint16_t>(v[8])}},
      static_cast<std::uint16_t>(v[9]), static_cast<std::uint16_t>(v[10]), static_cast<std::uint16_t>(v[11]))};
    CHECK(std::array<int, 6>{pose.angles[0], pose.angles[1], pose.angles[2], state.pitch_rate, state.bank_rate, step} == sample.output);
  }
}

TEST_CASE("Actor acceleration and displacement match native word and fractional results", "[game][actors]") {
  /// Validate shared 8597 projection independently of the projectile speed controller
  for(auto const &sample : darker::test_reference::actor_motion_samples) {
    auto const &v{sample.input};
    darker::game::object_pose pose{
      .position{static_cast<std::uint16_t>(v[0]), static_cast<std::uint16_t>(v[1]), static_cast<std::uint16_t>(v[2])},
      .fractions{static_cast<std::uint8_t>(v[3]), static_cast<std::uint8_t>(v[4]), static_cast<std::uint8_t>(v[5])},
      .angles{static_cast<std::uint16_t>(v[6]), static_cast<std::uint16_t>(v[7]), static_cast<std::uint16_t>(v[8])},
      .speed{static_cast<std::uint16_t>(v[9])},
    };
    darker::game::advance_actor_speed(pose, static_cast<std::uint8_t>(v[10]), static_cast<std::uint8_t>(v[11]),
      static_cast<std::uint8_t>(v[12]), static_cast<std::uint16_t>(v[13]));
    CHECK(std::array<int, 7>{pose.position[0], pose.position[1], pose.position[2], pose.fractions[0], pose.fractions[1], pose.fractions[2], pose.speed} == sample.output);
  }
}

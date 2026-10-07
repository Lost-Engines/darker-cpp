#include <catch2/catch_test_macros.hpp>
#include "game/flight_camera.h"
#include "reference/camera_look_samples.h"
#include "reference/dropped_camera_samples.h"
#include "reference/flight_camera_samples.h"
#include "reference/missile_camera_samples.h"

TEST_CASE("Player camera position and distance smoothing match native views", "[game][camera]") {
  /// Cover all four attached views, distance settings, fractional positions and ground clamping
  for(auto const &sample : darker::test_reference::flight_camera_samples) {
    darker::game::object_pose player;
    for(std::size_t i{0}; i < 3; ++i) {
      player.position[i] = static_cast<std::uint16_t>(sample.position[i]);
      player.angles[i] = static_cast<std::uint16_t>(sample.angles[i]);
    }
    for(std::size_t i{0}; i < 2; ++i) player.fractions[i] = static_cast<std::uint8_t>(sample.fractions[i]);
    auto const &v{sample.input};
    darker::game::flight_camera camera{.mode{static_cast<darker::game::camera_mode>(v[0])}, .distance_step{static_cast<std::uint8_t>(v[2])}, .distance{static_cast<std::uint16_t>(v[3])}};
    auto const result{camera.view(player, static_cast<std::uint16_t>(v[1]), v[4] != 0)};
    CAPTURE(sample.position, sample.angles, v);
    CHECK(std::array<int, 9>{result.position[0], result.position[1], result.position[2], result.fractions[0], result.fractions[1],
      result.angles[0], result.angles[1], result.angles[2], camera.distance} == sample.output);
  }
}


TEST_CASE("Tab look and recentering follow original camera controls", "[game][camera]") {
  /// Include wrapped offsets, both steering signs and the landed look constraint
  for(auto const &sample : darker::test_reference::camera_look_samples) {
    auto const &v{sample.input};
    darker::game::flight_camera camera{.mode{static_cast<darker::game::camera_mode>(v[6])},
      .look_heading{static_cast<std::uint16_t>(v[0])}, .look_pitch{static_cast<std::uint16_t>(v[1])}};
    camera.update_look({.bank{static_cast<std::uint16_t>(v[2])}, .pitch{static_cast<std::uint16_t>(v[3])}}, v[4] != 0, static_cast<std::uint16_t>(v[5]), v[7] != 0);
    CAPTURE(v);
    CHECK(std::array<int, 2>{camera.look_heading, camera.look_pitch} == sample.output);
  }
}


TEST_CASE("Dropped cameras retain original anchors and tracking angles", "[game][camera]") {
  /// Drop at the craft's whole-word position, then move the craft before viewing it
  for(auto const &sample : darker::test_reference::dropped_camera_samples) {
    darker::game::object_pose anchor, player;
    for(std::size_t i{0}; i < 3; ++i) {
      anchor.position[i] = static_cast<std::uint16_t>(sample.anchor[i]);
      anchor.angles[i] = static_cast<std::uint16_t>(sample.angles[i]);
      player.position[i] = static_cast<std::uint16_t>(sample.player[i]);
    }
    darker::game::flight_camera camera;
    camera.drop(static_cast<darker::game::camera_mode>(sample.mode[0]), anchor);
    auto const result{camera.view(player, 1)};
    CAPTURE(sample.anchor, sample.player, sample.mode);
    CHECK(std::array<int, 8>{result.position[0], result.position[1], result.position[2], result.fractions[0], result.fractions[1],
      result.angles[0], result.angles[1], result.angles[2]} == sample.output);
  }
}

TEST_CASE("Missile camera positions match native attached and impact views", "[game][camera]") {
  /// Cover all four attached missile modes, both visibility phases, original distances and ground adjustment
  for(auto const &sample : darker::test_reference::missile_camera_samples) {
    darker::game::object_pose shot;
    for(size_t axis{0}; axis < 3; ++axis) {
      shot.position[axis] = static_cast<uint16_t>(sample.position[axis]);
      shot.angles[axis] = static_cast<uint16_t>(sample.angles[axis]);
    }
    for(size_t axis{0}; axis < 2; ++axis) shot.fractions[axis] = static_cast<uint8_t>(sample.fractions[axis]);
    auto const &v{sample.input};
    darker::game::flight_camera camera{.mode{static_cast<darker::game::camera_mode>(v[0])},.distance_step{static_cast<uint8_t>(v[2])},.distance{static_cast<uint16_t>(v[3])}};
    auto const result{camera.view(shot,static_cast<uint16_t>(v[1]),v[4] != 0,
      v[5] ? darker::game::camera_subject::missile_effect : darker::game::camera_subject::missile)};
    CAPTURE(sample.position,sample.angles,v);
    CHECK(std::array<int,9>{result.position[0],result.position[1],result.position[2],result.fractions[0],result.fractions[1],
      result.angles[0],result.angles[1],result.angles[2],camera.distance} == sample.output);
  }
}

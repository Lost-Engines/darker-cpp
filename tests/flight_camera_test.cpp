#include <catch2/catch_test_macros.hpp>
#include "game/camera_target.h"
#include "game/flight_camera.h"
#include "reference/camera_look_samples.h"
#include "reference/dropped_camera_samples.h"
#include "reference/flight_camera_samples.h"
#include "reference/missile_camera_samples.h"
#include "reference/object_view_samples.h"

TEST_CASE("Player camera position and distance smoothing match native views", "[game][camera]") {
  /// Cover all four attached views, distance settings, fractional positions and ground clamping
  for(auto const &sample : darker::test_reference::flight_camera_samples) {
    darker::game::object_pose player;
    player.angles.heading = static_cast<uint16_t>(sample.angles[0]);
    player.angles.pitch = static_cast<uint16_t>(sample.angles[1]);
    player.angles.roll = static_cast<uint16_t>(sample.angles[2]);
    for(size_t i{0}; i < 3; ++i) {
      player.position[i] = static_cast<uint16_t>(sample.position[i]);
    }
    for(size_t i{0}; i < 2; ++i) player.fractions[i] = static_cast<uint8_t>(sample.fractions[i]);
    auto const &v{sample.input};
    darker::game::flight_camera camera{
      .mode{static_cast<darker::game::camera_mode>(v[0])},
      .distance_step{static_cast<uint8_t>(v[2])},
      .distance{static_cast<uint16_t>(v[3])}
    };
    auto const result{camera.view(player, static_cast<uint16_t>(v[1]), v[4] != 0)};
    CAPTURE(sample.position, sample.angles, v);
    CHECK(std::array<int, 9>{result.position.column, result.position.row, result.position.height, result.fractions.column, result.fractions.row,
      result.angles.heading, result.angles.pitch, result.angles.roll, camera.distance} == sample.output);
  }
}

TEST_CASE("Tab look and recentering follow original camera controls", "[game][camera]") {
  /// Include wrapped offsets, both steering signs and the landed look constraint
  for(auto const &sample : darker::test_reference::camera_look_samples) {
    auto const &v{sample.input};
    darker::game::flight_camera camera{
      .mode{static_cast<darker::game::camera_mode>(v[6])},
      .look_heading{static_cast<uint16_t>(v[0])},
      .look_pitch{static_cast<uint16_t>(v[1])}
    };
    camera.update_look({
      .bank{static_cast<uint16_t>(v[2])},
      .pitch{static_cast<uint16_t>(v[3])}
    }, v[4] != 0, static_cast<uint16_t>(v[5]), v[7] != 0);
    CAPTURE(v);
    CHECK(std::array<int, 2>{camera.look_heading, camera.look_pitch} == sample.output);
  }
}

TEST_CASE("Dropped cameras retain original anchors and tracking angles", "[game][camera]") {
  /// Drop at the craft's whole-word position, then move the craft before viewing it
  for(auto const &sample : darker::test_reference::dropped_camera_samples) {
    darker::game::object_pose anchor, player;
    anchor.angles.heading = static_cast<uint16_t>(sample.angles[0]);
    anchor.angles.pitch = static_cast<uint16_t>(sample.angles[1]);
    anchor.angles.roll = static_cast<uint16_t>(sample.angles[2]);
    for(size_t i{0}; i < 3; ++i) {
      anchor.position[i] = static_cast<uint16_t>(sample.anchor[i]);
      player.position[i] = static_cast<uint16_t>(sample.player[i]);
    }
    darker::game::flight_camera camera;
    camera.drop(static_cast<darker::game::camera_mode>(sample.mode[0]), anchor);
    auto const result{camera.view(player, 1)};
    CAPTURE(sample.anchor, sample.player, sample.mode);
    CHECK(std::array<int, 8>{result.position.column, result.position.row, result.position.height, result.fractions.column, result.fractions.row,
      result.angles.heading, result.angles.pitch, result.angles.roll} == sample.output);
    if(sample.mode[0] == 5) {
      // native 2448 jumps to the same fixed-anchor branch when the selected object disappears
      camera.mode = darker::game::camera_mode::object;
      auto const fallback{camera.view(player, 1, false, darker::game::camera_subject::absent_object)};
      CHECK(std::array<int, 8>{fallback.position.column, fallback.position.row, fallback.position.height, fallback.fractions.column, fallback.fractions.row,
        fallback.angles.heading, fallback.angles.pitch, fallback.angles.roll} == sample.output);
    }
  }
}

TEST_CASE("Missile camera positions match native attached and impact views", "[game][camera]") {
  /// Cover all four attached missile modes, both visibility phases, original distances and ground adjustment
  for(auto const &sample : darker::test_reference::missile_camera_samples) {
    darker::game::object_pose shot;
    shot.angles.heading = static_cast<uint16_t>(sample.angles[0]);
    shot.angles.pitch = static_cast<uint16_t>(sample.angles[1]);
    shot.angles.roll = static_cast<uint16_t>(sample.angles[2]);
    for(size_t axis{0}; axis < 3; ++axis) {
      shot.position[axis] = static_cast<uint16_t>(sample.position[axis]);
    }
    for(size_t axis{0}; axis < 2; ++axis) shot.fractions[axis] = static_cast<uint8_t>(sample.fractions[axis]);
    auto const &v{sample.input};
    darker::game::flight_camera camera{
      .mode{static_cast<darker::game::camera_mode>(v[0])},
      .distance_step{static_cast<uint8_t>(v[2])},
      .distance{static_cast<uint16_t>(v[3])}
    };
    auto const result{camera.view(shot, static_cast<uint16_t>(v[1]), v[4] != 0,
      v[5] ? darker::game::camera_subject::missile_effect : darker::game::camera_subject::missile)};
    CAPTURE(sample.position, sample.angles, v);
    CHECK(std::array<int, 9>{result.position.column, result.position.row, result.position.height, result.fractions.column, result.fractions.row,
      result.angles.heading, result.angles.pitch, result.angles.roll, camera.distance} == sample.output);
  }
}

TEST_CASE("Underground following and death views retain the player position", "[game][camera]") {
  /// Native 24A7 selects the same 24E6 path as full-screen, without advancing the following distance
  for(auto const &sample : darker::test_reference::flight_camera_samples) {
    if(sample.input[0] != 3) continue;
    darker::game::object_pose player;
    player.angles.heading = static_cast<uint16_t>(sample.angles[0]);
    player.angles.pitch = static_cast<uint16_t>(sample.angles[1]);
    player.angles.roll = static_cast<uint16_t>(sample.angles[2]);
    for(size_t i{0}; i < 3; ++i) {
      player.position[i] = static_cast<uint16_t>(sample.position[i]);
    }
    for(size_t i{0}; i < 2; ++i) player.fractions[i] = static_cast<uint8_t>(sample.fractions[i]);
    for(auto const mode : {darker::game::camera_mode::behind, darker::game::camera_mode::level}) {
      darker::game::flight_camera camera{
        .mode{mode},
        .distance_step{static_cast<uint8_t>(sample.input[2])},
        .distance{static_cast<uint16_t>(sample.input[3])}
      };
      auto const result{camera.view(player, static_cast<uint16_t>(sample.input[1]), sample.input[4] != 0, darker::game::camera_subject::player, true)};
      CHECK(std::array<int, 9>{result.position.column, result.position.row, result.position.height, result.fractions.column, result.fractions.row,
        result.angles.heading, result.angles.pitch, result.angles.roll, camera.distance} == sample.output);
    }
  }
}

TEST_CASE("F7 rays and range rejection match native camera selection", "[game][camera]") {
  for(auto const &sample : darker::test_reference::object_camera_rays) {
    darker::game::object_pose camera, target;
    camera.angles.heading = static_cast<uint16_t>(sample.angles[0]);
    camera.angles.pitch = static_cast<uint16_t>(sample.angles[1]);
    camera.angles.roll = static_cast<uint16_t>(sample.angles[2]);
    for(size_t axis{0}; axis < 3; ++axis) {
      camera.position[axis] = static_cast<uint16_t>(sample.position[axis]);
      target.position[axis] = static_cast<uint16_t>(sample.target[axis]);
    }
    auto const end{darker::game::camera_ray_end(camera)};
    CAPTURE(sample.position, sample.angles, sample.target);
    CHECK(std::array<int, 3>{end[0], end[1], end[2]} == sample.end);
    CHECK(darker::game::camera_target_in_range(camera, target) == (sample.accepted[0] != 0));
  }
}

TEST_CASE("F7 follows live and destroyed objects at the original distances", "[game][camera]") {
  for(auto const &sample : darker::test_reference::object_camera_views) {
    darker::game::object_pose object;
    object.angles.heading = static_cast<uint16_t>(sample.angles[0]);
    object.angles.pitch = static_cast<uint16_t>(sample.angles[1]);
    object.angles.roll = static_cast<uint16_t>(sample.angles[2]);
    for(size_t axis{0}; axis < 3; ++axis) {
      object.position[axis] = static_cast<uint16_t>(sample.position[axis]);
    }
    for(size_t axis{0}; axis < 2; ++axis) object.fractions[axis] = static_cast<uint8_t>(sample.fractions[axis]);
    auto const &v{sample.input};
    darker::game::flight_camera camera{
      .mode{darker::game::camera_mode::object},
      .distance_step{static_cast<uint8_t>(v[1])},
      .distance{static_cast<uint16_t>(v[2])}
    };
    auto const result{camera.view(object, static_cast<uint16_t>(v[0]), false,
      v[3] ? darker::game::camera_subject::object_effect : darker::game::camera_subject::object)};
    CAPTURE(sample.position, sample.angles, v);
    CHECK(std::array<int, 9>{result.position.column, result.position.row, result.position.height, result.fractions.column, result.fractions.row,
      result.angles.heading, result.angles.pitch, result.angles.roll, camera.distance} == sample.output);
  }
}

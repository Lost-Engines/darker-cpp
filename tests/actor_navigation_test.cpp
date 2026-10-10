#include <catch2/catch_test_macros.hpp>
#include <array>
#include <utility>
#include <vector>
#include "game/actor_navigation.h"
#include "reference/actor_navigation_samples.h"
#include "reference/actor_target_samples.h"
#include "reference/threat_samples.h"

TEST_CASE("Aircraft threat ranking matches native target, range and aim checks", "[game][actors]") {
  for(auto const &sample : darker::test_reference::threat_samples) {
    auto errors{sample.before};
    auto const &v{sample.input};
    darker::game::scenario_actor actor;
    actor.pose.angles = {
      .heading{v[0]},
      .pitch{v[1]},
      .roll{0}
    };
    actor.selected_target = v[5];
    darker::game::consider_aircraft_threat(errors, actor, {
      .heading{v[2]},
      .pitch{v[3]},
      .distance{v[4]}
    });
    REQUIRE(errors == sample.after);
  }
}

TEST_CASE("Actor manoeuvres match native speed, turn and firing decisions", "[game][actors]") {
  /// Exercise pursuit, close turns, height recovery and firing eligibility requests independently of shot emission
  for(auto const &sample : darker::test_reference::actor_navigation_samples) {
    auto const &v{sample.input};
    darker::game::object_definition definition{
      .base_speed{static_cast<uint8_t>(v[9])},
      .role_data{darker::game::craft_definition_data{0, 0, static_cast<uint8_t>(v[10]), 0, static_cast<uint8_t>(v[11]), static_cast<uint8_t>(v[12])}},
    };
    darker::game::scenario_actor actor{
      .parameters{
        .definition{&definition}
      },
      .pose{
        .position{
          .column{0},
          .row{0},
          .height{static_cast<uint16_t>(v[1])}
        },
        .angles{
          .heading{static_cast<uint16_t>(v[0])},
          .pitch{0},
          .roll{0}
        }
      },
      .awareness{
        .level{static_cast<uint16_t>(v[2])}
      },
      .behaviour{static_cast<uint8_t>(v[7]), 0, 0, 0, 0, static_cast<uint8_t>(v[8])},
    };
    auto const result{darker::game::choose_actor_manoeuvre(actor,
      {
        .heading{static_cast<uint16_t>(v[3])},
        .pitch{static_cast<uint16_t>(v[4])},
        .distance{static_cast<uint16_t>(v[5])},
        .climb{static_cast<uint8_t>(v[6])}
      })};
    CAPTURE(v);
    CHECK(std::array<int, 4>{result.pitch, result.turn_drive, result.speed, result.firing_distance ? *result.firing_distance : -1} == sample.output);
  }
}

TEST_CASE("Actor target selection retains pursuit until awareness reaches zero", "[game][actors]") {
  /// Compare the sticky player-target branch and script-target fallback against 8826
  for(auto const &sample : darker::test_reference::actor_target_samples) {
    auto const &v{sample.input};
    darker::game::scenario_actor actor{
      .awareness{
        .level{static_cast<uint16_t>(v[0])}
      },
      .behaviour{0, static_cast<uint8_t>(v[1])},
      .selected_target{static_cast<uint16_t>(v[2])},
      .target_token{static_cast<uint16_t>(v[3])},
    };
    darker::game::select_actor_target(actor);
    CHECK(actor.selected_target == sample.output[0]);
  }
}

TEST_CASE("Actor object courses preserve close-target height separation", "[game][actors]") {
  /// Include wrapping coordinates and approaches from either side of the target height
  for(auto const &sample : darker::test_reference::actor_object_course_samples) {
    auto const &v{sample.input};
    darker::game::object_pose actor{
      .position{
        .column{static_cast<uint16_t>(v[0])},
        .row{static_cast<uint16_t>(v[1])},
        .height{static_cast<uint16_t>(v[2])}
      }
    };
    darker::game::object_pose target{
      .position{
        .column{static_cast<uint16_t>(v[3])},
        .row{static_cast<uint16_t>(v[4])},
        .height{static_cast<uint16_t>(v[5])}
      }
    };
    auto const course{darker::game::actor_object_course(actor, target)};
    CAPTURE(v);
    CHECK(std::array<int, 3>{course.heading, course.pitch, course.distance} == sample.output);
  }
}

TEST_CASE("Actor cell courses match city offsets and nominal-height fallback", "[game][actors]") {
  /// Exercise geometry-based aim heights, out-of-map columns and the slot-22 speed-dependent height adjustment
  for(auto const &sample : darker::test_reference::actor_cell_course_samples) {
    auto const &v{sample.input};
    darker::game::object_definition definition{
      .role_data{darker::game::craft_definition_data{0, 0, static_cast<uint8_t>(v[6])}}
    };
    darker::game::scenario_actor actor{
      .parameters{
        .definition{&definition}
      },
      .pose{
        .position{
          .column{static_cast<uint16_t>(v[0])},
          .row{static_cast<uint16_t>(v[1])},
          .height{static_cast<uint16_t>(v[2])}
        },
        .speed{static_cast<uint16_t>(v[5])}
      },
      .definition_slot{static_cast<uint8_t>(v[4])},
    };
    auto const course{darker::game::actor_cell_course(actor, static_cast<uint16_t>(v[3]),
      {
        .column_fraction{static_cast<uint8_t>(v[7])},
        .row_fraction{static_cast<uint8_t>(v[8])},
        .collision_marker{static_cast<uint8_t>(v[9])}
      },
      {
        .height{static_cast<int16_t>(v[10])},
        .extent{static_cast<uint16_t>(v[11])}
      })};
    CAPTURE(v);
    CHECK(std::array<int, 3>{course.heading, course.pitch, course.distance} == sample.output);
  }
}

TEST_CASE("Actor clearance preserves reference-cell resets and wrapped height comparisons", "[game][actors]") {
  /// Neighbour scanning supplies a maximum height; these decisions retain the original signed comparison rules
  for(auto const &sample : darker::test_reference::actor_clearance_samples) {
    auto const &v{sample.input};
    darker::game::scenario_actor actor{
      .parameters{
        .flags_4c{static_cast<uint16_t>(v[3])}
      },
      .pose{
        .position{
          .column{static_cast<uint16_t>(v[0])},
          .row{static_cast<uint16_t>(v[1])},
          .height{static_cast<uint16_t>(v[2])}
        }
      },
      .clearance_floor{static_cast<uint16_t>(v[4])},
    };
    darker::game::actor_course course{
      .pitch{static_cast<uint16_t>(v[6])},
      .climb{static_cast<uint8_t>(v[7])}
    };
    darker::game::reset_actor_clearance(actor);
    darker::game::adjust_actor_clearance(actor, course, static_cast<uint16_t>(v[5]));
    CAPTURE(v);
    CHECK(std::array<int, 3>{course.pitch, course.climb, actor.clearance_floor} == sample.output);
  }
}

TEST_CASE("Actor neighbour avoidance matches original scan boundaries and height priority", "[game][actors]") {
  /// Equal heights use stable object order; high-speed neighbours can also change the climb byte
  for(auto const &sample : darker::test_reference::actor_neighbour_samples) {
    auto const &v{sample.input};
    darker::game::object_definition definition{
      .role_data{darker::game::craft_definition_data{0, 0, 0, 0, static_cast<uint8_t>(v[11])}}
    };
    darker::game::scenario_actor actor{
      .parameters{
        .definition{&definition},
        .flags_4c{static_cast<uint16_t>(v[9])}
      },
      .pose{
        .position{
          .column{static_cast<uint16_t>(v[0])},
          .row{static_cast<uint16_t>(v[1])},
          .height{static_cast<uint16_t>(v[2])}
        }
      },
      .clearance_floor{static_cast<uint16_t>(v[8])},
      .index{static_cast<uint8_t>(v[12] ? 2 : 1)},
    };
    darker::game::scenario_actor neighbour{
      .pose{
        .position{
          .column{static_cast<uint16_t>(v[3])},
          .row{static_cast<uint16_t>(v[4])},
          .height{static_cast<uint16_t>(v[5])}
        },
        .angles{
          .heading{0},
          .pitch{static_cast<uint16_t>(v[7])},
          .roll{0}
        },
        .speed{static_cast<uint16_t>(v[6])}
      },
      .index{static_cast<uint8_t>(v[12] ? 1 : 2)},
    };
    darker::game::actor_course course{
      .climb{static_cast<uint8_t>(v[10])}
    };
    darker::game::consider_actor_clearance(actor, neighbour, course);
    CAPTURE(v);
    CHECK(std::array<int, 3>{actor.clearance_floor, actor.parameters.flags_4c, course.climb} == sample.output);
  }
}

TEST_CASE("Actor city clearance matches eight-cell scanning and linked model states", "[game][actors]") {
  /// Synthetic linked headers expose alternate/damage traversal, signed height maxima and boundary-cell wrapping
  for(auto const &sample : darker::test_reference::actor_city_scan_samples) {
    auto const &v{sample.input};
    std::vector<std::byte> bytes(108);
    auto const word{[&](size_t const offset, int const value){
      auto const encoded{static_cast<uint16_t>(value)};
      bytes[offset] = static_cast<std::byte>(encoded & 255);
      bytes[offset + 1] = static_cast<std::byte>(encoded >> 8);
    }};
    bytes[0] = std::byte{1};
    bytes[3] = bytes[4] = std::byte{128};
    bytes[5] = static_cast<std::byte>(v[3]);
    bytes[7] = std::byte{3};
    word(10, 96);
    for(int n{0}; n < 6; ++n) {
      word(12 + n * 16, (((n + 1) % 6) - n) * 16);
      word(14 + n * 16, (((n + 2) % 6) - n) * 16);
      word(19 + n * 16, v[4 + n]);
      word(21 + n * 16, v[10 + n]);
    }
    darker::resources::geometry_bank bank{std::move(bytes)};
    darker::game::city_map cells{};
    for(int n{0}; n < 9; ++n) {
      auto const column{static_cast<uint8_t>(v[0] + n % 3 - 1)};
      auto const row{static_cast<uint8_t>(v[1] + n / 3 - 1)};
      if(column < 128 && row < 128) cells[row * 128 + column] = {
        .type{static_cast<uint8_t>(v[16 + n * 2])},
        .state{static_cast<uint8_t>(v[17 + n * 2])},
      };
    }
    darker::game::object_pose pose{
      .position{
        .column{static_cast<uint16_t>(v[0] * 256 + 128)},
        .row{static_cast<uint16_t>(v[1] * 256 + 128)},
        .height{0}
      }
    };
    CAPTURE(v);
    CHECK(darker::game::actor_city_clearance(pose, cells, bank, static_cast<uint8_t>(v[2])) == sample.output[0]);
  }
}

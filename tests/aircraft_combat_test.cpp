#include <catch2/catch_test_macros.hpp>
#include <array>
#include "game/aircraft_combat.h"
#include "game/mission_combat.h"
#include "game/object_definitions.h"
#include "maths/world_coordinates.h"
#include "reference/aircraft_bomb_samples.h"
#include "reference/aircraft_combat_samples.h"
#include "reference/aircraft_fire_samples.h"

TEST_CASE("Aircraft hit volumes match native extent sweeps", "[combat]") {
  /// Check both hit admission and native impact rounding at full altitude scale
  for(auto const &v : darker::test_reference::aircraft_sweeps) {
    CAPTURE(v);
    darker::game::object_pose const target{
      .position{
        .column{static_cast<uint16_t>(v[0])},
        .row{static_cast<uint16_t>(v[1])},
        .height{static_cast<uint16_t>(v[2])}
      }
    };
    darker::maths::world_position const start{
      .column{static_cast<uint16_t>(v[5])},
      .row{static_cast<uint16_t>(v[6])},
      .height{static_cast<uint16_t>(v[7])}
    };
    darker::maths::world_position end{
      .column{static_cast<uint16_t>(v[8])},
      .row{static_cast<uint16_t>(v[9])},
      .height{static_cast<uint16_t>(v[10])}
    };
    CHECK(darker::game::sweep_aircraft(target, v[3], v[4], start, end) == (v[11] != 0));
    CHECK(end == darker::maths::world_position{
      .column{static_cast<uint16_t>(v[12])},
      .row{static_cast<uint16_t>(v[13])},
      .height{static_cast<uint16_t>(v[14])}
    });
  }
}

TEST_CASE("Falling aircraft match the original destruction callback", "[combat]") {
  /// Check coupled bank decay, dive and displacement, retaining fractional positions
  for(auto const &v : darker::test_reference::aircraft_falls) {
    CAPTURE(v);
    darker::game::scenario_actor actor;
    actor.pose.position = {
      .column{static_cast<uint16_t>(v[0])},
      .row{static_cast<uint16_t>(v[1])},
      .height{static_cast<uint16_t>(v[2])}
    };
    actor.attitude = {static_cast<uint16_t>(v[3]), static_cast<uint16_t>(v[4])};
    actor.pose.angles = {
      .heading{static_cast<uint16_t>(v[5])},
      .pitch{static_cast<uint16_t>(v[6])},
      .roll{static_cast<uint16_t>(v[7])}
    };
    actor.pose.speed = static_cast<uint16_t>(v[8]);
    darker::game::advance_falling_aircraft(actor, v[9]);
    std::array<int, 12> const actual{actor.pose.position.column, actor.pose.position.row, actor.pose.position.height, actor.attitude.pitch_rate, actor.attitude.bank_rate,
      actor.pose.angles.heading, actor.pose.angles.pitch, actor.pose.angles.roll, actor.pose.speed, actor.pose.fractions.column, actor.pose.fractions.row, actor.pose.fractions.height};
    for(size_t i{0}; i < actual.size(); ++i) CHECK(actual[i] == v[i + 10]);
  }
}

TEST_CASE("Enemy gun checks match original world profiles, aim, timing and hits", "[combat]") {
  /// Native firing executes through the ray intersection, intercepting only damage application and effect spawning
  for(auto const &v : darker::test_reference::aircraft_guns) {
    CAPTURE(v);
    darker::game::scenario_actor actor;
    actor.definition_slot = 19;
    actor.behaviour.attack_control = static_cast<uint8_t>(v[16]);
    actor.selected_target = 0xd986;
    actor.parameters.definition = &darker::game::original_object_definitions[19];
    actor.pose.position = {
      .column{10000},
      .row{10000},
      .height{3000}
    };
    actor.pose.angles = {
      .heading{static_cast<uint16_t>(v[0])},
      .pitch{static_cast<uint16_t>(v[1])},
      .roll{0}
    };
    darker::game::object_pose const player{
      .position{
        .column{static_cast<uint16_t>(v[9])},
        .row{static_cast<uint16_t>(v[10])},
        .height{static_cast<uint16_t>(v[11])}
      }
    };
    uint16_t random{static_cast<uint16_t>(v[8])};
    auto const shot{darker::game::fire_skimma_gun(actor, player, v[7], v[12],
      {
        .heading{static_cast<uint16_t>(v[2])},
        .pitch{static_cast<uint16_t>(v[3])}
      }, v[4], v[5], v[6], random, v[17] == 0 ? 0x30 : 0x20)};
    CHECK(shot.has_value() == (v[13] != 0));
    CHECK((shot && shot->hit) == (v[14] != 0));
    CHECK(random == v[15]);
  }
}

TEST_CASE("Aircraft missile eligibility matches native firing settings and timer boundaries") {
  /// Execute the object-target branch independently from ray damage and projectile construction
  for(auto const &v : darker::test_reference::aircraft_fire_samples) {
    CAPTURE(v);
    darker::game::scenario_actor actor;
    actor.definition_slot = static_cast<uint8_t>(v[0]);
    actor.parameters.definition = &darker::game::original_object_definitions[actor.definition_slot];
    actor.behaviour.attack_control = static_cast<uint8_t>(v[1]);
    actor.pose.angles = {
      .heading{static_cast<uint16_t>(v[4])},
      .pitch{static_cast<uint16_t>(v[5])},
      .roll{0}
    };
    actor.selected_target = static_cast<uint16_t>(v[8]);
    actor.flags = static_cast<uint8_t>(v[9]);
    actor.last_shot = static_cast<uint16_t>(v[11]);
    auto const slot{darker::game::aircraft_projectile_definition(actor,static_cast<uint8_t>(v[10]),
      {
        .heading{static_cast<uint16_t>(v[6])},
        .pitch{static_cast<uint16_t>(v[7])}
      },
      static_cast<uint8_t>(v[3]),static_cast<uint16_t>(v[12]),static_cast<uint8_t>(v[2]),v[13] != 0)};
    CHECK((slot ? static_cast<int>(*slot) : -1) == v[14]);
  }
}

TEST_CASE("Mission firing pressure follows native clock-wrap thresholds") {
  /// Check both sides of the four/eight-wrap boundaries and preservation of a higher existing pressure
  darker::game::mission_combat combat{{}};
  for(auto const &sample : darker::test_reference::difficulty_samples) {
    combat.difficulty = static_cast<uint8_t>(sample[1]);
    combat.update_difficulty(sample[0]);
    CHECK(combat.difficulty == sample[2]);
  }
}

TEST_CASE("Aircraft bomb drops match native target, cooldown and exhausted-pool decisions") {
  /// Compare 8BE6 admission, recorded attempts and the forced launch pitch against native execution
  for(auto const &v : darker::test_reference::aircraft_bomb_samples) {
    CAPTURE(v);
    darker::game::scenario_actor actor;
    actor.parameters.definition = &darker::game::original_object_definitions[23];
    actor.flags = static_cast<uint8_t>(v[0]);
    actor.selected_target = static_cast<uint16_t>(v[2]);
    actor.last_shot = static_cast<uint16_t>(v[4]);
    darker::game::projectile_pool pool{darker::game::projectile_list::hostile};
    darker::game::launch_emitter const emitter{};
    if(!v[3]) while(pool.launch({
      .definition{darker::game::original_object_definitions[14]},
      .emitter{emitter}
    })) {}
    auto const *shot{darker::game::drop_aircraft_bomb(pool,actor,v[1] != 0,static_cast<uint16_t>(v[5]),0)};
    CHECK((shot != nullptr) == (v[6] != 0));
    CHECK(actor.last_shot == v[7]);
    if(shot) {
      CHECK(shot->placement.angles.pitch == v[8]);
      CHECK(shot->target_token == actor.selected_target);
      CHECK(shot->deadline == static_cast<uint16_t>(v[5] + 28*256));
      CHECK(shot->parameters.definition == &darker::game::original_object_definitions[14]);
    }
  }
}

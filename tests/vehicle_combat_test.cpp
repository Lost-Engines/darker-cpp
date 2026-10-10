#include <catch2/catch_test_macros.hpp>
#include <array>
#include "game/object_definitions.h"
#include "game/vehicle_combat.h"
#include "reference/vehicle_combat_samples.h"

TEST_CASE("Mobile launcher guards and paired shots match the native combat helper", "[game][weapons]") {
  /// Compare cadence, rearward cone, altitude, map boundaries, two-cell obstruction and failed allocation accounting
  for(auto const &s : darker::test_reference::vehicle_combat_samples) {
    CAPTURE(s);
    darker::game::scenario_actor vehicle;
    vehicle.parameters.definition = &darker::game::original_object_definitions[s[0]];
    vehicle.pose.position = {static_cast<uint16_t>(s[1]),static_cast<uint16_t>(s[2]),static_cast<uint16_t>(s[3])};
    vehicle.pose.angles.heading = static_cast<uint16_t>(s[4]*8192);
    vehicle.behaviour.attack_control = static_cast<uint8_t>(s[8]);
    vehicle.last_shot = static_cast<uint16_t>(s[9]);
    darker::game::object_pose const player{.position{static_cast<uint16_t>(s[5]),static_cast<uint16_t>(s[6]),static_cast<uint16_t>(s[7])}};
    darker::game::city_map cells{};
    std::array<darker::resources::city_type,2> types{};
    constexpr std::array<int,4> columns{0,-1,0,1}, rows{-1,0,1,0};
    auto column{static_cast<uint8_t>(s[1] >> 8)}, row{static_cast<uint8_t>(s[2] >> 8)};
    for(size_t i{0}; i < types.size(); ++i) {
      types[i].collision_marker = static_cast<uint8_t>(s[13+i]);
      column = static_cast<uint8_t>(column-columns[s[4]/2]);
      row = static_cast<uint8_t>(row-rows[s[4]/2]);
      if(column < 128 && row < 128) cells[row*128+column].type = static_cast<uint8_t>(i+1);
    }
    darker::game::projectile_pool pool{darker::game::projectile_list::hostile};
    darker::game::launch_emitter const emitter{};
    if(!s[12]) {
      while(pool.objects().free) REQUIRE(pool.launch({.definition{darker::game::original_object_definitions[18]},.emitter{emitter}}));
    }
    auto const *shot{darker::game::fire_vehicle_missile(pool,vehicle,player,cells,types,
      static_cast<uint8_t>(s[4]),static_cast<uint16_t>(s[10]),static_cast<uint8_t>(s[11]),0x400)};
    CHECK(vehicle.behaviour.attack_control == s[15]);
    CHECK(vehicle.last_shot == s[16]);
    CHECK((shot != nullptr) == (s[17] != 0));
    if(shot) {
      CHECK(shot->deadline == static_cast<uint16_t>(s[10]+s[18]));
      CHECK(shot->target_token == 0xd986);
      CHECK(shot->parameters.definition == &darker::game::original_object_definitions[18]);
      CHECK(shot->placement.angles.heading == static_cast<uint16_t>(vehicle.pose.angles.heading+0x8000));
      CHECK(shot->placement.angles.pitch == 0x0abe);
    }
  }
}

TEST_CASE("Vehicle routes call the native firing check only on supported actions", "[game][weapons]") {
  /// Preserve the route interpreter's combat tail call, including its cardinal direction input
  for(auto const &s : darker::test_reference::vehicle_firing_route_samples) {
    CAPTURE(s);
    darker::game::vehicle_route route{.origin{0},.command{static_cast<uint8_t>(s[0])}};
    darker::game::object_pose pose{.position{14720,20608,128},.angles{static_cast<uint16_t>(s[1]),0,0}};
    uint8_t flags{0};
    uint16_t random{0};
    auto const result{darker::game::advance_vehicle_route(route,pose,flags,{},static_cast<uint16_t>(s[2]),0,random)};
    CHECK(result.firing_direction.has_value() == (s[3] != 255));
    if(result.firing_direction) CHECK(*result.firing_direction == s[3]);
  }
}

#include <catch2/catch_test_macros.hpp>
#include <array>
#include "game/actor_activation.h"
#include "game/aircraft_spawning.h"
#include "reference/actor_activation_samples.h"
#include "reference/aircraft_spawning_samples.h"

TEST_CASE("Air reserve placement matches the native player and neighbour scans", "[actors]") {
  /// Exercise wrapping coordinates and signed altitude boundaries through complete native C39E calls
  for(auto const &sample : darker::test_reference::air_placement_samples) {
    CAPTURE(sample);
    darker::game::scenario_actor actor;
    darker::game::object_pose player;
    std::array<darker::game::scenario_actor, 3> neighbours;
    for(size_t axis{0}; axis < 3; ++axis) {
      actor.pose.position[axis] = static_cast<uint16_t>(sample[axis]);
      player.position[axis] = static_cast<uint16_t>(sample[axis + 3]);
      for(size_t i{0}; i < neighbours.size(); ++i) neighbours[i].pose.position[axis] = static_cast<uint16_t>(sample[6 + i * 3 + axis]);
    }
    darker::game::place_air_reserve(actor, player, neighbours);
    for(size_t axis{0}; axis < 3; ++axis) CHECK(actor.pose.position[axis] == sample[15 + axis]);
  }
}

TEST_CASE("Reserve activation preserves category heads and reverses each admitted batch", "[actors]") {
  /// Native C33E repeatedly pops the reserve head and prepends to the matching active list
  using darker::game::actor_category;
  std::vector<darker::game::scenario_actor> active{
    {
      .category{actor_category::air},
      .index{1}
    },
    {
      .category{actor_category::ground},
      .index{2}
    },
  };
  std::vector<darker::game::scenario_actor> reserves{
    {
      .category{actor_category::ground},
      .index{3}
    },
    {
      .category{actor_category::air},
      .index{4}
    },
    {
      .category{actor_category::air},
      .index{5}
    },
    {
      .category{actor_category::air},
      .index{6}
    },
  };
  darker::game::activate_scenario_reserves(active, reserves, actor_category::air, 2, {}, 1234);
  REQUIRE(active.size() == 4);
  CHECK(active[0].index == 5);
  CHECK(active[1].index == 4);
  CHECK(active[2].index == 1);
  CHECK(active[3].index == 2);
  CHECK(active[0].script.deadline == 1234);
  CHECK(active[1].script.deadline == 1234);
  REQUIRE(reserves.size() == 2);
  CHECK(reserves[0].index == 3);
  CHECK(reserves[1].index == 6);
}

TEST_CASE("Warehouse preparation matches native cell variants and platform parameters") {
  /// Occupied warehouses reserve model parameter slots one through eight without altering spawn timers
  for(auto const &v : darker::test_reference::aircraft_site_samples) {
    darker::game::city_map cells{};
    darker::game::aircraft_spawning state;
    for(size_t i{0}; i < darker::game::delphi_aircraft_sites.size(); ++i) {
      auto const site{darker::game::delphi_aircraft_sites[i]};
      cells[(site >> 8) * 128 + (site & 127)].state = static_cast<uint8_t>(v[i]);
    }
    darker::game::prepare_delphi_aircraft_sites(state, cells);
    for(size_t i{0}; i < darker::game::delphi_aircraft_sites.size(); ++i) {
      auto const site{darker::game::delphi_aircraft_sites[i]};
      CHECK(cells[(site >> 8) * 128 + (site & 127)].state == v[i + 8]);
      CHECK(state.platforms[i] == v[i + 16]);
    }
  }
}

TEST_CASE("Nightmare reserves keep their authored formation and fade in", "[actors][nightmare]") {
  using namespace darker::game;
  object_pose player;
  player.position = {11392, 17024, 2400};
  std::vector<scenario_actor> reserves(5);
  std::array<darker::maths::world_position, 5> const positions{{
    {14976, 17024, 2880}, {14976, 18048, 2880}, {15488, 17536, 2880},
    {14464, 17536, 2880}, {14976, 17536, 2880}
  }};
  for(unsigned int i{0}; i < reserves.size(); ++i) {
    reserves[i].index = static_cast<actor_index>(i + 1);
    reserves[i].pose.position = positions[i];
    reserves[i].parameters.update_entry = object_update::surface_actor;
  }
  std::vector<scenario_actor> active;
  activate_scenario_reserves(active, reserves, actor_category::air, 5, player, 65500, air_reserve_mode::nightmare);
  CHECK(reserves.empty());
  REQUIRE(active.size() == 5);
  for(auto const &actor : active) {
    CHECK(actor.pose.position.column == positions[actor.index - 1].column);
    CHECK(actor.pose.position.row == positions[actor.index - 1].row);
    CHECK(actor.pose.position.height == positions[actor.index - 1].height);
    CHECK(actor.parameters.update_entry == object_update::surface_actor);
    CHECK(actor.flags == 0x40);
    CHECK(actor.fade == 0);
    CHECK(actor.expiry == 220);
    CHECK(actor.script.deadline == 65500);
  }
}

TEST_CASE("Nightmare arrival clearance checks ground actors rather than other aircraft", "[actors][nightmare]") {
  using namespace darker::game;
  scenario_actor arriving;
  arriving.pose.position = {12000, 13000, 3000};
  std::array<scenario_actor, 1> neighbours{arriving};
  object_pose player;
  player.position = arriving.pose.position;
  place_air_reserve(arriving, player, neighbours, air_reserve_mode::nightmare);
  CHECK(arriving.pose.position.height == 3000);
  neighbours[0].category = actor_category::ground;
  place_air_reserve(arriving, player, neighbours, air_reserve_mode::nightmare);
  CHECK(arriving.pose.position.column == 12000);
  CHECK(arriving.pose.position.row == 13000);
  CHECK(arriving.pose.position.height == 3512);
}

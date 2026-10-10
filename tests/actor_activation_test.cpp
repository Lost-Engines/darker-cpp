#include <catch2/catch_test_macros.hpp>
#include <array>
#include "game/actor_activation.h"
#include "game/aircraft_spawning.h"
#include "reference/aircraft_spawning_samples.h"
#include "reference/actor_activation_samples.h"

TEST_CASE("Air reserve placement matches the native player and neighbour scans", "[actors]") {
  /// Exercise wrapping coordinates and signed altitude boundaries through complete native C39E calls
  for(auto const &sample : darker::test_reference::air_placement_samples) {
    CAPTURE(sample);
    darker::game::scenario_actor actor;
    darker::game::object_pose player;
    std::array<darker::game::scenario_actor,3> neighbours;
    for(size_t axis{0}; axis < 3; ++axis) {
      actor.pose.position[axis] = static_cast<uint16_t>(sample[axis]);
      player.position[axis] = static_cast<uint16_t>(sample[axis + 3]);
      for(size_t i{0}; i < neighbours.size(); ++i) neighbours[i].pose.position[axis] = static_cast<uint16_t>(sample[6 + i*3 + axis]);
    }
    darker::game::place_air_reserve(actor,player,neighbours);
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
  darker::game::activate_scenario_reserves(active,reserves,actor_category::air,2,{},1234);
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
      cells[(site >> 8)*128 + (site & 127)].state = static_cast<uint8_t>(v[i]);
    }
    darker::game::prepare_delphi_aircraft_sites(state,cells);
    for(size_t i{0}; i < darker::game::delphi_aircraft_sites.size(); ++i) {
      auto const site{darker::game::delphi_aircraft_sites[i]};
      CHECK(cells[(site >> 8)*128 + (site & 127)].state == v[i+8]);
      CHECK(state.platforms[i] == v[i+16]);
    }
  }
}

#include "actor_flight_check.h"
#include <array>
#include <format>
#include <iostream>
#include <stdexcept>
#include "game/actor_update.h"
#include "reference/actor_flight_samples.h"
#include "resources/archive_set.h"

void check_actor_flight(darker::resources::archive_set const &archives) {
  /// Compare sequential actor updates against original 8823 on the original Delphi map and first-mission placements
  darker::resources::geometry_bank const bank{archives.load({.archive{0}, .slot{30}})};
  auto cells{darker::game::make_city_map(archives.load({.archive{0}, .slot{68}}), true)};
  std::array<uint8_t, 256> limits{};
  for(size_t i{0}; i < bank.city_types().size(); ++i) limits[i + 1] = bank.city_types()[i].variant_limit;
  darker::game::assign_city_variants(cells, limits);
  darker::resources::scenario_resource const resource{archives.load({.archive{4}, .slot{0}})};
  auto const &record{resource.records().front()};
  auto actors{darker::game::make_scenario_group(record.groups[0], bank, 1, 0, record.shared.offset)};
  darker::game::object_pose player{.position{10000, 23700, 3000}};
  size_t sample{0};
  for(int frame{0}; frame < 1024; ++frame) {
    if(frame >= 256) {
      player.position = actors.back().pose.position;
      player.position.column += 100;
    }
    for(auto &actor : actors) {
      darker::game::advance_surface_actor(actor, player, actors, cells, bank, 0x20, 8);
      auto const &pose{actor.pose};
      std::array<int, 17> const actual{pose.position.column, pose.position.row, pose.position.height, pose.angles.heading, pose.angles.pitch, pose.angles.roll,
        pose.speed, actor.attitude.pitch_rate, actor.attitude.bank_rate, actor.selected_target, actor.parameters.flags_4c,
        actor.clearance_floor, actor.awareness.level, actor.awareness.cooldown, pose.fractions.column, pose.fractions.row, pose.fractions.height};
      auto const &expected{darker::test_reference::actor_flight_samples[sample++]};
      for(size_t field{0}; field < actual.size(); ++field) {
        if(actual[field] != expected[field]) throw std::runtime_error{std::format("Actor flight frame {}, actor {}, field {}: {} != {}", frame, actor.index, field, actual[field], expected[field])};
      }
    }
  }
  std::cout << "2,048 airborne actor updates match native trajectories on the original Delphi map." << std::endl;
}

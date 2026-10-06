#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>
#include "game/actor_awareness.h"
#include "game/actor_motion.h"
#include "game/mission_script.h"
#include "game/object_definition.h"
#include "game/object_pose.h"
#include "resources/geometry_bank.h"
#include "resources/scenario.h"

namespace darker::game {

struct scenario_actor {
  object_parameters parameters{};
  object_pose pose{};
  actor_attitude attitude{};
  actor_awareness awareness{};
  mission_script script{};
  std::array<std::uint16_t, 3> previous_position{};
  // Native 50–55: retained until all navigation consumers have named contracts.
  std::array<std::uint8_t, 6> behaviour{};
  std::uint16_t selected_target{0};
  std::uint16_t target_token{0};
  std::uint16_t current_cell{0};
  std::uint16_t clearance_floor{0};
  uint16_t expiry{0};
  uint16_t last_shot{0};
  std::uint8_t index{0};
  std::uint8_t definition_slot{0};
  std::uint8_t flags{0};
  std::uint8_t attributes{0};
  std::uint8_t lifecycle{255};
};

scenario_actor make_scenario_actor(resources::scenario_placement const &placement,
  object_definition const &definition, std::uint16_t model_token, std::int16_t model_height,
  std::uint8_t index, std::uint8_t world_mode, std::size_t shared_offset);

std::vector<scenario_actor> make_scenario_group(resources::scenario_group const &group, resources::geometry_bank const &bank,
  std::uint8_t first_index, std::uint8_t world_mode, std::size_t shared_offset);

} // namespace darker::game

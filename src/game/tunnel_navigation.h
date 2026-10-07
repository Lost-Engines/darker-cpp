#pragma once

#include <cstdint>
#include "game/scenario_actor.h"

namespace darker::game {

uint8_t choose_tunnel_heading(scenario_actor &actor, city_map const &cells, tunnel_network const &network);
void advance_tunnel_actor(scenario_actor &actor, object_pose const &player, std::span<scenario_actor> active,
  city_map const &cells, tunnel_network const &network, uint16_t frame_step);

} // namespace darker::game

#pragma once

#include <cstdint>
#include "game/hangar.h"

namespace darker::game {

void initialise_tunnel_entry(player_flight &player, uint16_t site, uint8_t heading, int16_t model_height);
void update_tunnel_portal(player_flight &player, city_map &cells, hangar_state &hangar, uint16_t frame_step);

} // namespace darker::game

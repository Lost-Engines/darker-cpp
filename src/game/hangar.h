#pragma once

#include <cstdint>
#include "game/city_map.h"
#include "game/player_flight.h"

namespace darker::game {

struct hangar_state {
  std::uint16_t return_site{0x7162};
  std::uint16_t extension{0};
  std::uint8_t sound_level{0};
};

void initialise_caero_hangar(player_flight &player, city_map &cells, hangar_state &hangar, std::int16_t model_height);
void advance_hangar_departure(player_flight &player, city_map &cells, hangar_state &hangar, std::uint16_t frame_step);

} // namespace darker::game

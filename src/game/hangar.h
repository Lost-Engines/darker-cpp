#pragma once

#include <cstdint>
#include "game/city_map.h"
#include "game/player_flight.h"

namespace darker::game {

enum class hangar_return_phase { none, approaching, settling, complete };

struct hangar_state {
  std::uint16_t return_site{0x7162};
  // C610 retains the script/supply-pad destination used by B926, separately from C81E.
  std::uint16_t next_return_site{0};
  std::uint16_t extension{0};
  std::uint8_t sound_level{0};
  hangar_return_phase returning{hangar_return_phase::none};
  uint16_t deadline{0};
};

void initialise_caero_hangar(player_flight &player, city_map &cells, hangar_state &hangar, std::int16_t model_height);
void advance_hangar_departure(player_flight &player, city_map &cells, hangar_state &hangar, std::uint16_t frame_step);

bool begin_hangar_return(player_flight &player, city_map &cells, hangar_state &hangar, bool objectives_complete);
void advance_hangar_return(player_flight &player, hangar_state &hangar, uint16_t frame_step, uint16_t clock);

} // namespace darker::game

#pragma once

#include <cstdint>
#include "game/city_map.h"

namespace darker::game {

struct player_flight;
enum class supply_phase { flight, approach, docked };

struct supply_pad_state {
  supply_phase phase{supply_phase::flight};
  uint16_t site{0};
  uint16_t offset{0x8080};
  uint8_t fraction{0};
};

struct supply_control {
  uint16_t output{700};
  bool supplementary_active{false};
};

void initialise_skimma_pad(player_flight &player, uint16_t site, uint8_t heading, int16_t model_height, bool upgraded);
bool begin_supply_approach(player_flight &player, city_map const &cells, supply_pad_state &pad) noexcept;
void advance_supply_motion(player_flight &player, supply_pad_state &pad, uint16_t output,
  bool supplementary_active, uint16_t pitch_control, uint16_t frame_step);

} // namespace darker::game

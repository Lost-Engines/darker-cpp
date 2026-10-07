#pragma once

#include <cstdint>
#include "game/caero_flight.h"
#include "game/tunnel_network.h"

namespace darker::game {

struct tunnel_flight_state {
  tunnel_connection connection{};
  uint16_t progress{0};
  uint16_t lookahead{0};
  uint16_t heading_rate{0};
  uint16_t filtered_pitch{0};
  uint16_t filtered_bank{0};
  uint16_t off_route_time{0};
  uint16_t resistance{0};
  uint16_t aim_heading{0};
  uint16_t aim_pitch{0};
  uint16_t aim_heading_rate{0};
  uint16_t aim_pitch_rate{0};
  bool aiming{false};
};

struct tunnel_flight_input {
  uint16_t pitch_reference{0};
  uint16_t bank_reference{0};
  uint16_t pitch_drive{0};
  uint16_t forward_setting{248};
  uint16_t angular_response{0};
  uint16_t aim_response{480};
  uint8_t cell_collision_marker{0};
  bool engine{true};
  bool brake{false};
};

void advance_tunnel_flight(caero_flight_state &craft, tunnel_flight_state &state, tunnel_flight_input input,
  uint16_t frame_step, city_map const &cells, tunnel_network const &network);

void advance_tunnel_motion(caero_flight_state &craft, tunnel_flight_state &state, tunnel_flight_input input,
  uint16_t frame_step, city_map const &cells, tunnel_network const &network);

} // namespace darker::game

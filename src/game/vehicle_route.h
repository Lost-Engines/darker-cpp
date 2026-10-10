#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include "game/object_pose.h"
#include "maths/world_coordinates.h"

namespace darker::game {

struct vehicle_route {
  size_t cursor{0};
  uint16_t origin{0xf000};
  uint8_t command{0};
  uint16_t effect_progress{0};
  uint8_t effect_countdown{0};
};

struct vehicle_route_effect {
  maths::world_position position{};
  uint16_t recipe{0};
  uint8_t phase{0};
  uint16_t sound_level{0};
};

struct vehicle_route_result {
  std::optional<uint8_t> firing_direction;
  std::optional<uint16_t> damage_cell;
  std::optional<uint16_t> deadline;
  std::optional<vehicle_route_effect> effect;
};

vehicle_route_result advance_vehicle_route(vehicle_route &route, object_pose &pose, uint8_t &flags,
  std::span<std::byte const> program, uint16_t clock, int16_t model_height, uint16_t &random_state);

} // namespace darker::game

#pragma once

#include <array>
#include <cstdint>
#include "maths/world_coordinates.h"

namespace darker::game {

struct effect_sound_definition {
  uint16_t duration;
  uint16_t pitch;
  uint16_t level;
  uint8_t patch;
  uint8_t flags;
};

struct effect_sound {
  maths::world_position position{};
  effect_sound_definition definition{};
  uint16_t deadline{0};
  uint32_t identity{0};
  uint16_t generation{0};
};

} // namespace darker::game

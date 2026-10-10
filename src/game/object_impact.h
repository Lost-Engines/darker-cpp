#pragma once

#include <cstdint>
#include "game/object_update.h"

namespace darker::game {

struct impact_rotation {
  uint16_t pitch{0};
  uint16_t turn{0};                                                            // BP+28: bank response for Caero, turning response in other callbacks
};

struct object_impact_state {
  impact_rotation rotation{};
  uint16_t impact_accumulator{0};
  uint16_t damage{0};
  object_update update_entry{object_update::inactive};
  uint16_t deadline{0};
  uint8_t flags{0};
};

enum class impact_effect : uint16_t {
  hit = 0x72df,
  fatal = 0x7319,
};

void apply_impact_rotation(impact_rotation &rotation, uint8_t amplitude, uint16_t &random_state) noexcept;
impact_effect apply_object_impact(object_impact_state &state, uint8_t strength, uint8_t resistance,
  bool underground, uint16_t clock, uint16_t &random_state);

} // namespace darker::game

#pragma once

#include <cstdint>
#include "game/object_pose.h"

namespace darker::game {

struct actor_awareness {
  std::uint16_t level{0};
  std::uint16_t cooldown{0};
};

struct awareness_parameters {
  std::uint8_t decay{0};
  std::uint8_t rise{0};
  std::uint8_t strength{0};
  std::uint8_t cooldown_shift{0};
};

void advance_actor_awareness(actor_awareness &state, object_pose const &actor, object_pose const &player,
  awareness_parameters parameters, std::uint16_t frame_step) noexcept;

} // namespace darker::game

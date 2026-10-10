#include "game/actor_awareness.h"
#include <algorithm>

namespace darker::game {

void advance_actor_awareness(actor_awareness &state, object_pose const &actor, object_pose const &player,
  awareness_parameters const parameters, uint16_t const frame_step) noexcept {
  /// 8AA8–8AFC follows the mission callback with proximity-dependent engagement and cooldown accounting
  auto const distance{horizontal_distance(actor.position, player.position)};
  auto const inverted{static_cast<uint16_t>(~static_cast<uint16_t>((distance < 4096 ? distance : 65535) << 4))};
  auto const falloff{(static_cast<uint32_t>(inverted) * inverted) >> 16};
  auto const target{static_cast<uint16_t>((falloff * (parameters.strength * 257u)) >> 16)};
  auto const elapsed{static_cast<uint16_t>(static_cast<uint32_t>(frame_step) << (parameters.cooldown_shift & 31))};
  state.cooldown -= std::min(state.cooldown, elapsed);
  if(state.level >= target) {
    auto const decay{static_cast<uint16_t>((frame_step & 255) * parameters.decay)};
    state.level -= std::min(state.level, decay);
  } else {
    auto const increment{(frame_step & 255) * parameters.rise};
    state.level = static_cast<uint16_t>(std::min<unsigned int>(target, state.level + increment));
  }
}

} // namespace darker::game

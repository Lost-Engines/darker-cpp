#include "game/object_impact.h"
#include <stdexcept>
#include "game/random.h"

namespace darker::game {

void apply_impact_rotation(impact_rotation &rotation, std::uint8_t const amplitude, std::uint16_t &random_state) noexcept {
  /// Reproduce 8568: one random word supplies two kicks, complemented using the old heading rate's low bits
  auto const random{next_random(random_state)};
  auto const heading{static_cast<std::uint16_t>((random & 255) * amplitude + amplitude * 256)};
  auto const pitch{static_cast<std::uint16_t>((random >> 8) * amplitude + amplitude * 256)};
  auto const old_turn{rotation.turn};
  rotation.turn = (old_turn & 1) ? static_cast<std::uint16_t>(~heading) : heading;
  rotation.pitch = (old_turn & 2) ? static_cast<std::uint16_t>(~(pitch >> 1)) : pitch >> 1;
}

impact_effect apply_object_impact(object_impact_state &state, std::uint8_t const strength, std::uint8_t const resistance,
  bool const underground, std::uint16_t const clock, std::uint16_t &random_state) {
  /// Apply CE38's ordinary object hit; zero resistance and inactive callbacks have separate effect/removal paths
  if(resistance == 0 || state.update_entry == 0) {
    throw std::invalid_argument{"Object impact requires nonzero resistance and an active callback"};
  }
  unsigned int scale{0};
  if(strength > resistance) {
    unsigned int const difference{static_cast<unsigned int>(strength - resistance)};
    scale = difference >= resistance ? 65535 : ((difference * 256 + resistance) / resistance) * 257;
  }
  auto kick{scale >> 2};
  if(underground) {
    if((kick >> 8) < 15) kick = (kick & 255) | (((state.impact_accumulator >> 10) + 24) << 8);
    kick >>= 1;
  }
  apply_impact_rotation(state.rotation, static_cast<std::uint8_t>(kick >> 8), random_state);
  unsigned int const accumulation{state.impact_accumulator + strength * 256u};
  state.impact_accumulator = static_cast<std::uint16_t>(accumulation > 65535 ? 65535 : accumulation);
  unsigned int const damage{state.damage + scale + 1};
  state.damage = static_cast<std::uint16_t>(damage);
  if(damage <= 65535) return impact_effect::hit;

  auto const half{(scale + state.damage) >> 1};
  auto const delay{(static_cast<std::uint16_t>(~half) >> 4) + 1024};
  state.damage = 65535;
  auto deadline{static_cast<std::uint16_t>(clock + delay + 256)};
  if(state.flags & 0x20) {
    deadline = static_cast<std::uint16_t>(clock + 256);
    if(static_cast<std::uint16_t>(state.deadline - deadline) & 0x8000) return impact_effect::fatal;
  }
  state.deadline = deadline;
  state.flags |= 0x20;
  state.update_entry = 0x8daa;
  return impact_effect::fatal;
}

} // namespace darker::game

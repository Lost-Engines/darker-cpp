#include "game/projectile_motion.h"
#include <algorithm>
#include <bit>
#include "game/flight_motion.h"
#include "game/object_deadline.h"

namespace darker::game {
namespace {

int16_t signed_word(int const value) {
  /// Preserve word wrapping before signed comparisons or shifts
  return std::bit_cast<int16_t>(static_cast<uint16_t>(value));
}

} // anonymous namespace

void advance_direct_projectile(object_pose &state, object_definition const &definition, game_duration const frame_step, uint16_t const speed_bonus) {
  /// CC64/CC87/858F approach definition speed, integrate its midpoint, then project motion
  auto const old_speed{signed_word(state.speed)};
  auto const target{signed_word(definition.base_speed * 16 + speed_bonus)};
  auto const step{static_cast<uint16_t>(frame_step * 4)};
  auto const candidate{signed_word(old_speed < target ? old_speed + step : old_speed - step)};
  auto const speed{old_speed < target ? std::min(candidate, target) : std::max(candidate, target)};
  advance_speed_motion(state, static_cast<uint16_t>(speed), frame_step);
}

bool update_projectile_deadline(projectile &record, clock_tick const clock) {
  /// Projectiles share the native object walker's fade and removal deadline rules
  return advance_object_deadline(record.flags, record.deadline, record.fade, record.placement.position.height, clock);
}

} // namespace darker::game

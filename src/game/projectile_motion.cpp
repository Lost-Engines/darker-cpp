#include "game/projectile_motion.h"
#include <algorithm>
#include <bit>
#include "maths/sine_table.h"

namespace darker::game {
namespace {

std::int16_t signed_word(int const value) {
  /// Preserve word wrapping before signed comparisons or shifts
  return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}

} // namespace

void advance_direct_projectile(object_pose &state, object_definition const &definition, std::uint16_t const frame_step) {
  /// CC64/CC87/858F approach definition speed, integrate its midpoint, then project motion
  auto const old_speed{signed_word(state.speed)};
  auto const target{static_cast<std::int16_t>(definition.base_speed * 16)};
  auto const step{static_cast<std::uint16_t>(frame_step * 4)};
  auto const candidate{signed_word(old_speed < target ? old_speed + step : old_speed - step)};
  auto const speed{old_speed < target ? std::min(candidate, target) : std::max(candidate, target)};
  state.speed = static_cast<std::uint16_t>(speed);
  auto const midpoint{signed_word(old_speed + (signed_word(speed - old_speed) >> 1))};
  auto const time{signed_word((frame_step & 255) << 8)};
  auto const distance{signed_word((static_cast<std::int32_t>(midpoint) * time) >> 14)};
  unsigned int const pitch{static_cast<unsigned int>(state.angles[1] >> 6)};
  unsigned int const heading{static_cast<unsigned int>(state.angles[0] >> 6)};
  auto const sine{[](unsigned int const angle){ return maths::original_sine[angle]; }};
  auto const cosine{[&](unsigned int const angle){ return sine((angle + 256) % 1024); }};
  displace_object(state, 2, (sine(pitch) * distance) >> 8);
  auto const horizontal{signed_word((cosine(pitch) * distance) >> 15)};
  displace_object(state, 1, -((cosine(heading) * horizontal) >> 11));
  displace_object(state, 0, -((sine(heading) * horizontal) >> 11));
}

bool update_projectile_deadline(projectile &record, std::uint16_t const clock) {
  /// 79E5–7A18 update timed flags/fade, stopping before expiry's world and mission effects
  auto mode{static_cast<std::uint8_t>(record.flags & 0x60)};
  if(mode == 0) return false;
  auto const delta{static_cast<std::uint16_t>(record.deadline - clock)};
  if((delta & 0x8000) != 0) {
    record.flags ^= mode;
    if((mode & 0x20) != 0) return true;
    record.fade = 255;
    return false;
  }
  std::uint8_t fade{255};
  if(delta < 256) fade = static_cast<std::uint8_t>(delta);
  else if(std::bit_cast<std::int8_t>(static_cast<std::uint8_t>(record.placement.position[2] >> 8)) >= 0x50) {
    record.flags |= 0x20;
    mode = 0x20;
    record.deadline = static_cast<std::uint16_t>(clock + 255);
  }
  record.fade = (mode & 0x20) != 0 ? fade : static_cast<std::uint8_t>(~fade);
  return false;
}

} // namespace darker::game

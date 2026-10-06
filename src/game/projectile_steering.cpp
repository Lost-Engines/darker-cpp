#include "game/projectile_steering.h"
#include <algorithm>
#include <bit>
#include <stdexcept>
#include "game/projectile_motion.h"

namespace darker::game {
namespace {

std::int16_t signed_word(int const value) {
  /// Interpret native intermediate words after truncation
  return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}

std::int16_t rounded_product(std::int16_t const value, std::uint16_t const step) {
  /// 83E6/8404 retain the CH contribution while packing the rounded product
  auto const product{static_cast<std::int32_t>(value) * signed_word(step)};
  return signed_word(((product + 128) >> 8) + (step & 0xff00));
}

} // namespace

angular_response calculate_angular_response(std::uint16_t const error, std::uint16_t const rate,
  std::uint16_t const response, std::uint16_t const frame_step) {
  /// 83BF–8411 clamp signed error, accelerate and damp angular rate, then integrate its midpoint
  int const sign{signed_word(error) < 0 ? -1 : 0};
  int const magnitude{static_cast<std::uint16_t>(error ^ sign)};
  auto const bounded{signed_word(std::min(magnitude, 0x2800) ^ sign)};
  auto const scaled{signed_word((static_cast<std::int32_t>(bounded) * signed_word(frame_step)) >> 8)};
  auto const acceleration{signed_word((static_cast<std::int32_t>(signed_word(response)) * scaled + 128) >> 8)};
  auto const doubled{static_cast<std::uint16_t>(frame_step * 2)};
  auto const previous{signed_word(rate)};
  auto const candidate{signed_word(previous + acceleration)};
  auto next{signed_word(candidate - rounded_product(previous, doubled))};
  if((static_cast<std::uint16_t>(candidate ^ next) & 0x8000) != 0) next = 0;
  auto const midpoint{signed_word(previous + (signed_word(next - previous) >> 1))};
  return {
    .rate{static_cast<std::uint16_t>(next)},
    .angle_delta{static_cast<std::uint16_t>(rounded_product(midpoint, doubled))},
    .frame_step{static_cast<std::uint16_t>(doubled >> 1)},
  };
}

void advance_homing_projectile(projectile &record, std::uint16_t const target_heading,
  std::uint16_t const target_pitch, std::uint16_t const frame_step) {
  /// CCDB updates pitch and heading before CC64 runs the shared speed and position integration
  if(!record.parameters.definition) throw std::invalid_argument{"homing projectile requires an object definition"};
  auto &angles{record.placement.angles};
  auto const pitch{calculate_angular_response(static_cast<std::uint16_t>(target_pitch - angles[1]),
    record.angular_motion[1], record.parameters.angular_response, frame_step)};
  record.angular_motion[1] = pitch.rate;
  angles[1] = static_cast<std::uint16_t>(angles[1] + pitch.angle_delta);
  auto const difference{static_cast<std::uint16_t>(target_heading - angles[0])};
  int const sign{signed_word(difference) < 0 ? -1 : 0};
  auto const adjusted{static_cast<std::uint16_t>(((difference >> 8) ^ (sign & 255)) >= 0x40
    ? -signed_word(difference) : signed_word(difference) + signed_word(sign ^ 0x00c0))};
  auto const heading{calculate_angular_response(adjusted, record.angular_motion[2], record.parameters.angular_response, pitch.frame_step)};
  record.angular_motion[2] = heading.rate;
  angles[0] = static_cast<std::uint16_t>(angles[0] + heading.angle_delta);
  advance_direct_projectile(record.placement, *record.parameters.definition, heading.frame_step);
}

} // namespace darker::game

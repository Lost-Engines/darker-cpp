#include "game/projectile_steering.h"
#include <algorithm>
#include <bit>
#include <stdexcept>
#include "game/projectile_motion.h"
#include "maths/direction.h"

namespace darker::game {
namespace {

std::int16_t signed_word(int const value) {
  /// Interpret native intermediate words after truncation
  return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}

} // namespace

void advance_mimic_projectile(projectile &record, object_pose const &player, uint16_t const remaining, uint16_t const frame_step) {
  /// CBCE copies player pitch and roll, then 8375 turns using midpoint bank and remaining-life response
  if(!record.parameters.definition) throw std::invalid_argument{"Mimic projectile requires an object definition"};
  auto &angles{record.placement.angles};
  record.parameters.motion[2] = static_cast<uint16_t>(remaining << 4);
  angles[1] = player.angles[1];
  auto const difference{signed_word(player.angles[2] - angles[2])};
  angles[2] = player.angles[2];
  auto const midpoint{static_cast<uint16_t>(angles[2] - (difference >> 1))};
  auto const turn{signed_word((signed_word(record.parameters.motion[2]) * signed_word(fold_bank_angle(midpoint))) >> 15)};
  auto const delta{signed_word((signed_word((frame_step & 255)*257) * turn) >> 15)};
  angles[0] = static_cast<uint16_t>(angles[0] + delta);
  advance_direct_projectile(record.placement,*record.parameters.definition,frame_step);
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

void advance_object_homing_projectile(projectile &record, object_pose const &target, std::uint16_t const frame_step) {
  /// CCB9 resolves object positions; self-targeting deliberately retains the current angles
  auto const angles{&target == &record.placement
    ? maths::direction_angles{.heading{record.placement.angles[0]}, .pitch{record.placement.angles[1]}}
    : maths::object_target_direction(record.placement.position, target.position)};
  advance_homing_projectile(record, angles.heading, angles.pitch, frame_step);
}

void advance_map_homing_projectile(projectile &record, map_guidance_target const target, std::uint16_t const frame_step) {
  /// CC9C's map branch computes an aim height, then 831E banks towards its resolved position
  if(!record.parameters.definition) throw std::invalid_argument{"map homing requires an object definition"};
  auto const &definition{*record.parameters.definition};
  unsigned int const shift{static_cast<unsigned int>(definition.role_data[6] & 31)};
  auto const height_offset{shift < 16 ? target.height_extent >> shift : 0};
  auto const height{static_cast<std::uint16_t>(target.height - height_offset)};
  auto const &position{record.placement.position};
  auto const x{static_cast<std::uint16_t>(position[0] - target.position[0])};
  auto const y{static_cast<std::uint16_t>(position[1] - target.position[1])};
  auto step{frame_step};
  if(!(static_cast<std::uint16_t>(x + 7) < 15 && static_cast<std::uint16_t>(y + 7) < 15)) {
    auto const direction{maths::direction_from_displacement({x, y, static_cast<std::uint16_t>(height - position[2])})};
    auto &angles{record.placement.angles};
    auto const heading_error{signed_word(direction.heading - angles[0])};
    // JO after doubling rejects +4000h, but accepts -4000h.
    if(heading_error >= -16384 && heading_error < 16384) {
      auto const pitch{calculate_angular_response(static_cast<std::uint16_t>(direction.pitch - angles[1]),
        record.angular_motion[1], record.parameters.angular_response, step)};
      angles[1] = static_cast<std::uint16_t>(angles[1] + pitch.angle_delta);
      record.angular_motion[1] = pitch.rate;
      auto const bank{signed_word((static_cast<std::int32_t>(heading_error) * signed_word(record.parameters.motion[0])) >> 15)};
      int const sign{bank < 0 ? -1 : 0};
      auto const magnitude{static_cast<std::uint16_t>((bank ^ sign) - sign)};
      auto const bounded{std::min(magnitude, record.parameters.motion[1])};
      auto const roll_target{signed_word(((bounded ^ sign) - sign) * 2)};
      auto const roll{calculate_angular_response(static_cast<std::uint16_t>(roll_target - angles[2]),
        record.angular_motion[2], record.parameters.angular_response, pitch.frame_step)};
      record.angular_motion[2] = roll.rate;
      angles[2] = static_cast<std::uint16_t>(angles[2] + roll.angle_delta);
      auto const midpoint{static_cast<std::uint16_t>(angles[2] - (signed_word(roll.angle_delta) >> 1))};
      auto const shaped{signed_word(fold_bank_angle(midpoint))};
      auto const turn{signed_word((static_cast<std::int32_t>(signed_word(record.parameters.motion[2])) * shaped) >> 15)};
      auto const time{signed_word((roll.frame_step & 255) * 257)};
      auto const heading_delta{signed_word((static_cast<std::int32_t>(time) * turn) >> 15)};
      angles[0] = static_cast<std::uint16_t>(angles[0] + heading_delta);
      step = roll.frame_step;
    }
  }
  advance_direct_projectile(record.placement, definition, step);
}

} // namespace darker::game

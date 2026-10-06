#include "game/player_flight.h"
#include <cstddef>
#include "game/object_definitions.h"

namespace darker::game {

object_pose &player_flight::pose() noexcept {
  /// Both craft callbacks expose the same position and attitude to collision and camera consumers
  return std::visit([](auto &state)->object_pose &{ return state.pose; }, craft);
}

object_pose const &player_flight::pose() const noexcept {
  /// Rendering borrows the current pose without altering simulation state
  return std::visit([](auto const &state)->object_pose const &{ return state.pose; }, craft);
}

void player_flight::command(flight_command const command) noexcept {
  /// Apply the craft-dependent flight key actions; sound and presentation remain event consumers
  if(lifecycle.flags & 0x20) return;
  auto *caero{std::get_if<caero_flight_state>(&craft)};
  switch(command) {
    case flight_command::engine:
      engine_flags ^= 1;
      if(auto *skimma{std::get_if<skimma_flight_state>(&craft)}) {
        if(lifecycle.flags & 0x10) engine_flags &= 0xfe;
        skimma->damage.shield_enabled = (engine_flags & 1) != 0;
      }
      break;
    case flight_command::altitude_hold:
      altitude_hold = !altitude_hold;
      desired_height = pose().position[2];
      break;
    case flight_command::boost:
      if(caero) activate_caero_boost(*caero);
      else if(upgraded) forward_setting = 640;
      break;
    case flight_command::speed_low:
      if(!caero) forward_setting = 248;
      break;
    case flight_command::speed_high:
      if(!caero) forward_setting = 500;
      break;
  }
}

city_collision_result player_flight::advance(flight_controls_input const input, bool const brake, std::uint16_t const frame_step,
  std::uint16_t const clock, resources::geometry_bank const &bank, std::span<city_cell, 128 * 128> const cells) {
  /// Compose native steering, the current craft callback and city collision without advancing unrelated actor or mission systems
  if(frame_step == 0) return {};
  if(lifecycle.crashing) {
    advance_player_crash(pose(), frame_step);
    return {};
  }
  auto const steering{update_flight_controls(controls, input, frame_step)};
  auto const previous{pose().position};
  bool const caero{std::holds_alternative<caero_flight_state>(craft)};
  auto const &definition{original_object_definitions[caero ? 25 : upgraded ? 27 : 26]};
  auto const gain{static_cast<std::uint16_t>(definition.angular_seed * 8)};
  auto const bias{static_cast<std::int8_t>(definition.role_data[5])};
  if(caero) {
    advance_caero_flight(std::get<caero_flight_state>(craft),
      {.angular_response{gain}, .drive_multiplier{static_cast<std::uint16_t>(definition.role_data[4] * 8)},
        .vertical_bias{bias}, .desired_height{desired_height}, .height_reference{controls.pitch.reference}},
      {.bank_drive{steering.bank}, .pitch_drive{steering.pitch}, .engine_flags{engine_flags}, .altitude_hold{altitude_hold}, .brake{brake}},
      frame_step, cells);
  } else {
    advance_skimma_flight(std::get<skimma_flight_state>(craft), {.angular_response{gain}, .vertical_bias{bias}},
      {.bank_drive{steering.bank}, .pitch_drive{steering.pitch}, .forward_setting{forward_setting}, .brake{brake}}, frame_step);
  }
  auto const mask{static_cast<std::uint8_t>(caero ? 0x20 : 0x60)};
  auto const contact{sweep_city(bank, cells, mask, previous, pose().position)};
  bool const protected_terrain{contact.contact == city_contact::terrain && (lifecycle.flags & 0x10)};
  if(contact.contact != city_contact::none && !protected_terrain && !(lifecycle.flags & 0x20)) {
    if(contact.contact == city_contact::building && contact.category == 2) {
      auto &cell{cells[contact.row * 128 + contact.column]};
      auto const model{bank.city_model_offset(cell.type, cell.state, mask)};
      auto const pool{bank.model_pool()};
      if(pool[model] != std::byte{0} || pool[model + 1] != std::byte{0}) cell.state = static_cast<std::uint8_t>(cell.state + 32);
    }
    if(start_player_crash(pose(), lifecycle, clock)) engine_flags = 0;
  }
  return contact;
}

} // namespace darker::game

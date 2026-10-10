#include "game/player_flight.h"
#include <cstddef>
#include <stdexcept>
#include "game/object_definitions.h"

namespace darker::game {

uint8_t player_flight::definition_slot() const noexcept {
  /// The scenario's low configuration nibble selects definitions 24–28, independently of the visible craft family
  if(scenario_configuration) return static_cast<uint8_t>(24+*scenario_configuration);
  return tunnel ? 28 : std::holds_alternative<caero_flight_state>(craft) ? 25 : upgraded ? 27 : 26;
}

uint8_t player_flight::world_damage_mask() const noexcept {
  /// Both definition 24's final Skimma and definition 25's Caero use Delphi's single damage-stage bit
  return definition_slot() <= 25 ? 0x20 : 0x60;
}

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

void player_flight::toggle_freeze() noexcept {
  /// B938 swaps the player motion callback with a no-op and clears speed on both transitions
  frozen = !frozen;
  pose().speed = 0;
}

void player_flight::advance_motion(flight_controls_input const input, bool const brake, uint16_t const frame_step,
  resources::geometry_bank const &bank, city_map const &cells, tunnel_network const *const network, supply_control const supply_input) {
  /// Advance controls and craft motion before the campaign's common collision phase
  if(frame_step == 0) return;
  if(lifecycle.crashing) {
    advance_player_crash(pose(),frame_step);
    return;
  }
  auto steering{update_flight_controls(controls, input, frame_step)};
  look_drive = input.look_around ? steering : flight_steering{};
  if(input.look_around) {
    controls.bank.reference = 0;
    controls.pitch.reference = 0;
    steering = {};
  }
  if(frozen) return;
  bool const caero{std::holds_alternative<caero_flight_state>(craft)};
  auto const &definition{original_object_definitions[noclip && caero ? 25 : definition_slot()]};
  auto const gain{static_cast<std::uint16_t>(definition.angular_seed * 8)};
  auto const bias{static_cast<std::int8_t>(definition.role_data.player().drive_bias)};
  if(tunnel && !noclip) {
    if(!caero || !network) throw std::logic_error{"Tunnel player flight requires a Caero and its route network"};
    auto const cell{tunnel->connection.cell};
    auto const type{cells.at((cell >> 8)*128 + (cell & 127)).type};
    auto const marker{type ? bank.city_types()[type - 1].collision_marker : uint8_t{0}};
    advance_tunnel_flight(std::get<caero_flight_state>(craft),*tunnel,
      {.pitch_reference{controls.pitch.reference},.bank_reference{controls.bank.reference},.pitch_drive{steering.pitch},
        .forward_setting{forward_setting},.angular_response{gain},
        .aim_response{static_cast<uint16_t>(original_object_definitions[0].angular_seed*8)},.cell_collision_marker{marker},
        .engine{(engine_flags & 1) != 0},.brake{brake}},frame_step,cells,*network);
  } else if(caero) {
    advance_caero_flight(std::get<caero_flight_state>(craft),
      {.angular_response{gain}, .drive_multiplier{static_cast<std::uint16_t>(definition.role_data.player().drive_multiplier * 8)},
        .vertical_bias{bias}, .desired_height{desired_height}, .height_reference{controls.pitch.reference}},
      {.bank_drive{steering.bank}, .pitch_drive{steering.pitch}, .engine_flags{engine_flags}, .altitude_hold{altitude_hold || scripted_altitude_hold}, .brake{brake}, .boost_cheat{boost_cheat}, .unlimited_power{noclip}},
      frame_step, cells);
  } else if(supply.phase != supply_phase::flight) {
    advance_supply_motion(*this,supply,supply_input.output,supply_input.supplementary_active,controls.pitch.reference,frame_step);
  } else {
    advance_skimma_flight(std::get<skimma_flight_state>(craft), {.angular_response{gain}, .vertical_bias{bias}},
      {.bank_drive{steering.bank}, .pitch_drive{steering.pitch}, .forward_setting{forward_setting}, .brake{brake}}, frame_step);
  }
}

void player_flight::apply_city_contact(city_collision_result const contact, uint16_t const clock,
  resources::geometry_bank const &bank, city_map &cells) {
  /// Apply 6EEC/6F84 only after object contacts have had their native chance to supersede the city hit
  if(noclip) return;
  auto const mask{world_damage_mask()};
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
}

city_collision_result player_flight::advance(flight_controls_input const input, bool const brake, uint16_t const frame_step,
  uint16_t const clock, resources::geometry_bank const &bank, city_map &cells,
  tunnel_network const *const network, supply_control const supply_input) {
  /// Compose standalone flight and city collision for callers without a mission actor simulation
  bool const collidable{frame_step != 0 && !lifecycle.crashing && !noclip};
  auto const previous{pose().position};
  advance_motion(input,brake,frame_step,bank,cells,network,supply_input);
  if(!collidable) return {};
  auto const contact{sweep_city(bank,cells,world_damage_mask(),previous,pose().position)};
  apply_city_contact(contact,clock,bank,cells);
  return contact;
}

} // namespace darker::game

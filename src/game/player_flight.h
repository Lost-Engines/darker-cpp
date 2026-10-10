#pragma once

#include <optional>
#include <variant>
#include "game/caero_flight.h"
#include "game/city_sweep.h"
#include "game/flight_controls.h"
#include "game/player_crash.h"
#include "game/skimma_flight.h"
#include "game/supply_pad.h"
#include "game/time.h"
#include "game/tunnel_flight.h"
#include "resources/scenario_configuration.h"

namespace darker::game {

enum class flight_command { engine, altitude_hold, boost, speed_low, speed_high };

struct player_flight {
  std::variant<caero_flight_state, skimma_flight_state> craft{};
  std::optional<tunnel_flight_state> tunnel;
  flight_controls_state controls{};
  flight_steering look_drive{};
  player_crash_state lifecycle{};
  uint16_t desired_height{0};
  uint16_t forward_setting{skimma_flight_rules::low_drive};
  uint8_t engine_flags{1};
  bool altitude_hold{false};
  bool scripted_altitude_hold{false};
  bool upgraded{false};
  bool boost_cheat{false};
  bool damage_cheat{false};
  bool frozen{false};
  bool noclip{false};
  supply_pad_state supply{};
  std::optional<resources::scenario_configuration> scenario_configuration;

  uint8_t definition_slot() const noexcept;
  uint8_t world_damage_mask() const noexcept;
  object_pose &pose() noexcept;
  object_pose const &pose() const noexcept;
  void command(flight_command command) noexcept;
  void toggle_freeze() noexcept;
  void advance_motion(flight_controls_input input, bool brake, game_duration frame_step,
    resources::geometry_bank const &bank, city_map const &cells, tunnel_network const *network = nullptr, supply_control supply_input = {});
  void apply_city_contact(city_collision_result contact, clock_tick clock, resources::geometry_bank const &bank, city_map &cells);
  city_collision_result advance(flight_controls_input input, bool brake, game_duration frame_step,
    clock_tick clock, resources::geometry_bank const &bank, city_map &cells, tunnel_network const *network = nullptr, supply_control supply_input = {});
};

} // namespace darker::game

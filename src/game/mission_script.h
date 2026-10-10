#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <vector>
#include "game/city_map.h"
#include "game/time.h"

namespace darker::game {

struct mission_script {
  size_t continuation{0};
  size_t checkpoint{0};
  uint16_t checkpoint_clock{0};
  clock_tick deadline{0};
  bool stopped{false};
};

enum class message_alignment { centre, left, right };

struct mission_message {
  size_t offset{0};
  uint8_t length{0};
  uint8_t width{0};
  uint16_t expiry{0};
  message_alignment alignment{message_alignment::centre};
  std::span<std::byte const> text{};
};

struct mission_context {
  std::span<std::byte const> program{};
  std::span<std::byte const> text{};
  std::span<uint8_t const> object_flags{};
  std::span<city_cell const> cells{};
  campaign_clock clock{0};
  uint8_t time_multiplier{50};
  bool objectives_complete{false};
  bool at_target_cell{false};
  bool suppress_messages{false};
  std::function<void(uint8_t)> set_difficulty{};
  std::function<void(uint8_t)> reset_score{};
  std::function<void(std::optional<uint16_t>)> set_altitude{};
  bool scripted_altitude_hold{false};
  uint8_t object_counter{0};
  uint8_t counter{0};
  uint8_t animation_parameter{0};
  size_t text_cursor{0};
  std::vector<mission_message> messages{};
  // the world admits reserves and returns the new objective-completion condition before script execution resumes
  std::function<bool(uint8_t, uint8_t)> activate_reserves{};
  std::function<void(uint8_t, uint8_t, uint8_t)> change_beacons{};
  uint16_t current_cell{0};
  std::function<void(uint16_t, bool)> set_target{};
  std::function<void(uint8_t)> select_weapon{};
  std::function<void(uint8_t)> set_building_attacks{};
  std::function<bool()> retire_distant_actor{};
  std::function<void(uint8_t)> set_aircraft_spawning{};
  std::function<uint16_t()> register_owner{};
  std::function<void(mission_script&)> exchange_context{};
  std::function<bool(uint8_t)> adjust_objectives{};
  std::function<size_t(std::span<std::byte const>)> replace_world_objectives{};
  std::function<void(uint8_t)> set_tunnel_oscillation{};
  uint8_t message_setting{0};
  uint8_t progress{0};
  uint16_t hud_reference{0xffff};
  uint16_t transition_output{700};
  std::function<void()> refill_weapon{};
  std::function<void()> reset_shield{};
  std::function<void(uint16_t)> toggle_weapons{};
  std::function<size_t(std::span<std::byte const>)> mark_aircraft_sites{};
};

size_t advance_mission_script(mission_script &script, mission_context &context);

} // namespace darker::game

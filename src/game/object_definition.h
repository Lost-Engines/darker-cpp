#pragma once

#include <array>
#include <bit>
#include <cstdint>
#include "game/object_update.h"

namespace darker::game {

template<typename T>
struct steering_response {
  T bank_response{};
  T bank_limit{};
  T turn_response{};
};

struct craft_definition_data {
  std::uint8_t acceleration{};
  std::uint8_t deceleration{};
  std::uint8_t cruise_height{};
  std::uint8_t cooldown_shift{};
  std::uint8_t cruise_speed{};
  std::uint8_t attack_speed{};
  std::uint8_t weapon_slot{};
  std::uint8_t flags{};
};

struct projectile_definition_data {
  std::uint8_t launch_cost{}; // also scales the firing delay for hostile weapons
  std::uint8_t lifetime{};
  std::array<std::uint8_t,4> reserved{};
  std::uint8_t steering_shift{};
  std::uint8_t flags{};
};

struct player_definition_data {
  std::array<std::uint8_t,2> reserved{};
  std::uint8_t initial_height{};
  std::uint8_t cooldown_shift{};
  std::uint8_t drive_multiplier{};
  std::uint8_t drive_bias{};
  std::uint8_t weapon_slot{};
  std::uint8_t flags{};
};

class object_role_data {
  // Preserve the native overlay, including unused bytes, without reading an inactive union member.
  std::array<std::uint8_t,8> bytes{};
public:
  constexpr object_role_data() = default;
  constexpr object_role_data(craft_definition_data value) : bytes{std::bit_cast<decltype(bytes)>(value)} {}
  constexpr object_role_data(projectile_definition_data value) : bytes{std::bit_cast<decltype(bytes)>(value)} {}
  constexpr object_role_data(player_definition_data value) : bytes{std::bit_cast<decltype(bytes)>(value)} {}

  constexpr auto craft() const noexcept->craft_definition_data { return std::bit_cast<craft_definition_data>(bytes); }
  constexpr auto projectile() const noexcept->projectile_definition_data { return std::bit_cast<projectile_definition_data>(bytes); }
  constexpr auto player() const noexcept->player_definition_data { return std::bit_cast<player_definition_data>(bytes); }
};

struct object_definition {
  std::uint16_t model_token{0};
  object_update update_entry{object_update::inactive};
  std::uint8_t angular_seed{0};
  steering_response<std::uint8_t> motion_seeds{};
  std::uint8_t impact_strength{0};
  std::uint8_t base_speed{0};
  object_role_data role_data{};
  std::uint16_t sound_entry{0};
  std::uint8_t fm_patch{0};
  std::uint8_t sound_level{0};
  std::uint16_t sound_pitch{0};
};

// Only the fields written by BF41–BF6E, not the complete runtime object.
struct object_parameters {
  object_definition const *definition{nullptr};
  std::uint16_t model_token{0};
  object_update update_entry{object_update::inactive};
  std::uint16_t flags_4c{0};
  std::uint16_t angular_response{0};
  steering_response<std::uint16_t> motion{};
};

void apply_object_definition(object_parameters &parameters, object_definition const &definition, std::uint16_t model_token);

} // namespace darker::game

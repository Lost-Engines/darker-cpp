#pragma once

#include <array>
#include <bit>
#include <cstdint>
#include <utility>
#include "game/object_update.h"

namespace darker::game {

using object_definition_index = uint8_t;
using impact_strength = uint8_t;

template<typename T>
struct steering_response {
  T bank_response{};
  T bank_limit{};
  T turn_response{};
};

struct craft_definition_data {
  uint8_t acceleration{0};
  uint8_t deceleration{0};
  uint8_t cruise_height{0};
  uint8_t cooldown_shift{0};
  uint8_t cruise_speed{0};
  uint8_t attack_speed{0};
  uint8_t weapon_slot{0};
  uint8_t flags{0};
};

enum class projectile_flag : uint8_t {
  ground_target = 2,
};

struct projectile_definition_data {
  uint8_t launch_cost{0};                                                      // also scales the firing delay for hostile weapons
  uint8_t lifetime{0};
  std::array<uint8_t, 4> reserved{};
  uint8_t steering_shift{0};
  uint8_t flags{0};

  constexpr bool has_flag(projectile_flag const flag) const noexcept {
    /// Test a named property while retaining the original packed definition byte
    return (flags & std::to_underlying(flag)) != 0;
  }
};

struct player_definition_data {
  std::array<uint8_t, 2> reserved{};
  uint8_t initial_height{0};
  uint8_t cooldown_shift{0};
  uint8_t drive_multiplier{0};
  uint8_t drive_bias{0};
  uint8_t weapon_slot{0};
  uint8_t flags{0};
};

class object_role_data {
  // preserve the native overlay, including unused bytes, without reading an inactive union member
  std::array<uint8_t, 8> bytes{};
public:
  constexpr object_role_data() = default;
  constexpr object_role_data(craft_definition_data value) :
    bytes{std::bit_cast<decltype(bytes)>(value)} {
    }
  constexpr object_role_data(projectile_definition_data value) : bytes{std::bit_cast<decltype(bytes)>(value)} {
  }
  constexpr object_role_data(player_definition_data value) : bytes{std::bit_cast<decltype(bytes)>(value)} {
  }

  constexpr auto craft() const noexcept->craft_definition_data {
    return std::bit_cast<craft_definition_data>(bytes);
  }
  constexpr auto projectile() const noexcept->projectile_definition_data {
    return std::bit_cast<projectile_definition_data>(bytes);
  }
  constexpr auto player() const noexcept->player_definition_data {
    return std::bit_cast<player_definition_data>(bytes);
  }
};

struct object_definition {
  uint16_t model_token{0};
  object_update update_entry{object_update::inactive};
  uint8_t angular_seed{0};
  steering_response<uint8_t> motion_seeds{};
  uint8_t impact_strength{0};
  uint8_t base_speed{0};
  object_role_data role_data{};
  uint16_t sound_entry{0};
  uint8_t fm_patch{0};
  uint8_t sound_level{0};
  uint16_t sound_pitch{0};
};

// only the fields written by BF41–BF6E, not the complete runtime object
struct object_parameters {
  object_definition const *definition{nullptr};
  uint16_t model_token{0};
  object_update update_entry{object_update::inactive};
  uint16_t flags_4c{0};
  uint16_t angular_response{0};
  steering_response<uint16_t> motion{};
};

void apply_object_definition(object_parameters &parameters, object_definition const &definition, uint16_t model_token);

} // namespace darker::game

#pragma once

#include <array>
#include <cstdint>

namespace darker::game {

struct object_definition {
  std::uint16_t model_token{0};
  std::uint16_t update_entry{0};
  std::uint8_t angular_seed{0};
  std::array<std::uint8_t, 3> motion_seeds{};
  std::uint8_t impact_strength{0};
  std::uint8_t base_speed{0};
  std::array<std::uint8_t, 8> role_data{};
  std::uint16_t sound_entry{0};
  std::uint8_t fm_patch{0};
  std::uint8_t sound_level{0};
  std::uint16_t sound_pitch{0};
};

// Only the fields written by BF41–BF6E, not the complete runtime object.
struct object_parameters {
  object_definition const *definition{nullptr};
  std::uint16_t model_token{0};
  std::uint16_t update_entry{0};
  std::uint16_t flags_4c{0};
  std::uint16_t angular_response{0};
  std::array<std::uint16_t, 3> motion{};
};

void apply_object_definition(object_parameters &parameters, object_definition const &definition, std::uint16_t model_token);

} // namespace darker::game

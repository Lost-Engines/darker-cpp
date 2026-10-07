#pragma once

#include <cstdint>

namespace darker::game {

struct impact_rotation {
  std::uint16_t pitch{0};
  std::uint16_t turn{0};                                                       // BP+28: bank response for Caero, turning response in other callbacks
};

struct object_impact_state {
  impact_rotation rotation{};
  std::uint16_t impact_accumulator{0};
  std::uint16_t damage{0};
  std::uint16_t update_entry{0};
  std::uint16_t deadline{0};
  std::uint8_t flags{0};
};

enum class impact_effect : std::uint16_t {
  hit = 0x72df,
  fatal = 0x7319,
};

void apply_impact_rotation(impact_rotation &rotation, std::uint8_t amplitude, std::uint16_t &random_state) noexcept;
impact_effect apply_object_impact(object_impact_state &state, std::uint8_t strength, std::uint8_t resistance,
  bool underground, std::uint16_t clock, std::uint16_t &random_state);

} // namespace darker::game

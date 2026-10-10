#pragma once

#include <cstdint>
#include <utility>

namespace darker::game {

enum class player_flag : uint8_t {
  ground_protection = 0x10,
  dead = 0x20,
};

constexpr bool has_player_flag(uint8_t flags, player_flag flag) noexcept {
  return (flags & std::to_underlying(flag)) != 0;
}

constexpr bool player_actions_blocked(uint8_t flags) noexcept {
  /// The native firing and landing gates reject either protection or death
  return has_player_flag(flags, player_flag::ground_protection) || has_player_flag(flags, player_flag::dead);
}

constexpr void set_player_flag(uint8_t &flags, player_flag flag, bool enabled) noexcept {
  /// Change only the selected bit, preserving all unrelated native flags
  auto const mask{std::to_underlying(flag)};
  flags = static_cast<uint8_t>(enabled ? flags | mask : flags & ~mask);
}

} // namespace darker::game

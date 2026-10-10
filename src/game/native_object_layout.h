#pragma once

#include <cstdint>

namespace darker::game {

// target tokens retain DOS record addresses; these are identities, never host pointers
struct native_object_layout {
  using token = uint16_t;
  static unsigned int constexpr record_bytes{112};
  static token constexpr player_projectiles{0xd1a6};
  static token constexpr hostile_projectiles{0xd6e6};
  static token constexpr actors{0xd986};
  static token constexpr player{actors};                                     // actor zero is the player
  static token constexpr no_target{0xffff};

  static constexpr token actor(unsigned int index) noexcept {
    /// Encode an actor index with the original wrapping address arithmetic
    return static_cast<token>(actors + index * record_bytes);
  }
};

struct projectile_limits {
  static unsigned int constexpr player_capacity{12};
  static unsigned int constexpr hostile_capacity{6};
};

// enlarging a pool also requires assigning a non-overlapping token range
static_assert(native_object_layout::player_projectiles + projectile_limits::player_capacity * native_object_layout::record_bytes == native_object_layout::hostile_projectiles);
static_assert(native_object_layout::hostile_projectiles + projectile_limits::hostile_capacity * native_object_layout::record_bytes == native_object_layout::actors);

} // namespace darker::game

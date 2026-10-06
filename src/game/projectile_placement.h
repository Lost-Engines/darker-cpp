#pragma once

#include <array>
#include <cstdint>

namespace darker::game {

struct launch_emitter {
  std::array<std::uint16_t, 3> position{};
  std::array<std::uint8_t, 3> fractions{};
  // Heading, pitch, roll, in native wrapping angle units.
  std::array<std::uint16_t, 3> angles{};
  std::uint16_t speed{0};
  std::uint8_t side_flags{0};
  std::uint8_t definition_strength{0};
};

struct projectile_placement {
  std::array<std::uint16_t, 3> position{};
  std::array<std::uint8_t, 3> fractions{};
  std::array<std::uint16_t, 3> angles{};
  std::uint16_t speed{0};
};

projectile_placement place_projectile(launch_emitter const &emitter);

} // namespace darker::game

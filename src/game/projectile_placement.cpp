#include "game/projectile_placement.h"
#include <bit>
#include "math/sine_table.h"

namespace darker::game {
namespace {

std::int16_t word(int const value) {
  /// Preserve native word overflow between fixed-point products
  return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}

std::int16_t product(std::int16_t const a, std::int16_t const b) {
  /// 2096 doubles a signed full product and takes its high word
  return word((static_cast<std::int32_t>(a) * b) >> 15);
}

} // namespace

projectile_placement place_projectile(launch_emitter const &emitter) {
  /// CB1F–CBCD calculate launch position and copy heading, pitch, roll and speed
  projectile_placement result{.position{emitter.position}, .fractions{emitter.fractions}, .angles{emitter.angles}, .speed{emitter.speed}};
  if(emitter.definition_strength == 0) {
    unsigned int const quadrant{static_cast<unsigned int>(static_cast<std::uint16_t>(emitter.angles[0] + 0x2000) >> 14)};
    std::array<int, 5> constexpr masks{-1, -1, 0, 0, -1};
    int x{((emitter.side_flags & 0x80) != 0 ? -6 : 6) ^ masks[quadrant]};
    int y{36 ^ masks[quadrant + 1]};
    if((quadrant & 1) != 0) { int const old_x{x}; x = y; y = old_x; }
    result.position[0] = static_cast<std::uint16_t>(emitter.position[0] + x);
    result.position[1] = static_cast<std::uint16_t>(emitter.position[1] + ~y);
    result.position[2] = static_cast<std::uint16_t>(emitter.position[2] + 160);
    result.angles[0] = static_cast<std::uint16_t>(emitter.angles[0] + 0x8000);
    result.angles[1] = 0x0abe;
    return result;
  }
  unsigned int const heading{(static_cast<std::uint16_t>(0x8000 - emitter.angles[0]) >> 6) ^ 1023u};
  unsigned int const pitch{static_cast<unsigned int>(emitter.angles[1] >> 6)};
  unsigned int const roll{static_cast<unsigned int>(emitter.angles[2] >> 6) ^ 1023u};
  auto const sine{[](unsigned int const index){ return math::original_sine[index]; }};
  auto const cosine{[&](unsigned int const index){ return sine((index + 256) % 1024); }};
  auto const intermediate{product(cosine(roll), sine(pitch))};
  auto const y{word(product(cosine(heading), intermediate) + product(sine(roll), sine(heading)))};
  auto const x{word(product(sine(heading), intermediate) - product(sine(roll), cosine(heading)))};
  auto const z{product(cosine(roll), cosine(pitch))};
  std::array<int, 3> const displacement{(46 * x) >> 8, (46 * y) >> 8, (-184 * z) >> 8};
  for(std::size_t i{0}; i < 3; ++i) {
    auto const position{static_cast<std::uint32_t>(static_cast<std::int32_t>(emitter.position[i]) * 256 + emitter.fractions[i] + displacement[i])};
    result.position[i] = static_cast<std::uint16_t>(position >> 8);
    result.fractions[i] = static_cast<std::uint8_t>(position);
  }
  return result;
}

} // namespace darker::game

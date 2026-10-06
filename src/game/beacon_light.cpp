#include "game/beacon_light.h"
#include <bit>
#include <stdexcept>

namespace darker::game {
namespace {

constexpr auto beacon_lookup() {
  /// Reproduce 0396's wrapping nine-cell lookup, including the out-of-map sentinel region
  std::array<std::uint8_t, 256> result{};
  std::uint8_t index{0xbd};
  std::uint8_t value{0xb8};
  do {
    value = static_cast<std::uint8_t>(value + 9);
    for(int count{0}; count < 9; ++count) result[index++] = value;
  } while(value != 0xbd);
  return result;
}

auto constexpr lookup{beacon_lookup()};

std::int16_t signed_word(int const value) {
  /// Wrap before interpreting the original signed coordinate products
  return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}

} // namespace

std::uint16_t beacon_light(std::span<city_cell const, 128 * 128> const cells,
  std::array<std::uint16_t, 3> const position, std::array<std::uint8_t, 2> const fractions) {
  /// 8450 samples one lattice cell of type 1, then applies its mutable strength to fixed-point distance attenuation
  if(signed_word(position[2]) >= 0x2d60) return 0;
  auto const x{lookup[position[0] >> 8]};
  auto const y{lookup[position[1] >> 8]};
  if(x >= 128 || y >= 128) return 0;
  auto const cell{cells[y * 128 + x]};
  if(cell.type != 1) return 0;
  auto const dx{signed_word((position[0] - x * 256) * 16 + (fractions[0] >> 4) - 2048)};
  auto const dy{signed_word((position[1] - y * 256) * 16 + (fractions[1] >> 4) - 2048)};
  auto const dz{signed_word(position[2] * 2 - 4800)};
  auto const squared{static_cast<std::uint32_t>(dx * dx) + static_cast<std::uint32_t>(dy * dy) + static_cast<std::uint32_t>(dz * dz)};
  auto denominator{static_cast<std::uint16_t>(squared >> 14)};
  if(denominator < 256) denominator = 256;
  denominator = static_cast<std::uint16_t>(denominator + 1024);
  auto const numerator{cell.state * 0x10101u};
  if(denominator == 0 || numerator / denominator > 65535) {
    throw std::domain_error{"Beacon distance produces an original 16-bit division fault"};
  }
  return static_cast<std::uint16_t>(numerator / denominator);
}

} // namespace darker::game

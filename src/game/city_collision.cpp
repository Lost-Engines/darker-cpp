#include "game/city_collision.h"
#include <bit>
#include <cstddef>
#include <stdexcept>

namespace darker::game {
namespace {

std::uint8_t byte(std::span<std::byte const> const data, std::size_t const offset) {
  /// Collision pointers refer to the bank's trailing world data, never host addresses
  if(offset >= data.size()) throw std::invalid_argument{"Truncated city collision stream"};
  return std::to_integer<std::uint8_t>(data[offset]);
}

int signed_byte(std::span<std::byte const> const data, std::size_t const offset) {
  /// Horizontal endpoints are signed byte offsets from the type's cell anchor
  return std::bit_cast<std::int8_t>(byte(data, offset));
}

} // namespace

std::vector<collision_box> city_collision_boxes(resources::geometry_bank const &bank, unsigned int const type,
  std::uint8_t const state, std::uint8_t const damage_mask, std::uint8_t const column, std::uint8_t const row, std::uint16_t const expansion) {
  /// 607A–60D2 expands the selected model's collision stream into category-bearing axis-aligned boxes
  auto const model{bank.city_model_offset(type, state, damage_mask)};
  auto const &descriptor{bank.city_types()[type - 1]};
  if(descriptor.collision_marker == 255) return {};
  auto const pool{bank.model_pool()};
  unsigned int const pointer{byte(pool, model + 4) | byte(pool, model + 5) << 8};
  if(pointer < 0x8000) throw std::invalid_argument{"City collision pointer precedes its world data"};
  std::size_t cursor{pointer - 0x8000};
  auto const data{bank.world_data()};
  int const column_origin{column * 256 + descriptor.column_fraction};
  int const row_origin{row * 256 + descriptor.row_fraction};
  int const base_height{descriptor.collision_marker * 32};
  std::vector<collision_box> result;
  for(;;) {
    auto opcode{byte(data, cursor++)};
    if(opcode >= 0xf8) return result;
    auto lower_height{base_height};
    if(opcode >= 0xf0) {
      lower_height += ((opcode & 7) << 8) | byte(data, cursor++);
      opcode = byte(data, cursor++);
    }
    int const upper_height{lower_height + ((opcode & 7) << 8) + byte(data, cursor++)};
    result.push_back({
      .minimum{{
        static_cast<std::uint16_t>(column_origin + signed_byte(data, cursor) - expansion),
        static_cast<std::uint16_t>(row_origin + signed_byte(data, cursor + 2) - expansion),
        static_cast<std::uint16_t>(lower_height - expansion),
      }},
      .maximum{{
        static_cast<std::uint16_t>(column_origin + signed_byte(data, cursor + 1) + 1 + expansion),
        static_cast<std::uint16_t>(row_origin + signed_byte(data, cursor + 3) + 1 + expansion),
        static_cast<std::uint16_t>(upper_height + expansion),
      }},
      .category{static_cast<std::uint8_t>(opcode >> 3)},
    });
    cursor += 4;
  }
}

} // namespace darker::game

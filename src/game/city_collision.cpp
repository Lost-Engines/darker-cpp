#include "game/city_collision.h"
#include <bit>
#include <cstddef>
#include <stdexcept>

namespace darker::game {
namespace {

uint8_t byte(std::span<std::byte const> const data, size_t const offset) {
  /// Collision pointers refer to the bank's trailing world data, never host addresses
  if(offset >= data.size()) throw std::invalid_argument{"Truncated city collision stream"};
  return std::to_integer<uint8_t>(data[offset]);
}

int signed_byte(std::span<std::byte const> const data, size_t const offset) {
  /// Horizontal endpoints are signed byte offsets from the type's cell anchor
  return std::bit_cast<int8_t>(byte(data, offset));
}

} // anonymous namespace

std::vector<collision_box> city_collision_boxes(resources::geometry_bank const &bank, unsigned int const type,
  uint8_t const state, uint8_t const damage_mask, uint8_t const column, uint8_t const row, uint16_t const expansion) {
  /// 607A–60D2 expands the selected model's collision stream into category-bearing axis-aligned boxes
  auto const model{bank.city_model_offset(type, state, damage_mask)};
  auto const &descriptor{bank.city_types()[type - 1]};
  if(descriptor.collision_marker == resources::city_type::background_marker) return {};
  auto const pool{bank.model_pool()};
  auto const pointer{static_cast<unsigned int>(byte(pool, model + 4) | byte(pool, model + 5) << 8)};
  unsigned int constexpr world_data_base{0x8000};                              // native bank-relative pointers place collision data above the model pool
  uint8_t constexpr first_terminator{0xf8};                                    // f8–ff end the stream
  uint8_t constexpr first_height_prefix{0xf0};                                 // f0–f7 add an 11-bit lower-height offset before the box
  uint8_t constexpr height_high_bits{7};                                       // low three opcode bits are the height's upper bits
  if(pointer < world_data_base) throw std::invalid_argument{"City collision pointer precedes its world data"};
  size_t cursor{pointer - world_data_base};
  auto const data{bank.world_data()};
  int const column_origin{column * 256 + descriptor.column_fraction};
  int const row_origin{row * 256 + descriptor.row_fraction};
  int const base_height{descriptor.collision_marker * 32};
  std::vector<collision_box> result;
  for(;;) {
    auto opcode{byte(data, cursor++)};
    if(opcode >= first_terminator) return result;
    auto lower_height{base_height};
    if(opcode >= first_height_prefix) {
      lower_height += ((opcode & height_high_bits) << 8) | byte(data, cursor++);
      opcode = byte(data, cursor++);
    }
    int const upper_height{lower_height + ((opcode & height_high_bits) << 8) + byte(data, cursor++)};
    // remaining opcode bits encode the impact category; horizontal inclusive maxima become exclusive with +1
    result.push_back({
      .bounds{
        vec3<uint16_t>{
          static_cast<uint16_t>(column_origin + signed_byte(data, cursor) - expansion),
          static_cast<uint16_t>(row_origin + signed_byte(data, cursor + 2) - expansion),
          static_cast<uint16_t>(lower_height - expansion),
        },
        vec3<uint16_t>{
          static_cast<uint16_t>(column_origin + signed_byte(data, cursor + 1) + 1 + expansion),
          static_cast<uint16_t>(row_origin + signed_byte(data, cursor + 3) + 1 + expansion),
          static_cast<uint16_t>(upper_height + expansion),
        },
      },
      .category{static_cast<collision_category>(opcode >> 3)},
    });
    cursor += 4;
  }
}

} // namespace darker::game

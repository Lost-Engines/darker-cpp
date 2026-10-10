#include "game/city_sweep.h"
#include <algorithm>
#include <bit>
#include "game/city_collision.h"
#include "game/collision_cells.h"
#include "game/collision_sweep.h"
#include "maths/world_coordinates.h"

namespace darker::game {
namespace {

int signed_word(std::uint16_t const value) noexcept {
  /// Preserve the signed position and height arithmetic used by 6527
  return std::bit_cast<std::int16_t>(value);
}

} // namespace

city_collision_result sweep_city(resources::geometry_bank const &bank, std::span<city_cell const, city_map_cell_count> const cells,
  std::uint8_t const damage_mask, maths::world_position const &start, maths::world_position &end,
  std::uint16_t const expansion, std::int16_t const terrain_height) {
  /// 6527 clips a below-ground endpoint, walks map cells and retains the final primitive hit within the first colliding cell
  auto clipped{end};
  int const old_height{signed_word(start.height) >> 1};
  int height{signed_word(end.height) >> 1};
  bool const terrain{height < terrain_height};
  if(terrain && old_height > terrain_height) {
    for(std::size_t axis{0}; axis < 2; ++axis) {
      int const displacement{signed_word(static_cast<std::uint16_t>(end[axis] - start[axis]))};
      clipped[axis] = static_cast<std::uint16_t>(start[axis] + displacement * (terrain_height - old_height) / (height - old_height));
    }
    height = terrain_height;
  }
  auto previous{start};
  previous[2] = static_cast<std::uint16_t>(old_height >> 2);
  clipped.height = static_cast<std::uint16_t>(height >> 2);
  for(auto const cell : swept_collision_cells(start.column, start.row, clipped.column, clipped.row)) {
    auto const object{cells[cell.row * city_map_size.column + cell.column]};
    if(object.type == 0) continue;
    auto const model{bank.city_model_offset(object.type, object.state, damage_mask)};
    auto const &descriptor{bank.city_types()[object.type - 1]};
    if(descriptor.collision_marker == resources::city_type::background_marker) continue;
    auto const header{bank.header_at(model)};
    auto const ceiling{static_cast<std::uint16_t>(header.extent - header.height - (descriptor.collision_marker * 256 + expansion)) >> 1};
    if(std::min(signed_word(previous[2]), signed_word(clipped.height)) >= ceiling) continue;
    city_collision_result result{};
    for(auto const &box : city_collision_boxes(bank, object.type, object.state, damage_mask, cell.column, cell.row, expansion)) {
      if(sweep_collision_box(box, previous, clipped)) {
        result = {
          .contact{city_contact::building},
          .column{cell.column},
          .row{cell.row},
          .category{box.category}
        };
      }
    }
    if(result.contact == city_contact::building) {
      end = clipped;
      end.height = static_cast<std::uint16_t>(end.height << 3);
      return result;
    }
  }
  if(terrain) {
    end = clipped;
    end.height = static_cast<std::uint16_t>(end.height << 3);
    return {
      .contact{city_contact::terrain}
    };
  }
  return {};
}

} // namespace darker::game

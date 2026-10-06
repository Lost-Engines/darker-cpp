#include "graphics/city_scene.h"
#include <algorithm>
#include <bit>
#include <stdexcept>

namespace darker::graphics {

namespace {

std::int16_t word(int const value) noexcept {
  /// Retain the culler's signed word arithmetic
  return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}

int magnitude(std::uint16_t const value) noexcept {
  /// CWD/XOR complements negative coordinates rather than taking their mathematical absolute value
  return value & 0x8000 ? static_cast<std::uint16_t>(~value) : value;
}

} // namespace

std::optional<city_draw_item> place_city_cell(resources::geometry_bank const &bank, game::city_cell const cell, std::uint16_t const index,
  std::uint8_t const damage_mask, camera_basis const &basis, camera_position const camera) {
  /// 2A1A selects the linked model, places its origin, chooses its drawing path and applies the extent cull
  if(!cell.type) return std::nullopt;
  if(index >= 128 * 128) throw std::invalid_argument{"City model position exceeds the map"};
  auto const offset{bank.city_model_offset(cell.type, cell.state, damage_mask)};
  auto const type{bank.city_types()[cell.type - 1]};
  auto const header{bank.header_at(offset)};
  bool const background{type.collision_marker == 255};
  auto placement{place_model(basis, camera, {
    .column{static_cast<std::uint16_t>((index % 128) * 256 + type.column_fraction)},
    .row{static_cast<std::uint16_t>((index / 128) * 256 + type.row_fraction)},
    .height{word(header.height - (background ? 0 : type.collision_marker * 256))},
  })};
  if(!background) placement.sorting_distance = static_cast<std::uint16_t>(placement.sorting_distance + header.extent);
  auto const diameter{word(header.extent * 2)};
  auto bound{word(placement.depth.whole - 32)};
  bool const near{bound <= diameter};
  bool const force_flat{!near && static_cast<std::uint8_t>(static_cast<std::uint16_t>(bound) >> 8) >= header.flat_distance};
  if(near && static_cast<std::uint16_t>(bound) >= static_cast<std::uint16_t>(diameter)) {
    if(word(bound + diameter) < 0) return std::nullopt;
    bound = -32;
  }
  bound = word(bound + word(diameter + 32));
  if(magnitude(placement.horizontal.whole) >= bound || magnitude(placement.vertical.whole) >= bound) return std::nullopt;
  return city_draw_item{.cell{index}, .model_offset{offset}, .placement{placement},
    .path{near ? model_path::near_clipped : model_path::direct}, .background{background}, .force_flat{force_flat}};
}

void order_city_models(std::vector<city_draw_item> &items) {
  /// Background records use the original LIFO list; ordinary records use descending distance with insertion-stable ties
  auto const first_sorted{std::stable_partition(items.begin(), items.end(), [](auto const &item){ return item.background; })};
  std::reverse(items.begin(), first_sorted);
  std::stable_sort(first_sorted, items.end(), [](auto const &left, auto const &right){
    return left.placement.sorting_distance > right.placement.sorting_distance;
  });
}

void collect_city_cells(std::span<game::city_cell const, 128 * 128> const cells, std::uint8_t const column, std::uint8_t const row,
  camera_angles const angles, unsigned int const radius, std::vector<std::uint16_t> &output) {
  /// 26EE traverses circular row spans, selecting the heading half unless pitch requires the full circle
  if(column >= 128 || row >= 128 || radius < 2 || radius > 32) throw std::invalid_argument{"City scan position or radius exceeds its supported bounds"};
  output.clear();
  auto const heading_phase{static_cast<std::uint16_t>(angles.heading + 15) >> 6};
  auto const pitch_phase{static_cast<std::uint16_t>(angles.pitch + 15) >> 6};
  auto const quadrant{((heading_phase >> 7) - 1) & 6};
  auto const pitch_quadrant{(pitch_phase >> 7) & 3};
  bool const full{pitch_quadrant == 1 || pitch_quadrant == 2};
  auto const span{[&](int const y, int const left, int const right){
    if(y < 0 || y >= 128) return;
    for(int x{std::max(0, left)}; x <= std::min(127, right); ++x) {
      auto const index{static_cast<std::uint16_t>(y * 128 + x)};
      if(cells[index].type) output.push_back(index);
    }
  }};
  auto const rows{[&](int const width, int const offset){
    if(full) {
      span(row + offset, column - width, column + width);
      span(row - offset, column - width, column + width);
    } else if(quadrant == 0 || quadrant == 4) {
      int const left{quadrant == 0 ? column - width : column};
      int const right{quadrant == 0 ? column : column + width};
      span(row - offset, left, right);
      span(row + offset, left, right);
    } else {
      span(quadrant == 2 ? row + offset : row - offset, column - width, column + width);
    }
  }};
  unsigned int width{0};
  unsigned int offset{radius};
  std::uint8_t error{static_cast<std::uint8_t>(radius / 2)};
  do {
    --offset;
    error = static_cast<std::uint8_t>(error - offset);
    unsigned int sum{0};
    do {
      ++width;
      sum = error + width;
      error = static_cast<std::uint8_t>(sum);
    } while(sum < 256);
    rows(static_cast<int>(width), static_cast<int>(offset));
  } while(width < offset);
  while(offset > 0) {
    --offset;
    bool const borrow{error < offset};
    error = static_cast<std::uint8_t>(error - offset);
    if(borrow) {
      ++width;
      error = static_cast<std::uint8_t>(error + width);
    } else if(offset == 0) break;
    rows(static_cast<int>(width), static_cast<int>(offset));
  }
  span(row, column - static_cast<int>(width), column + static_cast<int>(width));
}

std::size_t city_renderer::draw(framework::render::indexed_cockpit_framebuffer &target, resources::geometry_bank const &bank,
  std::span<game::city_cell const, 128 * 128> const cells, city_view const view, std::uint8_t const damage_mask,
  distance_shading const &lighting, model_animation animation) {
  /// Assemble and draw the ordinary city path; underground visibility propagation and dynamic objects remain separate
  auto const basis{make_camera_basis(view.angles)};
  camera_position const camera{
    .column{static_cast<std::uint16_t>(view.column * 4 + (view.column_fraction >> 6))},
    .row{static_cast<std::uint16_t>(view.row * 4 + (view.row_fraction >> 6))}, .altitude{view.altitude},
  };
  collect_city_cells(cells, static_cast<std::uint8_t>(view.column >> 8), static_cast<std::uint8_t>(view.row >> 8), view.angles, view.radius, candidates);
  items.clear();
  for(auto const index : candidates) {
    if(auto item{place_city_cell(bank, cells[index], index, damage_mask, basis, camera)}) items.push_back(*item);
  }
  order_city_models(items);
  for(auto const &item : items) {
    projection_parameters const projection{
      .axes{basis}, .horizontal{item.placement.horizontal}, .vertical{item.placement.vertical},
      .depth{item.placement.depth}, .origin{view.origin},
    };
    std::uint8_t light{255};
    if(view.beacon_lighting) {
      // 2D85 samples the nearest lattice cell's state without the charging routine's type check
      auto const column{((item.cell % 128 + 4) / 9) * 9};
      auto const row{((item.cell / 128 + 4) / 9) * 9};
      light = cells[row * 128 + column].state;
    }
    auto const colours{lighting.colours(item.placement.depth.whole, item.path, light)};
    animation.cell_state = cells[item.cell].state;
    draw_flat_model(target, bank.model_pool(), item.model_offset, projection, colours, view.bottom, item.path, animation);
  }
  return items.size();
}

} // namespace darker::graphics

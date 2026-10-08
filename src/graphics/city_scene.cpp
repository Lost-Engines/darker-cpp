#include "graphics/city_scene.h"
#include <algorithm>
#include <bit>
#include <ranges>
#include <stdexcept>
#include "graphics/particles.h"
#include "graphics/tunnel_visibility.h"

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

std::optional<city_draw_item> classify_model(model_placement const placement, resources::model_header const header) {
  /// Apply the shared extent cull and direct/near model-path selection
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
  return city_draw_item{.placement{placement},
    .path{near ? model_path::near_clipped : model_path::direct}, .force_flat{force_flat}};
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
  auto item{classify_model(placement, header)};
  if(item) {
    item->cell = index;
    item->model_offset = offset;
    item->background = background;
  }
  return item;
}

bool within_object_window(city_view const &view, std::array<uint16_t,3> const &position) noexcept {
  /// 26EE patches 2F35's byte window before word-sized projection can alias distant objects into nearby space
  auto const width{view.radius * 2 - 1};
  auto const column{static_cast<uint8_t>((position[0] >> 8) - (view.column >> 8) + view.radius - 1)};
  auto const row{static_cast<uint8_t>((position[1] >> 8) - (view.row >> 8) + view.radius - 1)};
  return column < width && row < width;
}

std::optional<city_draw_item> place_scene_object(resources::geometry_bank const &bank, scene_object const &object,
  camera_basis const &basis, camera_position camera, bool const underground) {
  /// 2F35 preserves the object's fractional origin before the same model extent cull as city geometry
  camera.column = static_cast<std::uint16_t>(camera.column - (object.pose.fractions[0] >> 6));
  camera.row = static_cast<std::uint16_t>(camera.row - (object.pose.fractions[1] >> 6));
  auto placement{place_model(basis, camera, {.column{object.pose.position[0]}, .row{object.pose.position[1]}, .height{word(-object.pose.position[2])}})};
  auto const header{bank.header_at(object.model_offset)};
  // BC94 patches 2EDC from ADD to SUB for underground moving objects.
  placement.sorting_distance = static_cast<std::uint16_t>(placement.sorting_distance + (underground ? -header.extent : header.extent));
  auto item{classify_model(placement, header)};
  if(item) {
    item->model_offset = object.model_offset;
    item->orientation = orient_model(basis, {.heading{object.pose.angles[0]}, .pitch{object.pose.angles[1]}, .roll{object.pose.angles[2]}});
    item->object_light = object.light;
  }
  return item;
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
  if(radius < 2 || radius > 32) throw std::invalid_argument{"City scan radius exceeds its supported bounds"};
  output.clear();
  auto const heading_phase{static_cast<std::uint16_t>(angles.heading + 15) >> 6};
  auto const pitch_phase{static_cast<std::uint16_t>(angles.pitch + 15) >> 6};
  auto const quadrant{((heading_phase >> 7) - 1) & 6};
  auto const pitch_quadrant{(pitch_phase >> 7) & 3};
  bool const full{pitch_quadrant == 1 || pitch_quadrant == 2};
  auto const span{[&](int const y, int const left, int const right){
    auto const wrapped_row{static_cast<std::uint8_t>(y)};
    if(wrapped_row >= 128) return;
    for(int x{left}; x <= right; ++x) {
      auto const wrapped_column{static_cast<std::uint8_t>(x)};
      if(wrapped_column >= 128) continue;
      auto const index{static_cast<std::uint16_t>(wrapped_row * 128 + wrapped_column)};
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
  distance_shading const &lighting, model_animation animation, std::span<scene_object const> const objects, particle_scene const *const particles) {
  /// Assemble the selected native visibility path before sorting world geometry, actors and particle effects
  auto const basis{make_camera_basis(view.angles)};
  camera_position const camera{
    .column{static_cast<std::uint16_t>(view.column * 4 + (view.column_fraction >> 6))},
    .row{static_cast<std::uint16_t>(view.row * 4 + (view.row_fraction >> 6))}, .altitude{view.altitude},
  };
  items.clear();
  auto const place{[&](uint16_t const index){
    if(auto item{place_city_cell(bank,cells[index],index,damage_mask,basis,camera)}) {
      items.push_back(*item);
      return true;
    }
    return false;
  }};
  if(view.underground) {
    visit_tunnel_cells(cells,static_cast<uint8_t>(view.column >> 8),static_cast<uint8_t>(view.row >> 8),tunnel_visibility,place);
  } else {
    tunnel_visibility.fill(0);
    collect_city_cells(cells,static_cast<uint8_t>(view.column >> 8),static_cast<uint8_t>(view.row >> 8),view.angles,view.radius,candidates);
    for(auto const index : candidates) place(index);
  }
  for(auto const &object : objects) {
    if(!within_object_window(view, object.pose.position)) continue;
    if(auto item{place_scene_object(bank, object, basis, camera, view.underground)}) items.push_back(*item);
  }
  if(particles) {
    auto const append{[&](auto const &emitters){
      for(auto const &emitter : emitters | std::views::reverse) {
        auto const phase{game::particle_phase(emitter, particles->clock)};
        if(!phase) continue;
        // 2F35 admits effects through the same wrapping coordinate window as moving objects.
        if(!within_object_window(view, emitter.position)) continue;
        auto const placement{place_model(basis, camera, {.column{emitter.position[0]}, .row{emitter.position[1]}, .height{word(-emitter.position[2])}})};
        items.push_back({.placement{placement}, .emitter{&emitter}, .phase{*phase}});
      }
    }};
    append(particles->effects.trails);
    append(particles->effects.emitters);
  }
  order_city_models(items);
  for(auto const &item : items) {
    if(item.emitter) {
      for(auto const point : project_emitter(*item.emitter, item.placement, basis, view.origin)) {
        draw_particle(target, particles->sheet, point, item.phase, view.bottom);
      }
      continue;
    }
    projection_parameters const projection{
      .axes{item.orientation.value_or(basis)}, .horizontal{item.placement.horizontal}, .vertical{item.placement.vertical},
      .depth{item.placement.depth}, .origin{view.origin},
    };
    std::uint8_t light{item.orientation ? item.object_light : std::uint8_t{255}};
    if(view.beacon_lighting && !item.orientation) {
      // 2D85 samples the nearest lattice cell's state without the charging routine's type check
      auto const column{((item.cell % 128 + 4) / 9) * 9};
      auto const row{((item.cell / 128 + 4) / 9) * 9};
      light = cells[row * 128 + column].state;
    }
    auto const colours{lighting.colours(item.placement.depth.whole, item.path, light)};
    animation.cell_state = item.orientation ? 0 : cells[item.cell].state;
    draw_model(target, bank.model_pool(), item.model_offset, projection, colours, view.bottom, item.path, animation,
      view.gouraud && !item.force_flat ? model_shading::gouraud : model_shading::flat);
  }
  return items.size();
}

} // namespace darker::graphics

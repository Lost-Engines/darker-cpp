#include "graphics/city_scene.h"
#include "graphics/screen_layout.h"
#include <algorithm>
#include <bit>
#include <ranges>
#include <stdexcept>
#include "game/beacon_light.h"
#include "graphics/near_clip.h"
#include "graphics/particles.h"
#include "graphics/tunnel_visibility.h"
#include "maths/world_coordinates.h"

namespace darker::graphics {

namespace {

render_geometry::coordinate word(render_geometry::accumulator const value) noexcept {
  /// Retain the culler's signed word arithmetic
  return render_geometry::wrap_coordinate(value);
}

int magnitude(render_geometry::coordinate_bits const value) noexcept {
  /// CWD/XOR complements negative coordinates rather than taking their mathematical absolute value
  return value & (render_geometry::coordinate_bits{1} << (render_geometry::whole_bits - 1)) ? static_cast<render_geometry::coordinate_bits>(~value) : value;
}

std::optional<city_draw_item> classify_model(model_placement const placement, resources::model_header const header) {
  /// Apply the shared extent cull and direct/near model-path selection
  auto const diameter{word(header.extent * 2)};
  auto bound{word(placement.depth.whole - render_geometry::near_depth)};
  bool const near{bound <= diameter};
  bool const force_flat{!near && static_cast<uint8_t>(static_cast<render_geometry::coordinate_bits>(bound) >> 8) >= header.flat_distance};
  if(near && static_cast<render_geometry::coordinate_bits>(bound) >= static_cast<render_geometry::coordinate_bits>(diameter)) {
    if(word(bound + diameter) < 0) return std::nullopt;
    bound = -render_geometry::near_depth;
  }
  bound = word(bound + word(diameter + render_geometry::near_depth));
  if(magnitude(placement.horizontal.whole) >= bound || magnitude(placement.vertical.whole) >= bound) return std::nullopt;
  return city_draw_item{
    .placement{placement},
    .path{near ? model_path::near_clipped : model_path::direct},
    .force_flat{force_flat}
  };
}

} // anonymous namespace

std::optional<city_draw_item> place_city_cell(resources::geometry_bank const &bank, game::city_cell const cell, uint16_t const index,
  uint8_t const damage_mask, camera_basis const &basis, camera_position const camera) {
  /// 2A1A selects the linked model, places its origin, chooses its drawing path and applies the extent cull
  if(!cell.type) return std::nullopt;
  if(index >= game::city_map_cell_count) throw std::invalid_argument{"City model position exceeds the map"};
  auto const offset{bank.city_model_offset(cell.type, cell.state, damage_mask)};
  auto const type{bank.city_types()[cell.type - 1]};
  auto const header{bank.header_at(offset)};
  bool const background{type.collision_marker == resources::city_type::background_marker};
  auto placement{place_model(basis, camera, {
    .column{static_cast<uint16_t>((index % game::city_map_size.column) * maths::world_format::units_per_cell + type.column_fraction)},
    .row{static_cast<uint16_t>((index / game::city_map_size.column) * maths::world_format::units_per_cell + type.row_fraction)},
    .height{word(header.height - (background ? 0 : type.collision_marker * 256))},
  })};
  if(!background) placement.sorting_distance = static_cast<render_geometry::sorting_distance>(placement.sorting_distance + header.extent);
  auto item{classify_model(placement, header)};
  if(item) {
    item->cell = index;
    item->model_offset = offset;
    item->background = background;
  }
  return item;
}

bool within_object_window(city_view const &view, maths::world_position const &position) noexcept {
  /// 26EE patches 2F35's byte window before word-sized projection can alias distant objects into nearby space
  auto const width{view.radius * 2 - 1};
  auto const column{static_cast<uint8_t>((position.column >> maths::world_format::cell_fraction_bits) - (view.column >> maths::world_format::cell_fraction_bits) + view.radius - 1)};
  auto const row{static_cast<uint8_t>((position.row >> maths::world_format::cell_fraction_bits) - (view.row >> maths::world_format::cell_fraction_bits) + view.radius - 1)};
  return column < width && row < width;
}

std::optional<city_draw_item> place_scene_object(resources::geometry_bank const &bank, scene_object const &object,
  camera_basis const &basis, camera_position camera, bool const underground) {
  /// 2F35 preserves the object's fractional origin before the same model extent cull as city geometry
  // retain two sub-word bits in projection units; subtracting from the camera adds the object's fractional displacement
  camera.column = static_cast<render_geometry::coordinate_bits>(camera.column - (object.pose.fractions.column >> render_geometry::camera_fraction_shift));
  camera.row = static_cast<render_geometry::coordinate_bits>(camera.row - (object.pose.fractions.row >> render_geometry::camera_fraction_shift));
  auto placement{place_model(basis, camera, {
    .column{object.pose.position.column},
    .row{object.pose.position.row},
    .height{word(-object.pose.position.height)}
  })};
  auto const header{bank.header_at(object.model_offset)};
  // BC94 patches 2EDC from ADD to SUB for underground moving objects
  placement.sorting_distance = static_cast<render_geometry::sorting_distance>(placement.sorting_distance + (underground ? -header.extent : header.extent));
  auto item{classify_model(placement, header)};
  if(item) {
    item->model_offset = object.model_offset;
    item->orientation = orient_model(basis, object.pose.angles);
    item->object_light = object.light;
    item->draw_record = object.native_id ? static_cast<uint16_t>(object.native_id + 14) : 0;
    item->distant_point = item->force_flat
      && static_cast<uint8_t>(static_cast<render_geometry::coordinate_bits>(placement.depth.whole - render_geometry::near_depth) >> 8) >= header.point_distance;
  }
  return item;
}

std::optional<screen_vertex> project_distant_object(model_placement const placement, screen_vertex const &origin, int const bottom, uint8_t const residue) {
  /// 2D32 consumes traversal AL for the Y divide, then projected Y's low byte for the X divide
  auto point{project_vertex({
    .horizontal{0},
    .vertical{word(placement.vertical.whole) * render_geometry::fraction_scale + residue},
    .depth{word(placement.depth.whole) * render_geometry::fraction_scale}
  }, origin)};
  if(point.y < 0 || point.y >= bottom) return std::nullopt;
  point.x = project_vertex({
    .horizontal{word(placement.horizontal.whole) * render_geometry::fraction_scale + static_cast<uint8_t>(point.y)},
    .vertical{0},
    .depth{word(placement.depth.whole) * render_geometry::fraction_scale}
  }, origin).x;
  if(point.x < 0 || point.x >= display_layout::width || point.y < 0 || point.y >= bottom) return std::nullopt;
  return point;
}

void order_city_models(std::vector<city_draw_item> &items) {
  /// Background records use the original LIFO list; ordinary records use descending distance with insertion-stable ties
  // 2B09 inserts a binary tree. At 2C49, AL is zero after a left descent,
  // or the current record's low byte when returning from its farther subtree
  auto const absent{items.size()};
  struct branches {
    size_t farther, nearer;
  };
  std::vector<branches> tree(items.size(), {absent, absent});
  size_t root{absent};
  for(size_t index{0}; index < items.size(); ++index) {
    items[index].projection_residue = 0;
    if(items[index].background) continue;
    if(root == absent) {
      root = index;
      continue;
    }
    auto parent{root};
    for(;;) {
      bool const farther{items[index].placement.sorting_distance > items[parent].placement.sorting_distance};
      auto &branch{farther ? tree[parent].farther : tree[parent].nearer};
      if(branch == absent) {
        branch = index;
        if(farther) items[parent].projection_residue = static_cast<uint8_t>(items[parent].draw_record);
        break;
      }
      parent = branch;
    }
  }
  auto const first_sorted{std::stable_partition(items.begin(), items.end(), [](auto const &item){
    return item.background;
  })};
  std::reverse(items.begin(), first_sorted);
  std::stable_sort(first_sorted, items.end(), [](auto const &left, auto const &right){
    return left.placement.sorting_distance > right.placement.sorting_distance;
  });
}

void collect_city_cells(std::span<game::city_cell const, game::city_map_cell_count> const cells, uint8_t const column, uint8_t const row,
  camera_angles const angles, unsigned int const radius, std::vector<uint16_t> &output) {
  /// 26EE traverses circular row spans, selecting the heading half unless pitch requires the full circle
  if(radius < scene_limits::minimum_scan_radius_cells || radius > scene_limits::maximum_scan_radius_cells) throw std::invalid_argument{"City scan radius exceeds its supported bounds"};
  output.clear();
  // angles wrap at 65536 units per turn; discard six bits to use the 1024-step sine-table phase
  // +15 is the native quantisation bias, not round-to-nearest (+32); its original rationale is unknown
  auto const heading_phase{static_cast<uint16_t>(angles.heading + 15) >> 6};
  auto const pitch_phase{static_cast<uint16_t>(angles.pitch + 15) >> 6};
  // 128 phase steps make a 45-degree octant; pair heading octants into four half-map scan directions
  // 0/2/4/6 are native word-table byte offsets: decreasing columns, increasing rows, increasing columns, decreasing rows
  auto const scan_direction_offset{((heading_phase >> 7) - 1) & 6};
  // fold pitch modulo half a turn: octants 1/2 select steep views towards either vertical pole
  auto const folded_pitch_octant{(pitch_phase >> 7) & 3};
  bool const scan_full_circle{folded_pitch_octant == 1 || folded_pitch_octant == 2};
  auto const span{[&](int const y, int const left, int const right){
    auto const wrapped_row{static_cast<uint8_t>(y)};
    if(wrapped_row >= game::city_map_size.row) return;
    for(int x{left}; x <= right; ++x) {
      auto const wrapped_column{static_cast<uint8_t>(x)};
      if(wrapped_column >= game::city_map_size.column) continue;
      auto const index{static_cast<uint16_t>(wrapped_row * game::city_map_size.column + wrapped_column)};
      if(cells[index].type) output.push_back(index);
    }
  }};
  auto const rows{[&](int const width, int const offset){
    if(scan_full_circle) {
      span(row + offset, column - width, column + width);
      span(row - offset, column - width, column + width);
    } else if(scan_direction_offset == 0 || scan_direction_offset == 4) {
      int const left{scan_direction_offset == 0 ? column - width : column};
      int const right{scan_direction_offset == 0 ? column : column + width};
      span(row - offset, left, right);
      span(row + offset, left, right);
    } else {
      span(scan_direction_offset == 2 ? row + offset : row - offset, column - width, column + width);
    }
  }};
  unsigned int width{0};
  unsigned int offset{radius};
  // the circle stepper uses byte subtraction and carry/borrow, not an unbounded signed error accumulator
  unsigned int constexpr byte_modulus{256};
  uint8_t error{static_cast<uint8_t>(radius / 2)};
  do {
    --offset;
    error = static_cast<uint8_t>(error - offset);
    unsigned int sum{0};
    do {
      ++width;
      sum = error + width;
      error = static_cast<uint8_t>(sum);
    } while(sum < byte_modulus);
    rows(static_cast<int>(width), static_cast<int>(offset));
  } while(width < offset);
  while(offset > 0) {
    --offset;
    bool const borrow{error < offset};
    error = static_cast<uint8_t>(error - offset);
    if(borrow) {
      ++width;
      error = static_cast<uint8_t>(error + width);
    } else if(offset == 0) break;
    rows(static_cast<int>(width), static_cast<int>(offset));
  }
  // the native traversal always includes the complete centre row, even for a half-map scan
  span(row, column - static_cast<int>(width), column + static_cast<int>(width));
}

size_t city_renderer::draw(framework::render::indexed_cockpit_framebuffer &target, resources::geometry_bank const &bank,
  std::span<game::city_cell const, game::city_map_cell_count> const cells, city_view const view, uint8_t const damage_mask,
  distance_shading const &lighting, model_animation animation, std::span<scene_object const> const objects, particle_scene const *const particles) {
  /// Assemble the selected native visibility path before sorting world geometry, actors and particle effects
  auto const basis{make_camera_basis(view.angles)};
  // position words hold 1/256-cell units; the extra fraction byte subdivides those units by another 256
  // projection needs 1/1024-cell units: multiply the word by four and retain the fraction's top two bits
  // narrowing deliberately wraps at 16 bits, matching the native camera-origin subtractors
  camera_position const camera{
    .column{static_cast<render_geometry::coordinate_bits>(view.column * render_geometry::world_to_camera_scale + (view.column_fraction >> render_geometry::camera_fraction_shift))},
    .row{static_cast<render_geometry::coordinate_bits>(view.row * render_geometry::world_to_camera_scale + (view.row_fraction >> render_geometry::camera_fraction_shift))},
    .altitude{view.altitude},
  };
  items.clear();
  auto const place{[&](uint16_t const index){
    if(auto item{place_city_cell(bank, cells[index], index, damage_mask, basis, camera)}) {
      items.push_back(*item);
      return true;
    }
    return false;
  }};
  // visibility scans address whole map cells: the high byte discards the word's eight sub-cell bits
  auto const camera_column{static_cast<uint8_t>(view.column >> maths::world_format::cell_fraction_bits)};
  auto const camera_row{static_cast<uint8_t>(view.row >> maths::world_format::cell_fraction_bits)};
  if(view.underground && !view.unrestricted_visibility) {
    visit_tunnel_cells(cells, camera_column, camera_row, tunnel_visibility, place);
  } else {
    tunnel_visibility.fill(0);
    collect_city_cells(cells, camera_column, camera_row, view.angles, view.radius, candidates);
    for(auto const index : candidates) place(index);
  }
  if(particles) {
    auto const append{[&](auto const &emitters){
      for(auto const &emitter : emitters | std::views::reverse) {
        auto const phase{game::particle_phase(emitter, particles->clock)};
        if(!phase) continue;
        // 2F35 admits effects through the same wrapping coordinate window as moving objects
        if(!within_object_window(view, emitter.position)) continue;
        auto const placement{place_model(basis, camera, {
          .column{emitter.position.column},
          .row{emitter.position.row},
          .height{word(-emitter.position.height)}
        })};
        items.push_back({
          .placement{placement},
          .emitter{&emitter},
          .phase{*phase}
        });
      }
    }};
    append(particles->effects.trails);
    append(particles->effects.emitters);
  }
  // 2BC1 scans the city, 6BF3 adds particles, then 2BD0/2BEA adds craft and projectiles
  for(auto const &object : objects) {
    if(!within_object_window(view, object.pose.position)) continue;
    if(auto item{place_scene_object(bank, object, basis, camera, view.underground)}) items.push_back(*item);
  }
  order_city_models(items);
  for(auto const &item : items) {
    if(item.emitter) {
      for(auto const point : project_emitter(*item.emitter, item.placement, basis, view.origin)) {
        draw_particle(target, particles->sheet, point, item.phase, view.bottom);
      }
      continue;
    }
    if(item.distant_point) {
      if(auto const point{project_distant_object(item.placement, view.origin, view.bottom, item.projection_residue)}) {
        auto const colour{bank.header_at(item.model_offset).point_colour};
        // 2D71 reuses the last mesh's shade table; it does not calculate distance or fade again
        unsigned int constexpr palette_ramp_mask{model_colours::ramp_mask};    // upper three bits select one of eight 32-colour ramps
        unsigned int constexpr palette_shade_mask{model_colours::shade_mask};  // lower five bits select the shade within that ramp
        auto const shaded_colour{(colour & palette_ramp_mask) + retained_colours.shades.at(colour & palette_shade_mask)};
        target.pixels[static_cast<size_t>(point->y) * target.width + point->x] = static_cast<uint8_t>(shaded_colour);
      }
      continue;
    }
    projection_parameters const projection{
      .axes{item.orientation.value_or(basis)},
      .horizontal{item.placement.horizontal},
      .vertical{item.placement.vertical},
      .depth{item.placement.depth},
      .origin{screen_vertex{view.origin}},
    };
    uint8_t light{item.orientation ? item.object_light : uint8_t{255}};
    if(view.beacon_lighting && !item.orientation) {
      // 2D85 samples the nearest lattice cell's state without the charging routine's type check
      auto const column{((item.cell % game::city_map_size.column + game::beacon_spacing_cells / 2) / game::beacon_spacing_cells) * game::beacon_spacing_cells};
      auto const row{((item.cell / game::city_map_size.column + game::beacon_spacing_cells / 2) / game::beacon_spacing_cells) * game::beacon_spacing_cells};
      light = cells[row * game::city_map_size.column + column].state;
    }
    auto const colours{lighting.colours(item.placement.depth.whole, item.path, light)};
    retained_colours = colours;
    animation.cell_state = item.orientation ? 0 : cells[item.cell].state;
    draw_model(target, bank.model_pool(), item.model_offset, projection, colours, view.bottom, item.path, animation,
      view.gouraud && !item.force_flat ? model_shading::gouraud : model_shading::flat);
  }
  return items.size();
}

} // namespace darker::graphics

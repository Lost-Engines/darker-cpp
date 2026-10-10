#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>
#include "game/city_map.h"
#include "game/effects.h"
#include "game/object_pose.h"
#include "graphics/camera.h"
#include "graphics/city_visibility.h"
#include "graphics/model_lighting.h"
#include "maths/world_coordinates.h"
#include "resources/geometry_bank.h"

namespace darker::graphics {

struct city_draw_item {
  uint16_t cell{0};
  size_t model_offset{0};
  model_placement placement{};
  model_path path{model_path::direct};
  bool background{false};
  bool force_flat{false};
  bool distant_point{false};
  std::optional<camera_basis> orientation{};
  uint8_t object_light{255};
  game::particle_emitter const *emitter{nullptr};
  uint8_t phase{0};
  uint16_t draw_record{0};
  uint8_t projection_residue{0};
};

std::optional<city_draw_item> place_city_cell(resources::geometry_bank const &bank, game::city_cell cell, uint16_t index,
  uint8_t damage_mask, camera_basis const &basis, camera_position camera);
void order_city_models(std::vector<city_draw_item> &items);

struct scene_object {
  size_t model_offset;
  game::object_pose pose;
  uint8_t light{255};
  uint16_t native_id{0};
};

std::optional<city_draw_item> place_scene_object(resources::geometry_bank const &bank, scene_object const &object,
  camera_basis const &basis, camera_position camera, bool underground = false);
std::optional<screen_vertex> project_distant_object(model_placement placement, screen_vertex const &origin, raster_viewport viewport, uint8_t residue = 0);

struct city_view {
  maths::world_format::position_component column{0};                                                          // original 1/256-cell position words
  maths::world_format::position_component row{0};
  maths::world_format::fraction_component column_fraction{0};
  maths::world_format::fraction_component row_fraction{0};
  render_geometry::coordinate altitude{0};
  camera_angles angles{};
  screen_vertex origin{display_layout::centre_x, cockpit_view_layout::caero_centre_y};
  unsigned int radius{scene_limits::surface_radius_cells};                                                     // BCE3–BCFB: Delphi/Halon radius (underground uses eight)
  raster_viewport viewport{
    .bottom{cockpit_view_layout::caero_height},
  };
  bool beacon_lighting{true};
  bool gouraud{true};
  bool underground{false};
  bool unrestricted_visibility{false};                                         // debug cameras can leave the connected tunnel cells
};

bool within_object_window(city_view const &view, maths::world_position const &position) noexcept;

struct particle_scene {
  game::effect_system const &effects;
  framework::render::const_indexed_surface sheet;
  uint16_t clock;
};

class city_renderer {
private:
  std::vector<uint16_t> candidates;
  std::vector<city_draw_item> items;
  std::array<uint8_t, game::city_map_cell_count> tunnel_visibility{};
  model_colours retained_colours{};

public:
  size_t draw(framework::render::indexed_surface target, resources::geometry_bank const &bank,
    std::span<game::city_cell const, game::city_map_cell_count> cells, city_view view, uint8_t damage_mask,
    distance_shading const &lighting, model_animation animation, std::span<scene_object const> objects = {}, particle_scene const *particles = nullptr);
};

} // namespace darker::graphics

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
#include "graphics/model_lighting.h"
#include "resources/geometry_bank.h"

namespace darker::graphics {

struct city_draw_item {
  std::uint16_t cell{0};
  std::size_t model_offset{0};
  model_placement placement{};
  model_path path{model_path::direct};
  bool background{false};
  bool force_flat{false};
  std::optional<camera_basis> orientation{};
  std::uint8_t object_light{255};
  game::particle_emitter const *emitter{nullptr};
  uint8_t phase{0};
};

std::optional<city_draw_item> place_city_cell(resources::geometry_bank const &bank, game::city_cell cell, std::uint16_t index,
  std::uint8_t damage_mask, camera_basis const &basis, camera_position camera);
void order_city_models(std::vector<city_draw_item> &items);

struct scene_object {
  std::size_t model_offset;
  game::object_pose pose;
  std::uint8_t light{255};
};

std::optional<city_draw_item> place_scene_object(resources::geometry_bank const &bank, scene_object const &object,
  camera_basis const &basis, camera_position camera);

struct city_view {
  std::uint16_t column{0};                                                     // original 1/256-cell position words
  std::uint16_t row{0};
  std::uint8_t column_fraction{0};
  std::uint8_t row_fraction{0};
  std::int16_t altitude{0};
  camera_angles angles{};
  screen_vertex origin{.x{160}, .y{84}};
  unsigned int radius{15};                                                    // BCE3–BCFB: Delphi/Halon radius (underground uses eight)
  int bottom{168};
  bool beacon_lighting{true};
  bool gouraud{true};
};

bool within_object_window(city_view const &view, std::array<uint16_t,3> const &position) noexcept;

struct particle_scene {
  game::effect_system const &effects;
  framework::render::indexed_cockpit_framebuffer const &sheet;
  uint16_t clock;
};

class city_renderer {
private:
  std::vector<std::uint16_t> candidates;
  std::vector<city_draw_item> items;

public:
  std::size_t draw(framework::render::indexed_cockpit_framebuffer &target, resources::geometry_bank const &bank,
    std::span<game::city_cell const, 128 * 128> cells, city_view view, std::uint8_t damage_mask,
    distance_shading const &lighting, model_animation animation, std::span<scene_object const> objects = {}, particle_scene const *particles = nullptr);
};

void collect_city_cells(std::span<game::city_cell const, 128 * 128> cells, std::uint8_t column, std::uint8_t row,
  camera_angles angles, unsigned int radius, std::vector<std::uint16_t> &output);

} // namespace darker::graphics

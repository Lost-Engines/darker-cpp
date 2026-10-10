#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include "graphics/model_projection.h"
#include "render/indexed_framebuffer.h"

namespace darker::graphics {

enum class model_shading { flat, gouraud };

enum class model_path { direct, near_clipped };

struct model_animation {
  std::array<int16_t, 256> parameters{};
  uint8_t cell_state{0};
};

void update_fountain_parameters(model_animation &animation, uint16_t clock) noexcept;

struct model_colours {
  static unsigned int constexpr shade_count{28};                               // remaining low-five-bit codes select dynamic lighting
  static unsigned int constexpr ramp_mask{0xe0};
  static unsigned int constexpr shade_mask{0x1f};
  std::array<uint8_t, shade_count> shades{};
  uint8_t dynamic{0};
  // 2D71 uses all five shade bits without the polygon dynamic-colour branch.
  // Its last four indices spill into the following contiguous shade row.
  std::array<uint8_t, shade_mask + 1 - shade_count> point_shade_tail{};

  uint8_t point_colour(uint8_t source) const noexcept {
    auto const shade{source & shade_mask};
    auto const value{shade < shade_count ? shades[shade] : point_shade_tail[shade - shade_count]};
    return static_cast<uint8_t>((source & ramp_mask) + value);
  }
};

// Owned by a renderer and reused across models. Only emitted prefixes are read;
// the interpreter separately resets its vertex-validity flags for each model.
struct model_workspace {
  std::array<camera_vertex, polygon_vertex_limit> camera_vertices{};
  std::array<screen_vertex, polygon_vertex_limit> vertices{};
  std::array<screen_vertex, clipped_polygon_vertex_limit> face{};
  std::array<shaded_vertex, clipped_polygon_vertex_limit> shaded_face{};
  std::array<uint16_t, polygon_vertex_limit> vertex_shades{};
  std::array<camera_vertex, polygon_vertex_limit> camera_face{};
};

void draw_model(model_workspace &workspace, framework::render::indexed_surface target, std::span<std::byte const> pool,
  size_t model_offset, projection_parameters projection, model_colours const &colours, raster_viewport viewport = {}, model_path path = model_path::direct, model_animation const &animation = {}, model_shading shading = model_shading::flat);

void draw_model(framework::render::indexed_surface target, std::span<std::byte const> pool,
  size_t model_offset, projection_parameters projection, model_colours const &colours, raster_viewport viewport = {}, model_path path = model_path::direct, model_animation const &animation = {}, model_shading shading = model_shading::flat);

} // namespace darker::graphics

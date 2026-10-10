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
};

void draw_model(framework::render::indexed_cockpit_framebuffer &target, std::span<std::byte const> pool,
  size_t model_offset, projection_parameters projection, model_colours const &colours, int bottom = 240, model_path path = model_path::direct, model_animation const &animation = {}, model_shading shading = model_shading::flat);

} // namespace darker::graphics

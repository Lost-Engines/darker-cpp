#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include "graphics/model_projection.h"
#include "render/indexed_framebuffer.h"

namespace darker::graphics {

enum class model_path { direct, near_clipped };

struct model_animation {
  std::array<std::int16_t, 256> parameters{};
  std::uint8_t cell_state{0};
};

void update_fountain_parameters(model_animation &animation, std::uint16_t clock) noexcept;

struct model_colours {
  std::array<std::uint8_t, 28> shades{};
  std::uint8_t dynamic{0};
};

void draw_flat_model(framework::render::indexed_cockpit_framebuffer &target, std::span<std::byte const> pool,
  std::size_t model_offset, projection_parameters projection, model_colours const &colours, int bottom = 240, model_path path = model_path::direct, model_animation const &animation = {});

} // namespace darker::graphics

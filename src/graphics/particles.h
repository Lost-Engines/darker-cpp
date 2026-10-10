#pragma once

#include <vector>
#include "game/effects.h"
#include "graphics/camera.h"
#include "render/indexed_framebuffer.h"

namespace darker::graphics {

struct particle_point {
  render_geometry::screen_coordinate x{0};
  render_geometry::screen_coordinate y{0};
  render_geometry::coordinate depth{0};
  bool operator==(particle_point const&) const = default;
};

std::vector<particle_point> project_emitter(game::particle_emitter const &emitter, model_placement centre,
  camera_basis const &basis, screen_vertex const &origin);
void draw_particle(framework::render::indexed_surface target,
  framework::render::const_indexed_surface sheet, particle_point point, uint8_t phase, raster_viewport viewport);

} // namespace darker::graphics

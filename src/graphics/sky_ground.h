#pragma once

#include "graphics/camera.h"
#include "render/indexed_framebuffer.h"

namespace darker::graphics {

void draw_sky_ground(framework::render::indexed_cockpit_framebuffer &target, camera_angles angles, screen_vertex const &origin, int bottom);

} // namespace darker::graphics

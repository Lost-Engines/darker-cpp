#pragma once

#include "render/frame_layout.h"

namespace darker::graphics {

struct cockpit_view_layout {
  static int constexpr caero_height{168};
  static int constexpr skimma_height{180};
  static int constexpr caero_top{8};
  static int constexpr caero_centre_y{caero_height / 2};
  static int constexpr skimma_centre_y{skimma_height / 2};
};

using framework::render::display_layout;
using framework::render::source_sheet_layout;

} // namespace darker::graphics

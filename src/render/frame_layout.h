#pragma once

namespace framework::render {

struct source_sheet_layout {
  static int constexpr width{320};
  static int constexpr height{200};
};

struct display_layout {
  static int constexpr width{320};
  static int constexpr height{240};
  static int constexpr right{width - 1};
  static int constexpr last_row{height - 1};
  static int constexpr centre_x{width / 2};
  static int constexpr centre_y{height / 2};
};

} // namespace framework::render

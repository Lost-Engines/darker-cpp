#pragma once

struct GLFWwindow;

namespace framework::platform {

struct input_state {
  double previous_x{0.0};
  double previous_y{0.0};
  bool has_previous_position{false};
  bool captured{false};
};

void capture_mouse(GLFWwindow &window, bool capture);
void install_input_logging(GLFWwindow &window, input_state &state);

} // namespace framework::platform

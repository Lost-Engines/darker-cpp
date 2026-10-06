#include "input.h"
#include <iostream>
#include <GLFW/glfw3.h>

namespace framework::platform {

void capture_mouse(GLFWwindow &window, bool const capture) {
  /// Discard the previous cursor position whenever capture mode changes
  auto &state{*static_cast<input_state*>(glfwGetWindowUserPointer(&window))};
  state.captured = capture;
  state.has_previous_position = false;
  glfwSetInputMode(&window, GLFW_CURSOR, capture ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
  if(glfwRawMouseMotionSupported()) glfwSetInputMode(&window, GLFW_RAW_MOUSE_MOTION, capture ? GLFW_TRUE : GLFW_FALSE);
  std::cout << "Mouse capture " << (capture ? "enabled" : "released") << std::endl;
}

void install_input_logging(GLFWwindow &window, input_state &state) {
  /// Keep callback state alive until after the GLFW window is destroyed
  glfwSetWindowUserPointer(&window, &state);
  glfwSetKeyCallback(&window, [](GLFWwindow *const window, int const key, int const scancode, int const action, int const modifiers){
    char const *const name{glfwGetKeyName(key, scancode)};
    char const *const action_name{action == GLFW_PRESS ? "press" : action == GLFW_RELEASE ? "release" : "repeat"};
    std::cout << "Key " << action_name << ": key=" << key << " scancode=" << scancode << " name=" << (name ? name : "special") << " modifiers=" << modifiers << std::endl;
    if(action != GLFW_PRESS) return;
    auto const &state{*static_cast<input_state*>(glfwGetWindowUserPointer(window))};
    if(key == GLFW_KEY_TAB) capture_mouse(*window, !state.captured);
    if(key == GLFW_KEY_ESCAPE) {
      if(state.captured) capture_mouse(*window, false);
      else glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
  });
  glfwSetCursorPosCallback(&window, [](GLFWwindow *const window, double const x, double const y){
    auto &state{*static_cast<input_state*>(glfwGetWindowUserPointer(window))};
    if(state.has_previous_position) {
      std::cout << "Mouse " << (state.captured ? "captured" : "free") << ": dx=" << x - state.previous_x << " dy=" << y - state.previous_y << std::endl;
    }
    state.previous_x = x;
    state.previous_y = y;
    state.has_previous_position = true;
  });
  glfwSetMouseButtonCallback(&window, [](GLFWwindow*, int const button, int const action, int const modifiers){
    std::cout << "Mouse button " << button << (action == GLFW_PRESS ? " press" : " release") << " modifiers=" << modifiers << std::endl;
  });
  glfwSetWindowFocusCallback(&window, [](GLFWwindow *const window, int const focused){
    auto &state{*static_cast<input_state*>(glfwGetWindowUserPointer(window))};
    state.has_previous_position = false;
    if(!focused && state.captured) capture_mouse(*window, false);
    std::cout << "Window " << (focused ? "focused" : "unfocused") << std::endl;
  });
}

} // namespace framework::platform

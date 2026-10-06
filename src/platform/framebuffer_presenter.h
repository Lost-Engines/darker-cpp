#pragma once

struct GLFWwindow;

namespace framework::render {
struct framebuffer;
}

namespace framework::platform {

class framebuffer_presenter {
private:
  GLFWwindow &window;
  unsigned int texture{0};

public:
  framebuffer_presenter(GLFWwindow &window);
  ~framebuffer_presenter();
  framebuffer_presenter(framebuffer_presenter const&) = delete;
  framebuffer_presenter &operator=(framebuffer_presenter const&) = delete;
  framebuffer_presenter(framebuffer_presenter&&) = delete;
  framebuffer_presenter &operator=(framebuffer_presenter&&) = delete;
  void present(render::framebuffer const &source);
};

} // namespace framework::platform

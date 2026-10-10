#pragma once

#include <span>
#include "render/framebuffer.h"

struct GLFWwindow;

namespace framework::platform {

class framebuffer_presenter {
private:
  GLFWwindow &window;
  unsigned int texture{0};
  int texture_width{320};
  int texture_height{200};

public:
  framebuffer_presenter(GLFWwindow &window);
  ~framebuffer_presenter();
  framebuffer_presenter(framebuffer_presenter const&) = delete;
  framebuffer_presenter &operator=(framebuffer_presenter const&) = delete;
  framebuffer_presenter(framebuffer_presenter &&) = delete;
  framebuffer_presenter &operator=(framebuffer_presenter &&) = delete;
  void present(std::span<render::rgba_pixel const> pixels, int width, int height);

  template<unsigned int rows>
  void present(render::basic_framebuffer<rows> const &source) {
    /// Present either supported fixed-size CPU surface
    present(source.pixels, static_cast<int>(source.width), static_cast<int>(source.height));
  }
};

} // namespace framework::platform

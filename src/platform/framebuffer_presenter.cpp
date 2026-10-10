#include "framebuffer_presenter.h"
#include <iostream>
#include <stdexcept>
#include <string>
#include <GLFW/glfw3.h>
#ifdef _WIN32
#include <GL/glext.h>
#endif
#include "render/framebuffer.h"

namespace framework::platform {

framebuffer_presenter::framebuffer_presenter(GLFWwindow &window) :
  window{window} {
  /// Allocate one texture in the caller's current OpenGL 2.1 context
  if(glfwGetCurrentContext() != &window) throw std::logic_error{"presenter requires its window's current GL context"};
  std::cout << "OpenGL: " << glGetString(GL_VERSION) << " / " << glGetString(GL_RENDERER) << std::endl;
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, render::framebuffer::width, render::framebuffer::height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
  if(glGetError() != GL_NO_ERROR) {
    glDeleteTextures(1, &texture);
    throw std::runtime_error{"cannot allocate the OpenGL framebuffer texture"};
  }
  glDisable(GL_DITHER);
  glEnable(GL_TEXTURE_2D);
  glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
}

framebuffer_presenter::~framebuffer_presenter() {
  /// Release the texture while its owning context is still current
  glDeleteTextures(1, &texture);
}

void framebuffer_presenter::present(std::span<render::rgba_pixel const> const pixels, int const source_width, int const source_height) {
  /// Upload CPU pixels and draw a nearest-filtered quad inside a letterboxed viewport
  if(source_width <= 0 || source_height <= 0 || pixels.size() != static_cast<std::size_t>(source_width) * static_cast<std::size_t>(source_height)) {
    throw std::invalid_argument{"invalid presentation surface dimensions"};
  }
  int width{0};
  int height{0};
  glfwGetFramebufferSize(&window, &width, &height);
  if(width == 0 || height == 0) return;
  auto const viewport{render::fit_viewport(width, height, source_width, source_height)};
  glViewport(0, 0, width, height);
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  glViewport(viewport.x, viewport.y, viewport.width, viewport.height);
  glBindTexture(GL_TEXTURE_2D, texture);
  if(texture_width != source_width || texture_height != source_height) {
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, source_width, source_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    texture_width = source_width;
    texture_height = source_height;
  }
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, source_width, source_height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
  glBegin(GL_TRIANGLE_STRIP);
  glTexCoord2f(0.0f, 0.0f);
  glVertex2f(-1.0f, 1.0f);
  glTexCoord2f(0.0f, 1.0f);
  glVertex2f(-1.0f, -1.0f);
  glTexCoord2f(1.0f, 0.0f);
  glVertex2f(1.0f, 1.0f);
  glTexCoord2f(1.0f, 1.0f);
  glVertex2f(1.0f, -1.0f);
  glEnd();
  if(auto const error{glGetError()}; error != GL_NO_ERROR) {
    throw std::runtime_error{"OpenGL presentation failed: " + std::to_string(error)};
  }
  glfwSwapBuffers(&window);
}

} // namespace framework::platform

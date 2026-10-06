#include "render/indexed_framebuffer.h"
#include <cstddef>

namespace framework::render {

void expand_palette(indexed_framebuffer const &source, colour_palette const &palette, framebuffer &target) noexcept {
  /// Resolve indices only at the presentation boundary
  for(std::size_t i{0}; i < source.pixels.size(); ++i) target.pixels[i] = palette[source.pixels[i]];
}

} // namespace framework::render

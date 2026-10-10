#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace darker::resources {

enum class font_face { interface, compact, wide };

struct font_glyph {
  uint8_t width{0};
  uint8_t height{0};
  uint8_t top{0};
  uint16_t advance{0};
  unsigned int stride{0};
  std::span<std::byte const> planes;
};

class font_resource {
private:
  std::vector<std::byte> data;

public:
  explicit font_resource(std::vector<std::byte> resource);
  font_glyph glyph(font_face face, uint8_t code, unsigned int alignment = 0) const;
};

} // namespace darker::resources

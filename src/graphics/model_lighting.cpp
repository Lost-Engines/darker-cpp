#include "graphics/model_lighting.h"
#include <algorithm>
#include <stdexcept>

namespace darker::graphics {

distance_shading::distance_shading(unsigned int const count) {
  /// B73C builds palette-index ramps towards shade one, with a half-step initial fraction and signed IDIV rounding
  if(count < 2 || count > 255) throw std::invalid_argument{"Distance shade table count exceeds its original byte range"};
  tables.resize(count);
  for(int shade{0}; shade < 28; ++shade) {
    int const step{((shade - 1) * 256) / static_cast<int>(count - 1)};
    for(unsigned int distance{0}; distance < count; ++distance) {
      tables[distance][shade] = static_cast<std::uint8_t>((shade * 256 + 128 - static_cast<int>(distance) * step) >> 8);
    }
  }
}

model_colours distance_shading::colours(std::uint16_t depth, model_path const path, std::uint8_t const light) const {
  /// 2CE4 folds beacon strength into distance; the near path clamps a negative origin depth before the wrapping subtraction
  if(path == model_path::near_clipped && (depth & 0x8000)) depth = 0;
  auto const adjusted{static_cast<std::uint16_t>(depth + 16 * (255 - light))};
  auto const index{std::min<std::size_t>(adjusted >> 8, tables.size() - 1)};
  return {
    .shades{tables[index]},
    .dynamic{static_cast<std::uint8_t>(0xf0 | (light >> 4))}
  };
}

} // namespace darker::graphics

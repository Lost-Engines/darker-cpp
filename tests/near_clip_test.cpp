#include <catch2/catch_test_macros.hpp>
#include "graphics/model_renderer.h"
#include "graphics/near_clip.h"
#include "reference/near_clip_samples.h"
#include "reference/near_model_samples.h"

TEST_CASE("Near-plane intersections match original integer halving and saturation", "[graphics][near_clip]") {
  /// Check edge crossings independently of rasterisation, including subpixel depth and coordinate saturation
  std::size_t index{0};
  for(auto const &sample : darker::test_reference::near_clip_samples) {
    CAPTURE(index);
    auto const point{darker::graphics::near_intersection(
      {.horizontal{sample.inside[0]}, .vertical{sample.inside[1]}, .depth{sample.inside[2]}},
      {.horizontal{sample.outside[0]}, .vertical{sample.outside[1]}, .depth{sample.outside[2]}},
      {.x{sample.origin[0]}, .y{sample.origin[1]}})};
    REQUIRE(point.x == sample.result[0]);
    REQUIRE(point.y == sample.result[1]);
    ++index;
  }
}


TEST_CASE("Near model bytecode reproduces native clipped frames", "[graphics][near_clip][models]") {
  /// Exercise clipping, winding, visibility skips and the flat fallback together with the rasteriser
  std::size_t index{0};
  for(auto const &sample : darker::test_reference::near_model_samples) {
    CAPTURE(index, sample.depth, sample.fraction);
    darker::graphics::projection_parameters const projection{
      .axes{{{.horizontal{16384}, .vertical{4096}}, {.depth{16384}}, {.vertical{16384}}}},
      .horizontal{.fraction{11}}, .vertical{.fraction{19}},
      .depth{.whole{static_cast<std::uint16_t>(sample.depth)}, .fraction{static_cast<std::uint8_t>(sample.fraction)}},
      .origin{.x{160}, .y{84}},
    };
    darker::graphics::model_colours colours{};
    for(std::size_t i{0}; i < colours.shades.size(); ++i) colours.shades[i] = static_cast<std::uint8_t>(i);
    std::array<std::byte, 64> bytes{};
    for(std::size_t i{0}; i < bytes.size(); ++i) bytes[i] = static_cast<std::byte>(sample.code[i]);
    framework::render::indexed_cockpit_framebuffer frame{};
    darker::graphics::draw_flat_model(frame, std::span{bytes}.first(sample.size), 0, projection, colours, 168, darker::graphics::model_path::near_clipped);
    std::uint64_t fingerprint{0xcbf29ce484222325};
    for(auto const pixel : frame.pixels) fingerprint = (fingerprint ^ pixel) * 0x100000001b3;
    REQUIRE(fingerprint == sample.fingerprint);
    ++index;
  }
}

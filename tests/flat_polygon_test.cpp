#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include "graphics/flat_polygon.h"
#include "graphics/gouraud_polygon.h"
#include "graphics/screen_primitives.h"
#include "graphics/sky_ground.h"
#include "reference/flat_polygon_samples.h"

TEST_CASE("Flat polygon coverage matches native clipping and VGA scanline boundaries", "[graphics][polygon]") {
  /// Compare the complete indexed frame against coverage captured before the original VGA writes
  size_t index{0};
  for(auto const &sample : darker::test_reference::flat_polygon_samples) {
    CAPTURE(index, sample.count, sample.pixels, sample.right, sample.bottom);
    framework::render::indexed_cockpit_framebuffer frame{};
    darker::graphics::draw_flat_polygon(frame, std::span{sample.vertices}.first(sample.count), 37, {
      .right{sample.right},
      .bottom{sample.bottom},
    });
    uint64_t fingerprint{0xcbf29ce484222325};
    int pixels{0};
    for(auto const pixel : frame.pixels) {
      fingerprint = (fingerprint ^ pixel) * 0x100000001b3;
      if(pixel != 0) ++pixels;
    }
    CHECK(pixels == sample.pixels);
    REQUIRE(fingerprint == sample.fingerprint);
    ++index;
  }
}

TEST_CASE("Rasterisation separates viewport bounds from target size and row stride", "[graphics][polygon]") {
  framework::render::indexed_cockpit_framebuffer native{};
  unsigned int constexpr width{43};
  unsigned int constexpr height{29};
  unsigned int constexpr stride{51};
  std::array<uint8_t, stride * height> storage;
  storage.fill(201);
  framework::render::indexed_surface const padded{storage, width, height, stride};
  for(int y{0}; y < padded.height; ++y) std::ranges::fill(padded.row(y), 0);
  darker::graphics::raster_viewport const viewport{
    .right{36},
    .bottom{23},
  };
  SECTION("Flat polygons") {
    std::array<darker::graphics::screen_vertex, 4> const polygon{{{-8, -5}, {50, 4}, {48, 32}, {-2, 27}}};
    darker::graphics::draw_flat_polygon(native, polygon, 37, viewport);
    darker::graphics::draw_flat_polygon(padded, polygon, 37, viewport);
  }
  SECTION("Shaded polygons") {
    std::array<darker::graphics::shaded_vertex, 3> const polygon{{{
      .position{-8, -5},
      .shade{0x2180},
    }, {
      .position{50, 4},
      .shade{0x2580},
    }, {
      .position{20, 32},
      .shade{0x2380},
    }}};
    darker::graphics::draw_gouraud_polygon(native, polygon, viewport);
    darker::graphics::draw_gouraud_polygon(padded, polygon, viewport);
  }
  SECTION("Discs and clipped lines") {
    for(framework::render::indexed_surface const surface : {framework::render::indexed_surface{native}, padded}) {
      darker::graphics::draw_disc(surface, {34, 18}, 12, 73, viewport);
      darker::graphics::draw_disc(surface, {40, 18}, 0, 73, viewport);
      darker::graphics::draw_world_line(surface, {-20, -2}, {60, 30}, 37, viewport);
    }
  }
  SECTION("Sky bands") {
    darker::graphics::camera_angles const angles{
      .pitch{600},
      .roll{2400},
    };
    darker::graphics::draw_sky_ground(native, angles, {20, 12}, viewport.bottom);
    darker::graphics::draw_sky_ground(padded, angles, {20, 12}, viewport.bottom);
  }
  for(unsigned int y{0}; y < height; ++y) {
    for(unsigned int x{0}; x < width; ++x) CHECK(storage[y * stride + x] == native.pixels[y * native.width + x]);
    for(unsigned int x{width}; x < stride; ++x) CHECK(storage[y * stride + x] == 201);
  }
}

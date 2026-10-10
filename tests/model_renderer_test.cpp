#include <catch2/catch_test_macros.hpp>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include "graphics/model_renderer.h"
#include "reference/model_renderer_samples.h"

TEST_CASE("Flat model bytecode reproduces complete native indexed frames", "[graphics][models]") {
  /// Compare calls, branches, coordinate reuse, visibility and colour selection together with rasterisation
  darker::graphics::model_workspace workspace;
  size_t case_index{0};
  for(auto const &sample : darker::test_reference::model_renderer_samples) {
    CAPTURE(case_index, sample.mirrored, sample.dynamic);
    darker::graphics::projection_parameters parameters{
      .axes{{
        {
        .horizontal{static_cast<int16_t>(sample.mirrored ? -16384 : 16384)},
        .vertical{4096}
      },
        {
        .horizontal{4096},
        .vertical{-4096}
      },
        {
        .vertical{16384}
      },
      }},
      .horizontal{
        .fraction{11}
      },
      .vertical{
        .fraction{19}
      },
      .depth{
        .whole{256}
      },
      .origin{160, 120},
    };
    darker::graphics::model_colours colours{
      .dynamic{static_cast<uint8_t>(sample.dynamic)}
    };
    for(size_t i{0}; i < colours.shades.size(); ++i) colours.shades[i] = static_cast<uint8_t>(sample.dynamic == 17 ? 27 - i : i);
    std::array<std::byte, 64> bytes{};
    for(size_t i{0}; i < bytes.size(); ++i) bytes[i] = static_cast<std::byte>(sample.code[i]);
    framework::render::indexed_cockpit_framebuffer frame{};
    darker::graphics::draw_model(workspace, frame, std::span{bytes}.first(sample.size), 0, parameters, colours);
    uint64_t fingerprint{0xcbf29ce484222325};
    for(auto const pixel : frame.pixels) fingerprint = (fingerprint ^ pixel) * 0x100000001b3;
    REQUIRE(fingerprint == sample.fingerprint);
    ++case_index;
  }
}

TEST_CASE("Reused model storage does not make previous vertices valid", "[graphics][models]") {
  darker::graphics::model_workspace workspace;
  framework::render::indexed_cockpit_framebuffer frame{};
  darker::graphics::projection_parameters const projection{
    .depth{
      .whole{256},
    },
  };
  auto const &sample{darker::test_reference::model_renderer_samples.front()};
  std::array<std::byte, 64> model{};
  for(unsigned int i{0}; i < model.size(); ++i) model[i] = static_cast<std::byte>(sample.code[i]);
  darker::graphics::draw_model(workspace, frame, std::span{model}.first(sample.size), 0, projection, {});

  // A second model tries to reuse three vertices without emitting any of its own.
  std::array<std::byte, 17> invalid{};
  invalid[11] = std::byte{0x02}; // flat triangle
  invalid[12] = std::byte{1};    // colour
  invalid[13] = std::byte{0};
  invalid[14] = std::byte{1};
  invalid[15] = std::byte{2};
  invalid[16] = std::byte{0x0d}; // return
  REQUIRE_THROWS_AS(darker::graphics::draw_model(workspace, frame, invalid, 0, projection, {}), std::invalid_argument);
}

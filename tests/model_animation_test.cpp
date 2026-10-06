#include <catch2/catch_test_macros.hpp>
#include <bit>
#include "graphics/model_renderer.h"
#include "reference/model_animation_samples.h"

TEST_CASE("Gate interpolation reproduces both original projection paths", "[graphics][models]") {
  /// Include fractional translations and shallow depths which expose accumulator rounding differences
  for(auto const &sample : darker::test_reference::gate_samples) {
    CAPTURE(sample.near, sample.depth, sample.fraction, sample.gate);
    darker::graphics::projection_parameters const projection{
      .axes{{{.horizontal{16384}, .vertical{4096}}, {.depth{16384}}, {.vertical{16384}}}},
      .horizontal{.fraction{11}}, .vertical{.fraction{static_cast<std::uint8_t>(sample.fraction)}},
      .depth{.whole{static_cast<std::uint16_t>(sample.depth)}, .fraction{83}}, .origin{.x{160}, .y{84}},
    };
    darker::graphics::model_colours colours{};
    for(std::size_t i{0}; i < colours.shades.size(); ++i) colours.shades[i] = static_cast<std::uint8_t>(i);
    std::array<std::byte, darker::test_reference::gate_code.size()> code{};
    for(std::size_t i{0}; i < code.size(); ++i) code[i] = static_cast<std::byte>(darker::test_reference::gate_code[i]);
    darker::graphics::model_animation animation{};
    animation.parameters[0] = std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(sample.gate));
    framework::render::indexed_cockpit_framebuffer frame{};
    darker::graphics::draw_flat_model(frame, code, 0, projection, colours, 168,
      sample.near ? darker::graphics::model_path::near_clipped : darker::graphics::model_path::direct, animation);
    std::uint64_t fingerprint{0xcbf29ce484222325};
    for(auto const pixel : frame.pixels) fingerprint = (fingerprint ^ pixel) * 0x100000001b3;
    REQUIRE(fingerprint == sample.fingerprint);
  }
}

TEST_CASE("Fountain parameters match every step of the native cycle", "[graphics][models]") {
  /// Preserve the six staggered amplitudes, signed rounding and wrap into the next cycle
  std::uint64_t fingerprint{0xcbf29ce484222325};
  darker::graphics::model_animation animation{};
  for(unsigned int clock{0}; clock < 2048; ++clock) {
    darker::graphics::update_fountain_parameters(animation, static_cast<std::uint16_t>(clock));
    for(std::size_t i{9}; i < 15; ++i) {
      auto const word{static_cast<std::uint16_t>(animation.parameters[i])};
      for(auto const byte : {word & 255, word >> 8}) fingerprint = (fingerprint ^ byte) * 0x100000001b3;
    }
  }
  REQUIRE(fingerprint == darker::test_reference::fountain_cycle_fingerprint);
}

#include <catch2/catch_test_macros.hpp>
#include <array>
#include <cstddef>
#include <vector>
#include "graphics/palette_bitmap.h"
#include "render/indexed_framebuffer.h"

TEST_CASE("Palette skips retain previous colours and literals replace single entries") {
  darker::graphics::palette_state previous;
  previous.colours[0] = {.red{10}, .green{20}, .blue{30}};
  previous.defined.set(0);
  std::array const data{std::byte{1}, std::byte{42}, std::byte{13}, std::byte{255}, std::byte{255}, std::byte{251}, std::byte{99}};
  auto const decoded{darker::graphics::decode_palette(data, previous)};
  REQUIRE(decoded.bytes_consumed == 6);
  REQUIRE(decoded.palette.colours[0].red == 10);
  REQUIRE(decoded.palette.colours[1].red == 42);
  REQUIRE(decoded.palette.colours[1].green == 13);
  REQUIRE(decoded.palette.colours[1].blue == 255);
  REQUIRE(decoded.palette.defined.count() == 2);
  REQUIRE_FALSE(previous.defined[1]);
}

TEST_CASE("Palette decoder rejects truncation and skips beyond the palette") {
  REQUIRE_THROWS(darker::graphics::decode_palette({}));
  std::array const truncated{std::byte{2}, std::byte{3}};
  REQUIRE_THROWS(darker::graphics::decode_palette(truncated));
  std::array const overflow{std::byte{255}, std::byte{253}, std::byte{3}};
  REQUIRE_THROWS(darker::graphics::decode_palette(overflow));
}

TEST_CASE("Indexed bitmap keeps indices independent of palette presentation") {
  std::vector<std::byte> data{std::byte{20}, std::byte{40}, std::byte{60}, std::byte{255}, std::byte{253}};
  data.resize(data.size() + 320 * 200, std::byte{0});
  auto bitmap{darker::graphics::decode_bitmap(data)};
  framework::render::framebuffer output;
  framework::render::expand_palette(bitmap.image, bitmap.palette.colours, output);
  REQUIRE(output.pixels.front().red == 20);
  REQUIRE(output.pixels.back().blue == 60);
  bitmap.palette.colours[0].red = 100;
  framework::render::expand_palette(bitmap.image, bitmap.palette.colours, output);
  REQUIRE(output.pixels.front().red == 100);
  REQUIRE(bitmap.image.pixels.front() == 0);
  data.back() = std::byte{1};
  REQUIRE_THROWS(darker::graphics::decode_bitmap(data));
  data.pop_back();
  REQUIRE_THROWS(darker::graphics::decode_bitmap(data));
}

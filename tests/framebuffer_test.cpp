#include <catch2/catch_test_macros.hpp>
#include "render/framebuffer.h"

TEST_CASE("Presentation uses integer scaling and centred letterboxing") {
  auto const view{framework::render::fit_viewport(1000, 700)};
  REQUIRE(view.width == 960);
  REQUIRE(view.height == 600);
  REQUIRE(view.x == 20);
  REQUIRE(view.y == 50);
}

TEST_CASE("Small or minimised windows never produce an invalid viewport") {
  auto const small{framework::render::fit_viewport(160, 120)};
  REQUIRE(small.width == 160);
  REQUIRE(small.height == 100);
  REQUIRE(small.y == 10);
  auto const minimised{framework::render::fit_viewport(0, 0)};
  REQUIRE(minimised.width == 0);
  REQUIRE(minimised.height == 0);
}

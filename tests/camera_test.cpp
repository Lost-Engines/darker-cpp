#include <catch2/catch_test_macros.hpp>
#include <bit>
#include "graphics/camera.h"
#include "reference/camera_samples.h"

TEST_CASE("Camera coefficients and model origins match native fixed-point transforms", "[graphics][camera]") {
  /// Compare ordered camera products and placement, including wrapping coordinates and the sort estimate
  std::size_t index{0};
  for(auto const &sample : darker::test_reference::camera_samples) {
    CAPTURE(index);
    auto const basis{darker::graphics::make_camera_basis({.heading{sample.angles[0]}, .pitch{sample.angles[1]}, .roll{sample.angles[2]}})};
    for(std::size_t axis{0}; axis < basis.size(); ++axis) {
      REQUIRE(basis[axis].horizontal == sample.coefficients[axis * 3]);
      REQUIRE(basis[axis].vertical == sample.coefficients[axis * 3 + 1]);
      REQUIRE(basis[axis].depth == sample.coefficients[axis * 3 + 2]);
    }
    auto const placement{darker::graphics::place_model(basis,
      {.column{sample.camera[0]}, .row{sample.camera[1]}, .altitude{std::bit_cast<std::int16_t>(sample.camera[2])}},
      {.column{sample.origin[0]}, .row{sample.origin[1]}, .height{std::bit_cast<std::int16_t>(sample.origin[2])}})};
    REQUIRE(placement.horizontal.whole == sample.result[0]);
    REQUIRE(placement.horizontal.fraction == sample.result[1]);
    REQUIRE(placement.vertical.whole == sample.result[2]);
    REQUIRE(placement.vertical.fraction == sample.result[3]);
    REQUIRE(placement.depth.whole == sample.result[4]);
    REQUIRE(placement.depth.fraction == sample.result[5]);
    REQUIRE(placement.sorting_distance == sample.result[6]);
    ++index;
  }
}

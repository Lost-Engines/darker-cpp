#include <catch2/catch_test_macros.hpp>
#include <array>
#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>
#include "game/city_collision.h"
#include "maths/world_coordinates.h"

namespace {

std::vector<std::byte> collision_bank() {
  /// Three boxes distinguish a one-record height prefix from persistent base-height changes
  std::vector<std::byte> data(23);
  data[0] = std::byte{1};
  data[3] = std::byte{128};
  data[4] = std::byte{64};
  data[5] = std::byte{2};
  data[10] = std::byte{11};
  data[17] = std::byte{128};
  for(unsigned int const value : {0x10, 128, 0xf0, 16, 0xf8, 8, 0xf1, 32, 0x1a, 16, 0, 1, 2, 3, 0x08, 32, 0, 0, 0, 0, 0xf8}) {
    data.push_back(static_cast<std::byte>(value));
  }
  return data;
}

} // namespace

TEST_CASE("City collision boxes retain signed endpoints, categories and local height prefixes", "[game][collision]") {
  /// Original collision coordinates use cell anchors and exclusive horizontal upper endpoints
  darker::resources::geometry_bank const bank{collision_bank()};
  auto const boxes{darker::game::city_collision_boxes(bank, 1, 0, 0x20, 20, 30, 7)};
  REQUIRE(boxes.size() == 3);
  CHECK(boxes[0].bounds.min == vec3<std::uint16_t>{5225, 7729, 57});
  CHECK(boxes[0].bounds.max == vec3<std::uint16_t>{5272, 7760, 199});
  CHECK(boxes[0].category == 2);
  CHECK(boxes[1].bounds.min[2] == 345);
  CHECK(boxes[1].bounds.max[2] == 887);
  CHECK(boxes[1].category == 3);
  CHECK(boxes[2].bounds.min[2] == 57);
  CHECK(boxes[2].bounds.max[2] == 103);
}

TEST_CASE("City collision decoding rejects missing stream data and respects non-colliding types", "[game][collision]") {
  /// Malformed pointers and records must fail before a collision traversal reads outside an archive resource
  auto bytes{collision_bank()};
  bytes.pop_back();
  darker::resources::geometry_bank const truncated{bytes};
  REQUIRE_THROWS_AS(darker::game::city_collision_boxes(truncated, 1, 0, 0x20, 0, 0), std::invalid_argument);
  bytes[17] = std::byte{0};
  darker::resources::geometry_bank const invalid{bytes};
  REQUIRE_THROWS_AS(darker::game::city_collision_boxes(invalid, 1, 0, 0x20, 0, 0), std::invalid_argument);
  bytes[5] = std::byte{255};
  darker::resources::geometry_bank const non_colliding{std::move(bytes)};
  CHECK(darker::game::city_collision_boxes(non_colliding, 1, 0, 0x20, 0, 0).empty());
}

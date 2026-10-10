#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include "game/collision_cells.h"
#include "reference/collision_cells_samples.h"

TEST_CASE("Collision cell traversal matches native order and fractional crossings", "[game][collision]") {
  /// Equal slopes, axis-aligned movement and wrapping coordinates exercise the specialised traversal branches
  for(auto const &sample : darker::test_reference::collision_cells_samples) {
    CAPTURE(sample.x, sample.y, sample.end_x, sample.end_y);
    auto const cells{darker::game::swept_collision_cells(static_cast<uint16_t>(sample.x), static_cast<uint16_t>(sample.y),
      static_cast<uint16_t>(sample.end_x), static_cast<uint16_t>(sample.end_y))};
    uint64_t fingerprint{0xcbf29ce484222325};
    for(auto const cell : cells) {
      fingerprint = (fingerprint ^ cell.column) * 0x100000001b3;
      fingerprint = (fingerprint ^ cell.row) * 0x100000001b3;
    }
    REQUIRE(cells.size() == sample.count);
    CHECK(fingerprint == sample.fingerprint);
  }
}

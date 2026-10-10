#include <catch2/catch_test_macros.hpp>
#include "graphics/city_scene.h"
#include "maths/world_coordinates.h"
#include "reference/city_scan_samples.h"
#include "reference/distant_points_samples.h"
#include "reference/object_window_samples.h"

TEST_CASE("City scans preserve native candidate order and map boundaries", "[graphics][city]") {
  /// Match circular spans, heading halves, full-pitch scans and empty-cell skips
  darker::game::city_map cells{};
  for(std::size_t i{0}; i < cells.size(); ++i) cells[i].type = i % 7 == 0 ? 0 : 1;
  std::vector<std::uint16_t> candidates;
  std::size_t index{0};
  for(auto const &sample : darker::test_reference::city_scan_samples) {
    CAPTURE(index, sample.column, sample.row, sample.heading, sample.pitch, sample.radius);
    darker::graphics::collect_city_cells(cells, static_cast<std::uint8_t>(sample.column), static_cast<std::uint8_t>(sample.row),
      {.heading{static_cast<std::uint16_t>(sample.heading)}, .pitch{static_cast<std::uint16_t>(sample.pitch)}}, sample.radius, candidates);
    REQUIRE(candidates.size() == sample.count);
    std::uint64_t fingerprint{0xcbf29ce484222325};
    for(auto const cell : candidates) {
      for(auto const byte : {cell & 255, cell >> 8}) fingerprint = (fingerprint ^ byte) * 0x100000001b3;
    }
    REQUIRE(fingerprint == sample.fingerprint);
    ++index;
  }
}

TEST_CASE("Moving-object visibility matches the native wrapping byte window", "[graphics][city]") {
  /// Exercise both axes and both native radii before narrower projection arithmetic can alias distant coordinates
  for(auto const &sample : darker::test_reference::object_window_samples) {
    darker::graphics::city_view const view{.column{static_cast<uint16_t>(sample[0])}, .row{static_cast<uint16_t>(sample[1])}, .radius{sample[2]}};
    darker::maths::world_position const position{.column{static_cast<uint16_t>(sample[3])}, .row{static_cast<uint16_t>(sample[4])}, .height{0}};
    CAPTURE(sample);
    REQUIRE(darker::graphics::within_object_window(view, position) == static_cast<bool>(sample[5]));
  }
}

TEST_CASE("Distant moving objects project to the original single pixel", "[graphics][city]") {
  /// Native 2D32 drops fractional coordinates and rejects pixels outside the cockpit/full-screen viewport.
  for(auto const &sample : darker::test_reference::distant_points_samples) {
    CAPTURE(sample);
    darker::graphics::model_placement const placement{
      .horizontal{.whole{static_cast<uint16_t>(sample[0])}, .fraction{173}},
      .vertical{.whole{static_cast<uint16_t>(sample[1])}, .fraction{89}},
      .depth{.whole{static_cast<uint16_t>(sample[2])}, .fraction{255}},
    };
    auto const point{darker::graphics::project_distant_object(placement,{160,static_cast<int16_t>(sample[3]/2)},sample[3],static_cast<uint8_t>(sample[4]))};
    REQUIRE(point.has_value() == (sample[5] >= 0));
    if(point) {
      CHECK(point->x == sample[5]);
      CHECK(point->y == sample[6]);
    }
  }
}

TEST_CASE("Draw traversal supplies the native record byte after a farther subtree", "[graphics][city]") {
  /// Captured through native 2B09 insertion and 2C38 traversal, intercepting callbacks at 2D32
  std::array<uint16_t,6> constexpr distances{10,20,5,15,30,7};
  std::array<uint16_t,6> constexpr records{0x8060,0x8018,0x8048,0x8000,0x8078,0x8030};
  std::array<uint8_t,6> constexpr residues{0,0x18,0,0,0,0x30};
  std::vector<darker::graphics::city_draw_item> items;
  for(size_t i{0}; i < distances.size(); ++i) items.push_back({
    .placement{.sorting_distance{distances[i]}}, .draw_record{static_cast<uint16_t>(0x8000+i*24)},
  });
  darker::graphics::order_city_models(items);
  for(size_t i{0}; i < items.size(); ++i) {
    CHECK(items[i].draw_record == records[i]);
    CHECK(items[i].projection_residue == residues[i]);
  }
}

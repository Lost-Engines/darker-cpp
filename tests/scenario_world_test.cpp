#include <catch2/catch_test_macros.hpp>
#include "game/scenario_world.h"
#include "reference/scenario_world_samples.h"

TEST_CASE("Scenario cell setup and incremental objectives match native execution", "[game][scenario]") {
  /// Preserve FE list continuation and one-cell-per-frame advancement, including already damaged cells
  for(auto const &sample : darker::test_reference::scenario_world_samples) {
    CAPTURE(sample);
    darker::resources::scenario_record record;
    record.cell_lists[0].cells = {{1,2}};
    record.cell_lists[1].cells = {{3,4},{5,6}};
    record.cell_lists[1].terminator = static_cast<uint8_t>(sample[0]);
    record.cell_lists[2].cells = {{7,8},{9,10}};
    record.objective_cell_list = sample[0] == 254 ? 1 : 2;
    darker::game::city_map cells{};
    for(size_t i{0}; i < 5; ++i) cells[(i*2+2)*128+i*2+1].state = static_cast<uint8_t>(sample[i+2]);
    darker::game::apply_scenario_cells(cells,record);
    for(size_t i{0}; i < 5; ++i) CHECK(cells[(i*2+2)*128+i*2+1].state == sample[i+7]);
    darker::game::world_objectives objectives{.list{record.objective_cell_list}};
    auto const offset{[&]{ return (objectives.list == 1 ? 3u : 8u) + objectives.cursor*2; }};
    CHECK(offset() == sample[12]);
    for(size_t i{0}; i < 8; ++i) {
      objectives.advance(cells,record,static_cast<uint8_t>(sample[1]));
      CHECK(offset() == sample[13+i*2]);
      CHECK(objectives.complete(record) == static_cast<bool>(sample[14+i*2]));
    }
  }
}

TEST_CASE("Mission exit extinguishes only queued energy beacons", "[game][scenario]") {
  /// High and low nibbles select column and row on the nine-cell beacon lattice
  darker::game::city_map cells;
  cells.fill({.type{1},.state{255}});
  std::array<std::byte,3> const queue{std::byte{0x00},std::byte{0x1e},std::byte{0xe1}};
  darker::game::commit_beacon_queue(cells,queue);
  for(size_t i{0}; i < cells.size(); ++i) {
    CHECK(cells[i].type == 1);
    CHECK(cells[i].state == (i == 0 || i == 126*128+9 || i == 9*128+126 ? 0 : 255));
  }
}

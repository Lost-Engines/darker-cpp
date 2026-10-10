#include <catch2/catch_test_macros.hpp>
#include "game/beacon_changes.h"
#include "game/mission_script.h"
#include "reference/beacon_change_samples.h"

TEST_CASE("Beacon fade frames match native dimming and restoration", "[game][beacons]") {
  /// Compare all cells through saturation, timer wrapping and the frame after a fade disables itself
  for(auto const &sample : darker::test_reference::beacon_change_samples) {
    CAPTURE(sample.opcode, sample.origin, sample.count, sample.clock, sample.dimming);
    darker::game::city_map cells;
    for(size_t i{0}; i < cells.size(); ++i) cells[i] = {
      .type{1},
      .state{static_cast<uint8_t>(i * 37 + sample.seed)}
    };
    darker::game::beacon_changes fade;
    std::array<std::byte, 1> const queue{static_cast<std::byte>(sample.origin)};
    if(!sample.dimming) fade.command(0x14, 0, 0, 0, {});
    fade.command(static_cast<uint8_t>(sample.opcode), static_cast<uint8_t>(sample.origin), static_cast<uint8_t>(sample.count), static_cast<uint16_t>(sample.clock), queue);
    size_t index{0};
    for(auto const elapsed : {0, 16, 128, 255, 256, 257, 512}) {
      fade.advance(cells, static_cast<uint16_t>(sample.clock + elapsed));
      uint64_t hash{0xcbf29ce484222325};
      for(auto const cell : cells) {
        for(auto const byte : {cell.type, cell.state}) hash = (hash ^ byte) * 0x100000001b3;
      }
      CHECK(hash == sample.frames[index++]);
    }
  }
}

TEST_CASE("Mission beacon commands consume the queue and preserve their implicit wait", "[game][beacons]") {
  /// A sequential failure waits six scenario intervals before configuring the next beacon
  std::array<std::byte, 3> const program{std::byte{0x11}, std::byte{0x11}, std::byte{0x23}};
  std::array<std::byte, 2> const queue{std::byte{0x12}, std::byte{0x34}};
  darker::game::city_map cells;
  cells.fill({
    .type{1},
    .state{255}
  });
  darker::game::beacon_changes fade;
  darker::game::mission_script script;
  darker::game::mission_context context{
    .program{program}
  };
  context.change_beacons = [&](uint8_t opcode, uint8_t origin, uint8_t count){
    fade.command(opcode, origin, count, static_cast<uint16_t>(context.clock), queue);
  };
  darker::game::advance_mission_script(script, context);
  CHECK(script.deadline == 300);
  fade.advance(cells, 256);
  CHECK(cells[18 * 128 + 9].state == 0);
  CHECK(cells[36 * 128 + 27].state == 255);
  context.clock = 300;
  darker::game::advance_mission_script(script, context);
  fade.advance(cells, 556);
  CHECK(cells[36 * 128 + 27].state == 0);
  context.clock = 600;
  darker::game::advance_mission_script(script, context);
  CHECK(script.stopped);
}

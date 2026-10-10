#include <catch2/catch_test_macros.hpp>
#include <array>
#include "game/hangar.h"
#include "reference/hangar_samples.h"

TEST_CASE("Caero hangar placement matches native startup", "[game][hangar]") {
  /// Compare original placement arithmetic and the three linked cell states across map positions and model heights
  for(auto const &sample : darker::test_reference::hangar_start_samples) {
    darker::game::city_map cells{};
    auto const centre{(sample.site >> 8) * 128 + ((sample.site & 255) >> 1)};
    cells[centre].type = 17;
    darker::game::player_flight player;
    darker::game::hangar_state hangar{.return_site{static_cast<std::uint16_t>(sample.site)}};
    darker::game::initialise_caero_hangar(player, cells, hangar, static_cast<std::int16_t>(sample.model_height));
    auto const &pose{player.pose()};
    CHECK(std::array<int, 3>{pose.position.column, pose.position.row, pose.position.height} == sample.position);
    CHECK(std::array<int, 3>{pose.angles.heading, pose.angles.pitch, pose.angles.roll} == sample.angles);
    CHECK(player.lifecycle.flags == sample.flags);
    CHECK(player.engine_flags == 1);
    CHECK(std::get<darker::game::caero_flight_state>(player.craft).energy.buffer == 0x6000);
    for(auto const index : {centre - 128, centre, centre + 128}) CHECK(cells[index].state == 128);
  }
}

TEST_CASE("Caero gate extension and departure match native updates", "[game][hangar]") {
  /// Preserve the original distance asymmetry, extension saturation and delayed sound-level update
  for(auto const &sample : darker::test_reference::hangar_departure_samples) {
    darker::game::city_map cells{};
    auto const centre{(sample.site >> 8) * 128 + ((sample.site & 255) >> 1)};
    cells[centre].type = 17;
    cells[(sample.y >> 8) * 128 + (sample.x >> 8)].type = static_cast<std::uint8_t>(sample.type);
    darker::game::player_flight player;
    player.pose().position = {static_cast<std::uint16_t>(sample.x), static_cast<std::uint16_t>(sample.y), 0};
    player.lifecycle.flags = 16;
    darker::game::hangar_state hangar{.return_site{static_cast<std::uint16_t>(sample.site)},
      .next_return_site{static_cast<uint16_t>(sample.destination)}, .extension{static_cast<std::uint16_t>(sample.gate)}, .sound_level{0x35}};
    darker::game::advance_hangar_departure(player, cells, hangar, static_cast<std::uint16_t>(sample.step));
    CHECK(hangar.extension == sample.result);
    CHECK(hangar.return_site == sample.return_site);
    CHECK(hangar.sound_level == sample.sound);
    CHECK(player.lifecycle.flags == sample.flags);
    for(auto const index : {centre - 128, centre, centre + 128}) CHECK(cells[index].state == (sample.flags & 16 ? 0 : 128));
  }
}

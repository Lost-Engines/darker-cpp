#include <catch2/catch_test_macros.hpp>
#include <array>
#include <cstddef>
#include <vector>
#include "game/city_sweep.h"
#include "maths/world_coordinates.h"

TEST_CASE("Terrain sweeps preserve original half-height clipping and eighth-height contact quantisation", "[game][collision]") {
  /// No building geometry participates in these terrain-only contacts
  darker::resources::geometry_bank const bank{std::vector<std::byte>(4)};
  std::array<darker::game::city_cell, 128 * 128> const cells{};
  darker::maths::world_position const start{.column{1000}, .row{2000}, .height{100}};
  darker::maths::world_position end{.column{1100}, .row{2020}, .height{0}};
  auto result{darker::game::sweep_city(bank, cells, 0x20, start, end)};
  CHECK(result.contact == darker::game::city_contact::terrain);
  CHECK(end == darker::maths::world_position{.column{1080}, .row{2016}, .height{16}});
  end = {1100, 2020, 21};
  result = darker::game::sweep_city(bank, cells, 0x20, start, end);
  CHECK(result.contact == darker::game::city_contact::none);
  CHECK(end == darker::maths::world_position{.column{1100}, .row{2020}, .height{21}});
  end = {1100, 2020, 0};
  result = darker::game::sweep_city(bank, cells, 0x20, {1000, 2000, 16}, end);
  CHECK(result.contact == darker::game::city_contact::terrain);
  CHECK(end == darker::maths::world_position{.column{1100}, .row{2020}, .height{0}});
}

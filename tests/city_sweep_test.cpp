#include <catch2/catch_test_macros.hpp>
#include <array>
#include <cstddef>
#include <vector>
#include "game/city_sweep.h"

TEST_CASE("Terrain sweeps preserve original half-height clipping and eighth-height contact quantisation", "[game][collision]") {
  /// No building geometry participates in these terrain-only contacts
  darker::resources::geometry_bank const bank{std::vector<std::byte>(4)};
  std::array<darker::game::city_cell, 128 * 128> const cells{};
  std::array<std::uint16_t, 3> const start{1000, 2000, 100};
  std::array<std::uint16_t, 3> end{1100, 2020, 0};
  auto result{darker::game::sweep_city(bank, cells, 0x20, start, end)};
  CHECK(result.contact == darker::game::city_contact::terrain);
  CHECK(end == std::array<std::uint16_t, 3>{1080, 2016, 16});
  end = {1100, 2020, 21};
  result = darker::game::sweep_city(bank, cells, 0x20, start, end);
  CHECK(result.contact == darker::game::city_contact::none);
  CHECK(end == std::array<std::uint16_t, 3>{1100, 2020, 21});
  end = {1100, 2020, 0};
  result = darker::game::sweep_city(bank, cells, 0x20, {1000, 2000, 16}, end);
  CHECK(result.contact == darker::game::city_contact::terrain);
  CHECK(end == std::array<std::uint16_t, 3>{1100, 2020, 0});
}

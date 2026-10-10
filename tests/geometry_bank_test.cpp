#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>
#include "resources/geometry_bank.h"

namespace {

std::vector<std::byte> bank_bytes() {
  /// Three linked headers distinguish alternate-first selection from damage-first selection
  std::vector<std::byte> data(49);
  data[0] = std::byte{1};
  data[3] = std::byte{128};
  data[4] = std::byte{64};
  data[5] = std::byte{255};
  data[7] = std::byte{3};
  data[9] = std::byte{1};
  data[12] = std::byte{33};
  data[14] = std::byte{22};
  data[16] = std::byte{11};
  data[25] = std::byte{11};
  data[36] = std::byte{234};
  data[37] = std::byte{255};
  data[47] = std::byte{42};
  data[48] = std::byte{43};
  return data;
}

} // anonymous namespace

TEST_CASE("Geometry banks retain typed directories, aliases and bounded resource views", "[resources][models]") {
  /// Preserve original pool-relative offsets instead of rewriting or copying model bytecode
  darker::resources::geometry_bank const bank{bank_bytes()};
  REQUIRE(bank.city_types().size() == 1);
  CHECK(bank.city_types()[0].column_fraction == 128);
  CHECK(bank.city_types()[0].row_fraction == 64);
  CHECK(bank.city_types()[0].collision_marker == 255);
  CHECK(bank.city_types()[0].variant_limit == 3);
  CHECK(bank.special_models()[0] == bank.city_types()[0].model_offset);
  CHECK(bank.model_pool().size() == 33);
  REQUIRE(bank.world_data().size() == 2);
  CHECK(bank.world_data()[0] == std::byte{42});
  CHECK(bank.city_model_offset(1, 0, 0x20) == 0);
  CHECK(bank.city_model_offset(1, 0x80, 0x20) == 11);
  CHECK(bank.city_model_offset(1, 0xa0, 0x20) == 22);
  CHECK(bank.city_model_offset(1, 0xc0, 0x60) == 0);
  CHECK(bank.city_model_offset(1, 0xc0, 0x20) == 11);
}

TEST_CASE("Geometry banks reject truncated directories, pools and invalid model links", "[resources][models]") {
  /// Bounds checks cover original pack parsing and later state-driven link traversal
  auto const original{bank_bytes()};
  for(size_t size{0}; size < 47; ++size) {
    auto truncated{original};
    truncated.resize(size);
    REQUIRE_THROWS_AS(darker::resources::geometry_bank{std::move(truncated)}, std::invalid_argument);
  }
  auto invalid{original};
  invalid[1] = std::byte{32};
  REQUIRE_THROWS_AS(darker::resources::geometry_bank{std::move(invalid)}, std::invalid_argument);
  invalid = original;
  invalid[16] = std::byte{255};
  darker::resources::geometry_bank const bank{std::move(invalid)};
  REQUIRE_THROWS_AS(bank.city_model_offset(1, 0x80, 0x20), std::invalid_argument);
  REQUIRE_THROWS_AS(bank.city_model_offset(0, 0, 0x20), std::out_of_range);
}

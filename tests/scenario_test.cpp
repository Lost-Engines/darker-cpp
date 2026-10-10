#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>
#include "resources/campaign.h"
#include "resources/scenario.h"

namespace {

std::vector<std::byte> presentation() {
  /// A one-opcode shared program followed by three distinct language payloads
  std::vector<std::byte> result;
  for(unsigned char const value : {14, 0, 11, 0, 12, 0, 13, 0, 4, 0, 50, 255, 35, 65, 66, 67}) result.push_back(static_cast<std::byte>(value));
  return result;
}

} // anonymous namespace

TEST_CASE("Scenario language and program ranges survive resource copies", "[resources][scenario]") {
  /// Copying the owner must not leave language views pointing into the old allocation
  auto original{presentation()};
  auto const second{presentation()};
  original.insert(original.end(), second.begin(), second.end());
  darker::resources::scenario_resource const resource{std::move(original)};
  auto const copy{resource};
  REQUIRE(copy.records().size() == 2);
  CHECK(copy.records()[1].entry_offset == 28);
  CHECK_FALSE(copy.records()[0].player_program.has_value());
  CHECK(copy.language(1, darker::resources::scenario_language::english)[0] == std::byte{65});
  CHECK(copy.language(0, darker::resources::scenario_language::french)[0] == std::byte{66});
  CHECK(copy.language(1, darker::resources::scenario_language::german)[0] == std::byte{67});
  CHECK_THROWS_AS(copy.language(2, darker::resources::scenario_language::english), std::out_of_range);
  CHECK_THROWS_AS(copy.bytes({
    .offset{33},
    .size{0}
  }), std::out_of_range);
}

TEST_CASE("Scenario reader rejects truncated records and inverted language ranges", "[resources][scenario]") {
  /// Bad length fields and entries must fail before a bytecode consumer receives their ranges
  auto const original{presentation()};
  for(size_t size{1}; size < original.size(); ++size) {
    auto bytes{original};
    bytes.resize(size);
    CHECK_THROWS_AS(darker::resources::scenario_resource{std::move(bytes)}, std::invalid_argument);
  }
  for(auto const offset : {2, 4, 6, 8}) {
    auto bytes{original};
    bytes[offset] = std::byte{255};
    CHECK_THROWS_AS(darker::resources::scenario_resource{std::move(bytes)}, std::invalid_argument);
  }
}

TEST_CASE("Campaign stages select consecutive archive records", "[resources][scenario]") {
  /// Saved stages are one-based while resources and records are zero-based
  for(unsigned int stage{1}; stage <= 120; ++stage) {
    auto const selection{darker::resources::select_campaign_stage(static_cast<uint8_t>(stage))};
    CHECK(selection.resource.archive == 4);
    CHECK(selection.resource.slot * 8 + selection.record == stage - 1);
    CHECK(selection.record < 8);
  }
  CHECK_THROWS_AS(darker::resources::select_campaign_stage(0), std::out_of_range);
  CHECK_THROWS_AS(darker::resources::select_campaign_stage(121), std::out_of_range);
}

#include <catch2/catch_test_macros.hpp>
#include <array>
#include <cstddef>
#include <stdexcept>
#include "game/mission_script.h"
#include "reference/mission_script_samples.h"

TEST_CASE("Mission deadlines, checkpoint retries and message scheduling match native handlers", "[game][mission]") {
  /// Compare the scheduler with original code, including half-range clocks and overdue catch-up
  for(auto const &sample : darker::test_reference::mission_script_samples) {
    std::array<std::byte, 16> program{};
    for(std::size_t i{0}; i < sample.size; ++i) program[i] = static_cast<std::byte>(sample.program[i]);
    std::array<std::byte, 6> const text{std::byte{0}, std::byte{24}, std::byte{3}, std::byte{'A'}, std::byte{'B'}, std::byte{'C'}};
    auto const &input{sample.input};
    std::array<std::uint8_t, 4> const flags{0, 0, 0, static_cast<std::uint8_t>(input[6])};
    darker::game::city_map cells{};
    cells[20 * 128 + 10].state = static_cast<std::uint8_t>(input[6]);
    darker::game::mission_script script{
      .checkpoint{static_cast<std::size_t>(input[3])}, .checkpoint_clock{static_cast<std::uint16_t>(input[4])},
      .deadline{static_cast<std::uint16_t>(input[1])},
    };
    darker::game::mission_context context{
      .program{std::span{program}.first(sample.size)}, .text{text}, .object_flags{flags}, .cells{cells}, .clock{static_cast<std::uint32_t>(input[0])},
      .time_multiplier{static_cast<std::uint8_t>(input[2])}, .objectives_complete{input[5] == 0},
      .suppress_messages{input[8] != 0}, .object_counter{static_cast<std::uint8_t>(input[6])}, .counter{static_cast<std::uint8_t>(input[7])},
    };
    auto const count{darker::game::advance_mission_script(script, context)};
    std::array<int, 8> const result{
      static_cast<int>(script.stopped ? 0 : script.continuation), script.deadline, static_cast<int>(script.checkpoint), script.checkpoint_clock,
      script.stopped, static_cast<int>(context.text_cursor), context.messages.empty() ? 0 : context.messages.back().expiry, static_cast<int>(count),
    };
    INFO("clock=" << input[0] << ", opcode=" << sample.program[0]);
    CHECK(result == sample.output);
  }
}

TEST_CASE("First mission waits for objectives before its return message", "[game][mission]") {
  /// Carry the same live state through the independently traced first-mission timeline
  std::array<std::byte, 11> const program{std::byte{0x1a}, std::byte{0x1e}, std::byte{0x22}, std::byte{10},
    std::byte{0x0c}, std::byte{25}, std::byte{27}, std::byte{0x0c}, std::byte{40}, std::byte{42}, std::byte{0x23}};
  std::array<std::byte, 6> const text{std::byte{0}, std::byte{24}, std::byte{3}, std::byte{'A'}, std::byte{'B'}, std::byte{'C'}};
  darker::game::mission_script script{.deadline{1000}};
  darker::game::mission_context context{.program{program}, .text{text}, .clock{1000}};
  darker::game::advance_mission_script(script, context);
  CHECK(script.deadline == 1400);
  context.objectives_complete = true;
  context.clock = 1399;
  CHECK(darker::game::advance_mission_script(script, context) == 0);
  context.clock = 1400;
  darker::game::advance_mission_script(script, context);
  CHECK(script.deadline == 1900);
  CHECK(context.messages.empty());
  context.clock = 1900;
  darker::game::advance_mission_script(script, context);
  REQUIRE(context.messages.size() == 1);
  CHECK(context.messages[0].offset == 3);
  CHECK(context.messages[0].expiry == 3900);
  CHECK(script.deadline == 4000);
  CHECK_FALSE(script.stopped);
  context.clock = 4000;
  darker::game::advance_mission_script(script, context);
  CHECK(script.stopped);
}

TEST_CASE("Mission scripts diagnose unsupported commands and zero-time loops", "[game][mission]") {
  /// Incomplete reconstruction and malformed data must not silently advance a mission
  for(auto const opcode : {0, 0x22, 0x25}) {
    std::array<std::byte, 1> const program{static_cast<std::byte>(opcode)};
    darker::game::mission_script script;
    darker::game::mission_context context{.program{program}};
    CHECK_THROWS(darker::game::advance_mission_script(script, context));
  }
  std::array<std::byte, 2> const loop{std::byte{0x25}, std::byte{0xfe}};
  darker::game::mission_script script;
  darker::game::mission_context context{.program{loop}};
  CHECK_THROWS_AS(darker::game::advance_mission_script(script, context), std::runtime_error);
}

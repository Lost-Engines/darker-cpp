#include <catch2/catch_test_macros.hpp>
#include <array>
#include <bit>
#include <cstddef>
#include <stdexcept>
#include <utility>
#include "game/actor_activation.h"
#include "game/mission_script.h"
#include "game/skimma_weapons.h"
#include "reference/actor_retirement_samples.h"
#include "reference/mission_script_samples.h"
#include "reference/script_target_samples.h"
#include "reference/supply_script_samples.h"

TEST_CASE("Mission deadlines, checkpoint retries and message scheduling match native handlers", "[game][mission]") {
  /// Compare the scheduler with original code, including half-range clocks and overdue catch-up
  for(auto const &sample : darker::test_reference::mission_script_samples) {
    std::array<std::byte, 16> program{};
    for(size_t i{0}; i < sample.size; ++i) program[i] = static_cast<std::byte>(sample.program[i]);
    std::array<std::byte, 6> const text{std::byte{0}, std::byte{24}, std::byte{3}, std::byte{'A'}, std::byte{'B'}, std::byte{'C'}};
    auto const &input{sample.input};
    std::array<uint8_t, 4> const flags{0, 0, 0, static_cast<uint8_t>(input[6])};
    darker::game::city_map cells{};
    cells[20 * 128 + 10].state = static_cast<uint8_t>(input[6]);
    darker::game::mission_script script{
      .checkpoint{static_cast<size_t>(input[3])},
      .checkpoint_clock{static_cast<uint16_t>(input[4])},
      .deadline{static_cast<uint16_t>(input[1])},
    };
    darker::game::mission_context context{
      .program{std::span{program}.first(sample.size)},
      .text{text},
      .object_flags{flags},
      .cells{cells},
      .clock{static_cast<uint32_t>(input[0])},
      .time_multiplier{static_cast<uint8_t>(input[2])},
      .objectives_complete{input[5] == 0},
      .suppress_messages{input[8] != 0},
      .object_counter{static_cast<uint8_t>(input[6])},
      .counter{static_cast<uint8_t>(input[7])},
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
  darker::game::mission_script script{
    .deadline{1000}
  };
  darker::game::mission_context context{
    .program{program},
    .text{text},
    .clock{1000}
  };
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
    darker::game::mission_context context{
      .program{program}
    };
    CHECK_THROWS(darker::game::advance_mission_script(script, context));
  }
  std::array<std::byte, 2> const loop{std::byte{0x25}, std::byte{0xfe}};
  darker::game::mission_script script;
  darker::game::mission_context context{
    .program{loop}
  };
  CHECK_THROWS_AS(darker::game::advance_mission_script(script, context), std::runtime_error);
}

TEST_CASE("Script target assignments match native tokens flags and pacing", "[game][mission]") {
  /// Every targeting opcode yields for ten scenario intervals while preserving unrelated object flags
  for(auto const &v : darker::test_reference::script_target_samples) {
    std::vector<std::byte> program{static_cast<std::byte>(v[0])};
    if(v[0] < 4) program.push_back(static_cast<std::byte>(v[5] & 255));
    if(v[0] < 2) program.push_back(static_cast<std::byte>(v[5] >> 8));
    program.push_back(std::byte{0x23});
    auto token{static_cast<uint16_t>(v[4])};
    auto flags{static_cast<uint8_t>(v[2])};
    darker::game::mission_script script{
      .deadline{static_cast<uint16_t>(v[1])}
    };
    darker::game::mission_context context{
      .program{program},
      .clock{v[1]},
      .time_multiplier{static_cast<uint8_t>(v[6])},
      .current_cell{static_cast<uint16_t>(v[3])},
      .set_target{[&](uint16_t const target, bool const flag_02){
        token = target;
        flags = static_cast<uint8_t>((flags & 0xfd) | (flag_02 ? 2 : 0));
      }}
    };
    darker::game::advance_mission_script(script, context);
    CAPTURE(v);
    CHECK(token == v[7]);
    CHECK(flags == v[8]);
    CHECK(script.deadline == v[9]);
    CHECK(script.continuation == v[10]);
    CHECK_FALSE(script.stopped);
  }
}

TEST_CASE("Distant actor retirement matches native distance boundaries and script waits") {
  /// Execute opcode 08 with the actual retirement consumer, including byte wrapping and delayed removal
  for(auto const &v : darker::test_reference::actor_retirement_samples) {
    CAPTURE(v);
    darker::game::scenario_actor actor;
    actor.pose.position = {
      .column{static_cast<uint16_t>(v[0] * 256)},
      .row{static_cast<uint16_t>(v[1] * 256)},
      .height{0}
    };
    actor.flags = static_cast<uint8_t>(v[4]);
    actor.parameters.update_entry = darker::game::object_update::surface_actor;
    actor.expiry = 0x1234;
    darker::game::object_pose const player{
      .position{
        .column{static_cast<uint16_t>(v[2] * 256)},
        .row{static_cast<uint16_t>(v[3] * 256)},
        .height{0}
      }
    };
    std::array constexpr program{std::byte{8}, std::byte{0x23}};
    darker::game::mission_script script{
      .deadline{static_cast<uint16_t>(v[5])}
    };
    darker::game::mission_context context{
      .program{program},
      .clock{v[5]},
      .time_multiplier{static_cast<uint8_t>(v[6])},
      .retire_distant_actor{[&]{
        return darker::game::retire_distant_actor(actor, player, static_cast<uint16_t>(v[5]));
      }}
    };
    darker::game::advance_mission_script(script, context);
    CHECK(actor.flags == v[7]);
    CHECK(script.stopped == (v[8] != 0));
    CHECK(script.deadline == v[9]);
    CHECK(std::to_underlying(actor.parameters.update_entry) == v[10]);
    CHECK(actor.expiry == v[11]);
    if(!script.stopped) CHECK(script.continuation == 0);
  }
}

TEST_CASE("Supply script state and ammunition match the original scheduler", "[game][mission]") {
  /// Execute combined original command sequences including monotonic progress and wrapped mask operands
  for(auto const &s : darker::test_reference::supply_script_samples) {
    CAPTURE(s.input);
    std::array<std::byte, 15> program;
    for(size_t i{0}; i < program.size(); ++i) program[i] = static_cast<std::byte>(s.program[i]);
    auto mask{static_cast<uint16_t>(s.input[1])}, shield{static_cast<uint16_t>(s.input[2])};
    darker::game::weapon_ammunition ammunition{static_cast<uint8_t>(s.input[4]), static_cast<uint8_t>(s.input[5])};
    darker::game::mission_script script;
    darker::game::mission_context context{
      .program{program}
    };
    context.progress = static_cast<uint8_t>(s.input[0]);
    context.toggle_weapons = [&](uint16_t const value){
      mask ^= value;
    };
    context.reset_shield = [&]{
      shield = static_cast<uint16_t>((shield & 255) | 0xbf00);
    };
    context.refill_weapon = [&]{
      darker::game::refill_skimma_weapon(ammunition, static_cast<uint8_t>(s.input[3]));
    };
    darker::game::advance_mission_script(script, context);
    CHECK(script.stopped);
    CHECK(std::array<int, 8>{context.message_setting, context.hud_reference, context.transition_output, context.progress,
      mask, shield, ammunition.working, ammunition.reserve} == s.output);
  }
}

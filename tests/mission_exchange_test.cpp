#include <array>
#include <utility>
#include <catch2/catch_test_macros.hpp>
#include "game/mission_exchange.h"
#include "reference/mission_exchange_samples.h"

TEST_CASE("Mission owner registration stops the native owner and retains its predecessor", "[game][missions]") {
  /// Match C2E4's shared registration, reused checkpoint field and terminal scheduler state
  std::array const program{std::byte{0x24}};
  for(auto const &s : darker::test_reference::mission_owner_samples) {
    CAPTURE(s);
    uint16_t owner{static_cast<uint16_t>(s[1])};
    darker::game::mission_script script{.deadline{static_cast<uint16_t>(s[0])}};
    darker::game::mission_context context{.program{program},.clock{static_cast<uint32_t>(s[0])}};
    context.register_owner = [&]{ return std::exchange(owner,uint16_t{0xd986}); };
    CHECK(darker::game::advance_mission_script(script,context) == 1);
    CHECK(owner == s[2]);
    CHECK(script.checkpoint == static_cast<size_t>(s[3]));
    CHECK(script.deadline == s[4]);
    CHECK(script.stopped == (s[5] != 0));
  }
}

TEST_CASE("Consecutive mission exchanges resume immediately with the original message cursors", "[game][missions]") {
  /// Run both native 26 instructions in one scheduler call rather than yielding between contexts
  std::array const primary{std::byte{0x26},std::byte{0x23}};
  std::array const secondary{std::byte{0x0c},std::byte{1},std::byte{0},std::byte{0x26},std::byte{0x23}};
  std::array const primary_text{std::byte{1}};
  std::array const secondary_text{std::byte{24},std::byte{3},std::byte{65},std::byte{66},std::byte{67}};
  for(auto const &s : darker::test_reference::mission_exchange_samples) {
    darker::game::mission_script script{.deadline{static_cast<uint16_t>(s[0])}};
    darker::game::mission_context context{.program{primary},.text{primary_text},.clock{static_cast<uint32_t>(s[0])}};
    darker::game::mission_exchange exchange{.alternate{darker::game::mission_context_slot{secondary,secondary_text,0}}};
    context.exchange_context = [&](auto &active){ exchange.exchange(active,context,active.continuation); };
    CHECK(darker::game::advance_mission_script(script,context) == 4);
    CHECK(script.deadline == s[1]);
    CHECK(context.text.data() == primary_text.data());
    CHECK(context.text_cursor == static_cast<size_t>(s[2]));
    REQUIRE(exchange.alternate);
    CHECK(exchange.alternate->text.data() == secondary_text.data());
    CHECK(exchange.alternate->text_cursor == static_cast<size_t>(s[3]));
    CHECK(exchange.alternate->continuation == static_cast<size_t>(s[4]));
    CHECK(exchange.supplementary_active == (s[5] != 0));
    CHECK(script.stopped == (s[6] != 0));
    REQUIRE(context.messages.size() == 1);
    CHECK(context.messages.front().text.data() == secondary_text.data());
    CHECK(context.messages.front().offset == static_cast<size_t>(s[7]));
    CHECK(context.messages.front().expiry == s[8]);
  }
}

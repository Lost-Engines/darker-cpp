#include <catch2/catch_test_macros.hpp>
#include <array>
#include "game/game_clock.h"
#include "reference/game_clock_samples.h"

TEST_CASE("Game clock accumulation and frame snapshots match native timer sequences", "[game][clock]") {
  /// Include pause, wrapping, changed step limits and saturated frame time without involving hardware polling
  for(auto const &sample : darker::test_reference::game_clock_samples) {
    CAPTURE(sample.input, sample.before);
    auto const &before{sample.before};
    darker::game::game_clock clock{
      .ticks{static_cast<std::uint16_t>(before[0])},
      .frame_ticks{static_cast<std::uint16_t>(before[1])},
      .pending_changes{static_cast<std::uint16_t>(before[2])},
      .frame_changes{static_cast<std::uint16_t>(before[3])},
      .wraps{static_cast<std::uint8_t>(before[4])},
      .step_limit{static_cast<std::uint16_t>(sample.input[0])},
      .running{sample.input[1] != 0},
    };
    darker::game::advance_game_clock(clock, static_cast<std::uint64_t>(sample.input[2]));
    auto const step{sample.input[3] != 0 ? darker::game::consume_game_frame(clock) : 0};
    CHECK(step == sample.step);
    CHECK(std::array<int, 5>{clock.ticks, clock.frame_ticks, clock.pending_changes, clock.frame_changes, clock.wraps} == sample.after);
  }
}

TEST_CASE("Coalesced game clock interrupts retain wrapping and saturation across long host delays", "[game][clock]") {
  /// Fast-forward must remain equivalent to individual interrupts, including more than one complete clock period
  for(auto const limit : {80, 65535}) {
    darker::game::game_clock batched{
      .ticks{65530},
      .frame_ticks{65530},
      .step_limit{static_cast<std::uint16_t>(limit)}
    };
    auto individual{batched};
    darker::game::advance_game_clock(batched, 131079);
    for(unsigned int i{0}; i < 131079; ++i) darker::game::advance_game_clock(individual, 1);
    CHECK(batched.ticks == individual.ticks);
    CHECK(batched.pending_changes == individual.pending_changes);
    CHECK(darker::game::consume_game_frame(batched) == darker::game::consume_game_frame(individual));
  }
}

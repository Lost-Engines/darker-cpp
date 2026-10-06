#include <catch2/catch_test_macros.hpp>
#include <array>
#include <cstdint>
#include <stdexcept>
#include "game/skimma_weapons.h"
#include "reference/weapon_samples.h"

TEST_CASE("Skimma automatic reload matches original counters, byte signs and deadline wrapping") {
  for(auto const &sample : darker::test_reference::reload_samples) {
    CAPTURE(sample.weapon, sample.clock, sample.working, sample.reserve);
    darker::game::weapon_ammunition ammunition{
      .working{static_cast<std::uint8_t>(sample.working)}, .reserve{static_cast<std::uint8_t>(sample.reserve)},
    };
    darker::game::weapon_ring_state ring{.reload_deadline{123}, .spread{252}};
    bool const changed{darker::game::reload_skimma_weapon(ammunition, ring,
      static_cast<std::uint8_t>(sample.weapon), static_cast<std::uint16_t>(sample.clock))};
    CHECK(changed == (sample.next_working != sample.working));
    CHECK(ammunition.working == sample.next_working);
    CHECK(ammunition.reserve == sample.next_reserve);
    CHECK(ring.reload_deadline == sample.deadline);
    CHECK(ring.spread == sample.spread);
  }
}

TEST_CASE("Skimma ring timing matches native suppression, spent indicators and radius truncation") {
  for(auto const &sample : darker::test_reference::ring_samples) {
    for(auto const deadline : std::array<std::uint16_t, 3>{0, 1024, 65500}) {
      CAPTURE(sample.delta, sample.enabled, sample.spread, deadline);
      auto const display{darker::game::calculate_weapon_ring({.working{7}, .reserve{3}},
        {.reload_deadline{deadline}, .spread{static_cast<std::uint16_t>(sample.spread)}},
        static_cast<std::uint16_t>(deadline + sample.delta), static_cast<std::uint8_t>(sample.enabled | 0x80))};
      REQUIRE(display.has_value() == static_cast<bool>(sample.visible));
      if(display) {
        CHECK(display->radius == sample.radius);
        CHECK(display->remaining == sample.remaining);
      }
    }
  }
}

TEST_CASE("Skimma resupply restores native working and reserve capacities") {
  std::array<std::uint8_t, 3> constexpr working{14, 8, 10};
  std::array<std::uint8_t, 3> constexpr reserve{5, 3, 4};
  for(std::uint8_t weapon{0}; weapon < 3; ++weapon) {
    darker::game::weapon_ammunition ammunition{.working{1}, .reserve{1}};
    darker::game::refill_skimma_weapon(ammunition, weapon);
    CHECK(ammunition.working == working[weapon]);
    CHECK(ammunition.reserve == reserve[weapon]);
  }
  darker::game::weapon_ammunition ammunition;
  REQUIRE_THROWS_AS(darker::game::refill_skimma_weapon(ammunition, 3), std::invalid_argument);
}

TEST_CASE("Skimma ring draws before smoothing and leaves reload state unchanged before deadline") {
  for(auto const &sample : darker::test_reference::update_samples) {
    CAPTURE(sample.clock, sample.delta, sample.spread, sample.step, sample.enabled);
    darker::game::weapon_ring_state ring{
      .reload_deadline{static_cast<std::uint16_t>(sample.clock - sample.delta)}, .spread{static_cast<std::uint16_t>(sample.spread)},
    };
    auto const display{darker::game::update_weapon_ring({.working{7}, .reserve{3}}, ring,
      static_cast<std::uint16_t>(sample.clock), static_cast<std::uint8_t>(sample.enabled), static_cast<std::uint16_t>(sample.step))};
    CHECK(ring.reload_deadline == sample.deadline);
    CHECK(ring.spread == sample.next_spread);
    REQUIRE(display.has_value() == static_cast<bool>(sample.visible));
    if(display) {
      CHECK(display->radius == sample.radius);
      CHECK(display->remaining == sample.remaining);
    }
  }
}

TEST_CASE("Skimma status update reloads before reserve selection and preserves native target branches") {
  for(auto const &sample : darker::test_reference::status_samples) {
    CAPTURE(sample.slots, sample.selected, sample.flags, sample.working, sample.reserve, sample.target, sample.count);
    std::array<darker::game::skimma_weapon_slot, 3> weapons;
    for(auto &slot : weapons) {
      slot = {.ammunition{.working{static_cast<std::uint8_t>(sample.working)}, .reserve{static_cast<std::uint8_t>(sample.reserve)}},
        .flags{static_cast<std::uint8_t>(sample.flags)}};
    }
    darker::game::weapon_ring_state ring{.reload_deadline{123}, .spread{252}};
    auto const display{darker::game::update_skimma_weapon_status(std::span{weapons}.first(static_cast<std::size_t>(sample.slots)), ring,
      static_cast<std::uint8_t>(sample.selected), 65000, static_cast<std::int16_t>(sample.target), static_cast<std::uint16_t>(sample.count))};
    CHECK(display == sample.display);
    CHECK(weapons[0].flags == sample.flag0);
    CHECK(weapons[1].flags == sample.flag1);
    CHECK(weapons[2].flags == sample.flag2);
    for(std::size_t i{0}; i < weapons.size(); ++i) {
      CHECK(weapons[i].ammunition.working == (i == static_cast<std::size_t>(sample.selected) ? sample.next_working : sample.working));
      CHECK(weapons[i].ammunition.reserve == (i == static_cast<std::size_t>(sample.selected) ? sample.next_reserve : sample.reserve));
    }
    CHECK(ring.reload_deadline == sample.deadline);
    CHECK(ring.spread == sample.spread);
  }
}

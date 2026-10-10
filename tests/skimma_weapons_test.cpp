#include <catch2/catch_test_macros.hpp>
#include <array>
#include <bit>
#include <cstdint>
#include <stdexcept>
#include "game/object_definitions.h"
#include "game/skimma_weapons.h"
#include "maths/world_coordinates.h"
#include "reference/recoil_samples.h"
#include "reference/skimma_firing_samples.h"
#include "reference/skimma_gun_samples.h"
#include "reference/skimma_selection_samples.h"
#include "reference/weapon_samples.h"

TEST_CASE("Skimma automatic reload matches original counters, byte signs and deadline wrapping") {
  for(auto const &sample : darker::test_reference::reload_samples) {
    CAPTURE(sample.weapon, sample.clock, sample.working, sample.reserve);
    darker::game::skimma_armament armament;
    auto &ammunition{armament.slots[sample.weapon].ammunition};
    ammunition = {
      .working{static_cast<uint8_t>(sample.working)},
      .reserve{static_cast<uint8_t>(sample.reserve)},
    };
    auto &ring{armament.ring};
    ring = {
      .reload_deadline{123},
      .spread{252},
      .target_spread{17}
    };
    bool const changed{armament.reload(
      static_cast<uint8_t>(sample.weapon), static_cast<uint16_t>(sample.clock))};
    CHECK(changed == (sample.next_working != sample.working));
    CHECK(ammunition.working == sample.next_working);
    CHECK(ammunition.reserve == sample.next_reserve);
    CHECK(ring.reload_deadline == sample.deadline);
    CHECK(ring.spread == sample.spread);
    CHECK(ring.target_spread == 17);
  }
}

TEST_CASE("Skimma ring timing matches native suppression, spent indicators and radius truncation") {
  for(auto const &sample : darker::test_reference::ring_samples) {
    for(auto const deadline : std::array<uint16_t, 3>{0, 1024, 65500}) {
      CAPTURE(sample.delta, sample.enabled, sample.spread, deadline);
      auto const display{darker::game::calculate_weapon_ring({
        .working{7},
        .reserve{3}
      },
        {
          .reload_deadline{deadline},
          .spread{static_cast<uint16_t>(sample.spread)}
        },
        static_cast<uint16_t>(deadline + sample.delta), static_cast<uint8_t>(sample.enabled | 0x80))};
      REQUIRE(display.has_value() == static_cast<bool>(sample.visible));
      if(display) {
        CHECK(display->radius == sample.radius);
        CHECK(display->remaining == sample.remaining);
      }
    }
  }
}

TEST_CASE("Skimma resupply restores native working and reserve capacities") {
  std::array<uint8_t, 3> constexpr working{14, 8, 10};
  std::array<uint8_t, 3> constexpr reserve{5, 3, 4};
  for(uint8_t weapon{0}; weapon < 3; ++weapon) {
    darker::game::weapon_ammunition ammunition{
      .working{1},
      .reserve{1}
    };
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
      .reload_deadline{static_cast<uint16_t>(sample.clock - sample.delta)},
      .spread{static_cast<uint16_t>(sample.spread)},
      .target_spread{static_cast<uint16_t>(sample.target)},
    };
    auto const display{darker::game::update_weapon_ring({
      .working{7},
      .reserve{3}
    }, ring,
      static_cast<uint16_t>(sample.clock), static_cast<uint8_t>(sample.enabled), static_cast<uint16_t>(sample.step))};
    CHECK(ring.reload_deadline == sample.deadline);
    CHECK(ring.spread == sample.next_spread);
    CHECK(ring.target_spread == sample.target);
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
    darker::game::skimma_armament armament;
    auto &weapons{armament.slots};
    for(auto &slot : weapons) {
      slot = {
        .ammunition{
          .working{static_cast<uint8_t>(sample.working)},
          .reserve{static_cast<uint8_t>(sample.reserve)}
        },
        .flags{static_cast<uint8_t>(sample.flags)}
      };
    }
    auto &ring{armament.ring};
    ring = {
      .reload_deadline{123},
      .spread{252},
      .target_spread{17}
    };
    armament.selection = static_cast<uint8_t>(sample.selected);
    armament.update_status(65000, static_cast<int16_t>(sample.target), static_cast<uint16_t>(sample.count), sample.slots == 3);
    auto const display{armament.reserves};
    CHECK(display == sample.display);
    CHECK(weapons[0].flags == sample.flag0);
    CHECK(weapons[1].flags == sample.flag1);
    CHECK(weapons[2].flags == sample.flag2);
    for(size_t i{0}; i < weapons.size(); ++i) {
      CHECK(weapons[i].ammunition.working == (i == static_cast<size_t>(sample.selected) ? sample.next_working : sample.working));
      CHECK(weapons[i].ammunition.reserve == (i == static_cast<size_t>(sample.selected) ? sample.next_reserve : sample.reserve));
    }
    CHECK(ring.reload_deadline == sample.deadline);
    CHECK(ring.spread == sample.spread);
    CHECK(ring.target_spread == 17);
  }
}

TEST_CASE("Skimma recoil frame offsets match every native byte state across step boundaries") {
  for(auto const &sample : darker::test_reference::recoil_frames) {
    CAPTURE(sample.input);
    uint64_t checksum{14695981039346656037ULL};
    auto const append{[&](uint8_t const value){
      checksum = (checksum ^ value) * 1099511628211ULL;
    }};
    for(unsigned int previous{0}; previous < 256; ++previous) {
      auto const frame{darker::game::calculate_skimma_recoil(std::bit_cast<int8_t>(static_cast<uint8_t>(previous)), sample.input)};
      append(static_cast<uint8_t>(frame.next));
      for(auto const value : {frame.aim_offset, frame.shot_offset}) {
        auto const word{static_cast<uint16_t>(value)};
        append(static_cast<uint8_t>(word));
        append(static_cast<uint8_t>(word >> 8));
      }
    }
    CHECK(checksum == sample.checksum);
  }
}

TEST_CASE("Skimma random recoil kicks match every native input byte pair") {
  for(auto const &sample : darker::test_reference::recoil_kicks) {
    CAPTURE(sample.input);
    uint64_t checksum{14695981039346656037ULL};
    for(unsigned int previous{0}; previous < 256; ++previous) {
      auto const next{darker::game::kick_skimma_recoil(std::bit_cast<int8_t>(static_cast<uint8_t>(previous)), static_cast<uint8_t>(sample.input))};
      checksum = (checksum ^ static_cast<uint8_t>(next)) * 1099511628211ULL;
    }
    CHECK(checksum == sample.checksum);
  }
}

TEST_CASE("Skimma primary gun rays match native recoil and random spread", "[game][weapons]") {
  /// Compare endpoints and consumed random state before city or aircraft clipping
  for(auto const &sample : darker::test_reference::skimma_gun_samples) {
    CAPTURE(sample);
    darker::game::object_pose player{
      .position{
        .column{static_cast<uint16_t>(sample[0])},
        .row{static_cast<uint16_t>(sample[1])},
        .height{static_cast<uint16_t>(sample[2])}
      },
      .angles{
        .heading{static_cast<uint16_t>(sample[3])},
        .pitch{static_cast<uint16_t>(sample[4])},
        .roll{0}
      }
    };
    auto random{static_cast<uint16_t>(sample[6])};
    auto const end{darker::game::skimma_gun_endpoint(player, std::bit_cast<int16_t>(static_cast<uint16_t>(sample[5])), random)};
    CHECK(end == darker::maths::world_position{
      .column{static_cast<uint16_t>(sample[7])},
      .row{static_cast<uint16_t>(sample[8])},
      .height{static_cast<uint16_t>(sample[9])}
    });
    CHECK(random == sample[10]);
  }
}

TEST_CASE("Skimma secondary firing matches native status, reload and ammunition changes", "[game][weapons]") {
  /// Run the same status-before-firing sequence as C90A, retaining all three slots even on the ordinary craft
  for(auto const &sample : darker::test_reference::skimma_firing_samples) {
    CAPTURE(sample);
    darker::game::skimma_armament armament;
    auto &weapons{armament.slots};
    for(size_t i{0}; i < 3; ++i) weapons[i] = {
      .ammunition{
        .working{static_cast<uint8_t>(sample[5 + i])},
        .reserve{static_cast<uint8_t>(sample[8 + i])}
      },
      .flags{static_cast<uint8_t>(sample[2 + i])}
    };
    auto &ring{armament.ring};
    ring = {
      .reload_deadline{static_cast<uint16_t>(sample[12])},
      .spread{static_cast<uint16_t>(sample[13])}
    };
    auto &selected{armament.selection};
    selected = static_cast<uint8_t>(sample[1]);
    armament.update_status(static_cast<uint16_t>(sample[11]),
      std::bit_cast<int16_t>(static_cast<uint16_t>(sample[14])), static_cast<uint16_t>(sample[15]), sample[0] == 3);
    auto const reserve{armament.reserves};
    darker::game::projectile_pool pool;
    darker::game::launch_emitter const emitter{
      .definition_strength{40}
    };
    if(!sample[15]) {
      for(size_t i{0}; i < pool.capacity; ++i) REQUIRE(pool.launch({
        .definition{darker::game::original_object_definitions[0]},
        .emitter{emitter}
      }));
    }
    auto *shot{darker::game::fire_skimma_weapon(pool, weapons[selected], {
      .emitter{emitter},
      .weapon{selected},
      .player_flags{static_cast<uint8_t>(sample[16])},
      .pressed{sample[17] != 0},
      .model{0x400},
      .clock{static_cast<uint16_t>(sample[11])},
      .target{static_cast<uint16_t>(sample[14])}
    })};
    for(size_t i{0}; i < 3; ++i) {
      CHECK(weapons[i].flags == sample[18 + i]);
      CHECK(weapons[i].ammunition.working == sample[21 + i]);
      CHECK(weapons[i].ammunition.reserve == sample[24 + i]);
    }
    CHECK(ring.reload_deadline == sample[27]);
    CHECK(ring.spread == sample[28]);
    CHECK(reserve == sample[29]);
    CHECK((shot != nullptr) == (sample[30] != 0));
    if(shot) {
      CHECK(shot->parameters.definition == &darker::game::original_object_definitions[10 + selected]);
      CHECK(shot->target_token == sample[14]);
      CHECK(shot->deadline == static_cast<uint16_t>(sample[11] + 2560));
    }
  }
}

TEST_CASE("Skimma selection matches native enable toggles and reload delay", "[game][weapons]") {
  /// Compare switching, repeating and unavailable selections while retaining ammunition and unrelated ring state
  for(auto const &s : darker::test_reference::skimma_selection_samples) {
    CAPTURE(s);
    auto const byte{[](int const value){
      return static_cast<uint8_t>(value);
    }};
    auto const word{[](int const value){
      return static_cast<uint16_t>(value);
    }};
    darker::game::skimma_armament armament;
    auto &slots{armament.slots};
    for(size_t i{0}; i < 3; ++i) slots[i].flags = byte(s[2 + i]);
    auto &selected{armament.selection};
    selected = byte(s[1]);
    auto &ring{armament.ring};
    ring = {
      .reload_deadline{word(s[7])},
      .spread{word(s[8])},
      .target_spread{508}
    };
    auto target{word(s[9])};
    if(armament.select(true, byte(s[0]), word(s[5]), word(s[6]))) target = 0xffff;
    CHECK(selected == s[10]);
    CHECK(ring.reload_deadline == s[11]);
    CHECK(ring.spread == s[12]);
    CHECK(target == s[13]);
    for(size_t i{0}; i < 3; ++i) CHECK(slots[i].flags == s[14 + i]);
  }
}

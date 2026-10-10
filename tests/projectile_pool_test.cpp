#include <catch2/catch_test_macros.hpp>
#include <array>
#include <cstdint>
#include "game/object_definitions.h"
#include "game/projectile_pool.h"
#include "reference/placement_samples.h"

TEST_CASE("Assembled projectile creation matches native placement and constructor fields") {
  for(auto const &sample : darker::test_reference::placement_samples) {
    CAPTURE(sample.strength, sample.heading, sample.pitch, sample.roll, sample.edge);
    darker::game::launch_emitter const emitter{
      .position{sample.edge ? std::array<std::uint16_t, 3>{0, 65535, 0} : std::array<std::uint16_t, 3>{1000, 2000, 3000}},
      .fractions{sample.edge ? std::array<std::uint8_t, 3>{255, 1, 128} : std::array<std::uint8_t, 3>{0, 127, 255}},
      .angles{static_cast<std::uint16_t>(sample.heading), static_cast<std::uint16_t>(sample.pitch), static_cast<std::uint16_t>(sample.roll)},
      .speed{0x9876}, .side_flags{static_cast<std::uint8_t>(sample.edge ? 0x80 : 0)}, .definition_strength{static_cast<std::uint8_t>(sample.strength)},
    };
    auto const &definition{darker::game::original_object_definitions[10]};
    darker::game::projectile_launch const request{.definition{definition}, .emitter{emitter}, .clock{65530}, .lifetime{0x0a00}};
    darker::game::projectile_pool pool;
    auto *previous{pool.launch(request)};
    REQUIRE(previous != nullptr);
    previous->angular_motion.fill(0xa5a5);
    pool.recycle(*previous);
    auto const *record{pool.launch(request)};
    REQUIRE(record == previous);
    auto const &p{record->placement};
    std::array<int, 10> const placement{p.position[0], p.position[1], p.position[2], p.fractions[0], p.fractions[1], p.fractions[2],
      p.angles.heading, p.angles.pitch, p.angles.roll, p.speed};
    auto const &parameters{record->parameters};
    std::array<int, 15> const metadata{parameters.model_token, parameters.update_entry, parameters.flags_4c, parameters.angular_response,
      parameters.motion[0], parameters.motion[1], parameters.motion[2], record->angular_motion[0], record->angular_motion[1], record->angular_motion[2],
      record->flags, record->lifecycle, record->inherited_roll, record->deadline, record->target_token};
    CHECK(placement == sample.result);
    CHECK(metadata == sample.metadata);
    CHECK(parameters.definition == &definition);
  }
}

TEST_CASE("Projectile pool preserves native allocation order and does not evict on exhaustion") {
  darker::game::projectile_pool pool;
  darker::game::launch_emitter const emitter{.angles{0, 0, 0x2345}, .definition_strength{1}};
  darker::game::projectile_launch const request{
    .definition{darker::game::original_object_definitions[10]}, .emitter{emitter}, .model_token{0x4321},
    .clock{65530}, .lifetime{256}, .target_token{0x1234},
  };
  std::array<darker::game::projectile *, 12> allocated{};
  for(std::size_t i{0}; i < allocated.size(); ++i) {
    allocated[i] = pool.launch(request);
    REQUIRE(allocated[i] == &pool.records()[11 - i]);
    CHECK(pool.objects().head == allocated[i]);
    CHECK(pool.objects().tail == allocated[0]);
    CHECK(allocated[i]->deadline == 250);
    CHECK(allocated[i]->target_token == 0x1234);
    CHECK(allocated[i]->parameters.model_token == 0x4321);
  }
  CHECK(pool.objects().free == nullptr);
  CHECK(pool.launch(request) == nullptr);
  CHECK(pool.objects().head == allocated.back());
  CHECK(pool.objects().tail == allocated.front());
  CHECK(pool.objects().free == nullptr);
  auto *current{pool.objects().head};
  for(std::size_t i{allocated.size()}; i-- > 0;) {
    REQUIRE(current == allocated[i]);
    current = current->next;
  }
  CHECK(current == nullptr);
  CHECK(pool.recycle(*allocated[5]) == allocated[4]);
  CHECK(pool.launch(request) == allocated[5]);
  CHECK(pool.objects().head == allocated[5]);
  CHECK(pool.objects().tail == allocated[0]);
  CHECK(pool.objects().free == nullptr);
}

TEST_CASE("Hostile projectile pool has six independent native slots") {
  /// 1D47 follows the twelve player records with six records at D6E6, ending before the player craft at D986
  darker::game::projectile_pool hostile{darker::game::projectile_list::hostile}, player;
  darker::game::launch_emitter const emitter{.definition_strength{40}};
  darker::game::projectile_launch const request{.definition{darker::game::original_object_definitions[10]},.emitter{emitter}};
  REQUIRE(hostile.records().size() == 6);
  for(unsigned int i{0}; i < 6; ++i) {
    auto *shot{hostile.launch(request)};
    REQUIRE(shot != nullptr);
    CHECK(shot->native_id == 0xd6e6 + (5-i)*112);
    CHECK(hostile.resolve(shot->native_id) == shot);
    CHECK(player.resolve(shot->native_id) == nullptr);
  }
  CHECK(hostile.launch(request) == nullptr);
  CHECK(player.launch(request) != nullptr);
  CHECK(hostile.resolve(0xd986) == nullptr);
}

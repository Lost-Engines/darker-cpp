#include <catch2/catch_test_macros.hpp>
#include <array>
#include <cstdint>
#include "game/object_definitions.h"
#include "game/projectile_expiry.h"
#include "reference/expiry_samples.h"

TEST_CASE("Projectile expiry repairs native references and counters before list disposition") {
  for(auto const &sample : darker::test_reference::expiry_samples) {
    CAPTURE(sample.lifecycle, sample.completed, sample.outstanding, sample.selected);
    darker::game::projectile_pool pool;
    darker::game::launch_emitter const emitter;
    darker::game::projectile_launch const request{
      .definition{darker::game::original_object_definitions[0]},
      .emitter{emitter}
    };
    auto *tail{pool.launch(request)};
    auto *record{pool.launch(request)};
    auto *head{pool.launch(request)};
    REQUIRE(tail);
    REQUIRE(record);
    REQUIRE(head);
    REQUIRE(pool.resolve(record->native_id) == record);
    head->target_token = record->native_id;
    record->target_token = record->native_id;
    tail->target_token = 0x1234;
    record->flags = 8;
    record->lifecycle = static_cast<std::uint8_t>(sample.lifecycle);
    darker::game::projectile_references references{
      .selected_target{sample.selected ? record->native_id : head->native_id},
      .reference_2449{sample.selected ? record->native_id : tail->native_id},
      .missile_view{sample.selected ? record->native_id : static_cast<std::uint16_t>(0x1234)},
    };
    darker::game::objective_counters objectives{
      .completed{static_cast<std::uint8_t>(sample.completed)},
      .outstanding{static_cast<std::uint8_t>(sample.outstanding)}
    };
    darker::game::weapon_ring_state ring{
      .target_spread{17}
    };
    auto const *next{darker::game::expire_projectile(pool, *record, references, objectives, ring)};
    auto const id{[](darker::game::projectile const *const p)->int{ return p ? p->native_id : 0; }};
    auto const &list{pool.objects()};
    std::array<int, 17> const actual{id(list.head), id(list.tail), id(list.free), id(next),
      head->target_token, record->target_token, tail->target_token, references.selected_target, references.reference_2449,
      references.missile_view, ring.target_spread, objectives.completed, objectives.outstanding, record->flags, record->lifecycle,
      id(record->next), id(record->previous)};
    CHECK(actual == sample.result);
    CHECK(head->next == tail);
    CHECK(tail->previous == head);
  }
}

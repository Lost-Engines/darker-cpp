#include <catch2/catch_test_macros.hpp>
#include <array>
#include <cstdint>
#include "game/object_definitions.h"
#include "game/projectile_update.h"
#include "reference/update_samples.h"

TEST_CASE("Projectile update sequences preserve native deadline, snapshot and callback ordering") {
  darker::game::projectile record;
  std::array<std::uint8_t, 5> constexpr flags{0x20, 0x20, 0x40, 0, 0x20};
  std::array<std::uint16_t, 5> constexpr lifetimes{128, 1024, 128, 32, 0};
  std::array<std::uint16_t, 5> constexpr altitudes{8192, 0x5000, 8192, 65535, 8192};
  for(auto const &sample : darker::test_reference::update_frame_samples) {
    CAPTURE(sample.callback, sample.mode, sample.target_kind, sample.tick);
    if(sample.tick == 0) {
      record = {};
      auto const mode{static_cast<std::size_t>(sample.mode)};
      record.parameters.definition = &darker::game::original_object_definitions[0];
      record.parameters.update_entry = static_cast<std::uint16_t>(sample.callback);
      record.parameters.angular_response = 480;
      record.placement = {.position{0, 65535, altitudes[mode]}, .fractions{255, 127, 1}, .angles{8192, 4096, 1234}, .speed{1000}};
      record.angular_motion = {0, 100, 65535};
      record.previous_position = {0x1111, 0x2222, 0x3333};
      record.flags = flags[mode];
      record.fade = 123;
      record.deadline = static_cast<std::uint16_t>(65500 + lifetimes[mode]);
    }
    darker::game::object_pose const target{.position{static_cast<std::uint16_t>(sample.target[0]),
      static_cast<std::uint16_t>(sample.target[1]), static_cast<std::uint16_t>(sample.target[2])}};
    // Expiry must not need a target or execute the motion callback.
    auto const *resolved{sample.result[18] ? nullptr : sample.target_kind == 2 ? &record.placement : &target};
    auto const status{darker::game::update_projectile(record, static_cast<std::uint16_t>(sample.clock),
      static_cast<std::uint16_t>(sample.step), resolved)};
    auto const &p{record.placement};
    std::array<int, 19> const actual{p.position[0], p.position[1], p.position[2], p.fractions[0], p.fractions[1], p.fractions[2],
      record.angular_motion[1], record.angular_motion[2], p.angles[0], p.angles[1], p.angles[2], p.speed,
      record.previous_position[0], record.previous_position[1], record.previous_position[2], record.flags, record.fade, record.deadline,
      status == darker::game::projectile_update_result::expired ? 1 : 0};
    CHECK(actual == sample.result);
  }
}

#include <catch2/catch_test_macros.hpp>
#include <array>
#include <cstdint>
#include "game/angular_motion.h"
#include "reference/angular_motion_samples.h"

TEST_CASE("Driven angular response matches native gain and damping entry points", "[game][flight]") {
  /// Compare both entry points used by craft controls and correction paths
  for(auto const &sample : darker::test_reference::angular_motion_samples) {
    CAPTURE(sample.entry, sample.rate, sample.gain, sample.drive, sample.step);
    auto const actual{sample.entry == 0x83df
      ? darker::game::integrate_angular_rate(static_cast<std::uint16_t>(sample.rate), static_cast<std::uint16_t>(sample.drive), static_cast<std::uint16_t>(sample.step))
      : darker::game::calculate_driven_angular_response(static_cast<std::uint16_t>(sample.rate), static_cast<std::uint16_t>(sample.gain),
          static_cast<std::uint16_t>(sample.drive), static_cast<std::uint16_t>(sample.step))};
    CHECK(actual.rate == sample.result_rate);
    CHECK(actual.angle_delta == sample.delta);
    CHECK(actual.frame_step == sample.result_step);
  }
}

TEST_CASE("Bank folding and attitude normalisation match every native angle", "[game][flight]") {
  /// Fingerprint native words in little-endian order, without storing half a million fixture bytes
  std::uint64_t fingerprint{0xcbf29ce484222325};
  for(unsigned int angle{0}; angle < 65536; ++angle) {
    std::array<std::uint16_t, 3> angles{0x1234, static_cast<std::uint16_t>(angle), 0x5678};
    auto const folded{darker::game::fold_bank_angle(static_cast<std::uint16_t>(angle))};
    darker::game::normalise_attitude(angles);
    for(auto const word : {folded, angles[0], angles[1], angles[2]}) {
      for(auto const byte : {word & 255, word >> 8}) {
        fingerprint = (fingerprint ^ static_cast<std::uint64_t>(byte)) * 0x100000001b3;
      }
    }
  }
  CHECK(fingerprint == darker::test_reference::angle_fold_fingerprint);
}

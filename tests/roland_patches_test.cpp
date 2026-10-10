#include <catch2/catch_test_macros.hpp>
#include <array>
#include <cstddef>
#include <stdexcept>
#include <vector>
#include "audio/roland_patches.h"

TEST_CASE("Roland setup MIDI extracts checked DT1 data and rejects damaged banks", "[audio]") {
  std::array<uint8_t, 35> const midi{
    'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 0, 0, 1, 1, 0xe0,
    'M', 'T', 'r', 'k', 0, 0, 0, 13,
    0, 0xf0, 10, 0x41, 0x10, 0x16, 0x12, 3, 1, 0x10, 127, 109, 0xf7
  };
  auto const bytes{std::as_bytes(std::span{midi})};
  auto const messages{darker::audio::roland_setup_messages(bytes)};
  REQUIRE(messages.size() == 1);
  REQUIRE(messages.front() == std::vector<uint8_t>{0xf0, 0x41, 0x10, 0x16, 0x12, 3, 1, 0x10, 127, 109, 0xf7});
  for(size_t length{0}; length < bytes.size(); ++length) {
    REQUIRE_THROWS_AS(darker::audio::roland_setup_messages(bytes.first(length)), std::invalid_argument);
  }
  auto damaged{midi};
  damaged[33] ^= 1;
  REQUIRE_THROWS_AS(darker::audio::roland_setup_messages(std::as_bytes(std::span{damaged})), std::invalid_argument);
  damaged = midi;
  damaged[23] = 0x90;
  REQUIRE_THROWS_AS(darker::audio::roland_setup_messages(std::as_bytes(std::span{damaged})), std::invalid_argument);
}

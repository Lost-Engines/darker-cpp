#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include "game/random.h"
#include "reference/world_samples.h"

TEST_CASE("Original random recurrence matches every native 16-bit state") {
  for(unsigned int page{0}; page < 256; ++page) {
    CAPTURE(page);
    uint64_t checksum{14695981039346656037ULL};
    for(unsigned int low{0}; low < 256; ++low) {
      auto state{static_cast<uint16_t>(page * 256 + low)};
      auto const value{darker::game::next_random(state)};
      CHECK(value == state);
      checksum = (checksum ^ static_cast<uint8_t>(value)) * 1099511628211ULL;
      checksum = (checksum ^ static_cast<uint8_t>(value >> 8)) * 1099511628211ULL;
    }
    CHECK(checksum == darker::test_reference::random_hashes[page]);
  }
}

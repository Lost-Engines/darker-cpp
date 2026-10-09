#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <cmath>
#include <span>
#include <string_view>
#include <vector>
#include "audio/sc55_synth.h"

TEST_CASE("SC-55 firmware produces notes independently of host buffer size", "[audio][sc55]") {
  if(std::string_view{DARKER_TEST_SC55_ROM_DIR}.empty()) SKIP("Supply DARKER_TEST_SC55_ROM_DIR for hardware synthesis verification");
  auto const render{[](size_t const block_size) {
    darker::audio::sc55_synth synth{DARKER_TEST_SC55_ROM_DIR, 48000};
    std::vector<float> output(48000 * 4);
    synth.reset();
    synth.send({0xc0, 0});
    synth.send({0xb0, 7, 100});
    synth.send({0x90, 60, 100});
    for(size_t offset{0}; offset < output.size();) {
      auto const count{std::min(block_size * 2, output.size() - offset)};
      synth.render(std::span{output}.subspan(offset, count));
      offset += count;
    }
    synth.reset();
    std::vector<float> tail(48000 * 8);
    synth.render(tail);
    double energy{0};
    for(size_t i{2}; i < output.size(); ++i) {
      REQUIRE(std::isfinite(output[i]));
      energy += std::abs(output[i] - output[i-2]);
    }
    REQUIRE(energy > 1.0);
    double late_energy{0};
    for(size_t i{tail.size()-48000}; i < tail.size(); ++i) late_energy += std::abs(tail[i]-tail[i-2]);
    REQUIRE(late_energy < energy * 0.1);
    return output;
  }};
  REQUIRE(render(512) == render(257));
}

#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <limits>
#include <span>
#include <vector>
#include "support/sine_wave.h"

TEST_CASE("PCM phase and ramp survive arbitrary callback boundaries") {
  framework::audio::sine_wave whole{48'000, 220.0, 0.02f};
  framework::audio::sine_wave chunked{48'000, 220.0, 0.02f};
  std::vector<float> expected(4096);
  std::vector<float> actual(4096);
  whole.fill_stereo(expected);
  chunked.fill_stereo(std::span{actual}.first(34));
  chunked.fill_stereo(std::span{actual}.subspan(34, 1000));
  chunked.fill_stereo(std::span{actual}.subspan(1034));
  REQUIRE(actual == expected);
  REQUIRE(actual.front() == 0.0f);
}

TEST_CASE("Steady tone is quiet, centred, stereo and at the requested frequency") {
  framework::audio::sine_wave tone{48'000, 220.0, 0.02f};
  std::vector<float> warmup(9600);
  tone.fill_stereo(warmup);
  std::vector<float> samples(96'000);
  tone.fill_stereo(samples);
  unsigned int crossings{0};
  double sum{0.0};
  for(std::size_t i{0}; i != samples.size(); i += 2) {
    REQUIRE(std::isfinite(samples[i]));
    REQUIRE(std::abs(samples[i]) <= 0.020001f);
    REQUIRE(samples[i] == samples[i + 1]);
    sum += samples[i];
    if(i != 0 && samples[i - 2] <= 0.0f && samples[i] > 0.0f) ++crossings;
  }
  REQUIRE(std::abs(sum / 48'000.0) < 0.000001);
  REQUIRE(crossings >= 219);
  REQUIRE(crossings <= 220);
}

TEST_CASE("Invalid oscillator configurations are rejected") {
  REQUIRE_THROWS((framework::audio::sine_wave{0, 220.0, 0.02f}));
  REQUIRE_THROWS((framework::audio::sine_wave{48'000, 24'000.0, 0.02f}));
  REQUIRE_THROWS((framework::audio::sine_wave{48'000, std::numeric_limits<double>::quiet_NaN(), 0.02f}));
  REQUIRE_THROWS((framework::audio::sine_wave{48'000, 220.0, -0.1f}));
}

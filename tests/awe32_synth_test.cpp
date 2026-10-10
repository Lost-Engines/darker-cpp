#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <span>
#include <string_view>
#include <vector>
#include "audio/awe32_synth.h"
#include "audio/fm_stream.h"
#include "resources/archive_set.h"

TEST_CASE("AWE32 native library sounds and releases notes independently of output blocks", "[audio][awe32]") {
  if(std::string_view{DARKER_TEST_AWE32_ROM}.empty() || std::string_view{DARKER_TEST_RESOURCE_DIR}.empty())
    SKIP("Supply DARKER_TEST_AWE32_ROM and DARKER_REFERENCE_DIR for native AWE32 verification");
  auto const driver{darker::resources::read_binary_file(std::filesystem::path{DARKER_TEST_RESOURCE_DIR} / "00_037.bin", 65536)};
  auto const render{[&](size_t const block) {
    darker::audio::awe32_synth synth{DARKER_TEST_AWE32_ROM, driver, 48000};
    synth.reset();
    synth.send({0xc0, 0});
    synth.send({0x90, 60, 100});
    std::vector<float> pcm(48000 * 4);
    for(size_t offset{}; offset < pcm.size();) {
      auto const count{std::min(block * 2, pcm.size() - offset)};
      synth.render(std::span{pcm}.subspan(offset, count));
      offset += count;
    }
    double energy{};
    REQUIRE(std::ranges::all_of(pcm, [](float value) { return std::isfinite(value); }));
    for(float const value : pcm) energy += std::abs(value);
    REQUIRE(energy > 1);
    synth.send({0x80, 60, 0});
    synth.reset();
    std::vector<float> tail(48000 * 8);
    synth.render(tail);
    double late{};
    for(size_t i{tail.size() - 48000}; i < tail.size(); ++i) late += std::abs(tail[i]);
    REQUIRE(late < energy * 0.1);
    return pcm;
  }};
  REQUIRE(render(512) == render(257));
}

TEST_CASE("All six AWE32 arrangements survive consecutive group changes", "[audio][awe32]") {
  if(std::string_view{DARKER_TEST_AWE32_ROM}.empty() || std::string_view{DARKER_TEST_RESOURCE_DIR}.empty())
    SKIP("Supply AWE32 ROM and extracted resource paths");
  auto const load{[](unsigned int const slot) {
    auto const filename{"00_" + std::to_string(slot).insert(0, 3 - std::to_string(slot).size(), '0') + ".bin"};
    return darker::resources::read_binary_file(std::filesystem::path{DARKER_TEST_RESOURCE_DIR} / filename, 65536);
  }};
  darker::audio::fm_stream stream{48000};
  std::array<std::vector<std::byte>,6> songs;
  for(unsigned int group{}; group < songs.size(); ++group) songs[group] = load(42 + group * 5);
  stream.configure_awe32_music(DARKER_TEST_AWE32_ROM, load(37), std::move(songs));
  for(int group{}; group < 6; ++group) {
    stream.select_music(group);
    std::vector<float> pcm(48000 * 12);
    stream.render(pcm);
    double energy{};
    REQUIRE(std::ranges::all_of(pcm, [](float value) { return std::isfinite(value); }));
    for(float const value : pcm) energy += std::abs(value);
    REQUIRE(energy > 1);
    if(group == 0) {
      // With the known ROM and opening arrangement, discarding register-read
      // PCM caused a 0.750-sized discontinuity; retaining it stays below 0.488.
      float largest_step{};
      for(size_t i{2}; i < pcm.size(); ++i)
        largest_step = std::max(largest_step, std::abs(pcm[i] - pcm[i - 2]));
      REQUIRE(largest_step < 0.6f);
    }
    stream.select_music(-1);
    std::array<float,512> silence{};
    stream.render(silence);
  }
}

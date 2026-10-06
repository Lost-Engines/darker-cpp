#include <catch2/catch_test_macros.hpp>
#include <array>
#include <bit>
#include <vector>
#include "audio/flight_sounds.h"
#include "audio/fm_driver.h"
#include "audio/fm_synth.h"
#include "reference/engine_sound_samples.h"
#include "reference/fm_samples.h"

TEST_CASE("FM driver preserves original patch, pitch, attenuation and key writes", "[audio]") {
  /// Replay a shared nine-voice sequence so cached block and patch values affect later writes
  darker::audio::fm_driver driver;
  for(auto const &sample : darker::test_reference::fm_samples) {
    auto const &v{sample.input};
    auto const result{driver.program(static_cast<std::uint8_t>(v[0]), static_cast<std::uint8_t>(v[1]),
      static_cast<std::uint16_t>(v[2]), static_cast<std::uint16_t>(v[3]), v[4] != 0, v[5] != 0)};
    CAPTURE(v);
    REQUIRE(result.count == sample.count);
    for(std::size_t i{0}; i < result.count; ++i) CHECK(result.writes[i].address * 256 + result.writes[i].value == sample.writes[i]);
  }
}


TEST_CASE("FM PCM matches the original-register sound auditions", "[audio]") {
  /// Compare one second of unmodified PCM against the independently exported native-register WAVs
  struct audition { std::uint8_t patch; std::uint16_t pitch, level; std::uint64_t hash; };
  std::array<audition, 2> const auditions{{{7, 408, 0xc4ff, 0xfa0c479fa69e0684}, {2, 686, 0xb000, 0x1043f88f475dc408}}};
  for(auto const &sample : auditions) {
    darker::audio::fm_driver driver;
    darker::audio::fm_synth synth{48000};
    synth.write(driver.program(0, sample.patch, sample.pitch, sample.level, true));
    std::vector<float> pcm(96000);
    synth.render(pcm);
    std::uint64_t hash{0xcbf29ce484222325};
    for(auto const value : pcm) {
      auto const word{std::bit_cast<std::uint16_t>(static_cast<std::int16_t>(value * 32768.0f))};
      for(auto const byte : {word & 255, word >> 8}) {
        hash ^= static_cast<unsigned int>(byte);
        hash *= 0x100000001b3;
      }
    }
    CHECK(hash == sample.hash);
  }
}


TEST_CASE("Player engine sound follows original pitch modulation and engine gates", "[audio]") {
  /// Compare both craft callbacks with the player hidden by the cockpit camera
  for(auto const &sample : darker::test_reference::engine_sound_samples) {
    auto const &v{sample.input};
    darker::game::player_flight player;
    if(v[0]) player.craft = darker::game::skimma_flight_state{};
    player.upgraded = v[1] != 0;
    player.pose().speed = static_cast<std::uint16_t>(v[2]);
    player.engine_flags = static_cast<std::uint8_t>(v[4]);
    player.lifecycle.crashing = v[5] != 0;
    darker::audio::flight_sounds sounds;
    auto const voice{sounds.advance(player, static_cast<std::uint16_t>(v[3]), false, v[6] != 0)[0]};
    CAPTURE(v);
    CHECK(voice.pitch == sample.output[0]);
    CHECK(voice.active == (sample.output[2] != 0));
    if(voice.active) CHECK(voice.level == sample.output[1]);
  }
}

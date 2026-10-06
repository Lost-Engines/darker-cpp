#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <cmath>
#include "audio/world_sounds.h"
#include "reference/world_sound_samples.h"

TEST_CASE("World sound admission and Doppler match native arithmetic", "[audio]") {
  /// Check wrapping positions, rejected distances and the original directional speed factors
  using namespace darker;
  for(auto const &v : test_reference::sound_admission) {
    CAPTURE(v);
    auto const level{audio::audible_level({static_cast<uint16_t>(v[0]),static_cast<uint16_t>(v[1]),static_cast<uint16_t>(v[2])},
      {static_cast<uint16_t>(v[3]),static_cast<uint16_t>(v[4]),static_cast<uint16_t>(v[5])}, static_cast<uint16_t>(v[6]), static_cast<uint8_t>(v[7]))};
    CHECK((level ? static_cast<int>(*level) : -1) == v[8]);
  }
  for(auto const &v : test_reference::sound_velocity) {
    CAPTURE(v);
    game::object_pose const pose{.angles{static_cast<uint16_t>(v[0]),static_cast<uint16_t>(v[1]),0}, .speed{static_cast<uint16_t>(v[2])}};
    CHECK(audio::doppler_factor(&pose, static_cast<uint16_t>(v[3]), static_cast<uint16_t>(v[4])) == v[5]);
  }
  for(auto const &v : test_reference::sound_pitch) {
    CAPTURE(v);
    game::object_pose const source{.angles{static_cast<uint16_t>(v[4]),static_cast<uint16_t>(v[5]),0}, .speed{static_cast<uint16_t>(v[6])}};
    game::object_pose const listener{.angles{static_cast<uint16_t>(v[7]),static_cast<uint16_t>(v[8]),0}, .speed{static_cast<uint16_t>(v[9])}};
    CHECK(audio::spatial_pitch(static_cast<uint16_t>(v[3]), {static_cast<uint16_t>(v[0]),static_cast<uint16_t>(v[1]),static_cast<uint16_t>(v[2])}, listener, v[10] ? &source : nullptr) == v[11]);
  }
}

TEST_CASE("Enemy gun endpoints match native sprite and sound construction", "[audio][effects]") {
  /// Hit/miss classification changes both sprite lifetime and sound level, retaining an independent sound deadline
  for(auto const &v : darker::test_reference::gun_effects) {
    darker::game::effect_system effects;
    effects.gun_impact({1234,5678,2400}, v[0] != 0, 65000);
    REQUIRE(effects.trails.size() == 1);
    REQUIRE(effects.gun_sounds.size() == 1);
    CHECK(effects.trails[0].flags == v[1]);
    CHECK(effects.trails[0].position == std::array<uint16_t,3>{static_cast<uint16_t>(v[2]),static_cast<uint16_t>(v[3]),static_cast<uint16_t>(v[4])});
    auto const &sound{effects.gun_sounds[0]};
    CHECK(sound.definition.pitch == v[5]);
    CHECK(sound.definition.level == v[6]);
    CHECK(sound.deadline == v[7]);
    CHECK(sound.definition.patch == v[8]);
    CHECK(sound.definition.flags == v[9]);
    effects.advance(static_cast<uint16_t>(65000 + 256), 8);
    CHECK(effects.gun_sounds.empty());
  }
}

TEST_CASE("World voices retain channels and retrigger replacement sources", "[audio]") {
  /// A recipe's layers coexist with player sound; motion does not restart an already assigned note
  darker::game::mission_combat combat{{}};
  combat.effects.spawn(0x7319,{0,0,0},0);
  darker::audio::world_sounds mixer;
  darker::audio::fm_frame player{};
  player[0] = {.pitch{400}, .level{0x8800}, .patch{7}, .active{true}};
  auto const first{mixer.mix(player,combat,{})};
  auto const second{mixer.mix(player,combat,{})};
  unsigned int active{0};
  for(size_t i{0}; i < first.size(); ++i) {
    CHECK(first[i].generation == second[i].generation);
    CHECK(first[i].patch == second[i].patch);
    if(first[i].active) ++active;
  }
  CHECK(active == combat.effects.sounds.size() + 1);
  combat.effects.advance(5000,8);
  auto const expired{mixer.mix(player,combat,{})};
  active = 0;
  for(auto const &voice : expired) if(voice.active) ++active;
  CHECK(active == 1);
}

TEST_CASE("Combat recipes produce finite PCM and release expired voices", "[audio]") {
  /// Exercise event lifetime, voice assignment and the real queued OPL synthesis path together
  darker::game::mission_combat combat{{}};
  darker::audio::world_sounds mixer;
  darker::audio::fm_stream stream{48000};
  std::array<float, 1536> pcm{};
  float peak{0};
  bool heard{false};
  for(uint16_t clock{0}; clock < 3000; clock += 8) {
    if(clock == 0 || clock == 128) combat.effects.spawn(0x7319, {0,0,0}, clock);
    if(clock == 256) combat.effects.gun_impact({0,0,0}, true, clock);
    combat.effects.advance(clock, 8);
    auto const voices{mixer.mix({}, combat, {})};
    if(clock >= 2000) for(auto const &voice : voices) CHECK_FALSE(voice.active);
    REQUIRE(stream.publish(voices));
    stream.render(pcm);
    REQUIRE(std::ranges::all_of(pcm, [](float const sample){ return std::isfinite(sample); }));
    for(auto const sample : pcm) {
      peak = std::max(peak, std::abs(sample));
      heard |= sample != 0;
    }
  }
  CHECK(heard);
  CHECK(peak > 0.01f);
  CHECK(peak < 1.0f);
}

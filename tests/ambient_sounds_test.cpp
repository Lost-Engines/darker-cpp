#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <cmath>
#include "audio/ambient_sounds.h"
#include "audio/world_sounds.h"
#include "maths/world_coordinates.h"
#include "reference/ambient_sound_samples.h"

TEST_CASE("Ambient callbacks and retained-voice timing match original fixed records", "[audio][ambient]") {
  /// Compare every mutable field and admission decision after native callback, timer and voice feedback
  for(auto const &v : darker::test_reference::ambient_sound_samples) {
    CAPTURE(v);
    darker::game::city_map cells;
    cells.fill({static_cast<uint8_t>(v[10]), static_cast<uint8_t>(v[9])});
    auto source{darker::audio::make_ambient_sources()[v[0]]};
    source.sound.deadline = static_cast<uint16_t>(v[4]);
    source.sound.definition.flags = static_cast<uint8_t>(v[12]);
    darker::audio::ambient_context const context{
      .listener{static_cast<uint16_t>(v[1] * 256), static_cast<uint16_t>(v[2] * 256)},
      .clock{static_cast<uint16_t>(v[3])},
      .changes{static_cast<uint16_t>(v[5])},
      .gate_site{static_cast<uint16_t>(v[8])},
      .gate_active{v[7] != 0},
      .supplementary{v[6] != 0}
    };
    CHECK(darker::audio::advance_ambient_source(source, context, cells, v[11] != 0) == (v[13] != 0));
    CHECK(source.sound.deadline == v[14]);
    CHECK(source.sound.definition.flags == v[15]);
    CHECK(source.sound.definition.pitch == v[16]);
    CHECK(source.sound.position == darker::maths::world_position{
      .column{static_cast<uint16_t>(v[17])},
      .row{static_cast<uint16_t>(v[18])},
      .height{static_cast<uint16_t>(v[19])}
    });
    CHECK(source.sound.definition.level == v[20]);
  }
}

TEST_CASE("Bell sources retain notes between tolls and are absent outside Delphi", "[audio][ambient]") {
  /// Exercise original repeat timing with live mixer ownership, independently of inaudible city sources
  darker::game::city_map cells{};
  darker::game::mission_combat combat{{}};
  darker::audio::ambient_sounds ambience;
  darker::audio::world_sounds mixer;
  darker::game::object_pose const listener{
    .position{
      .column{15104},
      .row{17408},
      .height{1024}
    }
  };
  darker::audio::ambient_context context{
    .listener{15104, 17408}
  };
  auto sources{ambience.advance(context, cells, 0, darker::resources::world_kind::delphi)};
  auto const first{mixer.mix({}, combat, listener, 0, sources)};
  auto const bell{std::ranges::find_if(first, [](auto const &note){
    return note.active && note.pitch == 1300;
  })};
  REQUIRE(bell != first.end());
  auto const channel{static_cast<size_t>(bell - first.begin())};
  CHECK((mixer.audible_ambient() & 0x3c0) == 0x3c0);
  context.clock = 8;
  sources = ambience.advance(context, cells, mixer.audible_ambient(), darker::resources::world_kind::delphi);
  auto const continued{mixer.mix({}, combat, listener, 8, sources)};
  CHECK(continued[channel].generation == bell->generation);
  context.clock = 1024;
  sources = ambience.advance(context, cells, mixer.audible_ambient(), darker::resources::world_kind::delphi);
  auto const repeated{mixer.mix({}, combat, listener, 1024, sources)};
  auto const toll{std::ranges::find_if(repeated, [](auto const &note){
    return note.active && note.pitch == 1300;
  })};
  REQUIRE(toll != repeated.end());
  CHECK(toll->generation != bell->generation);
  CHECK(ambience.advance(context, cells, 0, darker::resources::world_kind::halon).empty());
  CHECK(ambience.advance(context, cells, 0, darker::resources::world_kind::underground).empty());
}

TEST_CASE("Overlapping ambient sources render finite PCM through the live sound path", "[audio][ambient]") {
  /// Run several bell cycles and verify leaving Delphi releases the ambience
  darker::game::city_map cells{};
  darker::game::mission_combat combat{{}};
  darker::audio::ambient_sounds ambience;
  darker::audio::world_sounds mixer;
  darker::audio::fm_stream stream{48000};
  darker::game::object_pose const listener{
    .position{
      .column{15104},
      .row{17408},
      .height{1024}
    }
  };
  darker::audio::ambient_context context{
    .listener{15104, 17408}
  };
  std::array<float, 1536> pcm{};
  float peak{0};
  for(uint16_t clock{0}; clock < 4000; clock += 8) {
    context.changes = static_cast<uint16_t>(clock ^ context.clock);
    context.clock = clock;
    auto const sources{ambience.advance(context, cells, mixer.audible_ambient(), clock < 3500 ? darker::resources::world_kind::delphi : darker::resources::world_kind::halon)};
    auto const voices{mixer.mix({}, combat, listener, clock, sources)};
    REQUIRE(stream.publish(voices));
    stream.render(pcm);
    REQUIRE(std::ranges::all_of(pcm, [](float const sample){
      return std::isfinite(sample);
    }));
    for(auto const sample : pcm) peak = std::max(peak, std::abs(sample));
    if(clock >= 3500) CHECK(std::ranges::none_of(voices, [](auto const &note){
      return note.active;
    }));
  }
  CHECK(peak > 0.01f);
  CHECK(peak < 1.0f);
}

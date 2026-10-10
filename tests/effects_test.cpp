#include <catch2/catch_test_macros.hpp>
#include <bit>
#include "game/effects.h"
#include "graphics/particles.h"
#include "maths/world_coordinates.h"
#include "reference/effect_samples.h"

TEST_CASE("Effect ring projection retains native sprite order", "[effects]") {
  /// Include clipped rings, equal-depth samples and the original duplicate nearest sample
  for(auto const &v : darker::test_reference::effect_projection) {
    CAPTURE(v);
    darker::game::particle_emitter const emitter{
      .radius{static_cast<uint16_t>(v[0])},
      .angle{static_cast<uint16_t>(v[1])},
      .sampling{static_cast<uint16_t>(v[2])}
    };
    auto const term{[](int64_t const value){ return darker::graphics::projection_term{
      .whole{static_cast<uint16_t>(value >> 8)},
      .fraction{static_cast<uint8_t>(value)}
    }; }};
    darker::graphics::model_placement const centre{
      .horizontal{term(v[3])},
      .vertical{term(v[4])},
      .depth{term(v[5])}
    };
    darker::graphics::camera_basis const basis{{
      {static_cast<int16_t>(v[7]), static_cast<int16_t>(v[9]), static_cast<int16_t>(v[11])},
      {static_cast<int16_t>(v[6]), static_cast<int16_t>(v[8]), static_cast<int16_t>(v[10])}, {},
    }};
    auto const points{darker::graphics::project_emitter(emitter, centre, basis, {160, 84})};
    uint32_t hash{2166136261};
    for(auto const &point : points) {
      for(auto const value : {point.x, point.y, point.depth}) hash = (hash ^ static_cast<uint16_t>(value)) * 16777619;
    }
    CHECK(points.size() == static_cast<size_t>(v[12]));
    CHECK(hash == v[13]);
  }
}

TEST_CASE("Effect movement matches signed native byte arithmetic", "[effects]") {
  /// Check fractional height carries and wrapping words, including signed frame-step bytes
  for(auto const &v : darker::test_reference::effect_motion) {
    CAPTURE(v);
    darker::game::particle_emitter emitter{
      .position{
        .column{0},
        .row{0},
        .height{static_cast<uint16_t>(v[0])}
      },
      .height_fraction{static_cast<uint8_t>(v[1])},
      .start{static_cast<uint16_t>(v[7])},
      .radius{static_cast<uint16_t>(v[2])},
      .angle{static_cast<uint16_t>(v[3])},
      .radius_rate{static_cast<int8_t>(v[4])},
      .height_rate{static_cast<int8_t>(v[5])},
      .angle_rate{static_cast<int8_t>(v[6])}
    };
    darker::game::advance_emitter(emitter, static_cast<uint16_t>(v[7]), static_cast<uint16_t>(v[8]));
    CHECK(emitter.position.height == v[9]);
    CHECK(emitter.height_fraction == v[10]);
    CHECK(emitter.radius == v[11]);
    CHECK(emitter.angle == v[12]);
  }
}

TEST_CASE("Particle pixels follow native source selection and scanline masks", "[effects]") {
  /// Fill the source with a position-dependent pattern so wrong atlas rows and alignment remain observable
  framework::render::indexed_cockpit_framebuffer sheet;
  for(size_t y{0}; y < 240; ++y) {
    for(size_t x{0}; x < 320; ++x) sheet.pixels[y * 320 + x] = static_cast<uint8_t>((y * 37 + x * 13) % 255 + 1);
  }
  for(auto const &v : darker::test_reference::particle_pixels) {
    CAPTURE(v);
    framework::render::indexed_cockpit_framebuffer frame{};
    darker::graphics::draw_particle(frame, sheet, {
      .x{static_cast<int16_t>(v[2])},
      .y{80},
      .depth{static_cast<int16_t>(v[0])}
    }, static_cast<uint8_t>(v[1]), 240);
    uint32_t hash{2166136261};
    for(auto const pixel : frame.pixels) hash = (hash ^ pixel) * 16777619;
    CHECK(hash == v[3]);
  }
}

TEST_CASE("Effect phases and stationary trails match original records", "[effects]") {
  /// Keep waiting/expired states and the bright-to-smoke splice distinct from an ascending atlas animation
  for(auto const &v : darker::test_reference::effect_ages) {
    darker::game::particle_emitter const emitter{
      .start{1000},
      .animation{darker::game::emitter_animation{static_cast<uint8_t>(v[0])}}
    };
    auto const phase{darker::game::particle_phase(emitter, static_cast<uint16_t>(1000 + v[1]))};
    CHECK((phase ? static_cast<int>(*phase) : -1) == v[2]);
  }
  for(auto const &v : darker::test_reference::effect_trails) {
    auto const emitter{darker::game::make_damage_trail({16000, 17000, 2000}, static_cast<uint8_t>(v[0]), static_cast<uint16_t>(v[1]), 1000)};
    CHECK(emitter.position == darker::maths::world_position{
      .column{static_cast<uint16_t>(v[2])},
      .row{static_cast<uint16_t>(v[3])},
      .height{static_cast<uint16_t>(v[4])}
    });
    CHECK(emitter.animation.encoded() == v[5]);
  }
}

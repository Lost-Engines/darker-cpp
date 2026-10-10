#include <catch2/catch_test_macros.hpp>
#include <array>
#include <cstdint>
#include "graphics/bitmap_hud.h"
#include "graphics/navigation_hud.h"
#include "reference/navigation_samples.h"

TEST_CASE("Every compass phase matches original-code pixel captures") {
  for(std::uint8_t phase{0}; phase < 136; ++phase) {
    auto const points{darker::graphics::compass_points(phase)};
    auto const &expected{darker::test_reference::compass[phase]};
    for(std::size_t i{0}; i < points.size(); ++i) {
      REQUIRE(points[i].x == expected[i].x);
      REQUIRE(points[i].y == expected[i].y);
    }
  }
  REQUIRE(darker::graphics::compass_phase(0) == 34);
  REQUIRE(darker::graphics::compass_phase(16384) == 68);
  REQUIRE(darker::graphics::compass_phase(32768) == 102);
  REQUIRE(darker::graphics::compass_phase(49152) == 0);
  framework::render::indexed_cockpit_framebuffer screen;
  screen.pixels.fill(17);
  darker::graphics::update_compass(screen, 0, 34);
  REQUIRE(screen.pixels[215 * 320 + 30] == 0);
  auto const marker{darker::test_reference::compass[34]};
  REQUIRE(screen.pixels[marker[0].y * 320 + marker[0].x] == 0x9e);
  REQUIRE(screen.pixels[marker[2].y * 320 + marker[2].x] == 0x9c);
}

TEST_CASE("Normal radar projection matches all captured headings, clipping and group colours") {
  for(auto const &sample : darker::test_reference::radar) {
    auto const actual{darker::graphics::project_radar_contact({60 * 256, 60 * 256}, sample.heading,
      {
        .position{static_cast<std::uint16_t>((60 + sample.x) * 256), static_cast<std::uint16_t>((60 + sample.y) * 256)},
        .group{sample.group}
      })};
    REQUIRE(actual.has_value() == sample.pixel.has_value());
    if(actual) {
      REQUIRE(actual->position.x == sample.pixel->position.x);
      REQUIRE(actual->position.y == sample.pixel->position.y);
      REQUIRE(actual->colour == sample.pixel->colour);
    }
  }
}

TEST_CASE("Radar suppresses hidden and uncovered contacts and preserves draw order") {
  darker::graphics::world_position const player{0, 0};
  darker::graphics::radar_contact contact{
    .position{darker::graphics::world_position{player}},
    .group{darker::graphics::radar_group::a},
    .hidden{true}
  };
  REQUIRE_FALSE(darker::graphics::project_radar_contact(player, 0, contact));
  contact.hidden = false;
  contact.covered = false;
  REQUIRE_FALSE(darker::graphics::project_radar_contact(player, 0, contact));
  contact.covered = true;
  framework::render::indexed_cockpit_framebuffer screen;
  screen.pixels.fill(99);
  std::array<darker::graphics::radar_contact, 2> const contacts{{contact, {
    .position{darker::graphics::world_position{player}},
    .group{darker::graphics::radar_group::b}
  }}};
  darker::graphics::draw_radar_contacts(screen, player, 0, contacts);
  REQUIRE(screen.pixels[215 * 320 + 54] == 242);
  REQUIRE(screen.pixels[215 * 320 + 55] == 99);
}

TEST_CASE("Skimma masked callbacks match independently extracted sprite coverage") {
  framework::render::indexed_cockpit_framebuffer cache;
  for(std::size_t i{0}; i < cache.pixels.size(); ++i) cache.pixels[i] = static_cast<std::uint8_t>((i * 17 + 3) % 251);
  for(auto const &sample : darker::test_reference::skimma) {
    auto target{cache};
    darker::graphics::skimma_bitmap_state state;
    if(sample.bearing) state.bearing = sample.state;
    else state.weapons[sample.index] = sample.state;
    darker::graphics::update_skimma_bitmaps(cache, target, darker::graphics::craft::upgraded_skimma, {}, state);
    std::uint64_t checksum{14695981039346656037ULL};
    for(auto const pixel : target.pixels) checksum = (checksum ^ pixel) * 1099511628211ULL;
    REQUIRE(checksum == sample.checksum);
  }
}

TEST_CASE("Radar retains fractional positions and native word wrapping at world and range boundaries") {
  struct sample {
    darker::graphics::world_position player;
    darker::graphics::world_position contact;
    std::uint16_t heading;
    std::optional<darker::graphics::radar_pixel> expected;
  };
  // Native 5AC9/5AE9 captures, with 59A3 coverage supplied as true and DBC1 pixel writes intercepted.
  std::array<sample, 8> const samples{{
    {
      .player{15377, 15487},
      .contact{15376, 15742},
      .heading{1},
      .expected{darker::graphics::radar_pixel{{53, 215}, 249}}
    },
    {
      .player{15377, 15487},
      .contact{15632, 15486},
      .heading{8191},
      .expected{darker::graphics::radar_pixel{{54, 215}, 249}}
    },
    {
      .player{0, 0},
      .contact{65279, 511},
      .heading{65535},
      .expected{darker::graphics::radar_pixel{{53, 216}, 249}}
    },
    {
      .player{65520, 64},
      .contact{241, 65087},
      .heading{8191},
      .expected{darker::graphics::radar_pixel{{56, 214}, 249}}
    },
    {
      .player{32760, 65510},
      .contact{38135, 65253},
      .heading{1},
      .expected{darker::graphics::radar_pixel{{74, 213}, 237}}
    },
    {
      .player{32760, 65510},
      .contact{38136, 65253},
      .heading{1},
      .expected{}
    },
    {
      .player{0, 0},
      .contact{60160, 511},
      .heading{65535},
      .expected{}
    },
    {
      .player{0, 0},
      .contact{60159, 511},
      .heading{65535},
      .expected{}
    },
  }};
  for(auto const &sample : samples) {
    auto const actual{darker::graphics::project_radar_contact(sample.player, sample.heading, {
      .position{darker::graphics::world_position{sample.contact}},
      .group{darker::graphics::radar_group::a}
    })};
    REQUIRE(actual.has_value() == sample.expected.has_value());
    if(actual) {
      REQUIRE(actual->position.x == sample.expected->position.x);
      REQUIRE(actual->position.y == sample.expected->position.y);
      REQUIRE(actual->colour == sample.expected->colour);
    }
  }
}

TEST_CASE("Skimma weapon rings match all native placement and source-selection captures") {
  framework::render::indexed_cockpit_framebuffer cache;
  for(std::size_t i{0}; i < cache.pixels.size(); ++i) cache.pixels[i] = static_cast<std::uint8_t>((i * 17 + 3) % 251);
  for(auto const &sample : darker::test_reference::rings) {
    auto target{cache};
    darker::graphics::draw_skimma_weapon_ring(cache, target, darker::graphics::craft::upgraded_skimma, sample.weapon, sample.radius, sample.remaining);
    std::uint64_t checksum{14695981039346656037ULL};
    for(auto const pixel : target.pixels) checksum = (checksum ^ pixel) * 1099511628211ULL;
    REQUIRE(checksum == sample.checksum);
  }
  auto target{cache};
  REQUIRE_THROWS(darker::graphics::draw_skimma_weapon_ring(cache, target, darker::graphics::craft::skimma, 2, 63, 10));
}

TEST_CASE("Enlarged radar disc and heading match all 256 captured native surrounds") {
  for(unsigned int phase{0}; phase < 256; ++phase) {
    INFO("heading=" << phase * 256);
    framework::render::indexed_cockpit_framebuffer target{};
    darker::graphics::draw_enlarged_radar_surround(target, static_cast<std::uint16_t>(phase * 256));
    std::uint64_t checksum{14695981039346656037ULL};
    for(auto const pixel : target.pixels) checksum = (checksum ^ pixel) * 1099511628211ULL;
    REQUIRE(checksum == darker::test_reference::enlarged_surround_checksums[phase]);
  }
}

TEST_CASE("Enlarged contact scale, clipping and palette colours match native captures") {
  for(auto const &sample : darker::test_reference::enlarged_radar) {
    auto const actual{darker::graphics::project_radar_contact({60 * 256, 60 * 256}, sample.heading,
      {
        .position{static_cast<std::uint16_t>((60 + sample.x) * 256), static_cast<std::uint16_t>((60 + sample.y) * 256)},
        .group{sample.group}
      }, darker::graphics::radar_scale::enlarged)};
    REQUIRE(actual.has_value() == sample.pixel.has_value());
    if(actual) {
      REQUIRE(actual->position.x == sample.pixel->position.x);
      REQUIRE(actual->position.y == sample.pixel->position.y);
      REQUIRE(actual->colour == sample.pixel->colour);
    }
  }
}

TEST_CASE("Height-coded navigation contacts preserve signed byte wrapping and alignment-specific backgrounds") {
  framework::render::indexed_cockpit_framebuffer cache;
  for(std::size_t i{0}; i < cache.pixels.size(); ++i) cache.pixels[i] = static_cast<std::uint8_t>((i * 17 + 3) % 251);
  for(auto const &sample : darker::test_reference::heights) {
    auto target{cache};
    darker::graphics::draw_navigation_contact(cache, target, {156 + sample.alignment, 81}, sample.height, sample.reference);
    std::uint64_t checksum{14695981039346656037ULL};
    for(auto const pixel : target.pixels) checksum = (checksum ^ pixel) * 1099511628211ULL;
    REQUIRE(checksum == sample.checksum);
  }
}

TEST_CASE("Underground radar shares the grey contact ramp for every actor group") {
  /// 5876 selects 5C04 instead of the 5BFE/5C01 coloured entries; only the base colour changes
  for(auto const &sample : darker::test_reference::radar) {
    auto const actual{darker::graphics::project_radar_contact({60*256, 60*256},sample.heading,
      {
        .position{static_cast<uint16_t>((60+sample.x)*256), static_cast<uint16_t>((60+sample.y)*256)},
        .group{darker::graphics::radar_group::underground}
      })};
    REQUIRE(actual.has_value() == sample.pixel.has_value());
    if(!actual) continue;
    CHECK(actual->position.x == sample.pixel->position.x);
    CHECK(actual->position.y == sample.pixel->position.y);
    auto const original_base{sample.group == darker::graphics::radar_group::a ? 249 : 242};
    CHECK(actual->colour == sample.pixel->colour-original_base+22);
  }
}

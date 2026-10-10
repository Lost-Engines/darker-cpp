#include <catch2/catch_test_macros.hpp>
#include <bit>
#include "game/player_flight.h"
#include "game/supply_pad.h"
#include "maths/world_coordinates.h"
#include "reference/skimma_start_samples.h"
#include "reference/supply_entry_samples.h"
#include "reference/supply_motion_samples.h"

TEST_CASE("Supply movement and departure match original callbacks", "[game][supply]") {
  /// Retain fractional convergence, two-stage centring and the output/context/control departure gates
  for(auto const &s : darker::test_reference::supply_motion_samples) {
    CAPTURE(s);
    auto const word{[](int const value){ return static_cast<uint16_t>(value); }};
    darker::game::player_flight player;
    player.craft = darker::game::skimma_flight_state{
      .pose{
        .position{
          .column{word(s[1])},
          .row{word(s[2])},
          .height{word(s[3])}
        },
        .angles{
          .heading{word(s[4])},
          .pitch{0},
          .roll{0}
        }
      },
      .horizontal_velocity{word(s[5])},
      .vertical_velocity{word(s[6])}
    };
    player.lifecycle.flags = 0x10;
    darker::game::supply_pad_state pad{
      .phase{static_cast<darker::game::supply_phase>(s[0])},
      .site{0x2020},
      .offset{word(s[11])},
      .fraction{static_cast<uint8_t>(s[12])}
    };
    darker::game::advance_supply_motion(player,pad,word(s[7]),s[8] != 0,word(s[9]),word(s[10]));
    auto const &craft{std::get<darker::game::skimma_flight_state>(player.craft)};
    std::array<int,11> const actual{craft.pose.position.column,craft.pose.position.row,craft.pose.position.height,craft.pose.angles.heading,
      craft.horizontal_velocity,craft.vertical_velocity,craft.pose.speed,player.lifecycle.flags,static_cast<int>(pad.phase),pad.offset,pad.fraction};
    for(size_t i{0}; i < actual.size(); ++i) CHECK(actual[i] == s[13+i]);
  }
}

TEST_CASE("Supply approaches reach the original centre on the original frame", "[game][supply]") {
  /// Run complete multi-frame approaches, preserving carry and the intermediate heading alignment
  for(auto const &s : darker::test_reference::supply_approaches) {
    CAPTURE(s);
    darker::game::player_flight player;
    player.craft = darker::game::skimma_flight_state{
      .pose{
        .position{
          .column{0x1000},
          .row{0x2000},
          .height{400}
        }
      }
    };
    player.lifecycle.flags = 0x10;
    darker::game::supply_pad_state pad{
      .phase{darker::game::supply_phase::approach},
      .site{0x2020},
      .offset{0x1080}
    };
    int frames{0};
    while(pad.phase == darker::game::supply_phase::approach && frames < 6000) {
      darker::game::advance_supply_motion(player,pad,0,true,0,static_cast<uint16_t>(s[0]));
      ++frames;
    }
    CHECK(frames == s[1]);
    CHECK(pad.phase == darker::game::supply_phase::docked);
    CHECK(player.pose().position == darker::maths::world_position{
      .column{0x1080},
      .row{0x2080},
      .height{328}
    });
    CHECK(player.pose().angles.heading == s[2]);
  }
}

TEST_CASE("Supply entry matches original heading projection and eligibility", "[game][supply]") {
  /// Compare edge pixels, descent/output/height limits and the stored cell/subcell targets
  for(auto const &s : darker::test_reference::supply_entry_samples) {
    CAPTURE(s);
    auto const word{[](int const value){ return static_cast<uint16_t>(value); }};
    darker::game::player_flight player;
    player.craft = darker::game::skimma_flight_state{
      .pose{
        .position{
          .column{word(s[0])},
          .row{word(s[1])},
          .height{word(s[2])}
        },
        .angles{
          .heading{word(s[3])},
          .pitch{0},
          .roll{0}
        }
      },
      .horizontal_velocity{word(s[4])},
      .vertical_velocity{word(s[5])}
    };
    player.lifecycle.flags = static_cast<uint8_t>(s[6]);
    player.engine_flags = static_cast<uint8_t>(s[7]);
    darker::game::city_map cells{};
    cells[32*128+16].type = static_cast<uint8_t>(s[8]);
    darker::game::supply_pad_state pad{
      .site{0x1234},
      .offset{0x5678}
    };
    CHECK(darker::game::begin_supply_approach(player,cells,pad) == (s[9] != 0));
    CHECK(pad.site == s[10]);
    CHECK(pad.offset == s[11]);
    CHECK(player.lifecycle.flags == s[12]);
  }
}

TEST_CASE("Skimma starting placement matches original pad coordinates and model height", "[game][supply]") {
  /// Retain C7B3's low shield byte and the nonzero initial height across all starting headings
  for(auto const &s : darker::test_reference::skimma_start_samples) {
    CAPTURE(s);
    darker::game::player_flight player;
    player.craft = darker::game::skimma_flight_state{
      .damage{
        .shield_charge{static_cast<uint16_t>(s[3])}
      }
    };
    darker::game::initialise_skimma_pad(player,static_cast<uint16_t>(s[0]),static_cast<uint8_t>(s[1]),
      std::bit_cast<int16_t>(static_cast<uint16_t>(s[2])),true);
    auto const &craft{std::get<darker::game::skimma_flight_state>(player.craft)};
    CHECK(std::array<int,7>{craft.pose.position.column,craft.pose.position.row,craft.pose.position.height,craft.pose.angles.heading,
      craft.pose.angles.pitch,player.lifecycle.flags,craft.damage.shield_charge} == std::array<int,7>{s[4],s[5],s[6],s[7],s[8],s[9],s[10]});
  }
}

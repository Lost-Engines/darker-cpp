#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <bit>
#include <utility>
#include "game/scenario_setup.h"
#include "reference/scenario_setup_samples.h"

TEST_CASE("Embedded scenario setup matches original player and actor mutations", "[game][scenario]") {
  /// Compare all translated fields and retained placement caches with the original seven programs
  for(auto const &sample : darker::test_reference::scenario_setup_samples) {
    CAPTURE(sample);
    auto const input{std::span{sample}.subspan(3, 23)}, output{std::span{sample}.subspan(26, 23)};
    auto const word{[](int const value){
      return static_cast<uint16_t>(value);
    }};
    auto const kind{static_cast<darker::game::scenario_setup_kind>(sample[0])};
    darker::game::object_pose const pose{
      .position{
        .column{word(input[0])},
        .row{word(input[1])},
        .height{word(input[2])}
      },
      .angles{
        .heading{word(input[3])},
        .pitch{word(input[4])},
        .roll{word(input[5])}
      },
      .speed{word(input[6])}
    };
    auto actual{std::array<int, 23>{}};
    std::ranges::copy(input, actual.begin());
    darker::game::object_pose result;
    if(sample[0] == 0 || sample[0] == 1 || sample[0] == 4 || sample[0] == 5) {
      darker::game::player_flight player;
      player.craft = darker::game::caero_flight_state{
        .pose{pose},
        .energy{
          .reserve{word(input[19])},
          .boost{word(input[18])}
        },
        .horizontal_velocity{word(input[7])},
        .active_boost{word(input[20])}
      };
      player.lifecycle.flags = static_cast<uint8_t>(input[17]);
      darker::game::weapon_ammunition ammunition{static_cast<uint8_t>(input[21]), static_cast<uint8_t>(input[22])};
      darker::game::apply_player_scenario_setup(kind, player, std::bit_cast<int16_t>(word(sample[1])), ammunition);
      auto const &craft{std::get<darker::game::caero_flight_state>(player.craft)};
      result = player.pose();
      actual[7] = craft.horizontal_velocity;
      actual[17] = player.lifecycle.flags;
      actual[18] = craft.energy.boost;
      actual[19] = craft.energy.reserve;
      actual[20] = craft.active_boost;
      actual[21] = ammunition.working;
      actual[22] = ammunition.reserve;
      // player caches are derived from pose in C++; the original explicitly refreshes these fields
      actual[12] = result.position.column;
      actual[13] = result.position.row;
      actual[14] = result.position.height;
      actual[15] = actual[16] = (result.position.column >> 8) | (result.position.row & 0xff00);
    } else {
      darker::game::scenario_actor actor;
      actor.pose = pose;
      actor.parameters.model_token = word(input[8]);
      actor.expiry = word(input[9]);
      actor.script.deadline = word(input[10]);
      actor.parameters.update_entry = static_cast<darker::game::object_update>(word(input[11]));
      actor.previous_position = {word(input[12]), word(input[13]), word(input[14])};
      actor.current_cell = word(input[15]);
      actor.target_token = word(input[16]);
      actor.flags = static_cast<uint8_t>(input[17]);
      darker::game::apply_actor_scenario_setup(kind, actor, 0, word(sample[2]));
      result = actor.pose;
      actual[8] = actor.parameters.model_token;
      actual[9] = actor.expiry;
      actual[10] = actor.script.deadline;
      actual[11] = std::to_underlying(actor.parameters.update_entry);
      for(size_t i{0}; i < 3; ++i) actual[12 + i] = actor.previous_position[i];
      actual[15] = actor.current_cell;
      actual[16] = actor.target_token;
      actual[17] = actor.flags;
    }
    for(size_t i{0}; i < 3; ++i) actual[i] = result.position[i];
    actual[3] = result.angles.heading;
    actual[4] = result.angles.pitch;
    actual[5] = result.angles.roll;
    actual[6] = result.speed;
    CHECK(std::ranges::equal(actual, output));
  }
}

#include "hangar_flight_check.h"
#include <array>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include "game/hangar.h"
#include "game/mission_combat.h"
#include "reference/hangar_flight_samples.h"
#include "reference/hangar_return_samples.h"
#include "resources/archive_set.h"
#include "resources/geometry_bank.h"

namespace {

std::array<int, 27> launch_state(darker::game::player_flight const &player, darker::game::hangar_state const &hangar) {
  /// Observe the same persistent fields as the original executable launch trace
  auto const &state{std::get<darker::game::caero_flight_state>(player.craft)};
  auto const &pose{state.pose};
  return {pose.position[0], pose.position[1], pose.position[2], pose.fractions[0], pose.fractions[1], pose.fractions[2],
    pose.angles[0], pose.angles[1], pose.angles[2], pose.speed, state.horizontal_velocity, state.vertical_velocity,
    state.active_boost, state.energy.buffer, state.energy.reserve, state.energy.boost, state.startup_energy,
    state.forward_bias, state.pitch_assist_rate, state.damage.damage, state.repair_phase, state.damage.rotation.pitch,
    state.damage.rotation.turn, player.engine_flags, player.lifecycle.flags, hangar.extension, player.lifecycle.crashing};
}

} // anonymous namespace

void check_hangar_flight(darker::resources::archive_set const &archives, std::filesystem::path const &trace) {
  /// Start at HQ, wait six seconds, press boost once and compare every frame without further steering input
  darker::resources::geometry_bank const bank{archives.load({.archive{0}, .slot{30}})};
  auto cells{darker::game::make_city_map(archives.load({.archive{0}, .slot{68}}), true)};
  std::array<std::uint8_t, 256> limits{};
  for(std::size_t i{0}; i < bank.city_types().size(); ++i) limits[i + 1] = bank.city_types()[i].variant_limit;
  darker::game::assign_city_variants(cells, limits);
  darker::game::player_flight player;
  darker::game::hangar_state hangar;
  darker::game::initialise_caero_hangar(player, cells, hangar, bank.header_at(bank.special_models()[25]).height);
  std::ofstream csv;
  if(!trace.empty()) {
    csv.open(trace);
    if(!csv) throw std::runtime_error{"Cannot open launch trace output"};
    csv << "tick,x,y,z,fraction_x,fraction_y,fraction_z,heading,pitch,roll,speed,horizontal_velocity,vertical_velocity,active_boost,buffer,reserve,boost_pips,startup_charge,forward_bias,pitch_assist,damage,repair_phase,pitch_rate,bank_rate,engine,flags,gate,crashed\n";
  }
  std::string first_mismatch;
  auto const compare{[&](int const tick, std::array<int, 27> const &expected){
    auto const actual{launch_state(player, hangar)};
    if(csv.is_open()) {
      csv << tick;
      for(auto const value : actual) csv << ',' << value;
      csv << '\n'; // Trace rows are buffered rather than flushed per field.
    }
    for(std::size_t field{0}; field < actual.size(); ++field) {
      if(actual[field] != expected[field] && first_mismatch.empty()) {
        first_mismatch = "HQ launch differs at tick " + std::to_string(tick) + ", field " + std::to_string(field)
          + ": C++ " + std::to_string(actual[field]) + ", original " + std::to_string(expected[field]);
      }
    }
  }};
  darker::game::mission_combat combat{{}};
  auto const advance{[&](uint16_t const tick){
    auto const previous{player.pose().position};
    player.advance_motion({},false,8,bank,cells);
    combat.advance(player,cells,bank,tick,8,0,false,{},50,nullptr,false,false,previous);
    darker::game::advance_hangar_departure(player,cells,hangar,8);
  }};
  compare(-3000, darker::test_reference::hangar_flight_initial);
  for(std::uint16_t tick{8}; tick <= 3000; tick += 8) {
    advance(tick);
  }
  compare(0, darker::test_reference::hangar_flight_charged);
  player.command(darker::game::flight_command::boost);
  for(auto const &sample : darker::test_reference::hangar_flight_samples) {
    advance(static_cast<uint16_t>(3000+sample.tick));
    compare(sample.tick, sample.state);
  }
  if(!first_mismatch.empty()) throw std::runtime_error{first_mismatch};
  std::cout << darker::test_reference::hangar_flight_samples.size() << " hands-off HQ launch frames match original execution." << std::endl;
  player = {};
  hangar = {};
  cells = darker::game::make_city_map(archives.load({.archive{0}, .slot{68}}), true);
  player.pose().position = {12672, 28380, 500};
  player.pose().angles[0] = 0x8000;
  if(darker::game::begin_hangar_return(player, cells, hangar, false)) throw std::runtime_error{"Hangar admitted incomplete objectives"};
  if(!darker::game::begin_hangar_return(player, cells, hangar, true)) throw std::runtime_error{"Hangar rejected native approach"};
  for(auto const &sample : darker::test_reference::hangar_return_samples) {
    darker::game::advance_hangar_return(player, hangar, 8, sample[0]);
    darker::game::advance_hangar_departure(player, cells, hangar, 8);
    auto const &pose{player.pose()};
    auto const &rotation{std::get<darker::game::caero_flight_state>(player.craft).damage.rotation};
    std::array<int, 15> const actual{sample[0], pose.position[0], pose.position[1], pose.position[2],
      pose.fractions[0], pose.fractions[1], pose.fractions[2], rotation.pitch, rotation.turn,
      pose.angles[0], pose.angles[1], pose.angles[2], pose.speed, hangar.extension, static_cast<int>(hangar.returning)};
    for(size_t field{0}; field < actual.size(); ++field) {
      if(actual[field] != sample[field]) throw std::runtime_error{"Hangar return tick " + std::to_string(sample[0]) + ", field " + std::to_string(field)
        + ": " + std::to_string(actual[field]) + " != " + std::to_string(sample[field])};
    }
  }
  std::cout << "684 automatic hangar return frames match original execution." << std::endl;
}

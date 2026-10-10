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
#include "maths/world_coordinates.h"

namespace {

std::array<int, 27> launch_state(darker::game::player_flight const &player, darker::game::hangar_state const &hangar) {
  /// Observe the same persistent fields as the original executable launch trace
  auto const &state{std::get<darker::game::caero_flight_state>(player.craft)};
  auto const &pose{state.pose};
  return {pose.position.column, pose.position.row, pose.position.height, pose.fractions.column, pose.fractions.row, pose.fractions.height,
    pose.angles.heading, pose.angles.pitch, pose.angles.roll, pose.speed, state.horizontal_velocity, state.vertical_velocity,
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
  for(bool const noclip : {false,true}) {
    darker::game::city_map empty_city{};
    darker::game::player_flight probe;
    probe.frozen = true;
    probe.noclip = noclip;
    probe.pose().position = {.column{1100},.row{2020},.height{0}};
    auto standalone{probe};
    auto const contact{standalone.advance({},false,8,8,bank,empty_city)};
    if(standalone.lifecycle.crashing == noclip || (contact.contact == darker::game::city_contact::none) != noclip)
      throw std::runtime_error{"Noclip did not control standalone terrain collision"};
    darker::game::mission_combat collision{{}};
    collision.collide_player(probe,{1000,2000,100},empty_city,bank,8);
    if(probe.lifecycle.crashing == noclip || (collision.player_contact.contact == darker::game::city_contact::none) != noclip)
      throw std::runtime_error{"Noclip did not control campaign terrain collision"};
    if(noclip && probe.pose().position != darker::maths::world_position{.column{1100},.row{2020},.height{0}})
      throw std::runtime_error{"Noclip still clipped the player position to terrain"};
  }
  {
    darker::game::city_map unlit_city{};
    darker::game::player_flight surface;
    surface.noclip = true;
    auto &craft{std::get<darker::game::caero_flight_state>(surface.craft)};
    craft.flying = true;
    craft.pose.position = {.column{40000},.row{40000},.height{1536}};
    auto underground{surface};
    underground.tunnel.emplace();
    underground.scenario_configuration = 4;
    for(unsigned int frame{0}; frame < 512; ++frame) {
      darker::game::flight_controls_input const input{.right{frame < 128},.up{frame >= 128 && frame < 256}};
      // No tunnel network is supplied: free flight must never query or follow it.
      surface.advance_motion(input,false,8,bank,unlit_city);
      underground.advance_motion(input,false,8,bank,unlit_city);
      if(surface.pose().position != underground.pose().position || surface.pose().angles != underground.pose().angles)
        throw std::runtime_error{"Underground noclip did not use free Caero steering"};
      auto const &energy{std::get<darker::game::caero_flight_state>(underground.craft).energy};
      if(energy.buffer != 0xffff || energy.reserve != 0xcfff || energy.boost != 0xbfff)
        throw std::runtime_error{"Noclip flight power depleted outside beacon coverage"};
      if(frame % 32 == 0) {
        surface.command(darker::game::flight_command::boost);
        underground.command(darker::game::flight_command::boost);
      }
    }
    if(surface.pose().position == darker::maths::world_position{.column{40000},.row{40000},.height{1536}})
      throw std::runtime_error{"Noclip did not move beyond beacon coverage"};
  }
  // B938 freezes the callback, retaining velocity for release while clearing the measured speed.
  for(unsigned int kind{0}; kind < 3; ++kind) {
    darker::game::player_flight frozen;
    if(kind == 1) frozen.craft = darker::game::skimma_flight_state{};
    if(kind == 2) frozen.tunnel.emplace();
    frozen.pose() = {.position{.column{128},.row{2432},.height{2000}},.angles{.heading{123},.pitch{456},.roll{789}},.speed{500}};
    frozen.toggle_freeze();
    auto const before{frozen.pose()};
    for(unsigned int tick{0}; tick < 100; ++tick) frozen.advance_motion({.right{true}},false,8,bank,cells);
    if(!frozen.frozen || frozen.pose().position != before.position || frozen.pose().angles != before.angles || frozen.pose().speed != 0)
      throw std::runtime_error{"Lyndon freeze allowed player motion"};
    frozen.toggle_freeze();
    if(frozen.frozen || frozen.pose().speed != 0) throw std::runtime_error{"Lyndon release did not restore the motion callback"};
    if(kind != 2) {
      if(kind == 0) std::get<darker::game::caero_flight_state>(frozen.craft).flying = true;
      frozen.advance_motion({},false,8,bank,cells);
      if(frozen.pose().position == before.position && frozen.pose().fractions == before.fractions)
        throw std::runtime_error{"Player did not move after Lyndon release"};
    }
  }
  {
    darker::game::player_flight ordinary;
    auto &craft{std::get<darker::game::caero_flight_state>(ordinary.craft)};
    craft.flying = true;
    craft.pose.position = {.column{128},.row{2432},.height{500}};
    auto boosted{ordinary};
    boosted.boost_cheat = true;
    ordinary.advance_motion({},false,8,bank,cells);
    boosted.advance_motion({},false,8,bank,cells);
    if(std::get<darker::game::caero_flight_state>(boosted.craft).energy.boost <= craft.energy.boost)
      throw std::runtime_error{"Brooke activation did not reach Caero energy accounting"};
  }
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
    combat.advance(player, cells, bank,
        {.elapsed_ticks{tick}, .frame_step{8}, .changes{0}},
        {},
        {}, previous);
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
  player.pose().position = {.column{12672}, .row{28380}, .height{500}};
  player.pose().angles.heading = 0x8000;
  if(darker::game::begin_hangar_return(player, cells, hangar, false)) throw std::runtime_error{"Hangar admitted incomplete objectives"};
  if(!darker::game::begin_hangar_return(player, cells, hangar, true)) throw std::runtime_error{"Hangar rejected native approach"};
  for(auto const &sample : darker::test_reference::hangar_return_samples) {
    darker::game::advance_hangar_return(player, hangar, 8, sample[0]);
    darker::game::advance_hangar_departure(player, cells, hangar, 8);
    auto const &pose{player.pose()};
    auto const &rotation{std::get<darker::game::caero_flight_state>(player.craft).damage.rotation};
    std::array<int, 15> const actual{sample[0], pose.position.column, pose.position.row, pose.position.height,
      pose.fractions.column, pose.fractions.row, pose.fractions.height, rotation.pitch, rotation.turn,
      pose.angles.heading, pose.angles.pitch, pose.angles.roll, pose.speed, hangar.extension, static_cast<int>(hangar.returning)};
    for(size_t field{0}; field < actual.size(); ++field) {
      if(actual[field] != sample[field]) throw std::runtime_error{"Hangar return tick " + std::to_string(sample[0]) + ", field " + std::to_string(field)
        + ": " + std::to_string(actual[field]) + " != " + std::to_string(sample[field])};
    }
  }
  std::cout << "684 automatic hangar return frames match original execution." << std::endl;
}

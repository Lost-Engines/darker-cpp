#include "scenario_resource_check.h"
#include <algorithm>
#include <cstdint>
#include <format>
#include <iostream>
#include <ranges>
#include <stdexcept>
#include "game/scenario_setup.h"
#include "reference/scenario_samples.h"
#include "resources/archive_set.h"
#include "resources/scenario.h"

void check_scenario_resources(darker::resources::archive_set const &archives) {
  /// Compare all setup fields and exact multilingual bytes against the independently verified analysis export
  size_t records{0}, placements{0}, native_blocks{0}, constructed{0};
  std::array<darker::resources::geometry_bank, 3> const banks{darker::resources::geometry_bank{archives.load({0, 30})},
    darker::resources::geometry_bank{archives.load({0, 31})}, darker::resources::geometry_bank{archives.load({0, 32})}};
  darker::game::tunnel_network const network{archives.load({0, 78})};
  for(unsigned int slot{0}; slot < 16; ++slot) {
    darker::resources::scenario_resource const resource{archives.load({
      .archive{4},
      .slot{slot}
    })};
    for(size_t index{0}; index < resource.records().size(); ++index) {
      auto const &record{resource.records()[index]};
      auto const &reference{darker::test_reference::scenario_samples.at(records++)};
      if(reference.resource != slot || reference.record != index) throw std::runtime_error{"Scenario record directory differs from reference"};
      uint64_t fingerprint{0xcbf29ce484222325};
      auto const add{[&](uint32_t const value){
        for(unsigned int shift{0}; shift < 32; shift += 8) fingerprint = (fingerprint ^ ((value >> shift) & 255)) * 0x100000001b3;
      }};
      auto const range{[&](darker::resources::resource_range const value){
        add(static_cast<uint32_t>(value.offset));
        add(static_cast<uint32_t>(value.size));
      }};
      auto const bytes{[&](darker::resources::resource_range const value){
        for(auto const byte : resource.bytes(value)) add(std::to_integer<uint8_t>(byte));
      }};
      range(record.source);
      range(record.shared);
      add(static_cast<uint32_t>(record.entry_offset));
      add(record.time_multiplier);
      add(record.configuration);
      for(auto const language : record.languages) {
        range(language);
        bytes(language);
      }
      if(record.configuration != 255) {
        auto const configuration{record.configuration & 15};
        auto const &bank{banks[configuration == 4 ? 2 : configuration <= 1 ? 0 : 1]};
        auto const cells{darker::game::make_city_map(archives.load({0, configuration == 4 ? static_cast<unsigned int>(70 + (record.configuration >> 4)) : configuration <= 1 ? 68u : 69u}), configuration <= 1)};
        darker::game::player_flight player;
        if(configuration == 2 || configuration == 3) player.craft = darker::game::skimma_flight_state{};
        player.pose().position = {
          .column{0x4271},
          .row{0x5163},
          .height{3000}
        };
        darker::game::weapon_ammunition ammunition;
        auto const actors{darker::game::make_scenario_actors(record, resource, bank, player, ammunition, 0xff00,
          configuration == 4 ? std::optional{darker::game::tunnel_setup{network, cells}} : std::nullopt)};
        for(auto const &[source, group] : std::views::zip(record.groups, std::array{&actors.active, &actors.reserves, &actors.free})) {
          if(group->size() != source.objects.size()) throw std::runtime_error{"Scenario setup lost actor placements"};
          constructed += group->size();
        }
        if(slot == 12 && index == 4) {
          for(auto const &actor : actors.active) {
            if((actor.pose.position.column >> 8) != 0x42 || (actor.pose.position.row >> 8) != 0x51) {
              throw std::runtime_error{"Embedded escort setup did not anchor subsequent actors to the player"};
            }
          }
        }
        if(slot == 15 && index == 0) {
          auto const actor{std::ranges::find(actors.reserves, 2, &darker::game::scenario_actor::index)};
          if(actor == actors.reserves.end() || actor->parameters.model_token != bank.special_models()[25]) {
            throw std::runtime_error{"Linked nightmare setup did not copy the player model"};
          }
        }
        range(record.beacon_sequence);
        bytes(record.beacon_sequence);
        add(record.objective_cell_list);
        add(static_cast<uint32_t>(record.player_program.value()));
        for(auto const &list : record.cell_lists) {
          add(static_cast<uint32_t>(list.cells.size()));
          add(list.terminator);
          for(auto const cell : list.cells) {
            add(cell.column);
            add(cell.row);
          }
        }
        for(auto const &group : record.groups) {
          add(static_cast<uint32_t>(group.objects.size()));
          add(static_cast<uint32_t>(group.native_setup.size()));
          placements += group.objects.size();
          native_blocks += group.native_setup.size();
          for(auto const &object : group.objects) {
            range(object.source);
            add(static_cast<uint32_t>(object.form));
            add(object.definition_slot);
            add(object.counted);
            add(object.attributes);
            add(object.heading);
            add(object.position.column);
            add(object.position.row);
            for(auto const value : {object.behaviour.attack_control, object.behaviour.awareness_threshold, object.behaviour.awareness_decay,
              object.behaviour.awareness_rise, object.behaviour.awareness_strength, object.behaviour.evasion}) add(value);
            add(object.script_or_target ? *object.script_or_target : 0xffffffff);
            add(static_cast<uint32_t>(object.program_offset.value_or(0xffffffff)));
          }
          for(auto const &block : group.native_setup) {
            range(block.source);
            add(static_cast<uint32_t>(block.current_object));
            bytes(block.source);
          }
        }
      }
      if(fingerprint != reference.fingerprint) throw std::runtime_error{std::format("Scenario resource {}, record {} differs from reference", slot, index)};
    }
  }
  if(records != 124 || placements != 1857 || constructed != 1857 || native_blocks != 7) throw std::runtime_error{"Scenario coverage differs from reference"};
  std::cout << "124 scenarios, 1,857 placements, seven native blocks and all three language sections match the independent export." << std::endl;
}

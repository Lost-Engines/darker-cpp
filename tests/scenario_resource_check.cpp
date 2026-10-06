#include "scenario_resource_check.h"
#include <cstdint>
#include <format>
#include <iostream>
#include <stdexcept>
#include "reference/scenario_samples.h"
#include "resources/archive_set.h"
#include "resources/scenario.h"

void check_scenario_resources(darker::resources::archive_set const &archives) {
  /// Compare all setup fields and exact multilingual bytes against the independently verified analysis export
  std::size_t records{0}, placements{0}, native_blocks{0};
  for(unsigned int slot{0}; slot < 16; ++slot) {
    darker::resources::scenario_resource const resource{archives.load({.archive{4}, .slot{slot}})};
    for(std::size_t index{0}; index < resource.records().size(); ++index) {
      auto const &record{resource.records()[index]};
      auto const &reference{darker::test_reference::scenario_samples.at(records++)};
      if(reference.resource != slot || reference.record != index) throw std::runtime_error{"Scenario record directory differs from reference"};
      std::uint64_t fingerprint{0xcbf29ce484222325};
      auto const add{[&](std::uint32_t const value){
        for(unsigned int shift{0}; shift < 32; shift += 8) fingerprint = (fingerprint ^ ((value >> shift) & 255)) * 0x100000001b3;
      }};
      auto const range{[&](darker::resources::resource_range const value){
        add(static_cast<std::uint32_t>(value.offset));
        add(static_cast<std::uint32_t>(value.size));
      }};
      auto const bytes{[&](darker::resources::resource_range const value){
        for(auto const byte : resource.bytes(value)) add(std::to_integer<std::uint8_t>(byte));
      }};
      range(record.source);
      range(record.shared);
      add(static_cast<std::uint32_t>(record.entry_offset));
      add(record.time_multiplier);
      add(record.configuration);
      for(auto const language : record.languages) {
        range(language);
        bytes(language);
      }
      if(record.configuration != 255) {
        range(record.beacon_sequence);
        bytes(record.beacon_sequence);
        add(record.objective_cell_list);
        add(static_cast<std::uint32_t>(record.player_program.value()));
        for(auto const &list : record.cell_lists) {
          add(static_cast<std::uint32_t>(list.cells.size()));
          add(list.terminator);
          for(auto const cell : list.cells) {
            add(cell.column);
            add(cell.row);
          }
        }
        for(auto const &group : record.groups) {
          add(static_cast<std::uint32_t>(group.objects.size()));
          add(static_cast<std::uint32_t>(group.native_setup.size()));
          placements += group.objects.size();
          native_blocks += group.native_setup.size();
          for(auto const &object : group.objects) {
            range(object.source);
            add(static_cast<std::uint32_t>(object.form));
            add(object.definition_slot);
            add(object.counted);
            add(object.attributes);
            add(object.heading);
            for(auto const value : object.position) add(value);
            for(auto const value : object.motion) add(value);
            add(object.script_or_target ? *object.script_or_target : 0xffffffff);
            add(static_cast<std::uint32_t>(object.program_offset.value_or(0xffffffff)));
          }
          for(auto const &block : group.native_setup) {
            range(block.source);
            add(static_cast<std::uint32_t>(block.current_object));
            bytes(block.source);
          }
        }
      }
      if(fingerprint != reference.fingerprint) throw std::runtime_error{std::format("Scenario resource {}, record {} differs from reference", slot, index)};
    }
  }
  if(records != 124 || placements != 1857 || native_blocks != 7) throw std::runtime_error{"Scenario coverage differs from reference"};
  std::cout << "124 scenarios, 1,857 placements, seven native blocks and all three language sections match the independent export." << std::endl;
}

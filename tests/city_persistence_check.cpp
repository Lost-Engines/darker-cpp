#include "city_persistence_check.h"
#include <iostream>
#include <stdexcept>
#include <vector>
#include "game/city_persistence.h"
#include "game/scenario_world.h"
#include "resources/campaign.h"
#include "reference/city_persistence_samples.h"

void check_city_persistence(darker::resources::archive_set const &archives) {
  /// Compare complete streams and every restored cell through native-generated fingerprints
  auto const hash{[](auto const &values, auto read){
    uint64_t result{0xcbf29ce484222325};
    for(auto const &value : values) result = (result ^ read(value)) * 0x100000001b3;
    return result;
  }};
  for(auto const &sample : darker::test_reference::city_persistence_samples) {
    darker::resources::geometry_bank const bank{archives.load({0,sample.bank})};
    auto const source{archives.load({0,sample.map})};
    auto cells{darker::game::make_city_map(source,sample.bank == 30)};
    size_t j{0};
    for(auto &cell : cells) {
      if(cell.type == 0 || bank.city_types()[cell.type - 1].collision_marker == 255) continue;
      cell.state = static_cast<uint8_t>(((j % 4) << 5) | ((j * 7) & 31) | ((j & 1) << 7));
      ++j;
    }
    std::vector<std::byte> packed(sample.bytes);
    darker::game::pack_city_state(cells,bank.city_types(),packed);
    if(hash(packed,[](auto byte){ return std::to_integer<uint8_t>(byte); }) != sample.packed) throw std::runtime_error{"City packing differs from native BB90"};
    cells = darker::game::make_city_map(source,sample.bank == 30);
    darker::game::restore_city_state(cells,bank.city_types(),packed,static_cast<uint8_t>(sample.stage));
    if(hash(cells,[](auto cell){ return cell.state; }) != sample.restored) throw std::runtime_error{"City restoration differs from native BBC6/BBFC: map " + std::to_string(sample.map) + " stage " + std::to_string(sample.stage) + " got " + std::to_string(hash(cells,[](auto cell){ return cell.state; }))};
  }
  {
    // A controlled history, not a mission playthrough: carry observed 69/74 changes into 80.
    darker::resources::geometry_bank const bank{archives.load({0,30})};
    auto const map{archives.load({0,68})};
    auto cells{darker::game::make_city_map(map,true)};
    darker::resources::campaign_resources campaign{archives};
    std::array<std::byte,953> packed{};
    darker::game::pack_city_state(cells,bank.city_types(),packed);
    for(uint8_t stage{69}; stage <= 80; ++stage) {
      cells = darker::game::make_city_map(map,true);
      darker::game::restore_city_state(cells,bank.city_types(),packed,stage);
      auto const &scenario{campaign.scenario(stage)};
      auto const &record{scenario.records()[darker::resources::select_campaign_stage(stage).record]};
      darker::game::apply_scenario_cells(cells,record);
      if(stage == 80) break;
      if(stage == 69 || stage == 74) {
        for(auto const cell : record.cell_lists[1].cells) cells[cell.row*128+cell.column].state |= 0x20;
      }
      darker::game::commit_beacon_queue(cells,scenario.bytes(record.beacon_sequence));
      darker::game::pack_city_state(cells,bank.city_types(),packed);
    }
    for(auto const [column,row] : std::array<std::array<unsigned int,2>,3>{{{25,79},{24,80},{24,82}}}) {
      if((cells[row*128+column].state & 0x60) != 0x60)
        throw std::runtime_error{"Mission 74 tank damage/target flags did not persist into mission 80"};
    }
    auto const &mission69{campaign.scenario(69).records()[4]};
    for(auto const cell : mission69.cell_lists[1].cells) {
      if(!(cells[cell.row*128+cell.column].state & 0x20))
        throw std::runtime_error{"Mission 69 house damage did not persist into mission 80"};
    }
    for(auto const [column,row] : std::array<std::array<unsigned int,2>,4>{{{54,63},{63,63},{63,72},{54,72}}}) {
      if(cells[row*128+column].type != 1 || cells[row*128+column].state != 0)
        throw std::runtime_error{"Mission 69 beacon blackout did not persist into mission 80"};
    }
    auto fresh{darker::game::make_city_map(map,true)};
    darker::game::apply_scenario_cells(fresh,campaign.scenario(80).records()[7]);
    if(!(fresh[75*128+16].state & 0x40) || (fresh[79*128+25].state & 0x40)
      || (fresh[80*128+24].state & 0x40) || (fresh[82*128+24].state & 0x40)
      || fresh[72*128+63].state != 255)
      throw std::runtime_error{"Fresh mission 80 unexpectedly inherited earlier campaign changes"};
  }
  std::cout << "Both persistent city streams and complete restored maps match native BB90/BBC6/BBFC." << std::endl;
}

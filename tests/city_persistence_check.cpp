#include "city_persistence_check.h"
#include <iostream>
#include <stdexcept>
#include <vector>
#include "game/city_persistence.h"
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
  std::cout << "Both persistent city streams and complete restored maps match native BB90/BBC6/BBFC." << std::endl;
}

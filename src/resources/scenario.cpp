#include "resources/scenario.h"
#include <stdexcept>
#include <utility>

namespace darker::resources {
namespace {

class reader {
private:
  std::span<std::byte const> data;
  std::size_t cursor;
  std::size_t end;

public:
  reader(std::span<std::byte const> const source, std::size_t const start, std::size_t const limit) : data{source}, cursor{start}, end{limit} {
    /// Keep every setup read inside its record's shared section
    if(start > limit || limit > data.size()) throw std::invalid_argument{"Scenario range exceeds its resource"};
  }

  std::size_t position() const noexcept {
    /// Preserve original resource-relative addresses for scripts and diagnostics
    return cursor;
  }

  std::uint8_t byte() {
    /// Reject a truncated operand before advancing the cursor
    if(cursor == end) throw std::invalid_argument{"Truncated scenario record"};
    return std::to_integer<std::uint8_t>(data[cursor++]);
  }

  std::uint16_t word() {
    /// Decode unaligned little-endian operands explicitly
    auto const low{byte()};
    return static_cast<std::uint16_t>(low | byte() << 8);
  }

  void skip(std::size_t const count) {
    /// Retain native blocks as data; no embedded machine code is executed by the loader
    if(count > end - cursor) throw std::invalid_argument{"Scenario block exceeds its shared section"};
    cursor += count;
  }
};

void read_setup(scenario_record &record, std::span<std::byte const> const data) {
  /// Decode BE07's three groups without confusing reserve/free templates with active objects
  auto const end{record.shared.offset + record.shared.size};
  reader input{data, record.shared.offset + 4, end};
  record.beacon_sequence.offset = input.position();
  while(input.byte() != 255) {}
  record.beacon_sequence.size = input.position() - record.beacon_sequence.offset - 1;
  for(auto &list : record.cell_lists) {
    for(;;) {
      auto const column{input.byte()};
      if(column >= 128) {
        list.terminator = column;
        break;
      }
      auto const row{input.byte()};
      if(row >= 128) throw std::invalid_argument{"Scenario cell lies outside the map"};
      list.cells.push_back({
        .column{column},
        .row{row}
      });
    }
  }
  record.objective_cell_list = record.cell_lists[1].terminator == 254 ? 1 : 2;
  std::size_t object_index{0};
  for(auto &group : record.groups) {
    for(;;) {
      auto const start{input.position()};
      auto const header{input.byte()};
      if(header == 255) break;
      if(header == 254) {
        auto const instruction{input.byte()};
        if((instruction != 0x83 && instruction != 0x81) || input.byte() != 0xc6) {
          throw std::invalid_argument{"Unrecognised native scenario setup header"};
        }
        auto const length{static_cast<std::size_t>(instruction == 0x83 ? input.byte() : input.word())};
        auto const consumed{input.position() - start - 1};
        if(length < consumed) throw std::invalid_argument{"Invalid native scenario setup length"};
        input.skip(length - consumed);
        group.native_setup.push_back({
          .source{
            .offset{start + 1},
            .size{length}
          },
          .current_object{object_index}
        });
        continue;
      }
      scenario_placement object{
        .source{
          .offset{start}
        },
        .definition_slot{static_cast<std::uint8_t>(header & 63)},
        .counted{(header & 128) != 0},
      };
      if(object.definition_slot > 32) throw std::invalid_argument{"Unknown scenario object definition"};
      object.form = header & 64 ? placement_form::absolute_static : object.definition_slot > 28 ? placement_form::compact_special : placement_form::moving;
      if(object.form == placement_form::moving) object.attributes = input.byte();
      object.heading = static_cast<std::uint16_t>(input.byte() << 8);
      if(object.form == placement_form::absolute_static) {
        object.position = {input.word(), input.word()};
      } else {
        object.position = {static_cast<std::uint16_t>(input.byte() * 256 + 128), static_cast<std::uint16_t>(input.byte() * 256 + 128)};
        if(object.form == placement_form::moving) {
          object.behaviour = {input.byte(), input.byte(), input.byte(), input.byte(), input.byte(), input.byte()};
        }
        object.script_or_target = input.word();
        if(*object.script_or_target < 0x8000) {
          auto const target{input.position() + *object.script_or_target};
          if(target >= end) throw std::invalid_argument{"Scenario object program exceeds its shared section"};
          object.program_offset = target;
        }
      }
      object.source.size = input.position() - start;
      group.objects.push_back(object);
      ++object_index;
    }
  }
  if(input.position() == end) throw std::invalid_argument{"Missing player scenario program"};
  record.player_program = input.position();
}

} // namespace

scenario_resource::scenario_resource(std::vector<std::byte> resource) : data{std::move(resource)} {
  /// BAC1–BAF5 walks length-prefixed records; all stored ranges remain valid across moves and copies
  std::size_t cursor{0};
  while(cursor < data.size()) {
    reader header{data, cursor, data.size()};
    std::size_t const size{header.word()};
    std::array<std::size_t, 4> const boundaries{header.word(), header.word(), header.word(), size};
    if(size > data.size() - cursor - 2 || boundaries[0] < 10
      || boundaries[0] > boundaries[1] || boundaries[1] > boundaries[2] || boundaries[2] > size) {
      throw std::invalid_argument{"Invalid scenario language directory"};
    }
    scenario_record record{
      .source{
        .offset{cursor},
        .size{size + 2}
      },
      .shared{
        .offset{cursor + 8},
        .size{boundaries[0] - 6}
      },
    };
    for(std::size_t i{0}; i < record.languages.size(); ++i) {
      record.languages[i] = {
        .offset{cursor + 2 + boundaries[i]},
        .size{boundaries[i + 1] - boundaries[i]}
      };
    }
    reader shared{data, record.shared.offset, record.shared.offset + record.shared.size};
    record.entry_offset = record.shared.offset + shared.word();
    if(record.entry_offset >= record.shared.offset + record.shared.size) throw std::invalid_argument{"Scenario entry lies outside its shared section"};
    record.time_multiplier = shared.byte();
    record.configuration = shared.byte();
    if(record.configuration != 255) read_setup(record, data);
    directory.push_back(std::move(record));
    cursor += size + 2;
  }
}

std::span<scenario_record const> scenario_resource::records() const noexcept {
  /// Expose parsed records without lending pointers into another owner's byte storage
  return directory;
}

std::span<std::byte const> scenario_resource::bytes(resource_range const range) const {
  /// Borrow the exact script, native block or formatted text bytes without conversion
  if(range.offset > data.size() || range.size > data.size() - range.offset) throw std::out_of_range{"Scenario byte range exceeds its resource"};
  return std::span{data}.subspan(range.offset, range.size);
}

std::span<std::byte const> scenario_resource::language(std::size_t const record, scenario_language const language) const {
  /// Select original formatted language bytes, retaining page and message control codes
  return bytes(directory.at(record).languages.at(static_cast<std::size_t>(language)));
}

} // namespace darker::resources

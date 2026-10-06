#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace darker::resources {

struct resource_range {
  std::size_t offset{0};
  std::size_t size{0};
};

enum class scenario_language { english, french, german };
enum class placement_form { moving, compact_special, absolute_static };

struct scenario_cell {
  std::uint8_t column{0};
  std::uint8_t row{0};
};

struct scenario_cell_list {
  std::vector<scenario_cell> cells;
  std::uint8_t terminator{255};
};

struct scenario_placement {
  resource_range source{};
  placement_form form{placement_form::moving};
  std::uint8_t definition_slot{0};
  bool counted{false};
  std::uint8_t attributes{0};
  std::uint16_t heading{0};
  std::array<std::uint16_t, 2> position{};
  std::array<std::uint8_t, 6> motion{};
  std::optional<std::uint16_t> script_or_target;
  std::optional<std::size_t> program_offset;
};

struct scenario_native_setup {
  resource_range source{};
  std::size_t current_object{0};
};

struct scenario_group {
  std::vector<scenario_placement> objects;
  std::vector<scenario_native_setup> native_setup;
};

struct scenario_record {
  resource_range source{};
  resource_range shared{};
  std::array<resource_range, 3> languages{};
  std::size_t entry_offset{0};
  std::uint8_t time_multiplier{0};
  std::uint8_t configuration{255};
  resource_range beacon_sequence{};
  std::array<scenario_cell_list, 3> cell_lists;
  unsigned int objective_cell_list{2};
  std::array<scenario_group, 3> groups;
  std::optional<std::size_t> player_program;
};

class scenario_resource {
private:
  std::vector<std::byte> data;
  std::vector<scenario_record> directory;

public:
  explicit scenario_resource(std::vector<std::byte> resource);
  std::span<scenario_record const> records() const noexcept;
  std::span<std::byte const> bytes(resource_range range) const;
  std::span<std::byte const> language(std::size_t record, scenario_language language) const;
};

} // namespace darker::resources

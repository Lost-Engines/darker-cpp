#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>
#include "maths/world_coordinates.h"

namespace darker::resources {

struct resource_range {
  size_t offset{0};
  size_t size{0};
};

enum class scenario_language { english, french, german };
enum class placement_form { moving, compact_special, absolute_static };

struct scenario_cell {
  uint8_t column{0};
  uint8_t row{0};
};

struct scenario_cell_list {
  std::vector<scenario_cell> cells;
  uint8_t terminator{255};
};

struct actor_behaviour {
  uint8_t attack_control{};                                                    // aircraft engagement thresholds; vehicles alternate firing intervals with its high bit
  uint8_t awareness_threshold{};
  uint8_t awareness_decay{};
  uint8_t awareness_rise{};
  uint8_t awareness_strength{};
  uint8_t evasion{};
};

struct scenario_placement {
  resource_range source{};
  placement_form form{placement_form::moving};
  uint8_t definition_slot{0};
  bool counted{false};
  uint8_t attributes{0};
  uint16_t heading{0};
  maths::map_position position{};
  actor_behaviour behaviour{};
  std::optional<uint16_t> script_or_target;
  std::optional<size_t> program_offset;
};

struct scenario_native_setup {
  resource_range source{};
  size_t current_object{0};
};

struct scenario_group {
  std::vector<scenario_placement> objects;
  std::vector<scenario_native_setup> native_setup;
};

struct scenario_record {
  resource_range source{};
  resource_range shared{};
  std::array<resource_range, 3> languages{};
  size_t entry_offset{0};
  uint8_t time_multiplier{0};
  uint8_t configuration{255};
  resource_range beacon_sequence{};
  std::array<scenario_cell_list, 3> cell_lists;
  unsigned int objective_cell_list{2};
  std::array<scenario_group, 3> groups;
  std::optional<size_t> player_program;
};

class scenario_resource {
private:
  std::vector<std::byte> data;
  std::vector<scenario_record> directory;

public:
  explicit scenario_resource(std::vector<std::byte> resource);
  std::span<scenario_record const> records() const noexcept;
  std::span<std::byte const> bytes(resource_range range) const;
  std::span<std::byte const> language(size_t record, scenario_language language) const;
};

} // namespace darker::resources

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <vector>
#include "game/city_map.h"

namespace darker::game {

struct mission_script {
  std::size_t continuation{0};
  std::size_t checkpoint{0};
  std::uint16_t checkpoint_clock{0};
  std::uint16_t deadline{0};
  bool stopped{false};
};

enum class message_alignment { centre, left, right };

struct mission_message {
  std::size_t offset{0};
  std::uint8_t length{0};
  std::uint8_t width{0};
  std::uint16_t expiry{0};
  message_alignment alignment{message_alignment::centre};
};

struct mission_context {
  std::span<std::byte const> program{};
  std::span<std::byte const> text{};
  std::span<std::uint8_t const> object_flags{};
  std::span<city_cell const> cells{};
  std::uint32_t clock{0};
  std::uint8_t time_multiplier{50};
  bool objectives_complete{false};
  bool at_target_cell{false};
  bool suppress_messages{false};
  std::uint8_t object_counter{0};
  std::uint8_t counter{0};
  std::uint8_t animation_parameter{0};
  std::size_t text_cursor{0};
  std::vector<mission_message> messages{};
  // The world admits reserves and returns the new objective-completion condition before script execution resumes.
  std::function<bool(uint8_t,uint8_t)> activate_reserves{};
  uint16_t current_cell{0};
  std::function<void(uint16_t,bool)> set_target{};
};

std::size_t advance_mission_script(mission_script &script, mission_context &context);

} // namespace darker::game

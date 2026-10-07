#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include "game/mission_script.h"

namespace darker::game {

struct mission_context_slot {
  std::span<std::byte const> program;
  std::span<std::byte const> text;
  std::optional<size_t> continuation;
  size_t text_cursor{0};
  bool stopped{false};
};

struct mission_exchange {
  std::optional<mission_context_slot> alternate;
  bool supplementary_active{false};

  void enter_supply(mission_script &script, mission_context &context);
  void exchange(mission_script &script, mission_context &context, std::optional<size_t> outgoing_continuation);
};

} // namespace darker::game

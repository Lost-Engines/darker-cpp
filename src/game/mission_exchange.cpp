#include "game/mission_exchange.h"
#include <stdexcept>
#include <utility>

namespace darker::game {

void mission_exchange::enter_supply(mission_script &script, mission_context &context) {
  /// C776 saves the player's stored continuation and resets supply text even when re-entering through its saved restart branch
  if(!alternate || supplementary_active) throw std::logic_error{"Supply entry requires an inactive supplementary context"};
  alternate->text_cursor = 0;
  exchange(script,context,script.continuation);
}

void mission_exchange::exchange(mission_script &script, mission_context &context, std::optional<size_t> const outgoing_continuation) {
  /// C781 exchanges program/message cursors without exchanging the object's checkpoint or global interval
  if(!alternate || !alternate->continuation) throw std::logic_error{"Mission context has no known continuation to restore"};
  script.continuation = *std::exchange(alternate->continuation,outgoing_continuation);
  std::swap(context.program,alternate->program);
  std::swap(context.text,alternate->text);
  std::swap(context.text_cursor,alternate->text_cursor);
  script.deadline = static_cast<uint16_t>(context.clock);
  std::swap(script.stopped,alternate->stopped);
  supplementary_active = !supplementary_active;
}

} // namespace darker::game

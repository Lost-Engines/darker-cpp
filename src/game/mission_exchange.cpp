#include "game/mission_exchange.h"
#include <stdexcept>
#include <utility>

namespace darker::game {

void mission_exchange::exchange(mission_script &script, mission_context &context, std::optional<size_t> const outgoing_continuation) {
  /// C781 exchanges program/message cursors without exchanging the object's checkpoint or global interval
  if(!alternate || !alternate->continuation) throw std::logic_error{"Mission context has no known continuation to restore"};
  script.continuation = *std::exchange(alternate->continuation,outgoing_continuation);
  std::swap(context.program,alternate->program);
  std::swap(context.text,alternate->text);
  std::swap(context.text_cursor,alternate->text_cursor);
  script.deadline = static_cast<uint16_t>(context.clock);
  script.stopped = false;
  supplementary_active = !supplementary_active;
}

} // namespace darker::game

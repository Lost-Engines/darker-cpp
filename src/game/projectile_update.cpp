#include "game/projectile_update.h"
#include <stdexcept>
#include "game/projectile_motion.h"
#include "game/projectile_steering.h"

namespace darker::game {

projectile_update_result update_projectile(projectile &record, std::uint16_t const clock,
  std::uint16_t const frame_step, projectile_placement const *const target) {
  /// 79E5–7A2D handle expiry before saving the old position and invoking a motion callback
  if(update_projectile_deadline(record, clock)) return projectile_update_result::expired;
  if(!record.parameters.definition) throw std::invalid_argument{"projectile update requires an object definition"};
  auto const entry{record.parameters.update_entry};
  if(entry != 0xcc64 && entry != 0xcc61) throw std::invalid_argument{"projectile motion callback is not implemented"};
  if(entry == 0xcc61 && !target) throw std::invalid_argument{"homing update requires a resolved object target"};
  record.previous_position = record.placement.position;
  if(entry == 0xcc64) advance_direct_projectile(record.placement, *record.parameters.definition, frame_step);
  else advance_object_homing_projectile(record, *target, frame_step);
  return projectile_update_result::advanced;
}

} // namespace darker::game

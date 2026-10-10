#include "game/projectile_update.h"
#include <bit>
#include <stdexcept>
#include "game/dual_launch.h"
#include "game/projectile_motion.h"
#include "game/projectile_steering.h"

namespace darker::game {

projectile_update_result update_projectile(projectile &record, uint16_t const clock,
  uint16_t const frame_step, projectile_target const target) {
  /// 79E5–7A2D handle expiry before saving the old position and invoking a motion callback
  auto remaining{static_cast<uint16_t>(record.deadline - clock)};
  bool const altitude_expiry{(record.flags & 0x60) && remaining >= 256 && !(remaining & 0x8000)
    && std::bit_cast<int8_t>(static_cast<uint8_t>(record.placement.position.height >> 8)) >= 0x50};
  if(update_projectile_deadline(record, clock)) return projectile_update_result::expired;
  if(!record.parameters.definition) throw std::invalid_argument{"projectile update requires an object definition"};
  if(altitude_expiry) remaining = static_cast<uint16_t>(remaining - 255);
  auto const entry{record.parameters.update_entry};
  if(entry != object_update::direct_projectile && entry != object_update::homing_projectile && entry != object_update::mimic && entry != object_update::chargeable && entry != object_update::dual_launch) throw std::invalid_argument{"projectile motion callback is not implemented"};
  auto const *object{std::get_if<object_pose const*>(&target)};
  auto const *cell{std::get_if<map_guidance_target>(&target)};
  if(entry == object_update::homing_projectile && !cell && (!object || !*object)) throw std::invalid_argument{"homing update requires a resolved target"};
  if(entry == object_update::mimic && (!object || !*object)) throw std::invalid_argument{"Mimic update requires the player pose"};
  if(entry == object_update::chargeable && (!object || !*object)) throw std::invalid_argument{"Chargeable update requires an object target"};
  if(entry == object_update::dual_launch && (!object || !*object)) throw std::invalid_argument{"Dual Launch update requires a capsule target"};
  record.previous_position = record.placement.position;
  if(entry == object_update::dual_launch && *object != &record.placement) {
    auto const separation{dual_launch_separation(record.placement, **object)};
    if(separation < 20) return projectile_update_result::detonated;
    advance_dual_projectile(record, **object, separation, frame_step);
  } else if(entry == object_update::direct_projectile || entry == object_update::dual_launch) advance_direct_projectile(record.placement, *record.parameters.definition, frame_step);
  else if(entry == object_update::mimic) advance_mimic_projectile(record, **object, remaining, frame_step);
  else if(entry == object_update::chargeable) advance_chargeable_projectile(record, **object, remaining, frame_step);
  else if(cell) advance_map_homing_projectile(record, *cell, frame_step);
  else advance_object_homing_projectile(record, **object, frame_step);
  return projectile_update_result::advanced;
}

} // namespace darker::game

#include "game/projectile_expiry.h"
#include "game/objective_counter.h"

namespace darker::game {

projectile *expire_projectile(projectile_pool &pool, projectile &record, projectile_references &references,
  objective_counters &objectives, weapon_ring_state &ring) {
  /// 7A52 repairs targets and counters before choosing native unlink/recycle disposition
  for(auto *other{pool.objects().head}; other; other = other->next) {
    if(other->target_token == record.native_id) other->target_token = other->native_id;
  }
  record.flags |= 0x20;
  if(references.selected_target == record.native_id) {
    references.selected_target = 0xffff;
    ring.target_spread = 508;
  }
  if(references.reference_2449 == record.native_id) references.reference_2449 = 0;
  if(references.missile_view == record.native_id) references.missile_view = 0;
  auto const counted{static_cast<std::uint8_t>(record.lifecycle & 1)};
  objectives.completed = static_cast<std::uint8_t>(objectives.completed + counted);
  objectives.outstanding = adjust_objective_counter(objectives.outstanding,static_cast<uint8_t>(-counted));
  auto const lifecycle{static_cast<std::uint8_t>(record.lifecycle & 0xfe)};
  if(lifecycle == 0) return pool.unlink(record);
  if(lifecycle < 0xfe) record.lifecycle = static_cast<std::uint8_t>(lifecycle - 2);
  return pool.recycle(record);
}

} // namespace darker::game

#include "game/projectile_pool.h"

namespace darker::game {

projectile_pool::projectile_pool() {
  /// 1D3C/1D49 prepend twelve consecutive records to the projectile free list
  for(auto &record : storage) {
    record.next = list.free;
    list.free = &record;
  }
}

projectile *projectile_pool::launch(projectile_launch const request) {
  /// CB01 expands and positions an allocated record without clearing retained fields
  auto *record{allocate_object(list)};
  if(!record) return nullptr;
  record->deadline = static_cast<std::uint16_t>(request.clock + request.lifetime);
  record->inherited_roll = request.emitter.angles[2];
  record->target_token = request.target_token;
  apply_object_definition(record->parameters, request.definition, request.model_token);
  record->placement = place_projectile(request.emitter);
  record->flags = 0x20;
  record->lifecycle = 0xfe;
  record->angular_motion[1] = 0;
  record->angular_motion[2] = 0;
  return record;
}

projectile *projectile_pool::recycle(projectile &record) {
  /// Return the next active projectile while putting this active member on the free list
  return recycle_object(list, record);
}

object_list<projectile> const &projectile_pool::objects() const noexcept {
  /// Expose traversal heads without transferring ownership of the fixed storage
  return list;
}

std::span<projectile const> projectile_pool::records() const noexcept {
  /// Retain stable identities for the lifetime of this nonmoving pool
  return storage;
}

} // namespace darker::game

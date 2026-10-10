#include "game/projectile_pool.h"

namespace darker::game {

projectile_pool::projectile_pool(projectile_list const category)
  : count{category == projectile_list::player ? 12u : 6u}, first_id{static_cast<uint16_t>(category == projectile_list::player ? 0xd1a6 : 0xd6e6)} {
  /// 1D3C/1D49 build separate twelve-shot player and six-shot hostile free lists
  uint16_t id{first_id};
  for(auto &record : std::span{storage}.first(count)) {
    record.native_id = id;
    id = static_cast<uint16_t>(id + 112);
    record.next = list.free;
    list.free = &record;
  }
}

projectile *projectile_pool::launch(projectile_launch const request) {
  /// CB01 expands and positions an allocated record without clearing retained fields
  auto *record{allocate_object(list)};
  if(!record) return nullptr;
  record->deadline = static_cast<uint16_t>(request.clock + request.lifetime);
  record->inherited_roll = request.emitter.angles.roll;
  record->target_token = request.target_token;
  apply_object_definition(record->parameters, request.definition, request.model_token);
  record->placement = place_projectile(request.emitter);
  record->flags = 0x20;
  record->lifecycle = 0xfe;
  record->angular_motion.pitch = 0;
  record->angular_motion.turn = 0;
  return record;
}

projectile *projectile_pool::recycle(projectile &record) {
  /// Return the next active projectile while putting this active member on the free list
  return recycle_object(list, record);
}

projectile *projectile_pool::unlink(projectile &record) {
  /// Remove an active member without making it available for allocation
  return unlink_object(list, record);
}

projectile *projectile_pool::resolve(uint16_t const native_id) noexcept {
  /// Native IDs preserve original target references without dereferencing DOS addresses
  if(native_id < first_id) return nullptr;
  unsigned int const offset{static_cast<unsigned int>(native_id - first_id)};
  if(offset % 112 != 0 || offset / 112 >= count) return nullptr;
  return &storage[offset / 112];
}

object_list<projectile> const &projectile_pool::objects() const noexcept {
  /// Expose traversal heads without transferring ownership of the fixed storage
  return list;
}

std::span<projectile const> projectile_pool::records() const noexcept {
  /// Retain stable identities for the lifetime of this nonmoving pool
  return std::span{storage}.first(count);
}

} // namespace darker::game

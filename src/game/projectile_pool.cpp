#include "game/projectile_pool.h"

namespace darker::game {

projectile_pool::projectile_pool() {
  /// 1D3C/1D49 prepend twelve consecutive records to the projectile free list
  std::uint16_t id{0xd1a6};
  for(auto &record : storage) {
    record.native_id = id;
    id = static_cast<std::uint16_t>(id + 112);
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

projectile *projectile_pool::unlink(projectile &record) {
  /// Remove an active member without making it available for allocation
  return unlink_object(list, record);
}

projectile *projectile_pool::resolve(std::uint16_t const native_id) noexcept {
  /// Native IDs preserve original target references without dereferencing DOS addresses
  if(native_id < 0xd1a6) return nullptr;
  unsigned int const offset{static_cast<unsigned int>(native_id - 0xd1a6)};
  if(offset % 112 != 0 || offset / 112 >= storage.size()) return nullptr;
  return &storage[offset / 112];
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

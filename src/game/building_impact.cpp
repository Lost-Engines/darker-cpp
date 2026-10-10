#include "game/building_impact.h"
#include "game/city_map.h"
#include "game/object_definitions.h"

namespace darker::game {

bool projectile_damages_building(size_t const definition_slot, collision_category const category, uint8_t const state, bool const linked) {
  /// CDB9 requires a destruction link; ordinary player ground weapons additionally require a marked category-zero surface
  size_t constexpr first_non_projectile_definition{19};
  size_t constexpr first_non_caero_projectile_definition{10};                  // slots 0–9 are Caero weapons; later projectiles bypass its marked-surface gate
  if(category == collision_category::protected_surface || !linked) return false;
  if(category == collision_category::fragile) return true;
  if(definition_slot >= first_non_projectile_definition || !original_object_definitions.at(definition_slot).role_data.projectile().has_flag(projectile_flag::ground_target)) return false;
  return definition_slot >= first_non_caero_projectile_definition || (category == collision_category::ground_target && (state & city_cell::permitted_target));
}

} // namespace darker::game

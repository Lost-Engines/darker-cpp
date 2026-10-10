#include "game/building_impact.h"
#include "game/object_definitions.h"

namespace darker::game {

bool projectile_damages_building(size_t const definition_slot, uint8_t const category, uint8_t const state, bool const linked) {
  /// CDB9 requires a destruction link; ordinary player ground weapons additionally require a marked category-zero surface
  if(category == 1 || !linked) return false;
  if(category == 2) return true;
  if(definition_slot >= 19 || !(original_object_definitions.at(definition_slot).role_data.projectile().flags & 2)) return false;
  return definition_slot >= 10 || (category == 0 && (state & 0x40));
}

} // namespace darker::game

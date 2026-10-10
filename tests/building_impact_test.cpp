#include <catch2/catch_test_macros.hpp>
#include "game/building_impact.h"
#include "reference/building_impact_samples.h"

TEST_CASE("Projectile building damage admission matches native model and weapon guards", "[game][weapons]") {
  /// Cover shipped linked/unlinked variants, marked objectives, collision categories and definition classes
  for(auto const &s : darker::test_reference::building_impact_samples) {
    CAPTURE(s);
    CHECK(darker::game::projectile_damages_building(s[2], static_cast<darker::game::collision_category>(s[3]), static_cast<uint8_t>(s[1]), s[4] != 0) == (s[5] != 0));
  }
}

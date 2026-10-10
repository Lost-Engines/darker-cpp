#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include "game/scenario_actor.h"
#include "resources/geometry_bank.h"

namespace darker::game {

struct actor_course {
  uint16_t heading{0};
  uint16_t pitch{0};
  uint16_t distance{0};
  uint8_t climb{0};
};

struct actor_manoeuvre {
  uint16_t pitch{0};
  uint16_t turn_drive{0};
  uint8_t speed{0};
  // calls the firing eligibility helper with this distance byte; does not imply a shot is emitted
  std::optional<uint8_t> firing_distance{};
};

void consider_aircraft_threat(std::array<uint8_t, 4> &errors, scenario_actor const &actor, actor_course course) noexcept;

actor_manoeuvre choose_actor_manoeuvre(scenario_actor const &actor, actor_course course) noexcept;
void select_actor_target(scenario_actor &actor) noexcept;
actor_course actor_object_course(object_pose const &actor, object_pose const &target);
actor_course actor_cell_course(scenario_actor const &actor, uint16_t cell,
  resources::city_type const &type, resources::model_header const &model);
void reset_actor_clearance(scenario_actor &actor) noexcept;
void consider_actor_clearance(scenario_actor &actor, scenario_actor const &neighbour, actor_course &course) noexcept;
void adjust_actor_clearance(scenario_actor const &actor, actor_course &course, uint16_t nearby_height) noexcept;
uint16_t actor_city_clearance(object_pose const &actor, city_map const &cells,
  resources::geometry_bank const &bank, uint8_t damage_mask);

} // namespace darker::game

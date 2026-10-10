#include "game/weapon_target.h"
#include <bit>
#include "game/aircraft_combat.h"
#include "game/city_sweep.h"
#include "game/object_definitions.h"
#include "maths/sine_table.h"
#include "maths/world_coordinates.h"

#include "vectorstorm/vector/vector2.h"

#include "game/native_object_layout.h"

#include "game/object_catalogue.h"

namespace darker::game {

void weapon_target::clear() noexcept {
  /// CFCB releases the selected token and restores the untracked ring spread
  token = no_target;
  spread = untracked_spread;
}

maths::world_position target_ray_end(object_pose const &player) noexcept {
  /// 1E77/6D08 construct the unrolled forward targeting ray with the original truncation points
  auto const heading{player.angles.heading >> 6}, pitch{player.angles.pitch >> 6};
  auto const sine{maths::original_sine[heading]}, cosine{maths::original_sine[(heading + 256) % 1024]};
  auto const pitch_cosine{maths::original_sine[(pitch + 256) % 1024]};
  auto const x{(-(sine * pitch_cosine >> 16)) >> 2};
  auto const y{-((cosine * pitch_cosine >> 16) >> 2)};
  return {static_cast<uint16_t>(player.position.column + x), static_cast<uint16_t>(player.position.row + y),
    static_cast<uint16_t>((player.position.height & 0xfffe) + (maths::original_sine[pitch] >> 1) * 2)};
}

uint16_t acquire_caero_target(object_pose const &player, std::span<scenario_actor const> const actors,
  city_map const &cells, resources::geometry_bank const &bank, uint8_t const damage_mask) {
  /// 6D08 clips against the city, then selects the last intersecting aircraft without shortening the ray at its hull
  int constexpr target_ray_terrain_height{10};
  int constexpr actor_records_base{native_object_layout::actors};
  int constexpr actor_record_bytes{native_object_layout::record_bytes};
  auto end{target_ray_end(player)};
  auto const hit{sweep_city(bank, cells, damage_mask, player.position, end, 0, target_ray_terrain_height)};
  uint16_t selected{hit.contact == city_contact::building ? static_cast<uint16_t>(hit.row * 256 + hit.column) : weapon_target::no_target};
  for(auto const &actor : actors) {
    if(actor.category != actor_category::air) continue;
    auto candidate{end};
    if(sweep_aircraft(actor.pose, bank.header_at(actor.parameters.model_token).extent, 0, player.position, candidate)) {
      selected = static_cast<uint16_t>(actor_records_base + actor.index * actor_record_bytes);
    }
  }
  return selected;
}

void weapon_target::project(maths::world_position const &player,
  maths::world_position const &target, uint16_t const extent, maths::view_basis const &basis,
  uint8_t const secondary_weapon, uint8_t const cell_type, uint8_t const cell_state, bool const skimma) noexcept {
  /// CF94–D056 retain a resolved lock only inside its original view cone and target class
  int constexpr target_window_half_width_cells{16};
  int constexpr target_window_width_cells{32};
  int constexpr minimum_projection_depth{64};
  int constexpr projection_scale{256};
  int constexpr skimma_axis_limit{62};
  int constexpr caero_axis_limit{55};
  int constexpr skimma_radius_squared_limit{0x0e89};
  int constexpr caero_radius_squared_limit{0x0b64};
  int constexpr beacon_model_type{1};
  int constexpr permitted_ground_target_flag{0x40};
  if(!skimma && !secondary_weapon) {
    clear();
    return;
  }
  if(token == weapon_target::no_target) return;
  auto const dx{static_cast<uint16_t>(target.column - player.column)};
  auto const dy{static_cast<uint16_t>(target.row - player.row)};
  if(static_cast<uint8_t>((dx >> 8) + target_window_half_width_cells) >= target_window_width_cells || static_cast<uint8_t>((dy >> 8) + target_window_half_width_cells) >= target_window_width_cells) {
    clear();
    return;
  }
  auto const height{static_cast<uint16_t>(player.height - target.height + (extent >> 2))};
  auto const x{std::bit_cast<int16_t>(static_cast<uint16_t>(dx * 8))};
  auto const y{std::bit_cast<int16_t>(static_cast<uint16_t>(dy * 8))};
  auto const z{std::bit_cast<int16_t>(height)};
  auto const transform{[&](int16_t maths::view_axis::*const member){
    auto const product{static_cast<uint32_t>(basis[1].*member * x)
      - static_cast<uint32_t>(basis[0].*member * y) + static_cast<uint32_t>(basis[2].*member * z)};
    return std::bit_cast<int16_t>(static_cast<uint16_t>(product >> 16));
  }};
  auto const depth{transform(&maths::view_axis::depth)};
  if(depth < minimum_projection_depth) {
    clear();
    return;
  }
  horizontal = static_cast<int16_t>(transform(&maths::view_axis::horizontal) * projection_scale / depth);
  auto const horizontal_magnitude{horizontal < 0 ? ~horizontal : horizontal};
  if(horizontal_magnitude >= (skimma ? skimma_axis_limit : caero_axis_limit)) {
    clear();
    return;
  }
  vertical = static_cast<int16_t>(transform(&maths::view_axis::vertical) * projection_scale / depth);
  auto const vertical_magnitude{vertical < 0 ? ~vertical : vertical};
  if(vertical_magnitude >= (skimma ? skimma_axis_limit : caero_axis_limit)) {
    clear();
    return;
  }
  auto const radial{vec2<int>{horizontal_magnitude, vertical_magnitude}.length_sq()};
  if(radial >= (skimma ? skimma_radius_squared_limit : caero_radius_squared_limit)) {
    clear();
    return;
  }
  if(!skimma) distance = static_cast<uint8_t>(radial >> 8);
  bool const ground{original_object_definitions[skimma ? object_catalogue::skimma_weapon(secondary_weapon) : secondary_weapon - 1].role_data.projectile().has_flag(projectile_flag::ground_target)};
  if(ground ? (token & weapon_target::aircraft_token_bit) || (!skimma && (cell_type == beacon_model_type || !(cell_state & permitted_ground_target_flag))) : !(token & weapon_target::aircraft_token_bit)) {
    clear();
    return;
  }
  if(skimma) {
    uint16_t root{0};
    for(uint16_t bit{128}; bit; bit >>= 1) {
      auto const candidate{static_cast<uint16_t>(root | bit)};
      if(candidate * candidate <= radial * 4) root = candidate;
    }
    spread = static_cast<uint16_t>(root * 4);
  } else spread = height;
}

void weapon_target::project_caero(maths::world_position const &player,
  maths::world_position const &target, uint16_t const extent, maths::view_basis const &basis,
  uint8_t const secondary_weapon, uint8_t const cell_type, uint8_t const cell_state) noexcept {
  /// Preserve the Caero target class and marked-building restrictions
  project(player, target, extent, basis, secondary_weapon, cell_type, cell_state, false);
}

void weapon_target::project_skimma(maths::world_position const &player,
  maths::world_position const &target, uint16_t const extent, maths::view_basis const &basis,
  uint8_t const weapon, bool const enabled, bool const reloading, bool const destructible) noexcept {
  /// CF4A accepts enabled, reloaded weapons and rejects indestructible map targets after projection
  uint8_t constexpr selectable_weapon_count{3};
  if(!enabled || reloading || weapon >= selectable_weapon_count) {
    clear();
    return;
  }
  project(player, target, extent, basis, weapon, 0, 0, true);
  if(!(token & weapon_target::aircraft_token_bit) && !destructible) clear();
}

} // namespace darker::game

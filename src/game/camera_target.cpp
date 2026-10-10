#include "game/camera_target.h"
#include <bit>
#include "game/aircraft_combat.h"
#include "game/city_sweep.h"
#include "game/projectile_steering.h"
#include "maths/view_basis.h"

namespace darker::game {

std::array<uint16_t,3> camera_ray_end(object_pose const &camera) noexcept {
  /// 2591 casts from the rendered camera along its installed depth coefficients.
  auto const basis{maths::make_view_basis(camera.angles)};
  return {static_cast<uint16_t>(camera.position[0] + (basis[1].depth >> 3)),
    static_cast<uint16_t>(camera.position[1] - (basis[0].depth >> 3)),
    static_cast<uint16_t>(((std::bit_cast<int16_t>(camera.position[2]) >> 1) - (basis[2].depth >> 1))*2)};
}

bool camera_target_in_range(object_pose const &player, object_pose const &target) noexcept {
  /// 841C/269E retain the asymmetric negative-X complement and wrapping distance sum.
  auto const x{std::bit_cast<int16_t>(static_cast<uint16_t>(player.position[0]-target.position[0]))};
  auto const y{std::bit_cast<int16_t>(static_cast<uint16_t>(player.position[1]-target.position[1]))};
  return static_cast<uint16_t>((x < 0 ? ~x : x) + (y < 0 ? -y : y)) < 0x1000;
}

std::optional<camera_target> pick_camera_target(object_pose const &camera, object_pose const &player,
  uint16_t const player_extent, std::optional<uint8_t> const excluded, std::span<scenario_actor> const actors,
  city_map const &cells, resources::geometry_bank const &bank, uint8_t const damage_mask) {
  /// 257D visits player, ground, static and aircraft lists after clipping the ray against the city.
  if(!camera_target_in_range(player,camera)) return std::nullopt;
  auto end{camera_ray_end(camera)};
  auto const hit{sweep_city(bank,cells,damage_mask,camera.position,end,0,10)};
  std::optional<camera_target> result;
  if(hit.contact == city_contact::building) {
    auto const building{resolve_map_guidance(static_cast<uint16_t>(hit.row*256+hit.column),cells,bank,damage_mask)};
    result = camera_target{.anchor{.position{building.position[0],building.position[1],
      static_cast<uint16_t>(building.height + building.height_extent*4)}}};
  }
  auto candidate{end};
  if(excluded != 0 && sweep_aircraft(player,player_extent,0,camera.position,candidate))
    result = camera_target{.actor{0},.anchor{player}};
  constexpr std::array categories{actor_category::ground,actor_category::stationary,actor_category::air};
  if(auto const *actor{sweep_actor_groups(actors,bank,camera.position,end,0,categories,excluded)})
    result = camera_target{.actor{actor->index},.anchor{actor->pose}};
  if(result && !camera_target_in_range(player,result->anchor)) return std::nullopt;
  return result;
}

} // namespace darker::game

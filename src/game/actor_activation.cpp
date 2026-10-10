#include "game/actor_activation.h"
#include <algorithm>
#include <bit>
#include <stdexcept>
#include <utility>

namespace darker::game {

bool retire_distant_actor(scenario_actor &actor, object_pose const &player, uint16_t const clock) noexcept {
  /// C30A/8432 stop and expire a scripted actor once its wrapped cell distance reaches 36 cells
  auto const difference{[&](size_t const axis){
    return std::bit_cast<int8_t>(static_cast<uint8_t>((player.position[axis] >> 8) - (actor.pose.position[axis] >> 8)));
  }};
  int const x{difference(0)}, y{difference(1)};
  if(x * x + y * y < 0x510) return false;
  actor.parameters.update_entry = object_update::retired_actor;
  actor.flags |= 0x28;
  actor.expiry = clock;
  return true;
}

void place_air_reserve(scenario_actor &actor, object_pose const &player, std::span<scenario_actor const> const active) {
  /// C39E moves nearby reinforcements outside the player radius, then scans the active air list through C313
  auto const difference{[&](size_t const axis){
    return std::bit_cast<int8_t>(static_cast<uint8_t>((player.position[axis] >> 8) - (actor.pose.position[axis] >> 8)));
  }};
  int const x{difference(0)}, y{difference(1)};
  if(x * x + y * y < 0x510) {
    size_t const axis{x * x < y * y ? 1u : 0u};
    auto const cell{static_cast<uint8_t>((player.position[axis] >> 8) + (difference(axis) < 0 ? 36 : -36))};
    actor.pose.position[axis] = static_cast<uint16_t>((cell << 8) | (actor.pose.position[axis] & 255));
  }
  uint8_t overlaps{0}, clearance{0};
  for(auto const &other : active) {
    if(other.category != actor_category::air) continue;
    auto const x{static_cast<uint16_t>(other.pose.position.column - (actor.pose.position.column - 255))};
    auto const y{static_cast<uint16_t>(other.pose.position.row - (actor.pose.position.row - 255))};
    if(x >= 510 || y >= 510) continue;
    auto const delta{static_cast<uint16_t>(other.pose.position.height - 512 - actor.pose.position.height)};
    if(std::bit_cast<int16_t>(static_cast<uint16_t>(other.pose.position.height - 512)) >= std::bit_cast<int16_t>(actor.pose.position.height)) continue;
    auto const high{static_cast<uint8_t>(delta >> 8)};
    if(clearance >= high) clearance = high;
    overlaps = static_cast<uint8_t>(overlaps + (high >= 252));
  }
  if(overlaps) {
    auto const high{static_cast<uint8_t>((actor.pose.position.height >> 8) - clearance + 2)};
    actor.pose.position.height = static_cast<uint16_t>((high << 8) | (actor.pose.position.height & 255));
  }
}

void activate_scenario_reserves(std::vector<scenario_actor> &active, std::vector<scenario_actor> &reserves,
  actor_category const category, uint8_t const count, object_pose const &player, uint16_t const clock) {
  /// C33E pops each category's reserve head and prepends to its active list, reversing a multi-object admission
  unsigned int const requested{count ? count : 256u};
  if(std::ranges::count_if(reserves, [&](auto const &actor){
    return actor.category == category;
  }) < requested) {
    throw std::invalid_argument{"Mission requested more objects than its reserve category contains"};
  }
  for(unsigned int i{0}; i < requested; ++i) {
    auto const next{std::ranges::find_if(reserves, [&](auto const &actor){
      return actor.category == category;
    })};
    auto actor{*next};
    reserves.erase(next);
    if(actor.route) actor.route->origin = static_cast<uint16_t>(clock - 4096);
    else actor.script.deadline = clock;
    if(category == actor_category::air && !actor.tunnel) place_air_reserve(actor, player, active);
    auto const head{std::ranges::find_if(active, [&](auto const &other){
      return other.category >= category;
    })};
    active.insert(head, std::move(actor));
  }
}

} // namespace darker::game

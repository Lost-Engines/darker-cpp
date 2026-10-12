#include "game/actor_activation.h"
#include <algorithm>
#include <bit>
#include <stdexcept>
#include <utility>
#include "vectorstorm/vector/vector2.h"

namespace darker::game {
namespace {

int constexpr reserve_distance_cells{36};
int constexpr reserve_distance_squared{reserve_distance_cells * reserve_distance_cells};
int constexpr separation_half_width{255};                                      // 1/256-cell units; the native interval is [-255, 255)
int constexpr separation_width{separation_half_width * 2};
int constexpr height_clearance{512};                                           // two high-byte height units

} // anonymous namespace

bool retire_distant_actor(scenario_actor &actor, object_pose const &player, clock_tick const clock) noexcept {
  /// C30A/8432 stop and expire a scripted actor once its wrapped cell distance reaches 36 cells
  auto const difference{[&](size_t const axis){
    return std::bit_cast<int8_t>(static_cast<uint8_t>((player.position[axis] >> 8) - (actor.pose.position[axis] >> 8)));
  }};
  vec2<int> const separation{difference(0), difference(1)};
  if(separation.length_sq() < reserve_distance_squared) return false;
  actor.parameters.update_entry = object_update::retired_actor;
  actor.flags |= 0x28;
  actor.expiry = clock;
  return true;
}

void place_air_reserve(scenario_actor &actor, object_pose const &player, std::span<scenario_actor const> const active, air_reserve_mode const mode) {
  /// C39E moves nearby reinforcements outside the player radius, then scans the active air list through C313
  auto const difference{[&](size_t const axis){
    return std::bit_cast<int8_t>(static_cast<uint8_t>((player.position[axis] >> 8) - (actor.pose.position[axis] >> 8)));
  }};
  vec2<int> const separation{difference(0), difference(1)};
  if(mode == air_reserve_mode::campaign && separation.length_sq() < reserve_distance_squared) {
    size_t const axis{separation.x * separation.x < separation.y * separation.y ? 1u : 0u};
    auto const cell{static_cast<uint8_t>((player.position[axis] >> 8) + (difference(axis) < 0 ? reserve_distance_cells : -reserve_distance_cells))};
    actor.pose.position[axis] = static_cast<uint16_t>((cell << 8) | (actor.pose.position[axis] & 255));
  }
  uint8_t overlaps{0}, clearance{0};
  // C3E2 retains authored Nightmare positions and checks ground actors instead.
  auto const obstacle_category{mode == air_reserve_mode::nightmare ? actor_category::ground : actor_category::air};
  for(auto const &other : active) {
    if(other.category != obstacle_category) continue;
    auto const x{static_cast<uint16_t>(other.pose.position.column - (actor.pose.position.column - separation_half_width))};
    auto const y{static_cast<uint16_t>(other.pose.position.row - (actor.pose.position.row - separation_half_width))};
    if(x >= separation_width || y >= separation_width) continue;
    auto const delta{static_cast<uint16_t>(other.pose.position.height - height_clearance - actor.pose.position.height)};
    if(std::bit_cast<int16_t>(static_cast<uint16_t>(other.pose.position.height - height_clearance)) >= std::bit_cast<int16_t>(actor.pose.position.height)) continue;
    auto const high{static_cast<uint8_t>(delta >> 8)};
    if(clearance >= high) clearance = high;
    uint8_t constexpr nearest_lower_height_byte{252};                          // wrapped -4: admit the four immediately lower height bands
    overlaps = static_cast<uint8_t>(overlaps + (high >= nearest_lower_height_byte));
  }
  if(overlaps) {
    auto const high{static_cast<uint8_t>((actor.pose.position.height >> 8) - clearance + 2)};
    actor.pose.position.height = static_cast<uint16_t>((high << 8) | (actor.pose.position.height & 255));
  }
}

void activate_scenario_reserves(std::vector<scenario_actor> &active, std::vector<scenario_actor> &reserves,
  actor_category const category, uint8_t const count, object_pose const &player, clock_tick const clock, air_reserve_mode const mode) {
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
    if(category == actor_category::air && !actor.tunnel) {
      if(mode == air_reserve_mode::nightmare) {
        // C350/C35A choose C3E2 for Nightmare: fade in where the wave was authored.
        uint8_t constexpr appearing_flag{0x40};
        game_duration constexpr arrival_fade_ticks{256};
        actor.fade = 0;
        actor.flags = appearing_flag;
        actor.expiry = static_cast<clock_tick>(clock + arrival_fade_ticks);
      }
      place_air_reserve(actor, player, active, mode);
    }
    auto const head{std::ranges::find_if(active, [&](auto const &other){
      return other.category >= category;
    })};
    active.insert(head, std::move(actor));
  }
}

} // namespace darker::game

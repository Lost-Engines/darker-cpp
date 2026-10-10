#include "game/vehicle_combat.h"
#include <array>
#include <bit>
#include <utility>
#include "game/city_map.h"
#include "game/object_definitions.h"
#include "maths/direction.h"

#include "game/native_object_layout.h"

namespace darker::game {

projectile *fire_vehicle_missile(projectile_pool &pool, scenario_actor &vehicle, object_pose const &player,
  city_map const &cells, std::span<resources::city_type const> const types, uint8_t const direction,
  clock_tick const clock, uint8_t const difficulty, uint16_t const model) {
  /// 9185 checks paired-shot cadence, rearward aim and two clear cells before CAF0 attempts a homing launch
  if(!(vehicle.parameters.definition->role_data.craft().flags & 1)) return nullptr;
  auto const elapsed{static_cast<uint16_t>(clock - vehicle.last_shot)};
  auto const delay{static_cast<uint16_t>(((0x2ff ^ difficulty) << 3) - 0x400)};
  if(vehicle.behaviour.attack_control & 128 ? elapsed < 0x300 : elapsed < delay) return nullptr;
  auto const x{static_cast<uint16_t>(vehicle.pose.position.column - player.position.column)};
  auto const y{static_cast<uint16_t>(vehicle.pose.position.row - player.position.row)};
  auto const distance{static_cast<uint16_t>((x ^ ((x & 0x8000) ? 0xffff : 0)) + ((y & 0x8000) ? -y : y))};
  if(distance >= (0x200 + difficulty) * 4 || player.position.height >= static_cast<uint16_t>(distance * 8)) return nullptr;
  std::array<uint16_t, 5> constexpr reflection{0xffff, 0xffff, 0, 0, 0xffff};
  auto lateral{static_cast<uint16_t>(x ^ reflection.at(direction / 2))};
  auto longitudinal{static_cast<uint16_t>(y ^ reflection.at(direction / 2 + 1))};
  if(direction & 2) std::swap(lateral, longitudinal);
  if(std::bit_cast<int16_t>(longitudinal) <= 0) return nullptr;
  auto const angle{static_cast<uint8_t>(maths::direction_index(lateral, longitudinal) >> 3)};
  if(static_cast<uint8_t>(angle ^ ((angle & 128) ? 255 : 0)) >= 14) return nullptr;
  std::array<int, 4> constexpr columns{0, -1, 0, 1}, rows{-1, 0, 1, 0};
  auto column{static_cast<uint8_t>(vehicle.pose.position.column >> 8)}, row{static_cast<uint8_t>(vehicle.pose.position.row >> 8)};
  for(unsigned int cell{0}; cell < 2; ++cell) {
    column = static_cast<uint8_t>(column - columns.at(direction / 2));
    row = static_cast<uint8_t>(row - rows.at(direction / 2));
    // native TEST clears carry, so 9210 treats an off-map probe as unobstructed
    if((column | row) & 128) continue;
    auto const type{cells[row * city_map_size.column + column].type};
    if(type && types[type - 1].collision_marker != resources::city_type::background_marker) return nullptr;
  }
  vehicle.behaviour.attack_control = static_cast<uint8_t>(~vehicle.behaviour.attack_control);
  vehicle.last_shot = clock;
  auto const &definition{original_object_definitions[18]};
  launch_emitter const emitter{
    .position{vehicle.pose.position},
    .fractions{vehicle.pose.fractions},
    .angles{vehicle.pose.angles},
    .speed{vehicle.pose.speed},
    .side_flags{vehicle.flags},
    .definition_strength{vehicle.parameters.definition->impact_strength}
  };
  return pool.launch({
    .definition{definition},
    .emitter{emitter},
    .model_token{model},
    .clock{clock},
    .lifetime{static_cast<uint16_t>(definition.role_data.projectile().lifetime * 256)},
    .target_token{native_object_layout::player}
  });
}

} // namespace darker::game

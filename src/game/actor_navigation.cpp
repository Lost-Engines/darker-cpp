#include "game/actor_navigation.h"
#include <algorithm>
#include <bit>
#include "maths/direction.h"

namespace darker::game {
namespace {

actor_course target_course(object_pose const &actor, std::array<std::uint16_t, 3> const &target) {
  /// 88DC resolves the same position/angle convention used by projectile object guidance
  auto const direction{maths::object_target_direction(actor.position, target)};
  return {.heading{direction.heading}, .pitch{direction.pitch}, .distance{horizontal_distance(actor.position, target)}};
}

} // anonymous namespace

void select_actor_target(scenario_actor &actor) noexcept {
  /// 8826 retains pursuit of the player while awareness is nonzero, otherwise resumes the scripted target
  if(actor.awareness.level != 0) {
    if(actor.selected_target == 0xd986) return;
    if(actor.behaviour[1] < (actor.awareness.level >> 8)) {
      actor.selected_target = 0xd986;
      return;
    }
  }
  actor.selected_target = actor.target_token;
}

actor_course actor_object_course(object_pose const &actor, object_pose const &target) {
  /// 88B9 offsets close object targets above or below their height to avoid a direct collision course
  auto position{target.position};
  if(horizontal_distance(actor.position, position) < 511) {
    auto const difference{std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(actor.position[2] - position[2]))};
    position[2] = static_cast<std::uint16_t>(position[2] + (difference < 0 ? -512 : 512));
  }
  return target_course(actor, position);
}

actor_course actor_cell_course(scenario_actor const &actor, std::uint16_t const cell,
  resources::city_type const &type, resources::model_header const &model) {
  /// 8846 aims at the base descriptor rather than a damage variant, with a separate nominal-height fallback
  auto const column{static_cast<std::uint8_t>(cell)};
  auto const row{static_cast<std::uint8_t>(cell >> 8)};
  bool const fallback{column >= 128 || (type.collision_marker & 128) != 0};
  auto const &definition{*actor.parameters.definition};
  auto height{static_cast<std::uint16_t>(fallback ? definition.role_data[2] * 256 + (column >= 128 ? column : 0)
    : type.collision_marker * 256 - model.height + model.extent * 4)};
  if(actor.definition_slot == 22) height = static_cast<std::uint16_t>(height + 0x2400 - actor.pose.speed * 8);
  return target_course(actor.pose, {static_cast<std::uint16_t>(column * 256 + (fallback ? 128 : type.column_fraction)),
    static_cast<std::uint16_t>(row * 256 + (fallback ? 128 : type.row_fraction)), height});
}

void reset_actor_clearance(scenario_actor &actor) noexcept {
  /// 8908–8928 resets retained object clearance after leaving its reference cell, before scanning neighbours
  auto const magnitude{[](int const difference){
    auto const value{std::bit_cast<std::int8_t>(static_cast<std::uint8_t>(difference))};
    return static_cast<std::uint8_t>(value < 0 ? -value : value);
  }};
  auto const column{magnitude((actor.pose.position[0] >> 8) - (actor.parameters.flags_4c & 255))};
  auto const row{magnitude((actor.pose.position[1] >> 8) - (actor.parameters.flags_4c >> 8))};
  if(static_cast<std::uint8_t>(column + row) >= 3) actor.clearance_floor = 0x076c;
}

void adjust_actor_clearance(scenario_actor const &actor, actor_course &course, std::uint16_t const nearby_height) noexcept {
  /// 8966–89A2 applies the freshly scanned object clearance and surrounding building height
  auto const height{std::bit_cast<std::int16_t>(nearby_height)};
  auto const altitude{std::bit_cast<std::int16_t>(actor.pose.position[2])};
  if(height >= altitude) {
    auto const delta{static_cast<std::uint16_t>(height - altitude)};
    auto const climb{static_cast<std::uint8_t>(-((std::min<int>(delta, 0x07f7) >> 3) + 1))};
    course.climb = std::max(course.climb, climb);
  }
  auto const floor{std::max(height, std::bit_cast<std::int16_t>(actor.clearance_floor))};
  if(floor > altitude) {
    auto const delta{static_cast<std::uint16_t>(floor - altitude)};
    course.pitch = static_cast<std::uint16_t>(std::min<int>(delta, 0x04ff) * 4 + 0x100);
  }
}

void consider_actor_clearance(scenario_actor &actor, scenario_actor const &neighbour, actor_course &course) noexcept {
  /// 6DB5/8C50 scans a wrapping square and raises clearance above a nearby lower actor
  if(&actor == &neighbour) return;
  for(std::size_t axis{0}; axis < 2; ++axis) {
    if(static_cast<std::uint16_t>(neighbour.pose.position[axis] - actor.pose.position[axis] + 1023) >= 2046) return;
  }
  auto difference{static_cast<std::uint16_t>(neighbour.pose.position[2] - actor.pose.position[2])};
  if(difference == 0) difference = neighbour.index < actor.index ? 65535 : 0;
  if((difference >> 8) < 0xfc) return;
  auto const negative_distance{[](int const difference){
    auto const value{std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(difference))};
    return value < 0 ? value : -value;
  }};
  auto distance{static_cast<std::uint16_t>(negative_distance(neighbour.pose.position[0] - actor.pose.position[0])
    + negative_distance(neighbour.pose.position[1] - actor.pose.position[1]))};
  distance = static_cast<std::uint16_t>(distance + 256);
  if((distance >> 8) == 0) distance = 65535;
  if((distance >> 8) < 252) return;
  distance = static_cast<std::uint16_t>(distance + 1024);
  auto height{static_cast<std::uint16_t>(distance + neighbour.pose.position[2])};
  auto pitch{std::bit_cast<std::int16_t>(neighbour.pose.angles[1])};
  if(pitch > 0) {
    pitch = std::min<std::int16_t>(pitch, 0x05ff);
    height = static_cast<std::uint16_t>(height + pitch);
  }
  if(std::bit_cast<std::int16_t>(height) < std::bit_cast<std::int16_t>(actor.clearance_floor)) return;
  actor.clearance_floor = height;
  actor.parameters.flags_4c = static_cast<std::uint16_t>((neighbour.pose.position[0] >> 8) | (neighbour.pose.position[1] & 0xff00));
  if(neighbour.pose.speed >= actor.parameters.definition->role_data[4] * 12) {
    auto const value{static_cast<std::uint16_t>(height - pitch)};
    course.climb = static_cast<std::uint8_t>(~(value >> 2));
  }
}

std::uint16_t actor_city_clearance(object_pose const &actor, city_map const &cells,
  resources::geometry_bank const &bank, std::uint8_t const damage_mask) {
  /// 8935/8CDD checks eight surrounding cells, resolving alternate and damaged model headers
  std::int16_t maximum{0x076c};
  for(int row_delta{-1}; row_delta <= 1; ++row_delta) {
    auto const row{static_cast<std::uint8_t>((actor.position[1] >> 8) + row_delta)};
    if(row >= 128) continue;
    for(int column_delta{-1}; column_delta <= 1; ++column_delta) {
      if(row_delta == 0 && column_delta == 0) continue;
      auto const column{static_cast<std::uint8_t>((actor.position[0] >> 8) + column_delta)};
      if(column >= 128) continue;
      auto const cell{cells[row * 128 + column]};
      if(cell.type == 0) continue;
      auto const model{bank.header_at(bank.city_model_offset(cell.type, cell.state, damage_mask))};
      auto const marker{bank.city_types()[cell.type - 1].collision_marker};
      auto const height{static_cast<std::uint16_t>(-marker * 256 - model.height + model.extent * 5)};
      maximum = std::max(maximum, std::bit_cast<std::int16_t>(height));
    }
  }
  return static_cast<std::uint16_t>(maximum);
}

actor_manoeuvre choose_actor_manoeuvre(scenario_actor const &actor, actor_course course) noexcept {
  /// 89A3–8A51 selects speed, pitch, turning and a possible firing check after target/obstacle resolution
  auto const &definition{*actor.parameters.definition};
  auto const awareness{static_cast<std::uint8_t>(actor.awareness.level >> 8)};
  actor_manoeuvre result{.pitch{course.pitch}, .speed{definition.role_data[4]}};
  if(course.climb != 0) {
    auto const scaled{(course.climb * definition.role_data[4]) >> 8};
    result.speed = static_cast<std::uint8_t>((definition.role_data[4] + scaled) >> 1);
  } else if(course.distance < 0x8000) {
    auto const difference{std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(actor.pose.angles[0] - course.heading))};
    auto const error{static_cast<std::uint8_t>((difference >> 8) ^ (difference < 0 ? -1 : 0))};
    auto const distance{static_cast<std::uint8_t>((course.distance * 2) >> 8)};
    if(error < (actor.behaviour[0] >> 2) + 32) {
      if(error < 15 - (awareness >> 5)) {
        result.speed = definition.base_speed;
        if(distance <= (actor.behaviour[0] & 7) + 6) {
          result.firing_distance = distance;
          result.speed = definition.role_data[5];
        }
      }
    } else {
      auto const threshold{static_cast<std::uint8_t>(3 - std::min(255, actor.behaviour[5] + (awareness >> 5)))};
      if(distance < threshold) {
        auto const height{std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(definition.role_data[2] * 256 - actor.pose.position[2]))};
        result.pitch = static_cast<std::uint16_t>(height >> 1);
        course.heading = actor.pose.angles[0];
        result.speed = definition.base_speed;
      }
    }
  }
  auto const difference{std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(course.heading - actor.pose.angles[0]))};
  // The preceding native IMUL result is discarded: 7BF4 clamps pitch and leaves DI as the raw heading error.
  result.turn_drive = static_cast<std::uint16_t>(difference);
  auto const pitch{std::bit_cast<std::int16_t>(result.pitch)};
  result.pitch = static_cast<std::uint16_t>(std::clamp<int>(pitch, -0x1800, 0x1800));
  return result;
}

} // namespace darker::game

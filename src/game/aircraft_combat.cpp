#include "game/aircraft_combat.h"
#include <bit>
#include "game/angular_motion.h"
#include "game/collision_sweep.h"
#include "game/object_impact.h"
#include "game/random.h"
#include "maths/sine_table.h"

namespace darker::game {

bool sweep_aircraft(object_pose const &target, uint16_t const extent, uint16_t const expansion,
  std::array<uint16_t, 3> const &start, std::array<uint16_t, 3> &end) noexcept {
  /// 6D7B/60C8 use a model-extent cube with altitude divided by eight, independently of model orientation
  auto const radius{static_cast<uint16_t>((extent >> 2) + expansion)};
  auto centre{target.position};
  centre[2] = static_cast<uint16_t>(std::bit_cast<int16_t>(centre[2]) >> 3);
  collision_box box;
  for(size_t axis{0}; axis < 3; ++axis) {
    box.minimum[axis] = static_cast<uint16_t>(centre[axis] - radius);
    box.maximum[axis] = static_cast<uint16_t>(centre[axis] + radius);
  }
  auto previous{start};
  auto next{end};
  previous[2] = static_cast<uint16_t>(std::bit_cast<int16_t>(previous[2]) >> 3);
  next[2] = static_cast<uint16_t>(std::bit_cast<int16_t>(next[2]) >> 3);
  if(!sweep_collision_box(box, previous, next)) return false;
  end = next;
  end[2] = static_cast<uint16_t>(next[2] << 3);
  return true;
}

impact_effect hit_aircraft(scenario_actor &actor, uint8_t const strength, uint16_t const clock, uint16_t &random_state) {
  /// CE38 shares awareness with impact accumulation and field 64 with damage/recovery accounting
  object_impact_state state{
    .rotation{actor.attitude.pitch_rate, actor.attitude.bank_rate},
    .impact_accumulator{actor.awareness.level}, .damage{actor.awareness.cooldown},
    .update_entry{actor.parameters.update_entry}, .deadline{actor.expiry}, .flags{actor.flags},
  };
  auto const result{apply_object_impact(state, strength, actor.parameters.definition->impact_strength, false, clock, random_state)};
  actor.attitude = {state.rotation.pitch, state.rotation.turn};
  actor.awareness = {state.impact_accumulator, state.damage};
  actor.parameters.update_entry = state.update_entry;
  actor.expiry = state.deadline;
  actor.flags = state.flags;
  return result;
}

void advance_falling_aircraft(scenario_actor &actor, uint16_t frame_step) noexcept {
  /// 8DAA damps bank motion, pitches down according to bank and approaches the falling speed
  actor.previous_position = actor.pose.position;
  auto const bank{integrate_angular_rate(actor.attitude.bank_rate, 0, frame_step)};
  actor.attitude.bank_rate = bank.rate;
  actor.pose.angles[2] = static_cast<uint16_t>(actor.pose.angles[2] + bank.angle_delta);
  frame_step = bank.frame_step;
  auto const middle{static_cast<uint16_t>(actor.pose.angles[2] - (std::bit_cast<int16_t>(bank.angle_delta) >> 1))};
  auto const signed_bank{std::bit_cast<int16_t>(fold_bank_angle(middle))};
  auto const half{static_cast<uint16_t>(signed_bank ^ (signed_bank < 0 ? -1 : 0)) >> 1};
  auto const target{static_cast<uint16_t>(0xe800 - half - (half >> 2))};
  auto const pitch{calculate_angular_response(static_cast<uint16_t>(target - actor.pose.angles[1]), actor.attitude.pitch_rate, 32, frame_step)};
  actor.attitude.pitch_rate = pitch.rate;
  actor.pose.angles[1] = static_cast<uint16_t>(actor.pose.angles[1] + pitch.angle_delta);
  advance_actor_speed(actor.pose, 17, 64, 223, pitch.frame_step);
}

std::optional<gun_trace> fire_skimma_gun(scenario_actor const &actor, object_pose const &player, uint8_t const player_flags,
  uint16_t const player_extent, actor_course const course, uint8_t const distance, uint16_t const clock, uint16_t const changes, uint16_t &random_state) {
  /// 8B65's slot-19 close-range gun tests the original DX aim bounds and timer bits, then traces a randomised ray
  // 8C28 doubles DH before 8B7C compares it with 16; behaviour byte 50 only controls the later projectile branch.
  if(actor.definition_slot != 19 || distance >= 8) return std::nullopt;
  if(actor.selected_target != 0xd986 || (player_flags & 0x30)) return std::nullopt;
  auto const speed{actor.parameters.definition->base_speed};
  auto const pitch_error{static_cast<uint8_t>((static_cast<uint16_t>(course.pitch - actor.pose.angles[1]) >> 8) + speed)};
  if(pitch_error >= static_cast<uint8_t>(speed * 2)) return std::nullopt;
  auto const heading_error{static_cast<uint8_t>((static_cast<uint16_t>(course.heading - actor.pose.angles[0]) >> 8) + distance)};
  if(heading_error >= static_cast<uint8_t>(distance * 2)) return std::nullopt;
  auto const elapsed{static_cast<uint16_t>(clock - actor.last_shot)};
  if((elapsed & 0x100) || !(changes & 0x80)) return std::nullopt;
  auto const sine{[](uint16_t const angle){ return maths::original_sine[angle >> 6]; }};
  auto const cosine{[&](uint16_t const angle){ return sine(static_cast<uint16_t>(angle + 16384)); }};
  auto const heading{actor.pose.angles[0]};
  auto const pitch{actor.pose.angles[1]};
  auto x{-((sine(heading) * cosine(pitch)) >> 16)};
  auto y{(cosine(heading) * cosine(pitch)) >> 16};
  auto z{-(sine(pitch) >> 1)};
  auto const random{next_random(random_state)};
  x = (x >> 2) + std::bit_cast<int8_t>(static_cast<uint8_t>(random));
  y = (y >> 2) + std::bit_cast<int8_t>(static_cast<uint8_t>(random >> 8));
  auto const rotated{static_cast<uint16_t>((random & 0xff00) | std::rotr(static_cast<uint8_t>(random), 3))};
  z = (z >> 2) + (std::bit_cast<int8_t>(static_cast<uint8_t>(rotated >> 3)) >> 3);
  gun_trace result{.start{actor.pose.position}, .end{
    static_cast<uint16_t>(actor.pose.position[0] + (x >> 2)),
    static_cast<uint16_t>(actor.pose.position[1] - (y >> 2)),
    static_cast<uint16_t>(((std::bit_cast<int16_t>(actor.pose.position[2]) >> 1) - z) * 2),
  }};
  result.hit = sweep_aircraft(player, player_extent, 10, result.start, result.end);
  return result;
}

} // namespace darker::game

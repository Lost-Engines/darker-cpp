#include "game/skimma_weapons.h"
#include <algorithm>
#include <array>
#include <bit>
#include <stdexcept>
#include "game/object_definitions.h"
#include "game/random.h"
#include "maths/sine_table.h"
#include "maths/world_coordinates.h"

namespace darker::game {
namespace {

void validate_weapon(uint8_t const weapon) {
  /// The caller selects a craft-available slot; the native tables contain three slots
  if(weapon >= skimma_weapon_specifications.size()) throw std::invalid_argument{"Skimma weapon index must be below three"};
}

} // anonymous namespace

skimma_recoil_frame calculate_skimma_recoil(int8_t const previous, game_duration const frame_step) {
  /// C950–C96A retain the packed pre-clamp word for the shot offset, then clamp sign crossings
  auto const step{static_cast<uint8_t>(frame_step)};
  auto const low{static_cast<uint8_t>(previous + (previous < 0 ? step : -static_cast<int>(step)))};
  auto const packed{static_cast<uint16_t>((static_cast<uint16_t>(static_cast<uint8_t>(previous)) << 8) | low)};
  auto const next{static_cast<int8_t>(((static_cast<uint8_t>(previous) ^ low) & 0x80) != 0 ? 0 : std::bit_cast<int8_t>(low))};
  return {
    .next{next},
    .aim_offset{static_cast<int16_t>(next >> 2)},
    .shot_offset{static_cast<int16_t>(-(std::bit_cast<int16_t>(packed) >> 4))},
  };
}

int8_t kick_skimma_recoil(int8_t const current, uint8_t const random_byte) {
  /// C984–C98E apply the random 64–95 impulse through C9FE, preserving byte wrapping
  int const impulse{64 + (random_byte & 31)};
  return std::bit_cast<int8_t>(static_cast<uint8_t>(current + (current < 0 ? impulse : -impulse)));
}

maths::world_position skimma_gun_endpoint(object_pose const &player, int16_t const pitch_offset, uint16_t &random_state) noexcept {
  /// CD84 quarters the sight vector and adds three signed random components before 6D08 constructs the ray
  auto const heading{player.angles.heading >> 6};
  auto const pitch{static_cast<uint16_t>(player.angles.pitch + pitch_offset) >> 6};
  auto const cosine{maths::original_sine[(pitch + 256) % 1024]};
  auto const random{next_random(random_state)};
  auto const horizontal{-(maths::original_sine[heading] * cosine >> 16)};
  auto const forward{maths::original_sine[(heading + 256) % 1024] * cosine >> 16};
  auto const vertical{-(maths::original_sine[pitch] >> 1)};
  auto const x{std::bit_cast<int16_t>(static_cast<uint16_t>((horizontal >> 2) + std::bit_cast<int8_t>(static_cast<uint8_t>(random))))};
  auto const y{std::bit_cast<int16_t>(static_cast<uint16_t>((forward >> 2) + std::bit_cast<int8_t>(static_cast<uint8_t>(random >> 8))))};
  auto const mixed{static_cast<uint16_t>((random & 0xff00) | std::rotr(static_cast<uint8_t>(random), 3))};
  auto const z{std::bit_cast<int16_t>(static_cast<uint16_t>((vertical >> 2) + (std::bit_cast<int8_t>(static_cast<uint8_t>(mixed >> 3)) >> 3)))};
  return {static_cast<uint16_t>(player.position.column + (x >> 2)), static_cast<uint16_t>(player.position.row - (y >> 2)),
    static_cast<uint16_t>((player.position.height & 0xfffe) - z * 2)};
}

projectile *fire_skimma_weapon(projectile_pool &pool, skimma_weapon_slot &slot, skimma_fire_request const request) {
  /// C991 admits exact status 3, reuses the target-class firing handlers and deducts one working round on launch
  validate_weapon(request.weapon);
  if(!request.pressed || (request.player_flags & 0x20) || slot.flags != 3) return nullptr;
  bool const valid_target{request.weapon == 1 ? !(request.target & 0x8000) : (static_cast<uint16_t>(request.target + 1) & 0x8000) != 0};
  if(!valid_target) return nullptr;
  auto const &definition{original_object_definitions[skimma_weapon_specifications[request.weapon].definition]};
  auto *shot{pool.launch({
    .definition{definition},
    .emitter{request.emitter},
    .model_token{request.model},
    .clock{request.clock},
    .lifetime{static_cast<uint16_t>(definition.role_data.projectile().lifetime * 256)},
    .target_token{request.target}
  })};
  if(shot) --slot.ammunition.working;
  return shot;
}

bool skimma_armament::select(bool const upgraded, uint8_t const requested, uint16_t const available, clock_tick const clock) {
  /// C8A7 toggles a repeated selection; changing slots disables the others and starts the shared ring delay
  auto const weapons{std::span{slots}.first(upgraded ? upgraded_slot_count : ordinary_slot_count)};
  if(requested == 0 || requested > weapons.size() || !(available & (1u << (requested - 1)))) return false;
  auto const index{static_cast<uint8_t>(requested - 1)};
  if(index == selection) weapons[index].flags = static_cast<uint8_t>(~weapons[index].flags & 1);
  else {
    selection = index;
    ring.spread = untracked_spread;
    ring.reload_deadline = static_cast<uint16_t>(clock + reload_delay);
    for(auto &weapon : weapons) weapon.flags &= 0xfe;
    weapons[index].flags = 1;
  }
  return true;
}

void refill_skimma_weapon(weapon_ammunition &ammunition, uint8_t const weapon) {
  /// 5E44–5E58 refill both counters without changing the shared reload deadline
  validate_weapon(weapon);
  ammunition = {
    .working{skimma_weapon_specifications[weapon].working_capacity},
    .reserve{skimma_weapon_specifications[weapon].reserve_capacity}
  };
}

bool skimma_armament::reload(uint8_t const weapon, clock_tick const clock) {
  /// 5E1F consumes one reserve only when empty, then starts the wrapping 1024-tick deadline
  validate_weapon(weapon);
  auto &ammunition{slots[weapon].ammunition};
  if(ammunition.working != 0) return false;
  auto const next{static_cast<uint8_t>(ammunition.reserve - 1)};
  if((next & 0x80) != 0) return false;
  ammunition = {
    .working{skimma_weapon_specifications[weapon].working_capacity},
    .reserve{next}
  };
  ring.reload_deadline = static_cast<uint16_t>(clock + reload_delay);
  ring.spread = untracked_spread;
  return true;
}

std::optional<weapon_ring_display> calculate_weapon_ring(weapon_ammunition const ammunition,
  weapon_ring_state const ring, clock_tick const clock, uint8_t const enable_flags) {
  /// 5D66–5DF6 preserve signed deadline comparison and truncation before radius extraction
  if((enable_flags & 1) == 0) return std::nullopt;
  auto const delta{std::bit_cast<int16_t>(static_cast<uint16_t>(clock - ring.reload_deadline))};
  if(delta < 0) {
    auto const radius{static_cast<uint8_t>(static_cast<uint16_t>(delta * 64) >> 9)};
    if(radius < 15) return std::nullopt;
    return weapon_ring_display{
      .radius{radius},
      .remaining{0}
    };
  }
  auto const radius{static_cast<uint8_t>(static_cast<uint16_t>(ring.spread * 64) >> 8)};
  return weapon_ring_display{
    .radius{std::max<uint8_t>(15, radius)},
    .remaining{ammunition.working}
  };
}

std::optional<weapon_ring_display> update_weapon_ring(weapon_ammunition const ammunition,
  weapon_ring_state &ring, clock_tick const clock, uint8_t const enable_flags, game_duration const frame_step) {
  /// 5DF9 follows drawing only outside reload; even a disabled weapon advances smoothing
  auto const display{calculate_weapon_ring(ammunition, ring, clock, enable_flags)};
  auto const delta{std::bit_cast<int16_t>(static_cast<uint16_t>(clock - ring.reload_deadline))};
  if(delta >= 0) {
    ring.reload_deadline = clock;
    auto const spread{std::bit_cast<int16_t>(ring.spread)};
    auto const target{std::bit_cast<int16_t>(ring.target_spread)};
    // 7CDB approaches the mutable target using wrapping arithmetic and signed comparisons
    auto const next{std::bit_cast<int16_t>(static_cast<uint16_t>(spread < target ? spread + frame_step : spread - frame_step))};
    ring.spread = static_cast<uint16_t>(spread < target ? std::min(next, target) : std::max(next, target));
  }
  return display;
}

void skimma_armament::update_status(clock_tick const clock, int16_t const target, uint16_t const target_count, bool const upgraded) {
  /// C90A reloads the selected slot first, then updates status bits and the reserve display
  auto const weapons{std::span{slots}.first(upgraded ? upgraded_slot_count : ordinary_slot_count)};
  if(selection >= weapons.size()) {
    throw std::invalid_argument{"Skimma status requires two or three slots and an available selection"};
  }
  reload(selection, clock);
  reserves = 0;
  for(unsigned int i{static_cast<unsigned int>(weapons.size())}; i-- > 0;) {
    auto &slot{weapons[i]};
    slot.flags &= 0xfd;
    auto reserve{slot.ammunition.reserve};
    if(i == selection && (slot.flags & 1) != 0) {
      reserves = reserve;
      if(target == -1 || target_count == 0) continue;
      // CBW replaces AH with the sign of AL before testing working ammunition
      reserve = (slot.flags & 0x80) != 0 ? 255 : 0;
    }
    if((reserve | slot.ammunition.working) != 0) slot.flags |= 2;
  }
}

} // namespace darker::game

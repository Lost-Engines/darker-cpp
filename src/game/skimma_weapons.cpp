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

std::array<std::uint8_t, 3> constexpr working_capacity{14, 8, 10};
std::array<std::uint8_t, 3> constexpr reserve_capacity{5, 3, 4};

void validate_weapon(std::uint8_t const weapon) {
  /// The caller selects a craft-available slot; the native tables contain three slots
  if(weapon >= working_capacity.size()) throw std::invalid_argument{"Skimma weapon index must be below three"};
}

} // namespace

skimma_recoil_frame calculate_skimma_recoil(std::int8_t const previous, std::uint16_t const frame_step) {
  /// C950–C96A retain the packed pre-clamp word for the shot offset, then clamp sign crossings
  auto const step{static_cast<std::uint8_t>(frame_step)};
  auto const low{static_cast<std::uint8_t>(previous + (previous < 0 ? step : -static_cast<int>(step)))};
  auto const packed{static_cast<std::uint16_t>((static_cast<std::uint16_t>(static_cast<std::uint8_t>(previous)) << 8) | low)};
  auto const next{static_cast<std::int8_t>(((static_cast<std::uint8_t>(previous) ^ low) & 0x80) != 0 ? 0 : std::bit_cast<std::int8_t>(low))};
  return {
    .next{next},
    .aim_offset{static_cast<std::int16_t>(next >> 2)},
    .shot_offset{static_cast<std::int16_t>(-(std::bit_cast<std::int16_t>(packed) >> 4))},
  };
}

std::int8_t kick_skimma_recoil(std::int8_t const current, std::uint8_t const random_byte) {
  /// C984–C98E apply the random 64–95 impulse through C9FE, preserving byte wrapping
  int const impulse{64 + (random_byte & 31)};
  return std::bit_cast<std::int8_t>(static_cast<std::uint8_t>(current + (current < 0 ? impulse : -impulse)));
}

maths::world_position skimma_gun_endpoint(object_pose const &player, int16_t const pitch_offset, uint16_t &random_state) noexcept {
  /// CD84 quarters the sight vector and adds three signed random components before 6D08 constructs the ray
  auto const heading{player.angles.heading >> 6};
  auto const pitch{static_cast<uint16_t>(player.angles.pitch+pitch_offset) >> 6};
  auto const cosine{maths::original_sine[(pitch+256)%1024]};
  auto const random{next_random(random_state)};
  auto const horizontal{-(maths::original_sine[heading]*cosine >> 16)};
  auto const forward{maths::original_sine[(heading+256)%1024]*cosine >> 16};
  auto const vertical{-(maths::original_sine[pitch] >> 1)};
  auto const x{std::bit_cast<int16_t>(static_cast<uint16_t>((horizontal >> 2)+std::bit_cast<int8_t>(static_cast<uint8_t>(random))))};
  auto const y{std::bit_cast<int16_t>(static_cast<uint16_t>((forward >> 2)+std::bit_cast<int8_t>(static_cast<uint8_t>(random >> 8))))};
  auto const mixed{static_cast<uint16_t>((random & 0xff00) | std::rotr(static_cast<uint8_t>(random),3))};
  auto const z{std::bit_cast<int16_t>(static_cast<uint16_t>((vertical >> 2)+(std::bit_cast<int8_t>(static_cast<uint8_t>(mixed >> 3)) >> 3)))};
  return {static_cast<uint16_t>(player.position.column+(x >> 2)),static_cast<uint16_t>(player.position.row-(y >> 2)),
    static_cast<uint16_t>((player.position.height & 0xfffe)-z*2)};
}

projectile *fire_skimma_weapon(projectile_pool &pool, skimma_weapon_slot &slot, skimma_fire_request const request) {
  /// C991 admits exact status 3, reuses the target-class firing handlers and deducts one working round on launch
  validate_weapon(request.weapon);
  if(!request.pressed || (request.player_flags & 0x20) || slot.flags != 3) return nullptr;
  bool const valid_target{request.weapon == 1 ? !(request.target & 0x8000) : (static_cast<uint16_t>(request.target+1) & 0x8000) != 0};
  if(!valid_target) return nullptr;
  auto const &definition{original_object_definitions[10+request.weapon]};
  auto *shot{pool.launch({
    .definition{definition},
    .emitter{request.emitter},
    .model_token{request.model},
    .clock{request.clock},
    .lifetime{static_cast<uint16_t>(definition.role_data.projectile().lifetime*256)},
    .target_token{request.target}
  })};
  if(shot) --slot.ammunition.working;
  return shot;
}

bool select_skimma_weapon(std::span<skimma_weapon_slot> const weapons, uint8_t &selected, weapon_ring_state &ring,
  uint8_t const selection, uint16_t const available, uint16_t const clock) {
  /// C8A7 toggles a repeated selection; changing slots disables the others and starts the shared ring delay
  if(selection == 0 || selection > weapons.size() || selection > 3 || !(available & (1u << (selection-1)))) return false;
  auto const index{static_cast<uint8_t>(selection-1)};
  if(index == selected) weapons[index].flags = static_cast<uint8_t>(~weapons[index].flags & 1);
  else {
    selected = index;
    ring.spread = 508;
    ring.reload_deadline = static_cast<uint16_t>(clock+1024);
    for(auto &weapon : weapons) weapon.flags &= 0xfe;
    weapons[index].flags = 1;
  }
  return true;
}

void refill_skimma_weapon(weapon_ammunition &ammunition, std::uint8_t const weapon) {
  /// 5E44–5E58 refill both counters without changing the shared reload deadline
  validate_weapon(weapon);
  ammunition = {
    .working{working_capacity[weapon]},
    .reserve{reserve_capacity[weapon]}
  };
}

bool reload_skimma_weapon(weapon_ammunition &ammunition, weapon_ring_state &ring, std::uint8_t const weapon, std::uint16_t const clock) {
  /// 5E1F consumes one reserve only when empty, then starts the wrapping 1024-tick deadline
  validate_weapon(weapon);
  if(ammunition.working != 0) return false;
  auto const next{static_cast<std::uint8_t>(ammunition.reserve - 1)};
  if((next & 0x80) != 0) return false;
  ammunition = {
    .working{working_capacity[weapon]},
    .reserve{next}
  };
  ring.reload_deadline = static_cast<std::uint16_t>(clock + 1024);
  ring.spread = 508;
  return true;
}

std::optional<weapon_ring_display> calculate_weapon_ring(weapon_ammunition const ammunition,
  weapon_ring_state const ring, std::uint16_t const clock, std::uint8_t const enable_flags) {
  /// 5D66–5DF6 preserve signed deadline comparison and truncation before radius extraction
  if((enable_flags & 1) == 0) return std::nullopt;
  auto const delta{std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(clock - ring.reload_deadline))};
  if(delta < 0) {
    auto const radius{static_cast<std::uint8_t>(static_cast<std::uint16_t>(delta * 64) >> 9)};
    if(radius < 15) return std::nullopt;
    return weapon_ring_display{
      .radius{radius},
      .remaining{0}
    };
  }
  auto const radius{static_cast<std::uint8_t>(static_cast<std::uint16_t>(ring.spread * 64) >> 8)};
  return weapon_ring_display{
    .radius{std::max<std::uint8_t>(15, radius)},
    .remaining{ammunition.working}
  };
}

std::optional<weapon_ring_display> update_weapon_ring(weapon_ammunition const ammunition,
  weapon_ring_state &ring, std::uint16_t const clock, std::uint8_t const enable_flags, std::uint16_t const frame_step) {
  /// 5DF9 follows drawing only outside reload; even a disabled weapon advances smoothing
  auto const display{calculate_weapon_ring(ammunition, ring, clock, enable_flags)};
  auto const delta{std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(clock - ring.reload_deadline))};
  if(delta >= 0) {
    ring.reload_deadline = clock;
    auto const spread{std::bit_cast<std::int16_t>(ring.spread)};
    auto const target{std::bit_cast<std::int16_t>(ring.target_spread)};
    // 7CDB approaches the mutable target using wrapping arithmetic and signed comparisons.
    auto const next{std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(spread < target ? spread + frame_step : spread - frame_step))};
    ring.spread = static_cast<std::uint16_t>(spread < target ? std::min(next, target) : std::max(next, target));
  }
  return display;
}

std::uint8_t update_skimma_weapon_status(std::span<skimma_weapon_slot> const weapons, weapon_ring_state &ring,
  std::uint8_t const selected, std::uint16_t const clock, std::int16_t const target, std::uint16_t const target_count) {
  /// C90A reloads the selected slot first, then updates status bits and the reserve display
  if((weapons.size() != 2 && weapons.size() != 3) || selected >= weapons.size()) {
    throw std::invalid_argument{"Skimma status requires two or three slots and an available selection"};
  }
  reload_skimma_weapon(weapons[selected].ammunition, ring, selected, clock);
  std::uint8_t reserve_display{0};
  for(std::size_t i{weapons.size()}; i-- > 0;) {
    auto &slot{weapons[i]};
    slot.flags &= 0xfd;
    auto reserve{slot.ammunition.reserve};
    if(i == selected && (slot.flags & 1) != 0) {
      reserve_display = reserve;
      if(target == -1 || target_count == 0) continue;
      // CBW replaces AH with the sign of AL before testing working ammunition.
      reserve = (slot.flags & 0x80) != 0 ? 255 : 0;
    }
    if((reserve | slot.ammunition.working) != 0) slot.flags |= 2;
  }
  return reserve_display;
}

} // namespace darker::game

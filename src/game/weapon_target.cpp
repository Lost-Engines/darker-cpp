#include "game/weapon_target.h"
#include <bit>
#include "game/aircraft_combat.h"
#include "game/city_sweep.h"
#include "game/object_definitions.h"
#include "maths/sine_table.h"

namespace darker::game {

void weapon_target::clear() noexcept {
  /// CFCB releases the selected token and restores the untracked ring spread
  token = 0xffff;
  spread = 508;
}

std::array<uint16_t, 3> target_ray_end(object_pose const &player) noexcept {
  /// 1E77/6D08 construct the unrolled forward targeting ray with the original truncation points
  auto const heading{player.angles[0] >> 6}, pitch{player.angles[1] >> 6};
  auto const sine{maths::original_sine[heading]}, cosine{maths::original_sine[(heading + 256) % 1024]};
  auto const pitch_cosine{maths::original_sine[(pitch + 256) % 1024]};
  auto const x{(-(sine * pitch_cosine >> 16)) >> 2};
  auto const y{-((cosine * pitch_cosine >> 16) >> 2)};
  return {static_cast<uint16_t>(player.position[0] + x),static_cast<uint16_t>(player.position[1] + y),
    static_cast<uint16_t>((player.position[2] & 0xfffe) + (maths::original_sine[pitch] >> 1)*2)};
}

uint16_t acquire_caero_target(object_pose const &player, std::span<scenario_actor const> const actors,
  city_map const &cells, resources::geometry_bank const &bank, uint8_t const damage_mask) {
  /// 6D08 clips against the city before checking the airborne list in native order
  auto end{target_ray_end(player)};
  auto const hit{sweep_city(bank,cells,damage_mask,player.position,end,0,10)};
  uint16_t selected{hit.contact == city_contact::building ? static_cast<uint16_t>(hit.row*256+hit.column) : uint16_t{0xffff}};
  for(auto const &actor : actors) {
    if(actor.category != actor_category::air) continue;
    if(sweep_aircraft(actor.pose,bank.header_at(actor.parameters.model_token).extent,0,player.position,end))
      selected = static_cast<uint16_t>(0xd986 + actor.index*112);
  }
  return selected;
}

void project_caero_target(weapon_target &lock, std::array<uint16_t, 3> const &player,
  std::array<uint16_t, 3> const &target, uint16_t const extent, maths::view_basis const &basis,
  uint8_t const secondary_weapon, uint8_t const cell_type, uint8_t const cell_state) noexcept {
  /// CF94–D056 retain a resolved Caero lock only inside its original view cone and target class
  if(!secondary_weapon) { lock.clear(); return; }
  if(lock.token == 0xffff) return;
  auto const dx{static_cast<uint16_t>(target[0] - player[0])};
  auto const dy{static_cast<uint16_t>(target[1] - player[1])};
  if(static_cast<uint8_t>((dx >> 8) + 16) >= 32 || static_cast<uint8_t>((dy >> 8) + 16) >= 32) { lock.clear(); return; }
  auto const height{static_cast<uint16_t>(player[2] - target[2] + (extent >> 2))};
  auto const x{std::bit_cast<int16_t>(static_cast<uint16_t>(dx * 8))};
  auto const y{std::bit_cast<int16_t>(static_cast<uint16_t>(dy * 8))};
  auto const z{std::bit_cast<int16_t>(height)};
  auto const transform{[&](int16_t maths::view_axis::*const member){
    auto const product{static_cast<uint32_t>(basis[1].*member * x)
      - static_cast<uint32_t>(basis[0].*member * y) + static_cast<uint32_t>(basis[2].*member * z)};
    return std::bit_cast<int16_t>(static_cast<uint16_t>(product >> 16));
  }};
  auto const depth{transform(&maths::view_axis::depth)};
  if(depth < 64) { lock.clear(); return; }
  lock.horizontal = static_cast<int16_t>(transform(&maths::view_axis::horizontal) * 256 / depth);
  auto const horizontal{lock.horizontal < 0 ? ~lock.horizontal : lock.horizontal};
  if(horizontal >= 55) { lock.clear(); return; }
  lock.vertical = static_cast<int16_t>(transform(&maths::view_axis::vertical) * 256 / depth);
  auto const vertical{lock.vertical < 0 ? ~lock.vertical : lock.vertical};
  if(vertical >= 55) { lock.clear(); return; }
  auto const radial{horizontal*horizontal + vertical*vertical};
  if(radial >= 0x0b64) { lock.clear(); return; }
  lock.distance = static_cast<uint8_t>(radial >> 8);
  bool const ground{(original_object_definitions[secondary_weapon - 1].role_data[7] & 2) != 0};
  if(ground ? (lock.token & 0x8000) || cell_type == 1 || !(cell_state & 0x40) : !(lock.token & 0x8000)) { lock.clear(); return; }
  lock.spread = height;
}

} // namespace darker::game

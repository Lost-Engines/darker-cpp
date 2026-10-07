#include "audio/world_sounds.h"
#include <algorithm>
#include <bit>
#include <stdexcept>
#include <vector>
#include "game/object_definitions.h"
#include "maths/direction.h"
#include "maths/sine_table.h"

namespace darker::audio {

std::optional<uint16_t> audible_level(std::array<uint16_t, 3> const source, std::array<uint16_t, 3> const listener,
  uint16_t const level, uint8_t const flags) noexcept {
  /// 3488 rejects distant sources before subtracting the original squared-distance attenuation
  if(flags & 2) return level;
  std::array<int32_t, 3> delta{};
  for(size_t axis{0}; axis < 3; ++axis) delta[axis] = std::bit_cast<int16_t>(static_cast<uint16_t>(source[axis] - listener[axis]));
  auto const x{static_cast<uint32_t>(delta[0] * delta[0])};
  auto const y{static_cast<uint32_t>(delta[1] * delta[1])};
  if(x >= 0x1000000 || y >= 0x1000000) return std::nullopt;
  auto const z{static_cast<uint64_t>(delta[2] * delta[2]) * 4};
  if(z >= 0x100000000ULL) return std::nullopt;
  auto const distance{(x >> 8) + (y >> 8) + (z >> 16)};
  if(distance >= 0x4000 || distance * 4 >= level) return std::nullopt;
  return static_cast<uint16_t>(level - distance * 4);
}

uint16_t doppler_factor(game::object_pose const *const motion, uint16_t const heading, uint16_t const pitch) noexcept {
  /// 3ACD projects wrapping speed onto the source bearing using two cosine table products
  if(!motion) return 0x39d0;
  auto const cosine{[](uint16_t const angle){ return maths::original_sine[((angle >> 6) + 256) % 1024]; }};
  auto const product{(cosine(static_cast<uint16_t>(motion->angles[1] - pitch)) * cosine(static_cast<uint16_t>(motion->angles[0] - heading))) >> 16};
  auto const speed{std::bit_cast<int16_t>(static_cast<uint16_t>(motion->speed * 4))};
  return static_cast<uint16_t>(0x39d0 - ((speed * product) >> 16));
}

uint16_t spatial_pitch(uint16_t const pitch, std::array<uint16_t, 3> const source, game::object_pose const &listener,
  game::object_pose const *const source_motion, game::object_pose const *const listener_motion) noexcept {
  /// 39BA scales both horizontal differences by eight before 925C, then divides listener/source velocity factors
  auto const x{static_cast<uint16_t>((source[0] - listener.position[0]) * 8)};
  auto const y{static_cast<uint16_t>((source[1] - listener.position[1]) * 8)};
  auto const z{static_cast<uint16_t>(source[2] - listener.position[2])};
  auto const magnitude{[](uint16_t const value){ return value & 0x8000 ? static_cast<uint16_t>(-value) : value; }};
  auto const heading{static_cast<uint16_t>(maths::direction_index(x, y) << 5)};
  auto const elevation{static_cast<uint16_t>(maths::direction_index(z, std::max(magnitude(x), magnitude(y))) << 5)};
  return static_cast<uint16_t>((static_cast<uint32_t>(doppler_factor(listener_motion ? listener_motion : &listener, heading, elevation)) * pitch)
    / doppler_factor(source_motion, heading, elevation));
}

std::array<uint8_t,2> stereo_attenuation(std::array<uint16_t,3> const delta, maths::view_basis const &basis, uint16_t const level) noexcept {
  /// 3A2A transforms source bearing, shapes two sine-table gains and converts them to OPL carrier attenuation
  auto const x{std::bit_cast<int16_t>(static_cast<uint16_t>(delta[0]*8))};
  auto const y{std::bit_cast<int16_t>(static_cast<uint16_t>(delta[1]*8))};
  auto const z{std::bit_cast<int16_t>(delta[2])};
  auto const depth{static_cast<uint16_t>(((x*basis[1].depth) >> 16)+((z*basis[2].depth) >> 16)-((y*basis[0].depth) >> 16))};
  auto const side{static_cast<uint16_t>(((y*basis[0].horizontal) >> 16)-((x*basis[1].horizontal) >> 16)-((z*basis[2].horizontal) >> 16))};
  auto const phase{maths::direction_index(depth,side) >> 2};
  auto const gain{[](int16_t const sine){
    auto const harmonic{maths::original_sine[static_cast<uint16_t>(sine) >> 6] >> 2};
    auto const sum{static_cast<uint16_t>(sine+harmonic+(harmonic >> 2))};
    return static_cast<uint16_t>((sum*2) ^ (sum & 0x8000 ? 65535 : 0));
  }};
  auto const amplitude{std::min(65535u,unsigned{level}+(level >> 4)) >> 4};
  auto const left{gain(maths::original_sine[256+((-phase)&511)])};
  auto const right{gain(maths::original_sine[phase])};
  return {static_cast<uint8_t>(63^((left*amplitude) >> 22)),static_cast<uint8_t>(63^((right*amplitude) >> 22))};
}

fm_note object_sound(game::object_definition const &definition, game::object_pose const &pose,
  object_sound_state const state, uint16_t const clock) {
  /// 3556 supplies each non-player definition's callback with its original pitch, level and object state
  fm_note result{.pitch{definition.sound_pitch},.level{static_cast<uint16_t>(definition.sound_level*256+255)},.patch{definition.fm_patch}};
  switch(definition.sound_entry) {
  case 0x391d: {
    auto const phase{static_cast<uint8_t>(clock+state.identity)};
    auto const triangle{static_cast<uint8_t>(phase ^ (phase & 128 ? 255 : 0))};
    auto const amplitude{std::min(255u,64u+(state.damage >> 8))};
    result.pitch = static_cast<uint16_t>(result.pitch+(pose.speed >> 2)+((triangle*amplitude) >> 10));
    [[fallthrough]];
  }
  case 0x393d: result.active = !(state.flags & 0x28); break;
  case 0x3942:
    result.pitch = static_cast<uint16_t>(result.pitch-(std::bit_cast<int16_t>(pose.angles[1]) >> 4));
    if(pose.speed == 0) result.pitch >>= 1;
    result.active = !(state.flags & 8);
    break;
  case 0x3957: {
    auto const remaining{static_cast<uint16_t>(state.deadline-clock)};
    if(std::bit_cast<int8_t>(static_cast<uint8_t>(remaining >> 8)) > 0) {
      result.pitch = static_cast<uint16_t>(result.pitch+(remaining >> 4));
      result.active = !(state.flags & 8);
      break;
    }
    [[fallthrough]];
  }
  case 0x3969:
    if(state.flags & 0x20) {
      result.level = static_cast<uint16_t>((uint32_t{result.level}*(state.fade*257u)) >> 16);
      result.active = result.level != 0;
    } else result.active = !(state.flags & 8);
    break;
  default: throw std::invalid_argument{"Object sound requires a non-player sound callback"};
  }
  return result;
}

fm_frame world_sounds::mix(fm_frame const &player, game::mission_combat &combat, game::object_pose const &listener, uint16_t const clock, std::span<game::effect_sound const> const ambient, game::object_pose const *const listener_motion, game::object_pose const *const player_source) {
  /// Visit the original source groups before native candidate admission and physical channel allocation
  std::vector<sound_candidate> candidates;
  auto const basis{maths::make_view_basis({listener.angles[0],listener.angles[1],listener.angles[2]})};
  auto const append{[&](game::effect_sound const &sound, game::object_pose const *const motion, uint64_t const identity){
    auto const &definition{sound.definition};
    auto const level{audible_level(sound.position, listener.position, definition.level, definition.flags)};
    if(!level) return;
    auto const pitch{definition.flags & 4 ? definition.pitch : spatial_pitch(definition.pitch, sound.position, listener, motion, listener_motion)};
    candidates.push_back({identity, {.pitch{pitch}, .level{*level},
      .generation{static_cast<uint16_t>((identity >> 32) == 4 ? sound.identity : 0)},.patch{definition.patch}, .active{true}}});
    if(definition.flags & 1) candidates.back().note.attenuation = stereo_attenuation({
      static_cast<uint16_t>(sound.position[0]-listener.position[0]),static_cast<uint16_t>(sound.position[1]-listener.position[1]),
      static_cast<uint16_t>(sound.position[2]-listener.position[2])},basis,*level);
  }};
  auto const append_actors{[&](game::actor_category const category){
    for(auto const &actor : combat.actors) {
      if(actor.category != category) continue;
      if(!actor.parameters.definition) continue;
      auto const note{object_sound(*actor.parameters.definition,actor.pose,
        {.identity{static_cast<uint16_t>(0xd986+actor.index*112)},.flags{actor.flags},.damage{actor.awareness.cooldown},
          .fade{actor.fade},.deadline{actor.expiry}},clock)};
      if(!note.active) continue;
      game::effect_sound const sound{.position{actor.pose.position},.definition{
        .duration{0},.pitch{note.pitch},.level{note.level},.patch{note.patch},.flags{0x29}}};
      append(sound,&actor.pose,0x300000000ULL+actor.index);
    }
  }};
  for(auto const *pool : {&combat.projectiles,&combat.hostile_projectiles}) {
    for(auto *shot{pool->objects().head}; shot; shot = shot->next) {
      auto const &definition{*shot->parameters.definition};
      auto note{object_sound(definition,shot->placement,
        {.identity{shot->native_id},.flags{shot->flags},.fade{shot->fade},.deadline{shot->deadline}},clock)};
      if(!note.active) continue;
      if(&definition == &game::original_object_definitions[6]) note.pitch = combat.dual_launch_pitch;
      game::effect_sound const sound{.position{shot->placement.position},.definition{
        .duration{0},.pitch{note.pitch},.level{note.level},.patch{note.patch},.flags{0x29}}};
      // Object voices retain their native pool identity across updates.
      auto const identity{0x200000000ULL+shot->native_id};
      append(sound,&shot->placement,identity);
    }
  }
  append_actors(game::actor_category::ground);
  auto const append_player{[&](size_t const i){
    if(player[i].active) candidates.push_back({0x100000000ULL+i*65536,player[i]});
  }};
  if(player_source && player[0].active) {
    game::effect_sound const engine{.position{player_source->position},.definition{.duration{0},.pitch{player[0].pitch},
      .level{player[0].level},.patch{player[0].patch},.flags{0x29}}};
    append(engine,player_source,0x100000000ULL);
  } else append_player(0);
  append_actors(game::actor_category::air);
  // Fixed records are visited in address order: boost, beacons, charge, messages, ambient, switches, shield.
  if(player_source && player[1].active) {
    game::effect_sound const boost{.position{player_source->position},.definition{.duration{0},.pitch{player[1].pitch},
      .level{player[1].level},.patch{player[1].patch},.flags{1}}};
    auto const before{candidates.size()};
    append(boost,player_source,0x100010000ULL);
    if(candidates.size() != before) candidates.back().note.generation = player[1].generation;
  } else append_player(1);
  for(auto const &sound : ambient) if((sound.identity >> 16) < 2) append(sound,nullptr,0x400000000ULL+(sound.identity & 0xffff0000u));
  append_player(2);
  append_player(5);
  for(auto const &sound : ambient) if((sound.identity >> 16) >= 2) append(sound,nullptr,0x400000000ULL+(sound.identity & 0xffff0000u));
  if(player[3].patch == 4) append_player(3);
  append_player(6);
  if(player[3].patch != 4) append_player(3);
  append_player(4);
  for(auto *pool : {&combat.effects.sounds,&combat.effects.gun_sounds}) {
    // 363C retires a previously submitted transient when its voice was rejected or stolen.
    std::erase_if(*pool,[&](auto const &sound){
      return (sound.definition.flags & 0x20) && std::ranges::find(voices.identities(),sound.identity) == voices.identities().end();
    });
    for(auto i{pool->rbegin()}; i != pool->rend(); ++i) {
      append(*i,nullptr,i->identity);
      i->definition.flags |= 0x28;
    }
  }
  return voices.allocate(candidates);
}

uint16_t world_sounds::audible_player() const noexcept {
  /// 3599 requires an already-started fixed player record to retain its previous physical voice
  uint16_t result{0};
  for(auto const owner : voices.identities()) if((owner >> 32) == 1) result |= static_cast<uint16_t>(1u << ((owner >> 16) & 65535));
  return result;
}

uint16_t world_sounds::audible_ambient() const noexcept {
  /// Return fixed-record ownership for the following frame's native continuation checks
  uint16_t result{0};
  for(auto const owner : voices.identities()) if((owner >> 32) == 4) result |= static_cast<uint16_t>(1u << ((owner >> 16) & 65535));
  return result;
}

} // namespace darker::audio

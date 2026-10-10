#include "audio/flight_sounds.h"
#include <bit>
#include "game/object_definitions.h"

namespace darker::audio {

std::optional<uint16_t> chargeable_sound_pitch(uint16_t const charge, uint16_t const clock) noexcept {
  /// 370F raises the charge tone with stored energy and a triangular timer modulation
  if(!charge) return std::nullopt;
  auto const base{static_cast<uint16_t>(charge >> 5)};
  auto const phase{static_cast<uint8_t>(clock + base)};
  auto const triangle{static_cast<uint8_t>(phase ^ (phase & 128 ? 255 : 0))};
  return static_cast<uint16_t>(0x362 + ((base + triangle) >> 3));
}

void flight_sounds::trigger(flight_sound const effect, uint16_t const clock) noexcept {
  /// Supply original player-effect records in logical slots for the world voice allocator
  uint8_t channel{0};
  uint8_t patch{0};
  uint16_t pitch{0}, level{0}, duration{0};
  switch(effect) {
    case flight_sound::boost:
      channel = 1;
      patch = 2;
      pitch = 686;
      level = 0xb000;
      duration = 1056;
      break;
    case flight_sound::charged:
      channel = 2;
      patch = 32;
      pitch = 1372;
      level = 0xee00;
      duration = 640;
      break;
    case flight_sound::caero_switch:
      channel = 3;
      patch = 4;
      pitch = 6928;
      level = 0xce00;
      duration = 112;
      break;
    case flight_sound::skimma_switch:
      channel = 3;
      patch = 37;
      pitch = 3464;
      level = 0xe000;
      duration = 112;
      break;
    case flight_sound::message:
      channel = 6;
      patch = 31;
      pitch = 13056;
      level = 0xc000;
      duration = 160;
      break;
    case flight_sound::skimma_message:
      channel = 6;
      patch = 38;
      pitch = 3464;
      level = 0xd800;
      duration = 112;
      break;
    case flight_sound::shield_start:
      channel = 4;
      patch = 36;
      pitch = 0x302;
      level = 0xcc00;
      duration = 4080;
      shield_ready = false;
      break;
    case flight_sound::shield_ready:
      channel = 4;
      patch = 36;
      pitch = 0x362;
      level = 0xcc00;
      duration = 288;
      shield_ready = true;
      break;
  }
  voices[channel] = {
    .pitch{pitch},
    .level{level},
    .generation{static_cast<uint16_t>(voices[channel].generation + 1)},
    .patch{patch},
    .active{true}
  };
  submitted[channel] = false;
  deadlines[channel] = static_cast<uint16_t>(clock + duration);
}

fm_frame flight_sounds::advance(game::player_flight const &player, uint16_t const clock, bool const ready, bool const cockpit_hidden, uint16_t const weapon_charge, uint16_t const playing_mask) noexcept {
  /// Reproduce player engine callbacks 3980/3914 and timed record deadlines; world attenuation and Doppler remain separate
  for(size_t i{1}; i < voices.size(); ++i) {
    if(i == 5) continue;
    if(voices[i].active && (std::bit_cast<int16_t>(static_cast<uint16_t>(clock - deadlines[i])) >= 0
      || (submitted[i] && !(playing_mask & (1u << i))))) voices[i].active = false;
  }
  bool const caero{std::holds_alternative<game::caero_flight_state>(player.craft)};
  auto const &definition{game::original_object_definitions[player.definition_slot()]};
  auto pitch{definition.sound_pitch};
  if(!caero) {
    auto const phase{static_cast<uint8_t>(clock + 0x86)};
    auto const triangle{static_cast<uint8_t>(phase ^ (phase & 128 ? 255 : 0))};
    pitch = static_cast<uint16_t>(pitch + (player.pose().speed >> 2) + ((triangle * 64) >> 10));
  }
  voices[0] = {
    .pitch{pitch},
    .level{cockpit_hidden ? uint16_t{0x8800} : static_cast<uint16_t>(definition.sound_level * 256 + 255)},
    .patch{definition.fm_patch},
    .active{!player.lifecycle.crashing && (!caero || player.engine_flags == 1)}
  };
  if(!caero) {
    if(player.engine_flags != 1) voices[4].active = false;
    else if(ready && !shield_ready) trigger(flight_sound::shield_ready, clock);
  }
  auto const charge_pitch{chargeable_sound_pitch(weapon_charge, clock)};
  if(charge_pitch && !voices[5].active) ++voices[5].generation;
  voices[5].pitch = charge_pitch.value_or(0);
  voices[5].level = 0xd200;
  voices[5].patch = 0;
  voices[5].active = charge_pitch.has_value();
  for(size_t i{0}; i < voices.size(); ++i) submitted[i] = voices[i].active;
  return voices;
}

} // namespace darker::audio

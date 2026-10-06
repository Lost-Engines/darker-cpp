#include "game/hangar.h"
#include <bit>
#include <stdexcept>

namespace darker::game {
namespace {

std::size_t site_index(std::uint16_t const site) {
  /// Return sites use the world's packed byte address, with twice the column in its low byte
  auto const column{(site & 255) >> 1};
  auto const row{site >> 8};
  if((site & 1) || row == 0 || row >= 127) throw std::invalid_argument{"Invalid Caero hangar site"};
  return row * 128 + column;
}

void toggle_hangar(player_flight &player, city_map &cells, std::size_t const centre) {
  /// C6D2 toggles the landed flag and the gate, approach-light and interior alternate states together
  player.lifecycle.flags ^= 0x10;
  for(auto const index : {centre - 128, centre, centre + 128}) cells[index].state ^= 0x80;
}

} // namespace

void initialise_caero_hangar(player_flight &player, city_map &cells, hangar_state &hangar, std::int16_t const model_height) {
  /// BD34–BD98 places the Caero at the type-17 return sites used by the Delphi campaign
  auto const centre{site_index(hangar.return_site)};
  if(cells[centre].type != 17) throw std::invalid_argument{"Caero launch requires a campaign type-17 hangar"};
  player = {};
  player.engine_flags = 0;
  player.pose() = {
    .position{static_cast<std::uint16_t>((centre % 128) * 256 + 128), static_cast<std::uint16_t>((centre / 128) * 256 + 152),
      static_cast<std::uint16_t>(-104 - model_height)},
    .angles{0, 0x0a20, 0},
  };
  hangar.extension = 0;
  hangar.sound_level = 0;
  toggle_hangar(player, cells, centre);
}

void advance_hangar_departure(player_flight &player, city_map &cells, hangar_state &hangar, std::uint16_t const frame_step) {
  /// C5F9 opens the gate inside its cell, retracts it over the approach lights and closes the site after departure
  if(!(player.lifecycle.flags & 0x10) || player.lifecycle.crashing) return;
  auto const centre{site_index(hangar.return_site)};
  auto const &position{player.pose().position};
  auto const column{static_cast<unsigned int>(position[0] >> 8)};
  auto const row{static_cast<unsigned int>(position[1] >> 8)};
  auto const type{column < 128 && row < 128 ? cells[row * 128 + column].type : 255};
  if(type < 17 || type > 24) {
    hangar.extension = 0;
    hangar.sound_level = 0;
    toggle_hangar(player, cells, centre);
    return;
  }
  if(type & 1) {
    auto const delta{static_cast<std::uint16_t>(frame_step << 6)};
    auto const sum{static_cast<unsigned int>(hangar.extension) + delta};
    if(sum >= 0xe800) {
      hangar.extension = 0xe800;
      return;
    }
    hangar.extension = static_cast<std::uint16_t>(sum);
  } else {
    auto const distance{horizontal_distance(position, {static_cast<std::uint16_t>((centre % 128) * 256 + 128),
      static_cast<std::uint16_t>((centre / 128) * 256 + 152), 0})};
    auto value{static_cast<std::uint16_t>(152 - distance)};
    if(distance != 0 && distance <= 152) value = 65535;
    unsigned int const sum{value + 232u};
    hangar.extension = sum > 65535 ? static_cast<std::uint16_t>((sum & 255) * 257) : 0;
  }
  hangar.sound_level = static_cast<std::uint8_t>((hangar.extension & 255) | (hangar.extension >> 8));
}

} // namespace darker::game

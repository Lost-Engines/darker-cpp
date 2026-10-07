#include "game/tunnel_portal.h"
#include <array>
#include <stdexcept>
#include "game/object_definitions.h"

namespace darker::game {

void initialise_tunnel_entry(player_flight &player, uint16_t const site, uint8_t const heading, int16_t const model_height) {
  /// BD65 starts underground above the entry portal, with the original heading-dependent cell fractions and supplied energy
  if((site & 1) || (site >> 8) >= 128 || (heading & 63)) throw std::invalid_argument{"Invalid underground entry site or orientation"};
  constexpr std::array<std::array<uint8_t,2>,4> offsets{{{128,40},{216,128},{128,216},{40,128}}};
  auto const fraction{offsets[heading >> 6]};
  player = {};
  player.tunnel.emplace();
  player.tunnel->lookahead = 60;
  player.tunnel->resistance = 0x0d00;
  player.tunnel->off_route_time = 0x0200;
  player.tunnel->connection.cell = static_cast<uint16_t>((site & 0xff00) | ((site & 255) >> 1));
  player.lifecycle.flags = 0x10;
  player.forward_setting = static_cast<uint16_t>(original_object_definitions[28].role_data[4]*8);
  auto &craft{std::get<caero_flight_state>(player.craft)};
  craft.flying = true;
  craft.energy = {.buffer{0x6000},.reserve{0x1fff},.boost{0x1fff}};
  craft.pose.position = {static_cast<uint16_t>((site & 255)*128 + fraction[0]),
    static_cast<uint16_t>((site & 0xff00) + fraction[1]),static_cast<uint16_t>(1536 - model_height)};
  craft.pose.angles[0] = static_cast<uint16_t>(heading*256);
}

void update_tunnel_portal(player_flight &player, city_map &cells, hangar_state &hangar, uint16_t const frame_step) {
  /// C582/C5CD clear entry protection, select speed/lookahead beyond the portal and capture an aligned return
  if(!player.tunnel) throw std::invalid_argument{"Underground portal update requires tunnel player state"};
  if(player.lifecycle.crashing || hangar.returning != hangar_return_phase::none) return;
  if(player.lifecycle.flags & 0x10) {
    advance_hangar_departure(player,cells,hangar,frame_step);
    return;
  }
  auto const &position{player.pose().position};
  if((position[0] >> 8) != ((hangar.return_site & 255) >> 1)) return;
  auto const difference{static_cast<uint8_t>((position[1] >> 8) - (hangar.return_site >> 8))};
  bool const returning{(player.tunnel->connection.route & 0x80) != 0};
  if(difference == 0 && returning && player.forward_setting != 96) {
    hangar.returning = hangar_return_phase::approaching;
    player.forward_setting = 96;
  } else if(difference == 2) {
    player.forward_setting = returning ? 130 : 384;
    player.tunnel->lookahead = returning ? 64 : 172;
  }
}

} // namespace darker::game

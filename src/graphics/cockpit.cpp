#include "graphics/cockpit.h"
#include <algorithm>
#include <stdexcept>
#include "graphics/cockpit_tables.h"
#include "graphics/flight_instruments.h"

namespace darker::graphics {

unsigned int cockpit_resource_slot(std::uint8_t const configuration) {
  /// BC69 selects 00/15 for the Delphi Skimma; the other configurations use craft-specific sheets
  switch(configuration & 15) {
  case 0: return 15;
  case 1: case 4: return 16;
  case 2: return 17;
  case 3: return 18;
  default: throw std::invalid_argument{"Unknown cockpit configuration"};
  }
}

std::span<hud_component const> cockpit_components(craft const type) {
  /// Select the original Caero or shared Skimma directory
  switch(type) {
  case craft::caero: return components_4615;
  case craft::skimma:
  case craft::upgraded_skimma: return components_4d70;
  }
  throw std::invalid_argument{"unknown cockpit craft"};
}

std::size_t instrument_limit(craft const type, std::size_t const component) {
  /// Ordinary Skimma has sixteen engine-output steps; the upgrade has twenty
  auto const components{cockpit_components(type)};
  if(component >= components.size()) throw std::out_of_range{"cockpit component index"};
  if(type == craft::skimma && components[component].field == 0x454d) return 16;
  return components[component].strips.size();
}

framework::render::indexed_cockpit_framebuffer make_cockpit_cache(framework::render::indexed_framebuffer const &sheet) {
  /// BF72–BF94 copies the top 136 rows and repeats source rows 96–199 below them
  framework::render::indexed_cockpit_framebuffer result;
  copy_rectangle(sheet.pixels, result.pixels, {0, 0}, {0, 0}, 320, 136);
  copy_rectangle(sheet.pixels, result.pixels, {0, 96}, {0, 136}, 320, 104);
  return result;
}

void draw_skimma_shield_startup(framework::render::indexed_cockpit_framebuffer const &cache,
  framework::render::indexed_cockpit_framebuffer &target, std::uint8_t const state) {
  /// 5192 draws the startup range into the same mask as the ordinary shield-strength gauge
  if(state > 23) throw std::out_of_range{"Skimma shield startup state exceeds native range"};
  auto const range{skimma_shield_strips(state)};
  auto const &descriptor{components_4d70[1]};
  // 51AD converts native BL into a pulse width, with BH selecting its final strip.
  for(std::size_t i{range.end > range.first ? range.end - range.first - 1u : 0u}; i < range.end; ++i) {
    auto const &strip{descriptor.strips[i]};
    copy_mask(cache.pixels, target.pixels, {descriptor.on_source.x, descriptor.on_source.y + strip.y_offset},
      {descriptor.destination.x, descriptor.destination.y + strip.y_offset}, strip.rows);
  }
}

void clear_windscreen(framework::render::indexed_cockpit_framebuffer &target, craft const type, std::uint8_t const colour) {
  /// Reserve the original view rectangle for a later world renderer
  int const top{type == craft::caero ? 8 : 0};
  int const height{type == craft::caero ? 168 : 180};
  std::fill_n(target.pixels.begin() + top * 320, height * 320, colour);
}

void draw_caero_frame_edges(framework::render::indexed_cockpit_framebuffer const &cache,
  framework::render::indexed_cockpit_framebuffer &target) {
  /// 5575–559F restores the nonrectangular cockpit edge after the world and instruments
  std::array<mask_row, 7> constexpr upper_instrument{{
    {23,13}, {14,27}, {12,30}, {10,34}, {9,37}, {7,41}, {5,44},
  }};
  // DFB0 samples logical row 43 through the Caero +8 source row table; destination 169 is unshifted.
  copy_mask(cache.pixels,target.pixels,{72, 51},{48, 169},upper_instrument);
  std::fill_n(target.pixels.begin() + 8 * 320 + 122,76,152);
  std::fill_n(target.pixels.begin() + 175 * 320 + 182,53,21);
}

void draw_skimma_frame_edges(framework::render::indexed_cockpit_framebuffer const &cache,
  framework::render::indexed_cockpit_framebuffer &target) {
  /// 54FE–5531 restores three source-sheet masks around the Skimma's rectangular world view
  std::array<mask_row, 16> constexpr top{{
    {0,78}, {0,68}, {0,58}, {0,50}, {0,43}, {0,37}, {0,31}, {0,27},
    {0,23}, {0,19}, {0,16}, {0,13}, {0,10}, {0,7}, {0,4}, {0,1},
  }};
  std::array<mask_row, 9> constexpr instruments{{
    {75,79}, {53,126}, {39,157}, {32,179}, {30,193}, {29,204}, {28,212}, {17,223}, {7,233},
  }};
  std::array<mask_row, 29> constexpr left{{
    {0,1}, {0,2}, {0,3}, {0,4}, {0,5}, {0,6}, {0,8}, {0,9}, {0,10}, {0,12},
    {0,13}, {0,15}, {0,17}, {0,18}, {0,20}, {0,22}, {0,24}, {0,26}, {0,28}, {0,30},
    {0,32}, {0,34}, {0,37}, {0,39}, {0,42}, {0,45}, {0,48}, {0,52}, {0,58},
  }};
  copy_mask(cache.pixels, target.pixels, {0, 8}, {0, 0}, top);
  copy_mask(cache.pixels, target.pixels, {24, 8}, {80, 171}, instruments);
  copy_mask(cache.pixels, target.pixels, {216, 26}, {0, 151}, left);
}

void update_instrument(framework::render::indexed_cockpit_framebuffer const &cache,
  framework::render::indexed_cockpit_framebuffer &target, craft const type, std::size_t const component,
  std::uint8_t const old_state, std::uint8_t const new_state) {
  /// Translate 457B–45A6 strip-count changes and 51B8 scanline-mask copying
  auto const limit{instrument_limit(type, component)};
  auto const &descriptor{cockpit_components(type)[component]};
  std::size_t const old_count{static_cast<std::size_t>(old_state & 127)};
  std::size_t const new_count{static_cast<std::size_t>(new_state & 127)};
  if(old_count > limit || new_count > limit) throw std::out_of_range{"instrument state exceeds craft limit"};
  if(((old_state | new_state) & 128) && !(type == craft::caero && descriptor.field == 0x4552)) {
    throw std::invalid_argument{"alternate source is verified only for the Caero engine light"};
  }
  bool const restoring{new_count < old_count};
  auto source{restoring ? descriptor.destination : (new_state & 128) ? descriptor.alternate_source : descriptor.on_source};
  std::size_t const first{old_count == new_count ? (new_count ? new_count - 1 : 0) : std::min(old_count, new_count)};
  std::size_t const end{std::max(old_count, new_count)};
  for(std::size_t i{first}; i < end; ++i) {
    auto const &strip{descriptor.strips[i]};
    auto const sy{source.y + strip.y_offset};
    auto const dy{descriptor.destination.y + strip.y_offset};
    for(std::size_t row{0}; row < strip.rows.size(); ++row) {
      int const source_y{sy + static_cast<int>(row)};
      int const destination_y{dy + static_cast<int>(row)};
      // AF57 shifts logical rows 0–167 by eight in the normal Caero cockpit.
      int const source_offset{type == craft::caero && source_y < 168 ? 8 : 0};
      int const destination_offset{type == craft::caero && destination_y < 168 ? 8 : 0};
      copy_mask(cache.pixels, target.pixels, {source.x, source_y + source_offset},
        {descriptor.destination.x, destination_y + destination_offset}, strip.rows.subspan(row, 1));
    }
  }
}

} // namespace darker::graphics

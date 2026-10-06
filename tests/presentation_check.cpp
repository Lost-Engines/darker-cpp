#include "presentation_check.h"
#include <iostream>
#include <algorithm>
#include <stdexcept>
#include "presentation/player.h"
#include "reference/presentation_samples.h"

void check_presentations(darker::resources::archive_set const &archives) {
  /// Compare every startup and briefing animation frame with native-verified pixel streams
  for(unsigned int slot : {1, 2}) {
    auto const frames{darker::presentation::decode_animation(archives.load({1,slot}))};
    for(auto const &sample : darker::test_reference::presentation_samples) {
      if(sample.slot != slot) continue;
      auto const &frame{frames.at(sample.frame)};
      uint64_t fingerprint{0xcbf29ce484222325};
      for(auto const &pixel : frame) {
        for(unsigned int byte : {pixel.x & 255u, static_cast<unsigned int>(pixel.x >> 8), pixel.y & 255u, static_cast<unsigned int>(pixel.y >> 8), static_cast<unsigned int>(pixel.colour)}) {
          fingerprint = (fingerprint ^ byte) * 0x100000001b3;
        }
      }
      if(frame.size() != sample.pixels || fingerprint != sample.fingerprint) throw std::runtime_error{"Presentation pixels differ from native DF36"};
    }
  }
  darker::resources::font_resource const font{archives.load({0,29})};
  darker::resources::scenario_resource const mission{archives.load({4,0})};
  darker::presentation::player briefing{archives,font,mission,0};
  framework::render::cockpit_framebuffer frame{};
  // DA2F retains B2BD/B2C1 across pages: the second English page has no spacing command.
  darker::presentation::player quotation{archives,font,mission,0};
  quotation.continue_page();
  quotation.draw(frame);
  auto const background{archives.load({0,22})};
  auto const palette{darker::graphics::decode_palette(background,{})};
  framework::render::indexed_cockpit_framebuffer expected{};
  for(size_t i{0}; i < expected.pixels.size(); ++i) expected.pixels[i] = std::to_integer<uint8_t>(background[palette.bytes_consumed+i]);
  auto const page{darker::graphics::lay_out_text(mission.language(0,darker::resources::scenario_language::english).subspan(269),
    font,darker::resources::font_face::wide,{.x{24},.y{154},.colour{0xfffe},.margin{8},.line_step{11}})};
  for(auto const &glyph : page.glyphs) darker::graphics::draw_glyph(expected,font,darker::resources::font_face::wide,glyph.code,glyph.position,{.ink{255},.edge{254}});
  for(auto const x : {287,305}) darker::graphics::draw_glyph(expected,font,darker::resources::font_face::wide,x == 287 ? 60 : 62,{.x{x},.y{226}},{.ink{255},.edge{254}});
  framework::render::cockpit_framebuffer expected_rgb{};
  framework::render::expand_palette(expected,palette.palette.colours,expected_rgb);
  if(!std::ranges::equal(std::as_bytes(std::span{frame.pixels}),std::as_bytes(std::span{expected_rgb.pixels}))) throw std::runtime_error{"Quotation lost inherited spacing or original advance glyphs"};
  unsigned int pages{0};
  do {
    briefing.draw(frame);
    briefing.advance(2000);
    briefing.draw(frame);
    ++pages;
    if(pages > 4) throw std::runtime_error{"Unexpected briefing continuation"};
  } while(briefing.continue_page());
  if(pages != 4 || briefing.consumed_text() != 1085 || !briefing.finished()) throw std::runtime_error{"Briefing did not end at the first in-flight message"};
  // DABA is patched to image Y=24: stored animation rows are relative to that origin.
  darker::graphics::palette_state portrait_palette;
  for(auto id : {darker::resources::resource_id{0,22}, {0,24}, {3,12}, {0,23}, {3,9}}) portrait_palette = darker::graphics::decode_palette(archives.load(id),portrait_palette).palette;
  auto const portrait_frames{darker::presentation::decode_animation(archives.load({1,2}))};
  for(auto const &pixel : portrait_frames.back()) {
    auto const y{pixel.y + 24};
    if(pixel.x >= 120 || y >= 226) continue;
    auto const actual{frame.pixels[y*320+pixel.x]};
    auto const colour{portrait_palette.colours[pixel.colour]};
    if(actual.red != colour.red || actual.green != colour.green || actual.blue != colour.blue) throw std::runtime_error{"Briefing animation lost its scene-relative Y origin"};
  }
  darker::resources::scenario_resource const startup{archives.load({4,15})};
  darker::presentation::player intro{archives,font,startup,1};
  for(unsigned int tick{0}; tick < 2000; ++tick) { intro.advance(1); intro.draw(frame); }
  if(!intro.finished()) throw std::runtime_error{"Startup presentation failed to finish"};
  std::cout << "35 animation frames match native DF36; startup and four-page briefing complete with the expected message cursor." << std::endl;
}

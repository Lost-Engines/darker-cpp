#include "presentation_check.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include "presentation/front_end.h"
#include "presentation/player.h"
#include "reference/message_display_samples.h"
#include "reference/presentation_samples.h"

void check_presentations(darker::resources::archive_set const &archives) {
  /// Compare every startup and briefing animation frame with native-verified pixel streams
  for(auto const id : {darker::resources::resource_id{1,0},{1,1},{1,2},{1,3},{1,5},{3,2}}) {
    auto const frames{darker::presentation::decode_animation(archives.load(id))};
    for(auto const &sample : darker::test_reference::presentation_samples) {
      if(sample.archive != id.archive || sample.slot != id.slot) continue;
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
  for(auto const &sample : darker::test_reference::message_display_samples) {
    framework::render::indexed_cockpit_framebuffer caption_frame;
    caption_frame.pixels.fill(7);
    std::string const text{"Return to base."};
    darker::graphics::draw_message(caption_frame,font,static_cast<darker::resources::font_face>(sample.face),
      std::as_bytes(std::span{text}),{.x{static_cast<int>(sample.x)},.y{static_cast<int>(sample.y)}},
      static_cast<uint16_t>(sample.width),{.ink{24},.edge{18}});
    uint64_t fingerprint{0xcbf29ce484222325};
    for(auto const pixel : caption_frame.pixels) fingerprint = (fingerprint ^ pixel)*0x100000001b3;
    if(fingerprint != sample.fingerprint) throw std::runtime_error{"Counted message pixels differ from B20E: face="
      +std::to_string(sample.face)+", x="+std::to_string(sample.x)+", width="+std::to_string(sample.width)};
  }
  darker::resources::campaign_resources campaign{archives};
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
  for(auto const pointer : {std::array{283,230},std::array{284,225},std::array{301,239},std::array{302,225},std::array{310,224}}) {
    auto hovered{expected};
    auto const glyph{pointer[1] >= 225 && pointer[0] >= 284 ? (pointer[0] < 302 ? 60 : 62) : 0};
    if(glyph) darker::graphics::draw_glyph(hovered,font,darker::resources::font_face::wide,static_cast<uint8_t>(glyph),
      {.x{glyph == 60 ? 287 : 305},.y{226}},{.ink{253},.edge{252}});
    framework::render::expand_palette(hovered,palette.palette.colours,expected_rgb);
    quotation.draw(frame,pointer);
    if(!std::ranges::equal(std::as_bytes(std::span{frame.pixels}),std::as_bytes(std::span{expected_rgb.pixels})))
      throw std::runtime_error{"Presentation navigation differs from DA75/DA48 hover bounds and colours"};
  }
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
  for(uint8_t stage{2}; stage <= 16; ++stage) {
    darker::presentation::player next{archives,font,campaign.scenario(stage),darker::resources::select_campaign_stage(stage).record};
    size_t scenes{0};
    do {
      for(unsigned int tick{0}; tick < 10000; tick += 50) {
        next.advance(50);
        next.draw(frame);
      }
      if(++scenes > 8) throw std::runtime_error{"Campaign briefing failed to terminate"};
    } while(next.continue_page());
    if(!next.finished()) throw std::runtime_error{"Campaign briefing remains active: " + std::to_string(stage)};
  }
  darker::presentation::player launch_clip{archives,font,mission,1};
  launch_clip.advance(3000);
  auto const briefing_cursor{launch_clip.consumed_text()};
  if(!launch_clip.continue_page()) throw std::runtime_error{"Second mission has no launch clip continuation"};
  launch_clip.draw(frame);
  auto const blank{frame.pixels.front()};
  for(auto const pixel : frame.pixels) {
    if(pixel.red != blank.red || pixel.green != blank.green || pixel.blue != blank.blue) {
      throw std::runtime_error{"Second-mission clear-screen transition retained briefing text"};
    }
  }
  if(launch_clip.consumed_text() != briefing_cursor) throw std::runtime_error{"Clearing presentation text consumed mission messages"};
  darker::resources::scenario_resource const startup{archives.load({4,15})};
  darker::presentation::player intro{archives,font,startup,1};
  for(unsigned int tick{0}; tick < 2000; ++tick) { intro.advance(1); intro.draw(frame); }
  if(!intro.finished()) throw std::runtime_error{"Startup presentation failed to finish"};
  darker::presentation::player committal{archives,font,startup,2};
  for(unsigned int tick{0}; tick < 60000; tick += 33) {
    committal.advance(33);
    committal.draw(frame);
  }
  if(committal.finished() || committal.consumed_text() == 0 || committal.music != 5 || committal.input_policy != 4) {
    throw std::runtime_error{"Committal presentation failed to retain its text while looping"};
  }
  darker::resources::save_file saves;
  darker::presentation::front_end front{archives,font,campaign,saves};
  front.show_death(2);
  front.advance(60000);
  front.draw(frame);
  front.key(darker::presentation::front_key::back);
  front.draw(frame);
  if(!front.active()) throw std::runtime_error{"Dismissing death must return to the menu, not launch a mission"};
  // The hidden command must never be interpreted as a pilot name or a save-record setting.
  darker::resources::save_file command_saves;
  darker::presentation::front_end commands{archives,font,campaign,command_saves};
  using darker::presentation::front_key;
  commands.key(front_key::accept);
  commands.advance(1024);
  commands.key(front_key::accept);
  commands.advance(128);
  commands.key(front_key::accept);
  commands.key(front_key::one);
  commands.character('1');
  for(char const c : std::string{"Level X"}) commands.character(static_cast<unsigned int>(c));
  commands.key(front_key::accept);
  if(commands.level_skip_enabled() || command_saves.pilots[0].display_name() != "Level X") throw std::runtime_error{"Normal pilot name enabled Level X"};
  commands.save_requested = false;
  commands.key(front_key::select);
  auto const saved_before{darker::resources::encode_save(command_saves)};
  for(auto const phrase : {"level x","Level XI","Level X ","Level X"}) {
    commands.character('*');
    commands.advance(10000);
    commands.key(front_key::three);
    commands.character('3');
    if(!commands.editing_text()) throw std::runtime_error{"STAR THREE prompt did not open"};
    for(char const c : std::string{phrase}) commands.character(static_cast<unsigned int>(c));
    commands.key(front_key::accept);
    if(commands.level_skip_enabled() != (std::string_view{phrase} == "Level X")) throw std::runtime_error{"Hidden command matching is not exact"};
    if(commands.save_requested || darker::resources::encode_save(command_saves) != saved_before) throw std::runtime_error{"Hidden command changed a saved pilot"};
  }
  commands.key(front_key::one);
  commands.character('1');
  commands.show_death(0);
  commands.advance(60000);
  commands.key(front_key::back);
  if(!commands.level_skip_enabled()) throw std::runtime_error{"Death cleared the process-wide Level X patch"};
  // Interstitial records must never construct a world using their FF configuration.
  for(uint8_t const stage : std::array<uint8_t,9>{98,100,102,104,106,108,110,112,114}) {
    darker::resources::save_file interstitial_saves;
    interstitial_saves.pilots[0].stage = stage;
    interstitial_saves.pilots[0].weapons = 0x3ff;
    darker::presentation::front_end transition{archives,font,campaign,interstitial_saves,true};
    transition.key(front_key::one);
    transition.key(front_key::accept);
    for(unsigned int frame_index{0}; transition.active() && frame_index < 10000; ++frame_index) {
      transition.advance(32);
      transition.key(front_key::accept);
    }
    if(transition.active() || interstitial_saves.pilots[0].stage != stage+1 || !transition.save_requested)
      throw std::runtime_error{"Halon interstitial did not advance to its playable stage"};
    if(interstitial_saves.pilots[0].weapons != 0x3ff)
      throw std::runtime_error{"Presentation-only progression changed saved weapon state"};
  }
  for(uint8_t const outcome : std::array<uint8_t,5>{1,2,3,4,255}) {
    darker::resources::save_file challenge_save;
    challenge_save.pilots[0].stage = 41;
    challenge_save.pilots[0].weapons = 123;
    challenge_save.trailer = {std::byte{12},std::byte{0xa5}};
    auto const previous{darker::resources::encode_save(challenge_save)};
    darker::presentation::front_end challenge{archives,font,campaign,challenge_save,true};
    challenge.key(front_key::nightmare);
    challenge.key(front_key::erase);
    challenge.key(front_key::back);
    challenge.key(front_key::accept);
    for(unsigned int i{0}; challenge.active() && i < 200; ++i) { challenge.advance(32); challenge.key(front_key::accept); }
    if(challenge.active() || !challenge.nightmare_selected() || challenge.selected_record() != 0
      || &challenge.selected_scenario() != &campaign.supplementary() || challenge.save_requested)
      throw std::runtime_error{"Nightmare failed its separate menu/scenario entry"};
    if(!challenge.entry() || challenge.entry()->site != 0x4258 || challenge.entry()->heading != 0xc4)
      throw std::runtime_error{"Nightmare lost its Kismet Square starting position"};
    challenge.finish_nightmare(37,outcome);
    challenge.advance(60000);
    challenge.draw(frame);
    if(!challenge.active() || !challenge.save_requested || challenge_save.trailer[0] != std::byte{37})
      throw std::runtime_error{"Nightmare failed to retain its best score"};
    auto const after{darker::resources::encode_save(challenge_save)};
    if(!std::equal(previous.begin(),previous.begin()+6596,after.begin()) || after[6597] != previous[6597])
      throw std::runtime_error{"Nightmare changed a campaign record or the other trailer byte"};
    challenge.save_requested = false;
    challenge.finish_nightmare(9,255);
    if(challenge.save_requested || challenge_save.trailer[0] != std::byte{37})
      throw std::runtime_error{"A lower Nightmare score replaced the best score"};
    challenge.key(front_key::erase);
    challenge.key(front_key::yes);
    auto const erased{darker::resources::encode_save(challenge_save)};
    if(!challenge.save_requested || challenge_save.trailer[0] != std::byte{0}
      || !std::equal(previous.begin(),previous.begin()+6596,erased.begin()) || erased[6597] != previous[6597])
      throw std::runtime_error{"Erasing the Nightmare high score changed unrelated save data"};
  }
  darker::resources::save_file ending_save;
  ending_save.pilots[0].stage = 116;
  darker::presentation::front_end ending{archives,font,campaign,ending_save,true};
  ending.key(front_key::one);
  ending.key(front_key::accept);
  for(unsigned int i{0}; i < 8000; ++i) ending.advance(32);
  ending.draw(frame);
  if(!ending.active() || ending_save.pilots[0].stage != 116 || ending.save_requested)
    throw std::runtime_error{"Ending must retain the completed campaign instead of entering a nonexistent stage"};
  ending.key(front_key::back);
  if(!ending.active()) throw std::runtime_error{"Ending back button entered flight"};
  std::cout << "Startup, committal and first three mission animation frames match native DF36; startup and four-page briefing complete; committal animation loops and returns to the menu." << std::endl;
}

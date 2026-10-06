#include "presentation_check.h"
#include <iostream>
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
  unsigned int pages{0};
  do {
    briefing.draw(frame);
    briefing.advance(2000);
    briefing.draw(frame);
    ++pages;
    if(pages > 4) throw std::runtime_error{"Unexpected briefing continuation"};
  } while(briefing.continue_page());
  if(pages != 4 || briefing.consumed_text() != 1085 || !briefing.finished()) throw std::runtime_error{"Briefing did not end at the first in-flight message"};
  darker::resources::scenario_resource const startup{archives.load({4,15})};
  darker::presentation::player intro{archives,font,startup,1};
  for(unsigned int tick{0}; tick < 2000; ++tick) { intro.advance(1); intro.draw(frame); }
  if(!intro.finished()) throw std::runtime_error{"Startup presentation failed to finish"};
  std::cout << "35 animation frames match native DF36; startup and four-page briefing complete with the expected message cursor." << std::endl;
}

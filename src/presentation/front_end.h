#pragma once

#include <memory>
#include <string>
#include "presentation/player.h"

namespace darker::presentation {

enum class front_key { accept, back, up, down, erase_character, select, erase, quit, one, two, three, four };

class front_end {
private:
  enum class screen { introduction, title, games, name, run, erase, quit, briefing, flight };
  resources::archive_set const &archives;
  resources::font_resource const &font;
  resources::scenario_resource const &mission;
  resources::scenario_resource introduction;
  std::unique_ptr<player> scene;
  screen current{screen::introduction};
  screen previous{screen::games};
  framework::render::indexed_cockpit_framebuffer menu_background{}, title_background{};
  graphics::palette_state menu_palette, title_palette;
  std::array<std::string, 4> names;
  unsigned int selected{0};
  bool confirmation{false};
  unsigned int ignored_character{0};
  void choose_game();
  void begin_briefing();

public:
  bool quit_requested{false};
  front_end(resources::archive_set const &archives, resources::font_resource const &font, resources::scenario_resource const &mission);
  bool active() const noexcept;
  bool editing_name() const noexcept;
  size_t consumed_text() const noexcept;
  void key(front_key input);
  void character(unsigned int code);
  void click(int x, int y);
  void advance(uint32_t elapsed_ticks);
  void draw(framework::render::cockpit_framebuffer &output) const;
  void return_to_menu();
};

} // namespace darker::presentation

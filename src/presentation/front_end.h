#pragma once

#include <memory>
#include <string>
#include "presentation/player.h"
#include "resources/save_file.h"

namespace darker::presentation {

enum class front_key { accept, back, up, down, erase_character, select, erase, quit, one, two, three, four };

class front_end {
private:
  enum class screen { introduction, title, games, name, hidden_command, run, erase, quit, briefing, outcome, flight };
  resources::archive_set const &archives;
  resources::font_resource const &font;
  resources::scenario_resource const &mission;
  resources::scenario_resource introduction;
  std::unique_ptr<player> scene;
  screen current{screen::introduction};
  screen previous{screen::games};
  framework::render::indexed_cockpit_framebuffer menu_background{}, title_background{};
  graphics::palette_state menu_palette, title_palette;
  resources::save_file &save;
  std::string draft_name;
  std::string selection_prompt{"Select a game: 1,2,3,4"};
  bool star_prefix{false};
  bool level_x{false};
  bool unsupported_stage{false};
  unsigned int selected{0};
  int retained_music{-1};
  bool confirmation{false};
  unsigned int ignored_character{0};
  void choose_game();
  void begin_briefing();

public:
  bool quit_requested{false};
  bool save_requested{false};
  front_end(resources::archive_set const &archives, resources::font_resource const &font, resources::scenario_resource const &mission, resources::save_file &save, bool skip_intro = false);
  bool active() const noexcept;
  bool editing_text() const noexcept;
  bool level_skip_enabled() const noexcept;
  size_t consumed_text() const noexcept;
  uint16_t weapon_changes() const noexcept;
  int music_group() const noexcept;
  void key(front_key input);
  void character(unsigned int code);
  void click(int x, int y);
  void advance(uint32_t elapsed_ticks);
  void draw(framework::render::cockpit_framebuffer &output) const;
  void return_to_menu();
  void show_death(uint8_t completed_objects);
  resources::pilot_record &selected_pilot() noexcept;
  void continue_campaign();
};

} // namespace darker::presentation

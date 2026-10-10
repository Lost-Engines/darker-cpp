#pragma once

#include <memory>
#include <string>
#include "presentation/player.h"
#include "resources/campaign.h"
#include "resources/save_file.h"

namespace darker::presentation {

enum class front_key { accept, back, up, down, erase_character, select, erase, quit, one, two, three, four, nightmare, yes };

class front_end {
private:
  enum class screen { introduction, title, credits, games, name, hidden_command, run, erase, quit, briefing, outcome, flight };
  resources::archive_set const &archives;
  resources::font_resource const &font;
  resources::campaign_resources &campaign;
  resources::scenario_resource introduction;
  resources::scenario_language language;
  std::unique_ptr<player> scene;
  screen current{screen::introduction};
  screen previous{screen::games};
  framework::render::indexed_cockpit_framebuffer menu_background{}, title_background{}, credits_background{};
  graphics::palette_state menu_palette, title_palette;
  resources::save_file &save;
  resources::pilot_record challenge_pilot{
    .stage{1}
  };
  uint8_t challenge_score{0};
  std::string draft_name;
  std::string selection_prompt{"Select a game: 1,2,3,4 or N"};
  bool star_prefix{false};
  bool level_x{false};
  bool unsupported_stage{false};
  unsigned int selected{0};
  std::array<int, 2> pointer{-1, -1};
  int retained_music{-1};
  uint32_t title_ticks{0};
  bool confirmation{false};
  unsigned int ignored_character{0};
  void choose_game();
  void begin_briefing(bool continued_mission = false);
  void finish_briefing();
  void show_outcome(uint8_t outcome, uint8_t completed_objects);

public:
  bool quit_requested{false};
  bool save_requested{false};
  front_end(resources::archive_set const &archives, resources::font_resource const &font, resources::campaign_resources &campaign, resources::save_file &save, bool skip_intro = false,
    resources::scenario_language language = resources::scenario_language::english);
  bool active() const noexcept;
  bool nightmare_selected() const noexcept;
  resources::scenario_resource const &selected_scenario();
  size_t selected_record() const;
  void finish_nightmare(uint8_t completed_objects, uint8_t outcome);
  bool editing_text() const noexcept;
  bool level_skip_enabled() const noexcept;
  void enable_level_skip() noexcept;
  size_t consumed_text() const noexcept;
  uint16_t weapon_changes() const noexcept;
  std::optional<uint8_t> initial_difficulty() const noexcept;
  uint8_t initial_score() const noexcept;
  uint16_t departure_destination() const noexcept;
  std::optional<landing_entry> entry() const noexcept;
  int music_group() const noexcept;
  void key(front_key input);
  void character(unsigned int code);
  void point(int x, int y) noexcept;
  void click(int x, int y);
  void advance(uint32_t elapsed_ticks);
  void draw(framework::render::cockpit_framebuffer &output) const;
  void return_to_menu();
  void show_death(uint8_t completed_objects);
  void show_abort(uint8_t completed_objects);
  resources::pilot_record &selected_pilot() noexcept;
  void continue_campaign();
  void start_level(uint8_t stage);
  void previous_level();
};

} // namespace darker::presentation

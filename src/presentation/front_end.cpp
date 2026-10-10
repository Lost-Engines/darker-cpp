#include "presentation/front_end.h"
#include <algorithm>
#include <stdexcept>
#include "game/city_persistence.h"
#include "game/scenario_world.h"
#include "graphics/font.h"
#include "presentation/menu_text.h"

namespace darker::presentation {
namespace {

void load_image(resources::archive_set const &archives, resources::resource_id const id,
  framework::render::indexed_cockpit_framebuffer &frame, graphics::palette_state &palette, unsigned int const width,
  unsigned int const height, unsigned int const x, unsigned int const y) {
  /// Load original menu/title pixels with their palette, leaving the remaining page black
  auto const bytes{archives.load(id)};
  auto const decoded{graphics::decode_palette(bytes, palette)};
  palette = decoded.palette;
  if(bytes.size() - decoded.bytes_consumed != width * height) throw std::invalid_argument{"Unexpected front-end image size"};
  frame.pixels.fill(0);
  for(size_t row{0}; row < height; ++row) {
    for(size_t column{0}; column < width; ++column) {
      frame.pixels[(y + row) * framework::render::display_layout::width + x + column] = std::to_integer<uint8_t>(bytes[decoded.bytes_consumed + row * width + column]);
    }
  }
}

} // anonymous namespace

front_end::front_end(resources::archive_set const &archives, resources::font_resource const &font, resources::campaign_resources &campaign, resources::save_file &save, bool const skip_intro, resources::scenario_language const language)
  : archives{archives}, font{font}, campaign{campaign}, introduction{archives.load({4, 15})}, language{language}, save{save} {
  /// Startup 9CBD selects record one before the 00/14 title and 00/21 game-selection background
  load_image(archives, {0, 21}, menu_background, menu_palette, 320, 240, 0, 0);
  // 03F4/DF1D copy the lower palette; AFB1 supplies separate gains and DAC offsets for shaded panels
  for(size_t i{0}; i < 128; ++i) {
    auto const source{menu_palette.colours[i]};
    menu_palette.colours[i + 128] = {
      static_cast<uint8_t>((8 + graphics::palette_dac_component(source.r, 24)) * 4),
      static_cast<uint8_t>((9 + graphics::palette_dac_component(source.g, 25)) * 4),
      static_cast<uint8_t>((9 + graphics::palette_dac_component(source.b, 26)) * 4),
    };
  }
  load_image(archives, {0, 14}, title_background, title_palette, 280, 100, 16, 65);
  std::array<std::byte, 2> constexpr trademark{std::byte{'T'}, std::byte{'M'}};
  graphics::draw_text(title_background, font, resources::font_face::compact, trademark, {256, 84}, {
    .ink{129},
    .edge{130}
  });
  credits_background = menu_background;
  auto const logo{archives.load({0, 28})};
  if(logo.size() != 80 * 17) throw std::invalid_argument{"Unexpected credits logo size"};
  for(size_t y{0}; y < 17; ++y) {
    for(size_t x{0}; x < 80; ++x) {
      credits_background.pixels[(y + 48) * framework::render::display_layout::width + x + 120] = std::to_integer<uint8_t>(logo[y * 80 + x]);
    }
  }
  auto const credits{original_credits.at(static_cast<size_t>(language))};
  auto const credit_page{graphics::lay_out_text(std::as_bytes(std::span{credits}), font, resources::font_face::interface, {
    .y{96},
    .colour{0x7d00}
  })};
  for(auto const &glyph : credit_page.glyphs) graphics::draw_glyph(credits_background, font, resources::font_face::interface, glyph.code, glyph.position, {
    .ink{125},
    .edge{0}
  });

  selection_prompt = original_menu_text.at(static_cast<size_t>(language)).select;
  if(skip_intro) current = screen::games;
  else scene = std::make_unique<player>(archives, font, introduction, 1, 0, false, language);
}

bool front_end::active() const noexcept {
  /// Flight input remains unavailable until the final briefing scene is dismissed
  return current != screen::flight;
}

bool front_end::nightmare_selected() const noexcept {
  /// Original selection four names a separate challenge, not a fifth pilot record
  return selected == 4;
}

resources::scenario_resource const &front_end::selected_scenario() {
  /// Special selection bypasses BB12 and chooses archive 04/15 directly
  return nightmare_selected() ? campaign.supplementary() : campaign.scenario(selected_pilot().stage);
}

size_t front_end::selected_record() const {
  /// Nightmare always enters record zero independently of its best-score byte
  return nightmare_selected() ? 0 : resources::select_campaign_stage(save.pilots[selected].stage).record;
}

void front_end::finish_nightmare(uint8_t const completed_objects, uint8_t const outcome) {
  /// 3F23 retains the best byte-sized kill count; only outcome four presents the challenge victory
  if(!nightmare_selected()) throw std::logic_error{"Nightmare outcome requires the special selection"};
  challenge_score = completed_objects;
  if(challenge_score > std::to_integer<uint8_t>(save.trailer[0])) {
    save.trailer[0] = static_cast<std::byte>(challenge_score);
    save_requested = true;
  }
  current = screen::run;
  if(outcome == 4) {
    retained_music = -1;
    scene = std::make_unique<player>(archives, font, introduction, 4, completed_objects, false, language);
    current = screen::outcome;
  }
}

bool front_end::editing_text() const noexcept {
  /// Both native editor states receive text independently of menu command keys
  return current == screen::name || current == screen::hidden_command;
}

void front_end::enable_level_skip() noexcept {
  /// Share the process-wide cheat activation between the original hidden command and startup options
  level_x = true;
}

bool front_end::level_skip_enabled() const noexcept {
  /// The original executable patch lasts for the process, independently of pilot selection and deaths
  return level_x;
}

size_t front_end::consumed_text() const noexcept {
  /// The briefing's retained cursor is the start of the mission's counted messages
  return scene ? scene->consumed_text() : 0;
}

uint16_t front_end::weapon_changes() const noexcept {
  /// Briefing opcode 30 applies its range toggles to the saved weapon availability
  return scene ? scene->weapon_toggles : 0;
}

std::optional<uint8_t> front_end::initial_difficulty() const noexcept {
  /// Briefing opcode 36 overrides the ordinary saved-stage firing pressure
  return scene ? scene->difficulty : std::nullopt;
}

uint8_t front_end::initial_score() const noexcept {
  /// Nightmare's briefing resets the separate score accumulator with opcode 37
  return scene && scene->score ? *scene->score : 0;
}

uint16_t front_end::departure_destination() const noexcept {
  /// C246 redirects the return site when the player leaves the departure hangar
  return scene ? scene->departure_destination : 0;
}

std::optional<landing_entry> front_end::entry() const noexcept {
  /// C23B supplies the scenario's initial landing site and heading independently of the saved previous site
  return scene ? scene->entry : std::nullopt;
}

int front_end::music_group() const noexcept {
  /// Presentation scripts select music groups; flight stops music and menus select group zero
  if(current == screen::flight) return -1;
  if(current == screen::introduction || current == screen::briefing || current == screen::outcome) return scene->music ? *scene->music : retained_music;
  return 0;
}

void front_end::choose_game() {
  /// An empty slot asks for a name before exposing its run menu
  draft_name.clear();
  unsupported_stage = false;
  current = !nightmare_selected() && save.pilots[selected].stage == 0 ? screen::name : screen::run;
}

void front_end::begin_briefing(bool const continued_mission) {
  /// Select the current supported campaign record for briefing
  if(!nightmare_selected() && (save.pilots[selected].stage < 1 || save.pilots[selected].stage > 116)) {
    unsupported_stage = true;
    return;
  }
  retained_music = music_group();
  if(nightmare_selected()) { challenge_pilot = {
    .stage{1}
  };
  challenge_score = 0; }
  scene = std::make_unique<player>(archives, font, selected_scenario(), selected_record(), 0, continued_mission, language);
  current = screen::briefing;
  if(scene->finished() && scene->input_policy == 0) finish_briefing();
}

void front_end::finish_briefing() {
  /// Presentation-only records advance the saved stage without constructing a flight world
  auto &pilot{selected_pilot()};
  auto const record{selected_record()};
  if(selected_scenario().records()[record].configuration != 0xff) {
    current = screen::flight;
    return;
  }
  ++pilot.stage;
  save_requested = true;
  current = screen::run;
  begin_briefing(true);
}

void front_end::key(front_key const input) {
  /// Route menu commands separately from in-flight bindings
  if(current == screen::introduction) {
    current = screen::title;
    return;
  }
  if(current == screen::title) {
    if(title_ticks >= 1024) {
      current = screen::credits;
      title_ticks = 0;
    }
    return;
  }
  if(current == screen::credits) {
    if(title_ticks >= 128) current = screen::games;
    return;
  }
  if(current == screen::outcome) {
    if((input == front_key::back || input == front_key::accept) && (scene->input_policy & 4)) current = screen::run;
    return;
  }
  if(current == screen::briefing) {
    if(input == front_key::back && (scene->input_policy & 4)) {
      current = screen::run;
      return;
    }
    if(input == front_key::accept && (scene->input_policy & 1) && !scene->continue_page()) finish_briefing();
    return;
  }
  if(current == screen::hidden_command) {
    if(input == front_key::erase_character && !draft_name.empty()) draft_name.pop_back();
    if(input == front_key::accept || input == front_key::back) {
      if(input == front_key::accept && draft_name == "Level X") {
        enable_level_skip();
        selection_prompt = draft_name;
      }
      draft_name.clear();
      current = screen::games;
    }
    return;
  }
  if(current == screen::name) {
    if(input == front_key::erase_character && !draft_name.empty()) draft_name.pop_back();
    if(input == front_key::accept && !draft_name.empty()) {
      auto &pilot{save.pilots[selected]};
      pilot = {};
      pilot.stage = 1;
      pilot.set_name(draft_name);
      save_requested = true;
      current = screen::run;
    }
    if(input == front_key::back) {
      draft_name.clear();
      current = screen::games;
    }
    return;
  }
  if(current == screen::erase || current == screen::quit) {
    if(input == front_key::yes || input == front_key::nightmare) {
      confirmation = input == front_key::yes;
      key(front_key::accept);
      return;
    }
    if(input == front_key::up || input == front_key::down) confirmation = !confirmation;
    if(input == front_key::back) current = previous;
    else if(input == front_key::accept) {
      if(confirmation && current == screen::quit) quit_requested = true;
      if(confirmation && current == screen::erase) {
        if(nightmare_selected()) save.trailer[0] = std::byte{0};
        else save.pilots[selected] = {};
        save_requested = true;
        unsupported_stage = false;
      }
      current = !nightmare_selected() && save.pilots[selected].stage == 0 ? screen::games : previous;
    }
    return;
  }
  if(input == front_key::quit || input == front_key::back) {
    previous = current;
    current = screen::quit;
    confirmation = false;
    return;
  }
  if(current == screen::games) {
    if(input == front_key::three && star_prefix) {
      star_prefix = false;
      draft_name.clear();
      ignored_character = '3';
      current = screen::hidden_command;
      return;
    }
    if(input == front_key::up) selected = (selected + 4) % 5;
    if(input == front_key::down) selected = (selected + 1) % 5;
    if(input == front_key::nightmare) {
      selected = 4;
      choose_game();
    }
    if(input >= front_key::one && input <= front_key::four) {
      selected = static_cast<unsigned int>(input) - static_cast<unsigned int>(front_key::one);
      choose_game();
      ignored_character = 49 + selected;
    } else if(input == front_key::accept) choose_game();
  } else if(current == screen::run) {
    if(input == front_key::accept) begin_briefing();
    if(input == front_key::select) current = screen::games;
    if(input == front_key::erase) {
      previous = current;
      current = screen::erase;
      confirmation = false;
    }
  }
}

void front_end::character(unsigned int const code) {
  /// Accept the original font's ordinary printable name characters with a bounded field
  if(code == ignored_character) {
    ignored_character = 0;
    return;
  }
  ignored_character = 0;
  if(current == screen::games && code >= 32 && code <= 126) star_prefix = code == '*';
  if(editing_text() && code >= 32 && code <= 126 && draft_name.size() < 20) draft_name += static_cast<char>(code);
}

void front_end::point(int const x, int const y) noexcept {
  /// Retain framebuffer mouse coordinates for native presentation navigation highlighting
  pointer = {x, y};
}

void front_end::click(int const x, int const y) {
  /// Selection rows follow the original 36-pixel pitch; other screens expose their visible actions
  if(current == screen::games && x >= 48 && x < 272 && y >= 15 && y < 189 && (y - 15) % 36 < 30) {
    selected = static_cast<unsigned int>((y - 15) / 36);
    choose_game();
  } else if(current == screen::run && x >= 70 && x < 250 && y >= 160 && y < 224) {
    key(std::array{front_key::accept, front_key::select, front_key::erase, front_key::quit}[static_cast<size_t>((y - 160) / 16)]);
  } else if(current == screen::quit || current == screen::erase) {
    int const answer_y{current == screen::erase ? 216 : 168};
    if(y >= answer_y && y < answer_y + 15) {
      confirmation = x < 162;
      key(front_key::accept);
    }
  } else if((current == screen::briefing || current == screen::outcome) && y >= 225 && x >= 284 && x < 302) {
    if(scene->input_policy & 4) key(front_key::back);
  } else if(current == screen::introduction || current == screen::title || current == screen::credits || current == screen::briefing || current == screen::outcome) key(front_key::accept);
}

void front_end::advance(uint32_t const elapsed_ticks) {
  /// Menu pauses do not consume the flight clock; animation continues only in active presentation scenes
  if(current == screen::title || current == screen::credits) {
    auto const limit{current == screen::title ? uint32_t{5024} : uint32_t{128}};
    title_ticks = static_cast<uint32_t>(std::min(uint64_t{limit}, uint64_t{title_ticks}+ elapsed_ticks));
    if(current == screen::title && title_ticks == limit) {
      current = screen::credits;
      title_ticks = 0;
    }
    return;
  }
  if(current != screen::introduction && current != screen::briefing && current != screen::outcome) return;
  scene->advance(elapsed_ticks);
  if(current == screen::introduction && scene->finished()) current = screen::title;
  if(current == screen::briefing && scene->finished() && scene->input_policy == 0) finish_briefing();
}

resources::pilot_record &front_end::selected_pilot() noexcept {
  /// The selected slot owns the last committed campaign state
  return nightmare_selected() ? challenge_pilot : save.pilots[selected];
}

void front_end::start_level(uint8_t const stage) {
  /// Rebuild guaranteed campaign setup changes without inventing completed combat
  if(stage < 1 || stage > 116) throw std::out_of_range{"Level must be between 1 and 116"};
  if(nightmare_selected()) selected = 0;
  resources::pilot_record pilot{
    .stage{stage}
  };
  pilot.set_name("Level test");
  std::array<game::city_map, 2> cities{
    game::make_city_map(archives.load({0, 68}), true),
    game::make_city_map(archives.load({0, 69}), false),
  };
  for(uint8_t previous{1}; previous < stage; ++previous) {
    auto const &scenario{campaign.scenario(previous)};
    auto const index{resources::select_campaign_stage(previous).record};
    auto const &record{scenario.records()[index]};
    auto const configuration{record.configuration & 15};
    if(record.configuration != 0xff && configuration < 4) {
      auto &city{cities[configuration == 2 || configuration == 3 ? 1 : 0]};
      // retain authored setup flags (including radio outages), never infer destruction from objectives
      game::apply_scenario_cells(city, record);
      game::commit_beacon_queue(city, scenario.bytes(record.beacon_sequence));
    }
    player briefing{archives, font, scenario, index};
    for(unsigned int step{0}; !briefing.finished(); ++step) {
      if(step == 4096) throw std::runtime_error{"Level setup briefing failed to terminate"};
      briefing.advance(2000);
      briefing.continue_page();
    }
    pilot.weapons ^= briefing.weapon_toggles;
    // tunnel portal coordinates belong to their own map; completion retains the saved surface hangar
    if(record.configuration != 0xff && configuration < 4 && briefing.departure_destination) {
      pilot.return_site = briefing.departure_destination;
    }
  }
  // debug entry assumes the upgraded craft has completed its first pad service
  if(stage >= 101) pilot.weapons |= 4;
  for(unsigned int city{0}; city < cities.size(); ++city) {
    resources::geometry_bank const bank{archives.load({0, 30 + city})};
    game::pack_city_state(cities[city], bank.city_types(), city == 0 ? std::span<std::byte>{pilot.delphi} : std::span<std::byte>{pilot.halon});
  }
  selected_pilot() = pilot;
  save_requested = false;
  begin_briefing();
}

void front_end::previous_level() {
  /// Return to the preceding playable record rather than replaying an interlude into the same flight
  if(nightmare_selected()) return;
  auto stage{selected_pilot().stage};
  do {
    if(stage <= 1) break;
    --stage;
  } while(campaign.scenario(stage).records()[resources::select_campaign_stage(stage).record].configuration == 0xff);
  start_level(stage);
}

void front_end::continue_campaign() {
  /// Successful progression enters the next briefing directly when its runtime is supported
  current = screen::run;
  begin_briefing(true);
}

void front_end::show_death(uint8_t const completed_objects) {
  /// 3F15 forwards outcome 2 to D8D6, selecting the Kismet committal presentation in 04/15
  show_outcome(2, completed_objects);
}

void front_end::show_abort(uint8_t const completed_objects) {
  /// 7D63 selects outcome 3 when automatic return finishes with outstanding objectives
  show_outcome(3, completed_objects);
}

void front_end::show_outcome(uint8_t const outcome, uint8_t const completed_objects) {
  /// Failure presentations leave the committed pilot untouched for the next attempt
  retained_music = -1;
  scene = std::make_unique<player>(archives, font, introduction, outcome, completed_objects, false, language);
  current = screen::outcome;
}

void front_end::return_to_menu() {
  /// Retain saved pilot slots while leaving the finished or abandoned session
  current = screen::run;
}

void front_end::draw(framework::render::cockpit_framebuffer &output) const {
  /// Original menu imagery and interface font stay inside the indexed software-rendering path
  if(current == screen::introduction || current == screen::briefing || current == screen::outcome) {
    scene->draw(output, pointer);
    return;
  }
  if(current == screen::title) {
    framework::render::expand_palette(title_background, graphics::fade_palette(title_palette.colours, static_cast<uint16_t>(std::min(uint32_t{256}, title_ticks / 4))), output);
    return;
  }
  if(current == screen::credits) {
    framework::render::expand_palette(credits_background, graphics::fade_palette(menu_palette.colours, static_cast<uint16_t>(title_ticks * 2)), output);
    return;
  }
  auto const &labels{original_menu_text.at(static_cast<size_t>(language))};
  auto frame{menu_background};
  auto const text{[&](std::string_view const value, int const x, int const y, uint8_t const colour = 125){
    graphics::draw_text(frame, font, resources::font_face::interface, std::as_bytes(std::span{value}),
      {static_cast<int16_t>(x), static_cast<int16_t>(y)}, {
        .ink{colour},
        .edge{0}
      });
  }};
  auto const centred{[&](std::string_view const value, int const y, uint8_t const colour){
    unsigned int width{0};
    for(auto const code : value) width += code == ' ' ? 4 : font.glyph(resources::font_face::interface, static_cast<uint8_t>(code)).width;
    text(value, (framework::render::display_layout::width - static_cast<int>(width)) / 2, y, colour);
  }};
  auto const panel{[&](int const x, int const y, int const width, int const height){
    // 08E9 copies the menu rectangle through VGA XOR with bit mask 80
    for(int row{y}; row < y + height; ++row) {
      for(int column{x}; column < x + width; ++column) {
        frame.pixels[static_cast<size_t>(row * framework::render::display_layout::width + column)] ^= 128;
      }
    }
  }};
  auto const game_row{[&](unsigned int const slot, int const y, uint8_t const colour){
    panel(48, y - 5, 224, 30);
    if(slot == 4) {
      text("NIGHTMARE", 112, y, colour);
      graphics::text_cursor cursor{
        .x{52},
        .y{static_cast<uint16_t>(y + 11)},
        .colour{static_cast<uint16_t>(colour << 8)},
        .runtime_number{challenge_score}
      };
      for(auto const value : {labels.score, labels.best_score}) {
        std::string const bytes{std::string{value}+ '\0'};
        auto const page{graphics::lay_out_text(std::as_bytes(std::span{bytes}), font, resources::font_face::interface, cursor)};
        for(auto const &glyph : page.glyphs) graphics::draw_glyph(frame, font, resources::font_face::interface, glyph.code, glyph.position, {
          .ink{colour},
          .edge{0}
        });
        cursor = page.cursor;
        cursor.runtime_number = std::to_integer<uint8_t>(save.trailer[0]);
      }
    } else {
      text(std::string{labels.game}+ std::to_string(slot + 1), 112, y, colour);
      auto const &pilot{save.pilots[slot]};
      if(pilot.stage) {
        text(std::string{labels.level}+ std::to_string(pilot.stage), 162, y, colour);
        centred(pilot.display_name(), y + 11, colour);
      } else centred(labels.new_game, y + 10, colour);
    }
  }};
  if(current == screen::games) {
    for(unsigned int i{0}; i < 5; ++i) {
      auto const top{15 + static_cast<int>(i) * 36};
      bool const hover{pointer[0] >= 48 && pointer[0] < 272 && pointer[1] >= top && pointer[1] < top + 30};
      game_row(i, top + 5, hover ? 126 : 125);
    }
    panel(54, 217, 212, 18);
    centred(selection_prompt, 222, 125);
  } else if(editing_text()) {
    bool const hidden{current == screen::hidden_command};
    panel(48, hidden ? 91 : 171, 224, 54);
    text(hidden ? "STAR THREE" : labels.name_title.text, hidden ? 122 : labels.name_title.x, hidden ? 96 : 176);
    text(hidden ? "What do you want?" : labels.name_prompt.text, hidden ? 102 : labels.name_prompt.x, hidden ? 112 : 192);
    text(draft_name + "_", 56, hidden ? 128 : 208, 126);
  } else if(current == screen::run) {
    game_row(selected, 60, 125);
    panel(70, 155, 180, 70);
    if(unsupported_stage) text("This stage is not implemented yet", 32, 120);
    auto const action{[&](std::string_view const label, int const x, int const y){
      bool const hover{pointer[0] >= 70 && pointer[0] < 250 && pointer[1] >= y && pointer[1] < y + 16};
      text(label, x, y, hover ? 126 : 125);
    }};
    for(size_t i{0}; i < labels.actions.size(); ++i) action(labels.actions[i].text, labels.actions[i].x, 160 + static_cast<int>(i) * 16);
  } else {
    game_row(selected, 60, 125);
    if(current == screen::erase) {
      panel(70, 147, 180, 86);
      auto const &lines{nightmare_selected() ? labels.erase_score : labels.erase};
      for(size_t i{0}; i < lines.size(); ++i) text(lines[i].text, lines[i].x, 152 + static_cast<int>(i) * 16);
    } else {
      panel(60, 83, 200, 102);
      text(labels.quit.text, labels.quit.x, 136);
    }
    int const question_y{current == screen::erase ? 200 : 152};
    centred(labels.question, question_y, 125);
    text(labels.yes, 133, question_y + 16, confirmation ? 126 : 125);
    text("/", 157, question_y + 16, 125);
    text(labels.no, 169, question_y + 16, confirmation ? 125 : 126);
  }
  framework::render::expand_palette(frame, menu_palette.colours, output);
}

} // namespace darker::presentation

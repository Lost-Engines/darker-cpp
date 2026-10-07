#include "presentation/front_end.h"
#include <stdexcept>
#include "graphics/font.h"

namespace darker::presentation {
namespace {

void load_image(resources::archive_set const &archives, resources::resource_id const id,
  framework::render::indexed_cockpit_framebuffer &frame, graphics::palette_state &palette, unsigned int const width,
  unsigned int const height, unsigned int const x, unsigned int const y) {
  /// Load original menu/title pixels with their palette, leaving the remaining page black
  auto const bytes{archives.load(id)};
  auto const decoded{graphics::decode_palette(bytes,palette)};
  palette = decoded.palette;
  if(bytes.size() - decoded.bytes_consumed != width * height) throw std::invalid_argument{"Unexpected front-end image size"};
  frame.pixels.fill(0);
  for(size_t row{0}; row < height; ++row) for(size_t column{0}; column < width; ++column) {
    frame.pixels[(y + row) * 320 + x + column] = std::to_integer<uint8_t>(bytes[decoded.bytes_consumed + row * width + column]);
  }
}

} // namespace

front_end::front_end(resources::archive_set const &archives, resources::font_resource const &font, resources::campaign_resources &campaign, resources::save_file &save, bool const skip_intro)
  : archives{archives}, font{font}, campaign{campaign}, introduction{archives.load({4,15})}, save{save} {
  /// Startup 9CBD selects record one before the 00/14 title and 00/21 game-selection background
  load_image(archives,{0,21},menu_background,menu_palette,320,240,0,0);
  load_image(archives,{0,14},title_background,title_palette,280,100,16,65);
  if(skip_intro) current = screen::games;
  else scene = std::make_unique<player>(archives,font,introduction,1);
}

bool front_end::active() const noexcept {
  /// Flight input remains unavailable until the final briefing scene is dismissed
  return current != screen::flight;
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
  current = save.pilots[selected].stage == 0 ? screen::name : screen::run;
}

void front_end::begin_briefing() {
  /// Select the current supported campaign record for briefing
  if(save.pilots[selected].stage < 1 || save.pilots[selected].stage > 26) { unsupported_stage = true; return; }
  retained_music = music_group();
  scene = std::make_unique<player>(archives,font,campaign.scenario(save.pilots[selected].stage),resources::select_campaign_stage(save.pilots[selected].stage).record);
  current = screen::briefing;
}

void front_end::key(front_key const input) {
  /// Route menu commands separately from in-flight bindings
  if(current == screen::introduction) { current = screen::title; return; }
  if(current == screen::title) { current = screen::games; return; }
  if(current == screen::outcome) {
    if((input == front_key::back || input == front_key::accept) && (scene->input_policy & 4)) current = screen::run;
    return;
  }
  if(current == screen::briefing) {
    if(input == front_key::back && (scene->input_policy & 4)) { current = screen::run; return; }
    if(input == front_key::accept && (scene->input_policy & 1) && !scene->continue_page()) current = screen::flight;
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
    if(input == front_key::back) { draft_name.clear(); current = screen::games; }
    return;
  }
  if(current == screen::erase || current == screen::quit) {
    if(input == front_key::up || input == front_key::down) confirmation = !confirmation;
    if(input == front_key::back) current = previous;
    else if(input == front_key::accept) {
      if(confirmation && current == screen::quit) quit_requested = true;
      if(confirmation && current == screen::erase) {
        save.pilots[selected] = {};
        save_requested = true;
        unsupported_stage = false;
      }
      current = save.pilots[selected].stage == 0 ? screen::games : previous;
    }
    return;
  }
  if(input == front_key::quit || input == front_key::back) { previous = current; current = screen::quit; confirmation = false; return; }
  if(current == screen::games) {
    if(input == front_key::three && star_prefix) {
      star_prefix = false;
      draft_name.clear();
      ignored_character = '3';
      current = screen::hidden_command;
      return;
    }
    if(input == front_key::up) selected = (selected + 3) % 4;
    if(input == front_key::down) selected = (selected + 1) % 4;
    if(input >= front_key::one && input <= front_key::four) {
      selected = static_cast<unsigned int>(input) - static_cast<unsigned int>(front_key::one);
      choose_game();
      ignored_character = 49 + selected;
    } else if(input == front_key::accept) choose_game();
  } else if(current == screen::run) {
    if(input == front_key::accept) begin_briefing();
    if(input == front_key::select) current = screen::games;
    if(input == front_key::erase) { previous = current; current = screen::erase; confirmation = false; }
  }
}

void front_end::character(unsigned int const code) {
  /// Accept the original font's ordinary printable name characters with a bounded field
  if(code == ignored_character) { ignored_character = 0; return; }
  ignored_character = 0;
  if(current == screen::games && code >= 32 && code <= 126) star_prefix = code == '*';
  if(editing_text() && code >= 32 && code <= 126 && draft_name.size() < 20) draft_name += static_cast<char>(code);
}

void front_end::click(int const x, int const y) {
  /// Selection rows follow the original 36-pixel pitch; other screens expose their visible actions
  if(current == screen::games && x >= 15 && x < 305 && y >= 48 && y < 192) {
    selected = static_cast<unsigned int>((y - 48) / 36);
    choose_game();
  } else if(current == screen::run && y >= 152 && y < 216) {
    key(std::array{front_key::accept,front_key::select,front_key::erase,front_key::quit}[static_cast<size_t>((y - 152) / 16)]);
  } else if(current == screen::quit || current == screen::erase) {
    if(y >= 150 && y < 190) { confirmation = x < 160; key(front_key::accept); }
  } else if((current == screen::briefing || current == screen::outcome) && y >= 225 && x >= 284 && x < 302) {
    if(scene->input_policy & 4) key(front_key::back);
  } else if(current == screen::introduction || current == screen::title || current == screen::briefing || current == screen::outcome) key(front_key::accept);
}

void front_end::advance(uint32_t const elapsed_ticks) {
  /// Menu pauses do not consume the flight clock; animation continues only in active presentation scenes
  if(current != screen::introduction && current != screen::briefing && current != screen::outcome) return;
  scene->advance(elapsed_ticks);
  if(current == screen::introduction && scene->finished()) current = screen::title;
  if(current == screen::briefing && scene->finished() && scene->input_policy == 0) current = screen::flight;
}

resources::pilot_record &front_end::selected_pilot() noexcept {
  /// The selected slot owns the last committed campaign state
  return save.pilots[selected];
}

void front_end::continue_campaign() {
  /// Successful progression enters the next briefing directly when its runtime is supported
  current = screen::run;
  begin_briefing();
}

void front_end::show_death(uint8_t const completed_objects) {
  /// 3F15 forwards outcome 2 to D8D6, selecting the Kismet committal presentation in 04/15
  retained_music = -1;
  scene = std::make_unique<player>(archives,font,introduction,2,completed_objects);
  current = screen::outcome;
}

void front_end::return_to_menu() {
  /// Retain saved pilot slots while leaving the finished or abandoned session
  current = screen::run;
}

void front_end::draw(framework::render::cockpit_framebuffer &output) const {
  /// Original menu imagery and interface font stay inside the indexed software-rendering path
  if(current == screen::introduction || current == screen::briefing || current == screen::outcome) { scene->draw(output); return; }
  if(current == screen::title) { framework::render::expand_palette(title_background,title_palette.colours,output); return; }
  auto frame{menu_background};
  auto const text{[&](std::string const &value, int const x, int const y, uint8_t const colour = 125){
    graphics::draw_text(frame,font,resources::font_face::interface,std::as_bytes(std::span{value}),
      {.x{static_cast<int16_t>(x)},.y{static_cast<int16_t>(y)}},{.ink{colour},.edge{0}});
  }};
  if(current == screen::games) {
    for(unsigned int i{0}; i < 4; ++i) {
      auto const colour{static_cast<uint8_t>(i == selected ? 126 : 125)};
      text("Game " + std::to_string(i + 1),48,48 + static_cast<int>(i) * 36,colour);
      text(save.pilots[i].stage == 0 ? "Start a new game" : save.pilots[i].display_name(),48,62 + static_cast<int>(i) * 36,colour);
    }
    text(selection_prompt,48,222);
  } else if(editing_text()) {
    bool const hidden{current == screen::hidden_command};
    text(hidden ? "STAR THREE" : "START NEW GAME",hidden ? 104 : 88,176);
    text(hidden ? "What do you want?" : "Please enter your name",hidden ? 84 : 65,192);
    text(draft_name + "_",70,208,126);
  } else if(current == screen::run) {
    text(save.pilots[selected].display_name(),48,64,126);
    text("Game " + std::to_string(selected + 1) + "     Level " + std::to_string(save.pilots[selected].stage),48,88);
    if(unsupported_stage) text("This stage is not implemented yet",32,120);
    text("ENTER: Run this game",76,160);
    text("S: Select a different game",76,176);
    text("E: Erase this game",76,192);
    text("ESC: Quit to DOS",76,208);
  } else {
    text(current == screen::erase ? "ERASE GAME" : "QUIT TO DOS",96,96);
    text("Do you wish to proceed?",64,128);
    text("YES",100,160,confirmation ? 126 : 125);
    text("NO",192,160,confirmation ? 125 : 126);
  }
  framework::render::expand_palette(frame,menu_palette.colours,output);
}

} // namespace darker::presentation

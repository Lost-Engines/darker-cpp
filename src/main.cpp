#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <boost/program_options.hpp>
#include <boost/scope/scope_exit.hpp>
#include <GLFW/glfw3.h>
#include "audio/ambient_sounds.h"
#include "audio/flight_sounds.h"
#include "audio/fm_stream.h"
#include "audio/world_sounds.h"
#include "game/actor_activation.h"
#include "game/beacon_changes.h"
#include "game/beacon_light.h"
#include "game/camera_target.h"
#include "game/city_map.h"
#include "game/city_persistence.h"
#include "game/flight_camera.h"
#include "game/game_clock.h"
#include "game/hangar.h"
#include "game/mission_combat.h"
#include "game/mission_exchange.h"
#include "game/player_flight.h"
#include "game/scenario_world.h"
#include "game/scenario_setup.h"
#include "game/tunnel_portal.h"
#include "graphics/bitmap_hud.h"
#include "graphics/city_scene.h"
#include "graphics/cockpit.h"
#include "graphics/flight_instruments.h"
#include "graphics/font.h"
#include "graphics/formatted_text.h"
#include "graphics/navigation_hud.h"
#include "graphics/palette_bitmap.h"
#include "graphics/procedural_hud.h"
#include "graphics/radar_beacons.h"
#include "graphics/sky_ground.h"
#include "maths/direction.h"
#include "maths/sine_table.h"
#include "platform/audio_output.h"
#include "platform/framebuffer_presenter.h"
#include "presentation/front_end.h"
#include "render/framebuffer.h"
#include "resources/archive_set.h"
#include "resources/geometry_bank.h"
#include "resources/save_file.h"

namespace {

auto default_game_directory()->std::filesystem::path {
  if(auto const *data{std::getenv("XDG_DATA_HOME")}; data && *data && std::filesystem::path{data}.is_absolute())
    return std::filesystem::path{data} / "darker";
  auto const *home{std::getenv("HOME")};
  return std::filesystem::path{home && *home ? home : "."} / ".local/share/darker";
}

int startup_failure(std::string_view const message) {
  /// Report expected launch failures without converting runtime programming errors into normal exits
  std::cerr << "ERROR: " << message << std::endl;
  return EXIT_FAILURE;
}

enum class session_exit { none, menu, death, aborted, completed, previous };

struct flight_host {
  darker::game::player_flight player{};
  darker::audio::flight_sounds sounds;
  darker::audio::world_sounds world_audio;
  darker::audio::ambient_sounds ambient_audio;
  darker::game::flight_camera camera;
  bool pick_camera{false};
  darker::game::hangar_state hangar;
  darker::game::object_pose audio_listener, audio_motion;
  darker::game::mission_combat *combat{nullptr};
  uint16_t available_weapons{0};
  uint8_t score_base{0};
  bool primary_held{false};
  bool secondary_held{false};
  bool weapon_selection_blocked{false};
  bool briefing{false};
  darker::presentation::front_end *front{nullptr};
  bool shield_ready{false};
  uint8_t engine_indicator{0};
  bool gouraud{true};
  session_exit exit_requested{session_exit::none};
  std::uint16_t clock{0};
  std::uint16_t shield_deadline{0};
  bool mouse_started{false};
  bool paused{false};
  bool single_step{false};
  bool freeze_enabled{false};
  bool mouse_enabled{true};
  int resume_key{GLFW_KEY_UNKNOWN};

  bool key_down(GLFWwindow &window, int const key) const {
    /// A resume key remains consumed through repeats and held-key polling until its release
    return !paused && key != resume_key && glfwGetKey(&window,key) == GLFW_PRESS;
  }

  int cursor_mode(bool const captured) const noexcept {
    /// Keyboard-only debugging hides the pointer without grabbing it from the desktop
    return mouse_enabled ? (captured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL) : GLFW_CURSOR_HIDDEN;
  }

  double mouse_origin_x{0};
  double mouse_origin_y{0};

  darker::game::flight_controls_input input(GLFWwindow &window) {
    /// Supply wrapping relative mouse counters and held steering keys to the original control filter
    if(paused) return {.mouse_x{player.controls.bank.previous_mouse},.mouse_y{player.controls.pitch.previous_mouse}};
    double x{0}, y{0};
    if(mouse_enabled) glfwGetCursorPos(&window, &x, &y);
    if(!mouse_started) {
      mouse_origin_x = x;
      mouse_origin_y = y;
      mouse_started = true;
    }
    auto const down{[&](int const key){ return key_down(window,key); }};
    return {
      .left{down(GLFW_KEY_LEFT)}, .right{down(GLFW_KEY_RIGHT)}, .up{down(GLFW_KEY_UP)}, .down{down(GLFW_KEY_DOWN)},
      .control{down(GLFW_KEY_LEFT_CONTROL) || down(GLFW_KEY_RIGHT_CONTROL)},
      .look_around{down(GLFW_KEY_TAB)},
      .mouse_x{static_cast<std::uint16_t>(static_cast<std::int64_t>(x - mouse_origin_x))},
      .mouse_y{static_cast<std::uint16_t>(static_cast<std::int64_t>(mouse_origin_y - y))},
    };
  }
};

} // namespace

auto main(int const argc, char const *const argv[])->int {
  /// Run the reconstructed city flight path while scenario, actors and remaining presentation systems are recovered
  boost::program_options::options_description options{"Darker"};
  options.add_options()
    ("help,h", "show usage")
    ("data-dir", boost::program_options::value<std::string>(), "game directory: packs and DARKER.SAV (default: local installation, then user data directory)")
    ("language", boost::program_options::value<std::string>()->default_value("english"), "original text language: english, french or german")
    ("mute", "disable PCM sound output")
    ("no-mouse", "ignore all mouse input and hide the pointer; retain keyboard controls")
    ("noclip", "disable player collisions and tunnel guidance; provide unlimited flight power")
    ("cheat-lyndon", "enable Z to freeze or release player motion while the world continues")
    ("cheat-brooke", "enable the original accelerated Caero boost recharge cheat")
    ("cheat-life", "enable the original impact-damage cheat; scenery crashes remain lethal")
    ("cheat-level-x", "enable X to advance; Shift+X starts the previous playable level without saving")
    ("level", boost::program_options::value<int>(), "start at campaign level 1..116 with accumulated setup changes, without assumed combat damage; do not write saves")
    ("skip-intro", "start at game selection, skipping the startup presentation and title")
    ("scale", boost::program_options::value<int>()->default_value(4), "initial window scale: positive integer multiple of 320 x 240")
    ("music", boost::program_options::value<std::string>()->default_value("soundblaster_fm"), "music arrangement: none, soundblaster_fm, midi, roland-lapc, roland-sc55, roland-scc1a, gravis or soundblaster_awe32")
    ("roland-gm-bank", boost::program_options::value<std::string>(), "load the whole Roland MTGM.MID bank before Darker custom instruments on the same device")
    ("roland-gm-percussion-bank", boost::program_options::value<std::string>(), "supplement unmapped Roland percussion using Roland MTGM.MID on a separate emulated device")
    ("roland-gm-percussion-fallback", "supplement unmapped Roland percussion with General MIDI SoundFont sounds")
    ("gus-ram", boost::program_options::value<unsigned int>()->default_value(1024), "Gravis RAM in KiB: 256, 512, 768 or 1024")
    ("gus-dir", boost::program_options::value<std::string>(), "Gravis UltraSound directory (default: ULTRASND in game directory)")
    ("awe32-rom", boost::program_options::value<std::string>(), "AWE32 sample ROM (default: awe32.raw in game directory)")
    ("scc1a-rom-dir", boost::program_options::value<std::string>(), "SCC-1A v1.30 ROM directory (default: game directory)")
    ("sc55-rom-dir", boost::program_options::value<std::string>(), "SC-55 v1.21 ROM directory (default: game directory)")
    ("mt32-rom-dir", boost::program_options::value<std::string>(), "Roland ROM directory (default: game directory for --music=roland-lapc)")
    ("soundfont", boost::program_options::value<std::string>(), "SoundFont (.sf2) for sampled arrangements; otherwise search working directory and system fonts")
    ("opl", boost::program_options::value<std::string>()->default_value("dosbox"), "FM synthesis: dosbox (default, 44100 Hz) or nuked")
    ("craft", boost::program_options::value<std::string>()->default_value("caero"), "caero, skimma or upgraded; selects the corresponding city")
    ("seconds", boost::program_options::value<double>()->default_value(0.0), "close after this many seconds; zero waits")
    ("output", boost::program_options::value<std::string>(), "write RGB PPM without opening a window");
  boost::program_options::variables_map arguments;
  try {
    boost::program_options::store(boost::program_options::parse_command_line(argc, argv, options), arguments);
    if(arguments.contains("help")) {
      std::cout << options << std::endl;
      return EXIT_SUCCESS;
    }
    boost::program_options::notify(arguments);
  } catch(boost::program_options::error const &error) {
    return startup_failure(error.what());
  }
  auto const language_name{arguments["language"].as<std::string>()};
  if(language_name != "english" && language_name != "french" && language_name != "german") return startup_failure("--language must be english, french or german");
  auto const language{language_name == "english" ? darker::resources::scenario_language::english
    : language_name == "french" ? darker::resources::scenario_language::french : darker::resources::scenario_language::german};
  auto const scale{arguments["scale"].as<int>()};
  auto const music_name{arguments["music"].as<std::string>()};
  if(music_name == "roland") return startup_failure("--music=roland is ambiguous; choose --music=roland-lapc (MT-32/CM-32L family), --music=roland-sc55 or --music=roland-scc1a (Sound Canvas)");
  std::array<std::string_view,5> constexpr music_names{"soundblaster_fm", "midi", "roland-lapc", "gravis", "soundblaster_awe32"};
  auto const music_position{std::find(music_names.begin(), music_names.end(), (music_name == "roland-sc55" || music_name == "roland-scc1a") ? "midi" : music_name)};
  if(music_name != "none" && music_position == music_names.end()) return startup_failure("--music must be none, soundblaster_fm, midi, roland-lapc, roland-sc55, roland-scc1a, gravis or soundblaster_awe32");
  auto const music_variant{music_name == "none" ? darker::audio::music_variant::soundblaster : static_cast<darker::audio::music_variant>(music_position - music_names.begin())};
  if(arguments.contains("mt32-rom-dir") && music_variant != darker::audio::music_variant::lapc1) return startup_failure("--mt32-rom-dir requires --music=roland-lapc");
  bool const full_roland_bank{arguments.contains("roland-gm-bank")};
  bool const percussion_fallback{arguments.contains("roland-gm-percussion-fallback")};
  bool const percussion_bank_enabled{arguments.contains("roland-gm-percussion-bank")};
  if(full_roland_bank && (music_name != "roland-lapc" || percussion_fallback || percussion_bank_enabled || arguments.contains("soundfont"))) return startup_failure("--roland-gm-bank requires --music=roland-lapc without --soundfont or percussion fallback options");
  if(percussion_bank_enabled && (music_name != "roland-lapc" || percussion_fallback || arguments.contains("soundfont"))) return startup_failure("--roland-gm-percussion-bank requires --music=roland-lapc without --soundfont or --roland-gm-percussion-fallback");
  if(percussion_fallback && music_name != "roland-lapc") return startup_failure("--roland-gm-percussion-fallback requires --music=roland-lapc");
  if(arguments.contains("mt32-rom-dir") && arguments.contains("soundfont") && !percussion_fallback) return startup_failure("Choose either --mt32-rom-dir or --soundfont for LAPC-I playback");
  auto const gus_ram{arguments["gus-ram"].as<unsigned int>()};
  if(gus_ram < 256 || gus_ram > 1024 || gus_ram % 256) return startup_failure("--gus-ram must be 256, 512, 768 or 1024 (KiB)");
  if(!arguments["gus-ram"].defaulted() && (music_name != "gravis" || arguments.contains("soundfont"))) return startup_failure("--gus-ram requires --music=gravis without --soundfont");
  if(arguments.contains("gus-dir") && (music_name != "gravis" || arguments.contains("soundfont"))) return startup_failure("--gus-dir requires --music=gravis without --soundfont");
  if(arguments.contains("awe32-rom") && (music_name != "soundblaster_awe32" || arguments.contains("soundfont"))) return startup_failure("--awe32-rom requires --music=soundblaster_awe32 without --soundfont");
  if(arguments.contains("sc55-rom-dir") && music_name != "roland-sc55") return startup_failure("--sc55-rom-dir requires --music=roland-sc55");
  if(arguments.contains("scc1a-rom-dir") && music_name != "roland-scc1a") return startup_failure("--scc1a-rom-dir requires --music=roland-scc1a");
  if((music_name == "roland-sc55" || music_name == "roland-scc1a") && arguments.contains("soundfont")) return startup_failure("Sound Canvas emulation uses ROMs, not --soundfont");
  auto const opl_name{arguments["opl"].as<std::string>()};
  if(opl_name != "nuked" && opl_name != "dosbox") return startup_failure("--opl must be nuked or dosbox");
  constexpr int display_width{framework::render::cockpit_framebuffer::width};
  constexpr int display_height{framework::render::cockpit_framebuffer::height};
  if(scale < 1 || scale > std::numeric_limits<int>::max() / std::max(display_width,display_height)) {
    return startup_failure("--scale must be a positive integer whose window dimensions fit in an int");
  }
  if(arguments.contains("level") && (arguments["level"].as<int>() < 1 || arguments["level"].as<int>() > 116)) return startup_failure("--level must be between 1 and 116");
  bool debug_session{arguments.contains("level")};
  auto const name{arguments["craft"].as<std::string>()};
  if(name != "caero" && name != "skimma" && name != "upgraded") return startup_failure("unknown --craft");
  auto type{name == "caero" ? darker::graphics::craft::caero : name == "skimma" ? darker::graphics::craft::skimma : darker::graphics::craft::upgraded_skimma};
  if(debug_session && name != "caero") return startup_failure("--level selects its own craft; omit --craft");
  bool caero{type == darker::graphics::craft::caero};
  auto const seconds{arguments["seconds"].as<double>()};
  if(!std::isfinite(seconds) || seconds < 0) return startup_failure("--seconds must be finite and non-negative");
  auto const user_directory{default_game_directory()};
  auto const data_directory{arguments.contains("data-dir") ? std::filesystem::path{arguments["data-dir"].as<std::string>()}
    : std::filesystem::is_regular_file("DARKER.00") || std::filesystem::is_regular_file("darker.00")
      ? std::filesystem::path{"."} : user_directory};
  std::optional<darker::resources::archive_set> loaded_archives;
  try {
    loaded_archives.emplace(data_directory);
  } catch(std::runtime_error const &error) {
    return startup_failure(error.what());
  }
  auto const &archives{*loaded_archives};
  unsigned int const slot{caero ? 16u : type == darker::graphics::craft::skimma ? 17u : 18u};
  auto bitmap{darker::graphics::decode_bitmap(archives.load({.archive{0}, .slot{slot}}))};
  auto cache{darker::graphics::make_cockpit_cache(bitmap.image)};
  uint8_t world_mode{static_cast<uint8_t>(caero ? 0 : 1)};
  auto game_palette{bitmap.palette};
  auto cockpit{cache};
  darker::resources::geometry_bank bank{archives.load({.archive{0}, .slot{caero ? 30u : 31u}})};
  std::optional<darker::game::tunnel_network> tunnel_network;
  auto cells{darker::game::make_city_map(archives.load({.archive{0}, .slot{caero ? 68u : 69u}}), caero)};
  std::array<std::uint8_t, 256> variant_limits{};
  for(std::size_t i{0}; i < bank.city_types().size(); ++i) variant_limits[i + 1] = bank.city_types()[i].variant_limit;
  darker::game::assign_city_variants(cells, variant_limits);
  flight_host host;
  host.freeze_enabled = arguments.contains("cheat-lyndon");
  host.mouse_enabled = !arguments.contains("no-mouse");
  bool const noclip{arguments.contains("noclip")};
  bool const boost_cheat{arguments.contains("cheat-brooke")};
  bool const damage_cheat{arguments.contains("cheat-life")};
  if(caero) {
    darker::game::initialise_caero_hangar(host.player, cells, host.hangar, bank.header_at(bank.special_models()[25]).height);
  } else {
    host.player.craft = darker::game::skimma_flight_state{
      .pose{.position{12672, 14976, 1536}, .speed{500}}, .damage{.shield_charge{0xbfff}}, .horizontal_velocity{500},
    };
    host.player.upgraded = type == darker::graphics::craft::upgraded_skimma;
    host.player.engine_flags = 0;
  }
  host.player.noclip = noclip;
  host.player.boost_cheat = boost_cheat;
  host.player.damage_cheat = damage_cheat;
  darker::resources::campaign_resources campaign{archives};
  auto const *scenario{&campaign.scenario(1)};
  auto mission{scenario->records().front()};
  darker::game::beacon_changes beacon_changes;
  darker::game::world_objectives objectives{.list{mission.objective_cell_list}};
  darker::resources::font_resource const font{archives.load({.archive{0}, .slot{29}})};
  auto text{scenario->language(0, language)};
  std::unique_ptr<darker::presentation::front_end> front;
  auto const save_path{data_directory / "DARKER.SAV"};
  auto const previous_save{std::filesystem::exists(save_path) ? save_path : std::filesystem::path{"darker-cpp.sav"}};
  darker::resources::save_file saves;
  try {
    if(caero && std::filesystem::exists(previous_save)) saves = darker::resources::decode_save(darker::resources::read_binary_file(previous_save,darker::resources::save_file_size));
  } catch(std::runtime_error const &error) {
    return startup_failure(error.what());
  } catch(std::invalid_argument const &error) {
    return startup_failure(error.what());
  }
  if(caero) {
    front = std::make_unique<darker::presentation::front_end>(archives,font,campaign,saves,arguments.contains("skip-intro"),language);
    if(arguments.contains("cheat-level-x")) front->enable_level_skip();
    if(debug_session) front->start_level(static_cast<uint8_t>(arguments["level"].as<int>()));
    host.front = front.get();
  }
  darker::game::mission_script initial_script{
    .continuation{*mission.player_program - mission.shared.offset}, .checkpoint{*mission.player_program - mission.shared.offset},
  };
  auto script{initial_script};
  darker::game::mission_exchange exchange;
  darker::game::mission_context context{.program{scenario->bytes(mission.shared)}, .text{text}, .cells{cells}, .time_multiplier{mission.time_multiplier}, .text_cursor{0}};
  std::array<std::optional<darker::game::mission_message>,3> messages;
  host.briefing = front != nullptr;
  auto initial_actors{caero ? darker::game::make_scenario_group(mission.groups[0], bank, 1, 0, mission.shared.offset)
    : std::vector<darker::game::scenario_actor>{}};
  auto combat{std::make_unique<darker::game::mission_combat>(initial_actors)};
  if(caero) combat->reserves = darker::game::make_scenario_group(mission.groups[1],bank,static_cast<uint8_t>(1 + mission.groups[0].objects.size()),0,mission.shared.offset);
  auto const activate_reserves{[&](uint8_t const opcode, uint8_t const count){
    combat->activate_reserves(static_cast<darker::game::actor_category>(opcode - 9),
      count,host.player.pose(),static_cast<uint16_t>(context.clock));
    return objectives.complete(mission) && combat->remaining_objectives() == 0;
  }};
  auto const change_beacons{[&](uint8_t const opcode, uint8_t const origin, uint8_t const count){
    beacon_changes.command(opcode,origin,count,static_cast<uint16_t>(context.clock),scenario->bytes(mission.beacon_sequence));
  }};
  auto const select_weapon{[&](uint8_t const selection){
    if(host.player.lifecycle.flags & 0x20) return;
    auto mask{std::rotl(uint16_t{0x8000},selection)};
    if(selection >= 4) mask = static_cast<uint16_t>((mask & 0xff00) | static_cast<uint8_t>(mask + 1));
    host.available_weapons = mask;
    if(!caero) {
      darker::game::select_skimma_weapon(std::span{combat->skimma_weapons}.first(host.player.upgraded ? 3 : 2),
        combat->skimma_selection,combat->skimma_ring,selection,host.available_weapons,static_cast<uint16_t>(context.clock));
      combat->target = {};
    } else if(selection > 0 && selection < 4) combat->primary_weapon = selection;
    else if(selection >= 4) combat->secondary_weapon = selection;
  }};
  context.register_owner = [&]{ return std::exchange(combat->script_owner,uint16_t{0xd986}); };
  context.exchange_context = [&](auto &active){ exchange.exchange(active,context,active.continuation); };
  context.adjust_objectives = [&](uint8_t const operand){ combat->adjust_objectives(operand); return objectives.complete(mission) && combat->remaining_objectives() == 0; };
  auto const replace_world_objectives{[&](std::span<std::byte const> const program){
    auto const consumed{objectives.replace(cells,program)};
    context.objectives_complete = objectives.complete(mission) && combat->remaining_objectives() == 0;
    return consumed;
  }};
  context.replace_world_objectives = replace_world_objectives;
  context.select_weapon = select_weapon;
  context.set_building_attacks = [&](uint8_t const setting){ combat->building_attacks = setting != 0; };
  context.set_aircraft_spawning = [&](uint8_t const setting){ combat->spawning.enabled = setting != 0; };
  context.change_beacons = change_beacons;
  context.activate_reserves = activate_reserves;
  host.combat = combat.get();
  std::vector<darker::graphics::scene_object> objects;
  std::vector<darker::graphics::radar_contact> contacts;
  auto initial_player{host.player};
  auto initial_cells{cells};
  darker::graphics::city_renderer scene;
  darker::graphics::distance_shading const lighting;
  framework::render::indexed_cockpit_framebuffer display{}, world{};
  framework::render::cockpit_framebuffer output;
  auto const render{[&](std::uint16_t const clock, bool const enlarged, std::uint16_t const frame_step = 0){
    if(front && front->active()) {
      front->draw(output);
      return size_t{0};
    }
    auto actor{host.combat->camera_actor && *host.combat->camera_actor != 0
      ? std::ranges::find(combat->actors,*host.combat->camera_actor,&darker::game::scenario_actor::index) : combat->actors.end()};
    if(host.camera.mode == darker::game::camera_mode::object && (!host.combat->camera_actor
      || (*host.combat->camera_actor != 0 && actor == combat->actors.end()))) {
      host.combat->camera_actor.reset();
    }
    if(std::exchange(host.pick_camera,false) && !(host.player.lifecycle.flags & 16) && !host.player.tunnel) {
      if(auto const picked{darker::game::pick_camera_target(host.audio_listener,host.player.pose(),
        bank.header_at(bank.special_models()[host.player.definition_slot()]).extent,host.combat->camera_actor,
        combat->actors,cells,bank,host.player.world_damage_mask())}) {
        if(picked->actor) {
          host.combat->camera_actor = picked->actor;
          host.camera.mode = darker::game::camera_mode::object;
          actor = *picked->actor == 0 ? combat->actors.end()
            : std::ranges::find(combat->actors,*picked->actor,&darker::game::scenario_actor::index);
        } else {
          auto const &previous{actor != combat->actors.end() ? actor->pose : host.player.pose()};
          host.camera.drop(darker::game::camera_mode::fixed,picked->anchor);
          auto const direction{darker::maths::object_target_direction(picked->anchor.position,previous.position)};
          host.camera.anchor.angles = {direction.heading,direction.pitch,0};
          host.combat->camera_actor.reset();
        }
      }
    }
    auto const *watched{combat->missile_camera_enabled && host.camera.mode != darker::game::camera_mode::object
      ? combat->camera_projectile : nullptr};
    bool const watching_actor{host.camera.mode == darker::game::camera_mode::object && host.combat->camera_actor.has_value()};
    bool const cockpit_visible{!watched && host.camera.visible_mode() == darker::game::camera_mode::cockpit};
    bool const external{watched || (host.camera.visible_mode() != darker::game::camera_mode::cockpit && host.camera.visible_mode() != darker::game::camera_mode::fullscreen)};
    int const height{cockpit_visible ? (caero ? 168 : 180) : 240};
    auto const &pose{host.player.pose()};
    auto const subject{!watched ? darker::game::camera_subject::player : (watched->flags & 8) ? darker::game::camera_subject::missile_effect : darker::game::camera_subject::missile};
    auto const &camera_pose{watching_actor && actor != combat->actors.end() ? actor->pose : watched ? watched->placement : pose};
    auto const camera_subject{watching_actor
      ? (actor != combat->actors.end() && (actor->flags & 8) ? darker::game::camera_subject::object_effect : darker::game::camera_subject::object)
      : host.camera.mode == darker::game::camera_mode::object ? darker::game::camera_subject::absent_object : subject};
    auto const camera{host.camera.view(camera_pose,frame_step,!watching_actor && (host.player.lifecycle.flags & 16) != 0,camera_subject,host.player.tunnel.has_value())};
    host.audio_listener = camera;
    host.audio_motion = camera_pose;
    darker::graphics::city_view view{
      .column{camera.position[0]}, .row{camera.position[1]}, .column_fraction{camera.fractions[0]}, .row_fraction{camera.fractions[1]},
      .altitude{std::bit_cast<std::int16_t>(camera.position[2])},
      .angles{.heading{camera.angles[0]}, .pitch{camera.angles[1]}, .roll{camera.angles[2]}},
      .origin{.x{160}, .y{static_cast<std::int16_t>(height / 2)}}, .bottom{height},
    };
    combat->targeting_basis = darker::maths::make_view_basis(view.angles);
    view.underground = host.player.tunnel.has_value();
    view.unrestricted_visibility = host.player.noclip;
    view.radius = view.underground ? 8 : 15;
    view.beacon_lighting = world_mode == 0;
    view.gouraud = host.gouraud;
    darker::graphics::model_animation animation;
    animation.parameters[0] = std::bit_cast<std::int16_t>(host.hangar.extension);
    if(caero && !host.player.tunnel) std::ranges::copy(combat->spawning.platforms,animation.parameters.begin()+1);
    darker::graphics::update_fountain_parameters(animation, clock);
    darker::graphics::draw_sky_ground(world, view.angles, view.origin, height);
    objects.clear();
    contacts.clear();
    auto const coverage{darker::game::make_radar_coverage(cells,{pose.position[0],pose.position[1]},view.underground)};
    if(external && !host.player.tunnel && !host.player.lifecycle.crashing) objects.push_back({.model_offset{bank.special_models()[host.player.definition_slot()]}, .pose{pose}, .native_id{0xd986}});
    for(auto const category : {darker::game::actor_category::air, darker::game::actor_category::stationary, darker::game::actor_category::ground}) for(auto const &actor : combat->actors) {
      if(actor.category != category || (actor.flags & 8)) continue;
      objects.push_back({.model_offset{actor.parameters.model_token}, .pose{actor.pose}, .light{actor.fade}, .native_id{static_cast<uint16_t>(0xd986 + actor.index * 112)}});
      if(actor.category != darker::game::actor_category::stationary) {
        contacts.push_back({.position{actor.pose.position[0],actor.pose.position[1]},
          .group{view.underground ? darker::graphics::radar_group::underground : actor.category == darker::game::actor_category::air ? darker::graphics::radar_group::a : darker::graphics::radar_group::b},
          .covered{coverage.contains(static_cast<uint8_t>(actor.pose.position[0] >> 8),static_cast<uint8_t>(actor.pose.position[1] >> 8))}});
      }
    }
    for(auto const *pool : {&combat->projectiles,&combat->hostile_projectiles}) {
      for(auto *shot{pool->objects().head}; shot; shot = shot->next) {
        if(view.underground && !(shot->flags & 8)) contacts.push_back({.position{shot->placement.position[0],shot->placement.position[1]},
          .group{darker::graphics::radar_group::underground}});
        if(!(shot->flags & 8) && !(shot == watched && host.camera.visible_mode() == darker::game::camera_mode::fullscreen)) objects.push_back({.model_offset{shot->parameters.model_token}, .pose{shot->placement}, .light{shot->fade}, .native_id{shot->native_id}});
      }
    }
    darker::graphics::particle_scene const particles{.effects{combat->effects}, .sheet{cache}, .clock{clock}};
    auto const count{scene.draw(world, bank, cells, view, world_mode == 0 ? 0x20 : 0x60, lighting, animation, objects, &particles)};
    display = cockpit;
    auto const components{darker::graphics::cockpit_components(type)};
    std::array<std::uint8_t, 9> instruments{};
    if(auto const *state{std::get_if<darker::game::caero_flight_state>(&host.player.craft)}) {
      auto const measured{darker::graphics::measure_caero_instruments(*state, clock)};
      host.engine_indicator = darker::graphics::caero_engine_indicator(host.engine_indicator, host.player.engine_flags & 1, state->pose.speed);
      instruments = {host.player.tunnel ? uint8_t{0} : measured.altitude, measured.impact, measured.damage_lights, measured.power_cells, measured.charging,
        state->energy.incoming_display, state->energy.reserve_display, host.engine_indicator,
        darker::graphics::caero_receiver_indicator(clock,messages[0].has_value(),
          script.stopped,context.objectives_complete,host.player.lifecycle.flags)};
    } else {
      auto const &skimma{std::get<darker::game::skimma_flight_state>(host.player.craft)};
      auto const measured{darker::graphics::measure_skimma_instruments(pose.position[2], skimma.damage.shield_charge,
        skimma.damage.shield_enabled, false, clock, host.shield_deadline)};
      host.shield_ready = measured.shield_ready_sound;
      darker::graphics::draw_skimma_shield_startup(cache, display, measured.shield_startup);
      instruments[0] = measured.low_altitude;
      instruments[1] = measured.shield;
      instruments[2] = darker::graphics::skimma_speed_instrument(pose.speed, host.player.upgraded);
      instruments[3] = combat->skimma_reserves;
    }
    for(std::size_t i{0}; i < components.size(); ++i) darker::graphics::update_instrument(cache, display, type, i, 0, instruments[i]);
    darker::graphics::copy_rectangle(world.pixels, display.pixels, {.x{0}, .y{0}}, {.x{0}, .y{cockpit_visible && caero ? 8 : 0}}, 320, height);
    auto const grid{darker::game::beacon_grid_coordinates({host.player.pose().position[0],host.player.pose().position[1]})};
    darker::graphics::radar_view_state const navigation{
      .player{.x{view.column}, .y{view.row}}, .heading{view.angles.heading},
      .row{host.player.tunnel ? uint8_t{0} : grid[1]}, .column{host.player.tunnel ? uint8_t{0} : grid[0]},
    };
    bool const sights_visible{!watched && (cockpit_visible || host.camera.visible_mode() == darker::game::camera_mode::fullscreen)};
    int const sight_y{cockpit_visible ? (caero ? 92 : 90) : 120};
    if(sights_visible && caero) {
      auto const attitude{darker::graphics::calculate_attitude(view.angles.pitch >> 6, view.angles.roll >> 6, static_cast<std::int8_t>(view.angles.pitch >> 8), false, sight_y)};
      darker::graphics::draw_screen_line(display, attitude.first, attitude.last, attitude.colour);
      darker::graphics::draw_attitude_surround(display, combat->weapon_ready ? 0x0019 : 0xff19, sight_y);
      if(combat->target.token != 0xffff) {
        bool const centred{combat->target.distance < 2};
        uint16_t const colours{static_cast<uint16_t>((combat->secondary_ready ? 0xe9f3 : 0x030c) + (centred ? 0x0606 : 0))};
        darker::graphics::draw_target_marker(display,centred ? darker::graphics::target_marker::small : darker::graphics::target_marker::large,
          {.x{static_cast<int16_t>(160 + combat->target.horizontal)},.y{static_cast<int16_t>(sight_y + combat->target.vertical)}},
          static_cast<uint8_t>(colours),static_cast<uint8_t>(colours >> 8));
      }
    }
    if(cockpit_visible && caero) {
      darker::graphics::draw_aircraft_threats(display,font,combat->threat_errors);
      darker::graphics::update_caero_bitmaps(cache, display, {}, {.row{navigation.row}, .column{navigation.column}, .primary_weapon{combat->primary_weapon}, .secondary_weapon{combat->secondary_weapon}});
      darker::graphics::update_compass(display, 0, darker::graphics::compass_phase(view.angles.heading));
      if(!host.player.tunnel) darker::graphics::draw_radar_beacons(display,cells,navigation.player,navigation.heading,coverage);
      darker::graphics::draw_radar_contacts(display, navigation.player, navigation.heading, contacts);
      if(!host.player.tunnel) darker::graphics::draw_radar_interference(display,navigation.player,navigation.heading,coverage,combat->random_state);
      darker::graphics::draw_caero_frame_edges(cache, display);
      if(enlarged) darker::graphics::draw_enlarged_radar(cache, display, navigation, contacts);
    } else if(cockpit_visible) {
      darker::graphics::draw_skimma_frame_edges(cache, display);
      darker::graphics::skimma_bitmap_state indicators{.bearing{darker::graphics::skimma_mission_bearing(
        context.hud_reference,pose.position[0],pose.position[1],pose.angles[0])}};
      for(size_t i{0}; i < indicators.weapons.size(); ++i) indicators.weapons[i] = combat->skimma_weapons[i].flags;
      darker::graphics::update_skimma_bitmaps(cache, display, type, {}, indicators);
    }
    if(sights_visible && !caero) {
      auto const &weapon{combat->skimma_weapons[combat->skimma_selection]};
      if(auto const ring{darker::game::update_weapon_ring(weapon.ammunition,combat->skimma_ring,clock,weapon.flags,frame_step)}) {
        darker::graphics::draw_skimma_weapon_ring(cache,display,type,combat->skimma_selection,ring->radius,ring->remaining,sight_y - 2);
      }
      darker::graphics::draw_target_marker(display, darker::graphics::target_marker::skimma_aim,
        {.x{164}, .y{static_cast<int16_t>(sight_y + combat->skimma_aim_offset)}}, 14, 14);
    }
    darker::graphics::draw_missile_camera_indicator(display, clock, combat->missile_camera_enabled, watched != nullptr);
    for(size_t const channel : {1u,0u,2u}) if(auto const &message{messages[channel]}) {
      auto const width{static_cast<uint16_t>(message->width + (message->alignment == darker::game::message_alignment::centre
        && message->width < context.message_setting ? 256 : 0))};
      int const x{message->alignment == darker::game::message_alignment::left ? 12
        : message->alignment == darker::game::message_alignment::right ? 308-width : (321-width)/2};
      darker::graphics::draw_message(display,font,darker::resources::font_face::compact,
        message->text.subspan(message->offset,message->length),{.x{x},.y{231}},width,{.ink{24},.edge{18}});
    }
    framework::render::expand_palette(display, game_palette.colours, output);
    return count;
  }};
  render(0, false);
  if(arguments.contains("output")) {
    std::ofstream file{arguments["output"].as<std::string>(), std::ios::binary};
    file.exceptions(std::ios::failbit | std::ios::badbit);
    file << "P6\n320 240\n255\n";
    for(auto const &pixel : output.pixels) {
      file.put(static_cast<char>(pixel.red));
      file.put(static_cast<char>(pixel.green));
      file.put(static_cast<char>(pixel.blue));
    }
    file.close();
    return EXIT_SUCCESS;
  }
  glfwSetErrorCallback([](int const code, char const *const message){
    std::cerr << "ERROR: GLFW " << code << ": " << message << std::endl;
  });
  if(!glfwInit()) return startup_failure("GLFW initialisation failed");
  boost::scope::scope_exit terminate_glfw{[]{ glfwTerminate(); }};
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
  std::unique_ptr<GLFWwindow, decltype(&glfwDestroyWindow)> const window{
    glfwCreateWindow(display_width * scale, display_height * scale, "Darker", nullptr, nullptr), glfwDestroyWindow,
  };
  if(!window) return startup_failure("cannot create the GLFW window");
  glfwMakeContextCurrent(window.get());
  glfwSwapInterval(1);
  glfwSetInputMode(window.get(), GLFW_CURSOR, host.cursor_mode(!caero));
  if(host.mouse_enabled && glfwRawMouseMotionSupported()) glfwSetInputMode(window.get(), GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
  glfwSetWindowUserPointer(window.get(), &host);
  glfwSetKeyCallback(window.get(), [](GLFWwindow *const window, int const key, int, int const action, int const modifiers){
    auto &host{*static_cast<flight_host *>(glfwGetWindowUserPointer(window))};
    if(action == GLFW_RELEASE) {
      if(key == host.resume_key) host.resume_key = GLFW_KEY_UNKNOWN;
      return;
    }
    if(key == host.resume_key) return;
    if(host.front && host.front->active()) {
      using darker::presentation::front_key;
      if(action != GLFW_PRESS && key != GLFW_KEY_BACKSPACE) return;
      switch(key) {
      case GLFW_KEY_KP_ENTER:
      case GLFW_KEY_ENTER: host.front->key(front_key::accept); break;
      case GLFW_KEY_SPACE: if(!host.front->editing_text()) host.front->key(front_key::accept); break;
      case GLFW_KEY_ESCAPE: host.front->key(front_key::back); break;
      case GLFW_KEY_UP: host.front->key(front_key::up); break;
      case GLFW_KEY_DOWN: host.front->key(front_key::down); break;
      case GLFW_KEY_BACKSPACE: host.front->key(front_key::erase_character); break;
      case GLFW_KEY_S: host.front->key(front_key::select); break;
      case GLFW_KEY_E: host.front->key(front_key::erase); break;
      case GLFW_KEY_Q: host.front->key(front_key::quit); break;
      case GLFW_KEY_1: host.front->key(front_key::one); break;
      case GLFW_KEY_2: host.front->key(front_key::two); break;
      case GLFW_KEY_3: host.front->key(front_key::three); break;
      case GLFW_KEY_4: host.front->key(front_key::four); break;
      case GLFW_KEY_N: host.front->key(front_key::nightmare); break;
      case GLFW_KEY_Y: host.front->key(front_key::yes); break;
      default: break;
      }
      return;
    }
    if(host.paused || key == GLFW_KEY_PAUSE || key == GLFW_KEY_NUM_LOCK) {
      if(action != GLFW_PRESS) return;
      // Leave desktop switching chords to the window manager without resuming simulation.
      bool const modifier_key{key == GLFW_KEY_LEFT_SHIFT || key == GLFW_KEY_RIGHT_SHIFT
        || key == GLFW_KEY_LEFT_CONTROL || key == GLFW_KEY_RIGHT_CONTROL
        || key == GLFW_KEY_LEFT_ALT || key == GLFW_KEY_RIGHT_ALT
        || key == GLFW_KEY_LEFT_SUPER || key == GLFW_KEY_RIGHT_SUPER};
      if(host.paused && (modifier_key || (modifiers & (GLFW_MOD_ALT | GLFW_MOD_SUPER)))) return;
      bool const pause_key{key == GLFW_KEY_PAUSE || key == GLFW_KEY_NUM_LOCK};
      if(host.paused && pause_key) { host.single_step = true; return; }
      host.paused = pause_key;
      if(!host.paused) host.resume_key = key;
      glfwSetInputMode(window,GLFW_CURSOR,host.cursor_mode(!host.paused));
      if(!host.paused) {
        host.mouse_started = false;
        host.player.controls.bank.previous_mouse = 0;
        host.player.controls.pitch.previous_mouse = 0;
      }
      return;
    }
    if(key == GLFW_KEY_F9 && action == GLFW_PRESS) host.gouraud = !host.gouraud;
    if(key == GLFW_KEY_ESCAPE) {
      if(host.front) host.exit_requested = session_exit::menu;
      else glfwSetWindowShouldClose(window, GLFW_TRUE);
      return;
    }
    if(action == GLFW_PRESS && (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) && host.hangar.returning == darker::game::hangar_return_phase::complete) {
      host.exit_requested = session_exit::completed;
      return;
    }
    if(action == GLFW_PRESS && key >= GLFW_KEY_F1 && key <= GLFW_KEY_F6) {
      auto const selected{static_cast<darker::game::camera_mode>(key - GLFW_KEY_F1)};
      if(key >= GLFW_KEY_F5) {
        if(!(host.player.lifecycle.flags & 16)) {
          host.combat->camera_actor.reset();
          host.camera.drop(selected,host.player.pose());
        }
      } else {
        if(host.camera.mode == darker::game::camera_mode::fixed) {
          host.camera.look_heading = 0;
          host.camera.look_pitch = 0;
          host.camera.looking = false;
        }
        host.camera.mode = selected;
      }
      if(key == GLFW_KEY_F1 || key == GLFW_KEY_F4) host.camera.distance = 0x8000;
    }
    if(action == GLFW_PRESS && (key == GLFW_KEY_F7 || key == GLFW_KEY_GRAVE_ACCENT)) host.pick_camera = true;
    if(action == GLFW_PRESS && key == GLFW_KEY_COMMA && host.camera.distance_step > 0) --host.camera.distance_step;
    if(action == GLFW_PRESS && key == GLFW_KEY_PERIOD && host.camera.distance_step < 5) ++host.camera.distance_step;
    if((key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) && host.player.lifecycle.crashing) {
      if(action == GLFW_PRESS) host.exit_requested = session_exit::death;
      return;
    }
    if(host.player.lifecycle.crashing) return;
    if(key == GLFW_KEY_Z && action == GLFW_PRESS && host.freeze_enabled) {
      host.player.toggle_freeze();
      return;
    }
    if(key == GLFW_KEY_X && action == GLFW_PRESS && host.front && host.front->level_skip_enabled()
      && !(host.player.lifecycle.flags & 0x20)) {
      // B926 restores C610 into C81E and requests the ordinary successful mission exit.
      if(modifiers & GLFW_MOD_SHIFT) {
        if(!host.front->nightmare_selected()) host.exit_requested = session_exit::previous;
        return;
      }
      host.hangar.return_site = host.hangar.next_return_site;
      host.exit_requested = session_exit::completed;
      return;
    }
    using darker::audio::flight_sound;
    using darker::game::flight_command;
    switch(key) {
      case GLFW_KEY_1:
      case GLFW_KEY_2:
      case GLFW_KEY_3:
      case GLFW_KEY_5:
      case GLFW_KEY_6:
      case GLFW_KEY_8:
      case GLFW_KEY_9:
      case GLFW_KEY_0:
        if(host.combat && action == GLFW_PRESS && !host.weapon_selection_blocked) {
          auto const selection{static_cast<uint8_t>(key == GLFW_KEY_0 ? 10 : key-GLFW_KEY_0)};
          if(!(host.available_weapons & (1u << (selection-1)))) break;
          bool const skimma{std::holds_alternative<darker::game::skimma_flight_state>(host.player.craft)};
          if(skimma && selection < 4) {
            if(!darker::game::select_skimma_weapon(std::span{host.combat->skimma_weapons}.first(host.player.upgraded ? 3 : 2),
              host.combat->skimma_selection,host.combat->skimma_ring,selection,host.available_weapons,host.clock)) break;
            host.combat->target = {};
          } else if(selection < 4) host.combat->primary_weapon = selection;
          else host.combat->secondary_weapon = selection;
          host.sounds.trigger(skimma ? flight_sound::skimma_switch : flight_sound::caero_switch,host.clock);
        }
        break;
      case GLFW_KEY_CAPS_LOCK:
        if(host.combat && action == GLFW_PRESS) host.combat->target.clear();
        break;
      case GLFW_KEY_M:
        if(host.combat && action == GLFW_PRESS) {
          host.combat->missile_camera_enabled = !host.combat->missile_camera_enabled;
          if(!host.combat->missile_camera_enabled && (host.camera.mode == darker::game::camera_mode::cockpit || host.camera.mode == darker::game::camera_mode::fullscreen)) host.camera.distance = 0x8000;
        }
        break;
      case GLFW_KEY_E:
        host.player.command(flight_command::engine);
        if(host.player.engine_flags & 1) {
          host.shield_deadline = static_cast<std::uint16_t>(host.clock + 0x6ff);
          if(!std::holds_alternative<darker::game::caero_flight_state>(host.player.craft)) host.sounds.trigger(flight_sound::shield_start, host.clock);
        }
        host.sounds.trigger(std::holds_alternative<darker::game::caero_flight_state>(host.player.craft) ? flight_sound::caero_switch : flight_sound::skimma_switch, host.clock);
        break;
      case GLFW_KEY_A: host.player.command(flight_command::altitude_hold); break;
      case GLFW_KEY_KP_ENTER:
      case GLFW_KEY_ENTER: {
        auto const *caero{std::get_if<darker::game::caero_flight_state>(&host.player.craft)};
        auto const previous{caero ? caero->energy.boost : 0};
        host.player.command(flight_command::boost);
        if(caero && caero->energy.boost < previous) host.sounds.trigger(flight_sound::boost, host.clock);
        break;
      }
      case GLFW_KEY_MINUS: host.player.command(flight_command::speed_low); break;
      case GLFW_KEY_EQUAL: host.player.command(flight_command::speed_high); break;
      default: break;
    }
  });
  framework::platform::framebuffer_presenter presenter{*window};
  darker::audio::fm_stream audio{framework::platform::audio_output::sample_rate,
    opl_name == "dosbox" ? darker::audio::fm_backend::dosbox : darker::audio::fm_backend::nuked};
  glfwSetCharCallback(window.get(), [](GLFWwindow *window, unsigned int code){
    auto &host{*static_cast<flight_host *>(glfwGetWindowUserPointer(window))};
    if(host.front) host.front->character(code);
  });
  glfwSetCursorPosCallback(window.get(), [](GLFWwindow *const window, double const x, double const y){
    auto &host{*static_cast<flight_host *>(glfwGetWindowUserPointer(window))};
    if(!host.mouse_enabled || !host.front || !host.front->active()) return;
    int width{0},height{0};
    glfwGetWindowSize(window,&width,&height);
    auto const viewport{framework::render::fit_viewport(width,height,320,240)};
    if(viewport.width > 0 && viewport.height > 0) host.front->point(static_cast<int>((x-viewport.x)*320/viewport.width),static_cast<int>((y-viewport.y)*240/viewport.height));
  });
  glfwSetMouseButtonCallback(window.get(), [](GLFWwindow *window, int button, int action, int){
    auto &host{*static_cast<flight_host *>(glfwGetWindowUserPointer(window))};
    if(!host.mouse_enabled || !host.front || !host.front->active() || button != GLFW_MOUSE_BUTTON_LEFT || action != GLFW_PRESS) return;
    int width{0},height{0}; double x{0},y{0};
    glfwGetWindowSize(window,&width,&height); glfwGetCursorPos(window,&x,&y);
    auto const viewport{framework::render::fit_viewport(width,height,320,240)};
    if(viewport.width > 0 && viewport.height > 0) host.front->click(static_cast<int>((x - viewport.x) * 320 / viewport.width),static_cast<int>((y - viewport.y) * 240 / viewport.height));
  });
  std::unique_ptr<framework::platform::audio_output> audio_device;
  if(!arguments.contains("mute")) {
    if(front && music_name != "none") {
      std::array<std::vector<std::byte>,6> songs;
      for(unsigned int group{0}; group < songs.size(); ++group) songs[group] = archives.load({0,38 + static_cast<unsigned int>(music_variant) + group * 5});
      auto const find_soundfont{[&]()->std::filesystem::path {
        if(arguments.contains("soundfont")) return arguments["soundfont"].as<std::string>();
        for(auto const &candidate : {data_directory / "soundfont.sf2", std::filesystem::path{"soundfont.sf2"},
          std::filesystem::path{"/usr/share/sounds/sf2/FluidR3_GM.sf2"}, std::filesystem::path{"/usr/share/sounds/sf2/TimGM6mb.sf2"}}) {
          if(std::filesystem::is_regular_file(candidate)) return candidate;
        }
        return {};
      }};
      if(music_variant == darker::audio::music_variant::soundblaster) audio.configure_music(archives.load({0,33}), std::move(songs));
      else if(music_variant == darker::audio::music_variant::gus && !arguments.contains("soundfont")) {
        auto const directory{arguments.contains("gus-dir") ? std::filesystem::path{arguments["gus-dir"].as<std::string>()} : data_directory / "ULTRASND"};
        audio.configure_gus_music(directory, std::move(songs), gus_ram);
      }
      else if(music_variant == darker::audio::music_variant::awe32 && !arguments.contains("soundfont")) {
        auto const rom{arguments.contains("awe32-rom") ? std::filesystem::path{arguments["awe32-rom"].as<std::string>()} : data_directory / "awe32.raw"};
        audio.configure_awe32_music(rom, archives.load({0,37}), std::move(songs));
      }
      else if(music_name == "roland-sc55" || music_name == "roland-scc1a") {
        bool const scc1a{music_name == "roland-scc1a"};
        auto const option{scc1a ? "scc1a-rom-dir" : "sc55-rom-dir"};
        auto const rom_directory{arguments.contains(option) ? std::filesystem::path{arguments[option].as<std::string>()} : data_directory};
        audio.configure_sc55_music(rom_directory, std::move(songs), scc1a ? darker::audio::sound_canvas_model::scc1a : darker::audio::sound_canvas_model::sc55);
        std::cout << "Music: " << (scc1a ? "SCC-1A v1.30" : "SC-55 v1.21") << " hardware emulation, original SCC-1/General MIDI arrangement" << std::endl;
      }
      else if(music_variant == darker::audio::music_variant::lapc1 && (!arguments.contains("soundfont") || percussion_fallback)) {
        auto const rom_directory{arguments.contains("mt32-rom-dir") ? std::filesystem::path{arguments["mt32-rom-dir"].as<std::string>()} : data_directory};
        auto const percussion_font{percussion_fallback ? find_soundfont() : std::filesystem::path{}};
        if(percussion_fallback && percussion_font.empty()) return startup_failure("Roland percussion fallback needs a SoundFont: supply --soundfont=path/to/bank.sf2");
        audio.configure_roland_music(rom_directory, archives.load({0,35}), std::move(songs), percussion_font, percussion_bank_enabled ? std::filesystem::path{arguments["roland-gm-percussion-bank"].as<std::string>()} : std::filesystem::path{},
          full_roland_bank ? std::filesystem::path{arguments["roland-gm-bank"].as<std::string>()} : std::filesystem::path{});
        if(full_roland_bank) std::cout << "Roland setup: full GM bank followed by Darker custom instruments on one device" << std::endl;
        if(percussion_bank_enabled) std::cout << "Unmapped Roland percussion: separate Munt device using " << arguments["roland-gm-percussion-bank"].as<std::string>() << std::endl;
        if(percussion_fallback) std::cout << "Unmapped Roland percussion: General MIDI fallback using " << percussion_font << std::endl;
        std::cout << "Music: LAPC-I arrangement, Munt emulation with original custom Roland timbres" << std::endl;
      } else {
        auto const font{find_soundfont()};
        if(font.empty()) return startup_failure("Sampled music needs a SoundFont: supply --soundfont=path/to/bank.sf2");
        audio.configure_sampled_music(music_variant, font, std::move(songs));
        std::cout << "Music: " << music_name << " arrangement, SoundFont rendition using " << font
                  << " (not original hardware synthesis)" << std::endl;
      }
    }
    try {
      audio_device = std::make_unique<framework::platform::audio_output>([](void *const data, std::span<float> const output) noexcept {
        static_cast<darker::audio::fm_stream *>(data)->render(output);
      }, &audio);
    } catch(std::runtime_error const &error) {
      std::cerr << "WARNING: continuing without sound: " << error.what() << std::endl;
    }
  }
  auto const start{std::chrono::steady_clock::now()};
  std::uint64_t previous_interrupts{0};
  darker::game::game_clock game_clock;
  // F15A's VGA timing gives 800 pixels per line and 527 lines at the mode-13h 25.175 MHz clock.
  // AF61 waits for retrace; host swap synchronisation alone can exceed this rate or provide no pacing.
  auto const display_interval{std::chrono::duration_cast<std::chrono::steady_clock::duration>(
    std::chrono::duration<double>{800.0 * 527.0 / 25'175'000.0})};
  auto next_frame{std::chrono::steady_clock::now()};
  bool pause_reported{false};
  while(!glfwWindowShouldClose(window.get())) {
    host.weapon_selection_blocked = exchange.supplementary_active;
    glfwPollEvents();
    auto const frame_time{std::chrono::steady_clock::now()};
    if(frame_time < next_frame) {
      glfwWaitEventsTimeout(std::chrono::duration<double>(next_frame - frame_time).count());
      continue;
    }
    next_frame = frame_time + display_interval;
    if(host.exit_requested != session_exit::none) {
      bool const previous_level{host.exit_requested == session_exit::previous};
      if(previous_level) debug_session = true;
      bool const died{host.exit_requested == session_exit::death};
      bool const aborted{host.exit_requested == session_exit::aborted};
      auto const completed{static_cast<uint8_t>(combat->completed_objectives)};
      bool const completed_mission{host.exit_requested == session_exit::completed};
      if(front) darker::game::commit_beacon_queue(cells,scenario->bytes(mission.beacon_sequence));
      if(completed_mission && front && !front->nightmare_selected()) {
        auto updated{front->selected_pilot()};
        if(!host.player.tunnel) {
          darker::game::pack_city_state(cells,bank.city_types(),world_mode == 1 ? std::span<std::byte>{updated.halon} : std::span<std::byte>{updated.delphi});
          updated.return_site = host.hangar.return_site;
          updated.weapons = host.available_weapons;
        }
        ++updated.stage;
        auto updated_saves{saves};
        auto const slot_index{static_cast<size_t>(&front->selected_pilot() - saves.pilots.data())};
        updated_saves.pilots[slot_index] = updated;
        if(!debug_session) darker::resources::write_save(save_path,updated_saves);
        saves = updated_saves;
      }
      host.player = initial_player;
      host.camera = {};
      host.combat->camera_actor.reset();
      host.pick_camera = false;
      host.hangar = {};
      host.sounds = {};
      host.world_audio = {};
      host.shield_ready = false;
      if(audio_device) audio.publish({});
      cells = initial_cells;
      combat = std::make_unique<darker::game::mission_combat>(initial_actors);
      host.combat = combat.get();
      host.primary_held = false;
      host.secondary_held = false;
      host.engine_indicator = 0;
      host.briefing = front != nullptr;
      if(front) {
        if(front->nightmare_selected()) front->finish_nightmare(static_cast<uint8_t>(host.score_base+completed),context.progress ? context.progress : died ? 2 : aborted ? 3 : completed_mission ? 1 : 255);
        else if(previous_level) front->previous_level();
        else if(completed_mission) front->continue_campaign();
        else if(died) front->show_death(completed);
        else if(aborted) front->show_abort(completed);
        else front->return_to_menu();
        glfwSetInputMode(window.get(),GLFW_CURSOR,host.cursor_mode(false));
      }
      script = initial_script;
      context.text_cursor = 0;
      context.messages.clear();
      messages.fill(std::nullopt);
      host.mouse_started = false;
      host.shield_deadline = 0;
      host.clock = 0;
      host.exit_requested = session_exit::none;
      game_clock = {};
      std::cout << (front ? (completed_mission ? (debug_session ? "Continuing the debug campaign without saving." : "Mission saved; continuing the campaign.") : died ? "Showing the Kismet committal sequence." : aborted ? "Showing the mission-aborted sequence." : previous_level ? "Starting the previous playable level without saving." : "Returned to the run menu.") : "Restarted the airborne checkpoint.") << std::endl;
    }
    auto const now{std::chrono::steady_clock::now()};
    double const elapsed{std::chrono::duration<double>(now - start).count()};
    if(seconds > 0 && elapsed >= seconds) break;
    auto const interrupts{static_cast<std::uint64_t>(elapsed * (1193180.0 / 2386))};
    bool const single_step{std::exchange(host.single_step,false)};
    if(host.paused && !single_step) {
      // Keep the last software frame intact and exclude paused wall time from the PIT clock.
      previous_interrupts = interrupts;
      if(!pause_reported) {
        auto const &pose{host.player.pose()};
        std::cout << "Paused: position " << pose.position[0] << ',' << pose.position[1] << ',' << pose.position[2]
          << "; heading/pitch/roll " << pose.angles[0] << ',' << pose.angles[1] << ',' << pose.angles[2]
          << "; player flags " << unsigned{host.player.lifecycle.flags}
          << "; return hangar cell " << ((host.hangar.return_site & 255) >> 1) << ',' << (host.hangar.return_site >> 8)
          << "; objectives complete " << context.objectives_complete << "; clock " << game_clock.frame_ticks << std::endl;
        glfwSetWindowTitle(window.get(),"Darker - paused (Pause steps; an ordinary key resumes)");
        if(audio_device) audio.publish({});
        pause_reported = true;
      }
      presenter.present(output);
      continue;
    }
    pause_reported = false;
    if(single_step) previous_interrupts = interrupts;
    if(front) {
      front->advance(static_cast<uint32_t>(interrupts - previous_interrupts));
      if(front->save_requested) {
        if(!debug_session) darker::resources::write_save(save_path,saves);
        front->save_requested = false;
      }
      if(front->quit_requested) break;
      bool const active{front->active()};
      if(host.briefing != active) {
        host.briefing = active;
        host.mouse_started = false;
        glfwSetInputMode(window.get(),GLFW_CURSOR,host.cursor_mode(!active));
        if(!active) {
          auto const &pilot{front->selected_pilot()};
          host.available_weapons = pilot.weapons ^ front->weapon_changes();
          scenario = &front->selected_scenario();
          auto const record{front->selected_record()};
          mission = scenario->records()[record];
          text = scenario->language(record,language);
          auto const configuration{static_cast<uint8_t>(mission.configuration & 15)};
          bool const underground{configuration == 4};
          world_mode = underground ? 2 : configuration == 2 || configuration == 3 ? 1 : 0;
          caero = configuration == 1 || underground;
          type = caero ? darker::graphics::craft::caero : configuration == 2 ? darker::graphics::craft::skimma : darker::graphics::craft::upgraded_skimma;
          bitmap = darker::graphics::decode_bitmap(archives.load({0,darker::graphics::cockpit_resource_slot(configuration)}));
          cache = darker::graphics::make_cockpit_cache(bitmap.image);
          cockpit = cache;
          bank = darker::resources::geometry_bank{archives.load({0,30u + world_mode})};
          cells = darker::game::make_city_map(archives.load({0,underground ? 70u + (mission.configuration >> 4) : 68u + world_mode}),world_mode == 0);
          game_palette = underground ? darker::graphics::decode_palette(archives.load({0,19}),bitmap.palette).palette : bitmap.palette;
          scene = {};
          if(underground) {
            tunnel_network.emplace(archives.load({0,78}));
            variant_limits.fill(0);
            for(size_t i{0}; i < bank.city_types().size(); ++i) variant_limits[i+1] = bank.city_types()[i].variant_limit;
            darker::game::assign_city_variants(cells,variant_limits);
          } else {
            tunnel_network.reset();
            darker::game::restore_city_state(cells,bank.city_types(),world_mode == 1 ? std::span<std::byte const>{pilot.halon} : std::span<std::byte const>{pilot.delphi},pilot.stage);
          }
          darker::game::apply_scenario_cells(cells,mission);
          objectives = {.list{mission.objective_cell_list}};
          host.hangar = {.next_return_site{front->departure_destination()}};
          if(pilot.stage > 1 && pilot.return_site != 0) host.hangar.return_site = pilot.return_site;
          if(underground) {
            auto const entry{front->entry()};
            if(!entry) throw std::logic_error{"Underground briefing did not supply an entry site"};
            host.hangar.return_site = entry->site;
            darker::game::initialise_tunnel_entry(host.player,entry->site,entry->heading,bank.header_at(bank.special_models()[28]).height);
          } else if(caero && front->nightmare_selected()) {
            auto const entry{front->entry()};
            if(!entry) throw std::logic_error{"Nightmare briefing did not supply its entry site"};
            host.hangar.return_site = entry->site;
            host.player = {};
            host.player.pose().position = {static_cast<uint16_t>((entry->site & 255)*128+128),
              static_cast<uint16_t>((entry->site & 0xff00)+128),0};
            host.player.pose().angles[0] = static_cast<uint16_t>(entry->heading*256);
            std::get<darker::game::caero_flight_state>(host.player.craft).energy.buffer = 0x6000;
          } else if(caero) darker::game::initialise_caero_hangar(host.player,cells,host.hangar,bank.header_at(bank.special_models()[25]).height);
          else {
            auto const entry{front->entry()};
            auto const site{entry ? entry->site : pilot.return_site};
            host.hangar.return_site = site;
            host.hangar.next_return_site = site;
            darker::game::initialise_skimma_pad(host.player,site,entry ? entry->heading : uint8_t{0},
              bank.header_at(bank.special_models()[configuration + 24]).height,configuration != 2);
          }
          host.player.noclip = noclip;
          host.player.boost_cheat = boost_cheat;
          host.player.damage_cheat = damage_cheat;
          host.player.scenario_configuration = configuration;
          if(configuration == 0) host.player.supply.phase = darker::game::supply_phase::flight;
          std::optional<darker::game::tunnel_setup> const tunnels{underground ? std::optional{darker::game::tunnel_setup{*tunnel_network,cells}} : std::nullopt};
          darker::game::weapon_ammunition second_weapon;
          darker::game::refill_skimma_weapon(second_weapon,1);
          auto groups{darker::game::make_scenario_actors(mission,*scenario,bank,host.player,second_weapon,0,tunnels)};
          initial_actors = std::move(groups[0]);
          initial_player = host.player;
          initial_cells = cells;
          auto const spawning{combat->spawning};
          combat = std::make_unique<darker::game::mission_combat>(initial_actors);
          combat->spawning = spawning;
          combat->spawning.enabled = true;
          if(world_mode == 0) darker::game::prepare_delphi_aircraft_sites(combat->spawning,cells);
          else {
            combat->spawning.halon = world_mode == 1;
            combat->spawning.sites.clear();
          }
          for(uint8_t i{0}; i < combat->skimma_weapons.size(); ++i) darker::game::refill_skimma_weapon(combat->skimma_weapons[i].ammunition,i);
          combat->reserves = std::move(groups[1]);
          combat->free_actors = std::move(groups[2]);
          combat->skimma_weapons[1].ammunition = second_weapon;
          combat->difficulty = front->initial_difficulty().value_or(static_cast<uint8_t>(pilot.stage*2));
          host.score_base = front->initial_score();
          host.combat = combat.get();
          initial_script = {.continuation{*mission.player_program - mission.shared.offset}, .checkpoint{*mission.player_program - mission.shared.offset}};
          script = initial_script;
          context = {.program{scenario->bytes(mission.shared)}, .text{text}, .cells{cells}, .time_multiplier{mission.time_multiplier}, .text_cursor{front->consumed_text()}};
          context.set_altitude = [&](std::optional<uint16_t> const height){
            host.player.scripted_altitude_hold = height.has_value();
            if(height) host.player.desired_height = *height;
          };
          context.set_difficulty = [&](uint8_t const value){ combat->difficulty = value; };
          context.reset_score = [&](uint8_t const value){ host.score_base = value; combat->completed_objectives = 0; };
          context.activate_reserves = activate_reserves;
          context.change_beacons = change_beacons;
          exchange = {};
          if(!underground && (mission.configuration >> 4)) {
            auto const &source{campaign.supplementary()};
            auto const index{static_cast<size_t>(mission.configuration >> 4)};
            auto const &record{source.records()[index]};
            exchange.alternate = darker::game::mission_context_slot{source.bytes(record.shared),
              source.language(index,language),record.entry_offset - record.shared.offset};
          }
          context.register_owner = [&]{ return std::exchange(combat->script_owner,uint16_t{0xd986}); };
          context.exchange_context = [&](auto &active){ exchange.exchange(active,context,active.continuation); };
          context.adjust_objectives = [&](uint8_t const operand){ combat->adjust_objectives(operand); return objectives.complete(mission) && combat->remaining_objectives() == 0; };
          context.replace_world_objectives = replace_world_objectives;
          context.select_weapon = select_weapon;
          context.refill_weapon = [&]{ darker::game::refill_skimma_weapon(combat->skimma_weapons[combat->skimma_selection].ammunition,combat->skimma_selection); };
          context.reset_shield = [&]{
            auto &charge{std::get<darker::game::skimma_flight_state>(host.player.craft).damage.shield_charge};
            charge = static_cast<uint16_t>((charge & 255) | 0xbf00);
          };
          context.toggle_weapons = [&](uint16_t const mask){ host.available_weapons ^= mask; };
          context.mark_aircraft_sites = [&](std::span<std::byte const> const program){ return darker::game::prepare_halon_aircraft_sites(combat->spawning,cells,program); };
          context.set_building_attacks = [&](uint8_t const setting){ combat->building_attacks = setting != 0; };
          context.set_aircraft_spawning = [&](uint8_t const setting){ combat->spawning.enabled = setting != 0; };
          host.ambient_audio = {};
          host.world_audio = {};
          beacon_changes = {};
          messages.fill(std::nullopt);
          game_clock = {};
          host.clock = 0;
          host.primary_held = false;
          host.secondary_held = false;
          // Poll the cursor-capture warp before establishing the flight mouse origin; asset loading is not flight time.
          auto const loaded_at{std::chrono::steady_clock::now()};
          previous_interrupts = static_cast<uint64_t>(std::chrono::duration<double>{loaded_at - start}.count() * (1193180.0 / 2386));
          continue;
        }
      }
    }
    game_clock.running = !host.briefing && host.hangar.returning != darker::game::hangar_return_phase::complete;
    darker::game::advance_game_clock(game_clock, single_step ? 8 : interrupts - previous_interrupts);
    previous_interrupts = interrupts;
    auto const step{darker::game::consume_game_frame(game_clock)};
    auto const *caero_state{std::get_if<darker::game::caero_flight_state>(&host.player.craft)};
    auto const previous_cells{caero_state ? caero_state->energy.boost >> 13 : 0};
    darker::game::city_collision_result contact;
    std::optional<std::array<uint16_t,3>> player_start;
    bool const primary_held{host.key_down(*window,GLFW_KEY_SPACE) || (host.mouse_enabled && !host.paused && glfwGetMouseButton(window.get(), GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)};
    bool const secondary_held{host.key_down(*window,GLFW_KEY_LEFT_ALT) || host.key_down(*window,GLFW_KEY_RIGHT_ALT)
      || (host.mouse_enabled && !host.paused && glfwGetMouseButton(window.get(),GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS)};
    bool const was_crashing{host.player.lifecycle.crashing};
    if(step != 0) {
      if(front) combat->spawn_aircraft(host.player,cells,bank,game_clock.frame_ticks,step);
      if(host.hangar.returning == darker::game::hangar_return_phase::none) {
        if(front) {
          player_start = host.player.pose().position;
          host.player.advance_motion(host.input(*window),host.key_down(*window,GLFW_KEY_BACKSPACE),
            step,bank,cells,tunnel_network ? &*tunnel_network : nullptr,
            {.output{context.transition_output},.supplementary_active{exchange.supplementary_active}});
        } else contact = host.player.advance(host.input(*window), host.key_down(*window,GLFW_KEY_BACKSPACE),
          step, game_clock.frame_ticks, bank, cells,tunnel_network ? &*tunnel_network : nullptr,
          {.output{context.transition_output},.supplementary_active{exchange.supplementary_active}});
      } else if(!host.player.frozen) {
        darker::game::advance_hangar_return(host.player, host.hangar, step, game_clock.frame_ticks);
        if(host.hangar.returning == darker::game::hangar_return_phase::complete) host.exit_requested = context.objectives_complete ? session_exit::completed : session_exit::aborted;
      }
      if(front) {
        combat->update_difficulty((static_cast<uint32_t>(game_clock.wraps) << 16) | game_clock.frame_ticks);
        beacon_changes.advance(cells,game_clock.frame_ticks);
        auto const *previous_missile{combat->camera_projectile};
        combat->advance(host.player,cells,bank,(static_cast<uint32_t>(game_clock.wraps) << 16) | game_clock.frame_ticks,
          step,game_clock.frame_changes,primary_held && !host.primary_held,scenario->bytes(mission.shared),mission.time_multiplier,tunnel_network ? &*tunnel_network : nullptr,secondary_held && !host.secondary_held,secondary_held,player_start,host.primary_held && !primary_held);
        contact = combat->player_contact;
        if(previous_missile && !combat->camera_projectile) host.camera.distance = 0x8000;
        context.clock = (static_cast<uint32_t>(game_clock.wraps) << 16) | game_clock.frame_ticks;
        objectives.advance(cells,mission,world_mode == 0 ? 0x20 : 0x60);
        context.objectives_complete = objectives.complete(mission) && combat->remaining_objectives() == 0;
        context.object_counter = static_cast<uint8_t>(combat->completed_objectives);
        context.counter = combat->world_damage_counter;
        context.object_flags = combat->status_flags(host.player.lifecycle.flags);
        context.suppress_messages = (host.player.lifecycle.flags & 0x20) != 0;
        context.messages.clear();
        darker::game::advance_mission_script(script, context);
        for(auto const &event : context.messages) {
          messages[static_cast<size_t>(event.alignment)] = event;
          host.sounds.trigger(caero ? darker::audio::flight_sound::message : darker::audio::flight_sound::skimma_message,game_clock.frame_ticks);
        }
        for(auto &message : messages) {
          if(message && std::bit_cast<int16_t>(static_cast<uint16_t>(game_clock.frame_ticks-message->expiry)) >= 0) message.reset();
        }
        if(host.player.tunnel) darker::game::update_tunnel_portal(host.player,cells,host.hangar,step);
        else if(caero) {
          darker::game::begin_hangar_return(host.player, cells, host.hangar, context.objectives_complete);
          darker::game::advance_hangar_departure(host.player, cells, host.hangar, step);
        } else if(world_mode == 1 && darker::game::begin_supply_approach(host.player,cells,host.player.supply)) {
          host.hangar.return_site = host.player.supply.site;
          host.hangar.next_return_site = host.player.supply.site;
          for(auto &weapon : combat->skimma_weapons) weapon.flags &= 0xfe;
          exchange.enter_supply(script,context);
        }
        if(context.progress) host.exit_requested = session_exit::completed;
      }
    }
    host.primary_held = primary_held;
    host.secondary_held = secondary_held;
    if(contact.contact != darker::game::city_contact::none
      && !(contact.contact == darker::game::city_contact::terrain && (host.player.lifecycle.flags & 16))) {
      std::cout << "Contact: " << (contact.contact == darker::game::city_contact::building ? "building" : "terrain")
                << "; position " << host.player.pose().position[0] << ',' << host.player.pose().position[1] << ',' << host.player.pose().position[2] << std::endl;
    }
    if(darker::game::player_crash_finished(host.player.lifecycle,game_clock.frame_ticks)) {
      host.exit_requested = session_exit::death;
      continue;
    }
    if(!front) combat->effects.advance(game_clock.frame_ticks,step);
    if(!was_crashing && host.player.lifecycle.crashing) {
      // 6F4F: recipe 7014, camera mode 2, distance 0205, largest following-distance setting.
      combat->effects.spawn(0x7014,host.player.pose().position,game_clock.frame_ticks);
      combat->missile_camera_enabled = false;
      host.player.engine_flags = 0;
      host.combat->camera_actor.reset();
      host.camera.mode = darker::game::camera_mode::level;
      host.camera.distance = 0x0205;
      host.camera.distance_step = 5;
    }
    bool const enlarged{caero && !host.player.tunnel && (host.key_down(*window,GLFW_KEY_INSERT) || host.key_down(*window,GLFW_KEY_KP_0))};
    host.camera.update_look(host.player.look_drive, host.key_down(*window,GLFW_KEY_TAB), step, (host.player.lifecycle.flags & 16) != 0);
    auto const clock{game_clock.frame_ticks};
    host.clock = clock;
    auto const count{render(clock, enlarged, step)};
    if(caero_state && (caero_state->energy.boost >> 13) > previous_cells) host.sounds.trigger(darker::audio::flight_sound::charged, clock);
    if(audio_device) {
      auto const player_sounds{host.sounds.advance(host.player, clock, host.shield_ready,
        host.camera.visible_mode() == darker::game::camera_mode::cockpit || host.camera.visible_mode() == darker::game::camera_mode::fullscreen,combat->weapon_charge,host.world_audio.audible_player())};
      audio.select_music(front ? front->music_group() : -1);
      auto const pose{host.audio_listener};
      auto const ambient{host.ambient_audio.advance({.listener{pose.position[0],pose.position[1]},.clock{clock},
        .changes{game_clock.frame_changes},.gate_site{host.hangar.return_site},.gate_active{host.hangar.sound_level != 0},
        .supplementary{exchange.supplementary_active}},cells,host.world_audio.audible_ambient(),world_mode)};
      audio.publish(host.briefing ? darker::audio::fm_frame{} : host.world_audio.mix(player_sounds,*combat,pose,clock,ambient,&host.audio_motion,&host.player.pose()));
    }
    if(caero && !host.briefing && !exchange.supplementary_active && combat->script_owner) {
      // 3E93's late-frame SI is not a recovered player continuation; blackout never returns through it.
      exchange.exchange(script,context,std::nullopt);
    }
    auto const status{host.paused ? " - paused (Pause steps; an ordinary key resumes)" : host.briefing ? " - menu / presentation"
      : host.hangar.returning == darker::game::hangar_return_phase::complete ? " - mission complete"
      : host.player.lifecycle.crashing ? (caero ? " - crashed" : " - crashed: restarting") : " - flight"};
    std::string const title{"Darker - " + std::string{world_mode == 2 ? "Underground" : world_mode == 0 ? "Delphi" : "Halon"} + " - " + std::to_string(count) + " models - " + (host.gouraud ? "Gouraud" : "flat") + status};
    glfwSetWindowTitle(window.get(), title.c_str());
    presenter.present(output);
  }
  return EXIT_SUCCESS;
}

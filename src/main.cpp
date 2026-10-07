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
#include <vector>
#include <boost/program_options.hpp>
#include <boost/scope/scope_exit.hpp>
#include <GLFW/glfw3.h>
#include "audio/flight_sounds.h"
#include "audio/fm_stream.h"
#include "audio/world_sounds.h"
#include "game/actor_activation.h"
#include "game/beacon_changes.h"
#include "game/beacon_light.h"
#include "game/city_map.h"
#include "game/city_persistence.h"
#include "game/flight_camera.h"
#include "game/game_clock.h"
#include "game/hangar.h"
#include "game/mission_combat.h"
#include "game/player_flight.h"
#include "game/scenario_world.h"
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
#include "maths/sine_table.h"
#include "platform/audio_output.h"
#include "platform/framebuffer_presenter.h"
#include "presentation/front_end.h"
#include "render/framebuffer.h"
#include "resources/archive_set.h"
#include "resources/geometry_bank.h"
#include "resources/save_file.h"

namespace {

int startup_failure(std::string_view const message) {
  /// Report expected launch failures without converting runtime programming errors into normal exits
  std::cerr << "ERROR: " << message << std::endl;
  return EXIT_FAILURE;
}

enum class session_exit { none, menu, death, completed };

struct flight_host {
  darker::game::player_flight player{};
  darker::audio::flight_sounds sounds;
  darker::audio::world_sounds world_audio;
  darker::game::flight_camera camera;
  darker::game::hangar_state hangar;
  darker::game::mission_combat *combat{nullptr};
  uint16_t available_weapons{0};
  bool primary_held{false};
  bool briefing{false};
  darker::presentation::front_end *front{nullptr};
  bool shield_ready{false};
  uint8_t engine_indicator{0};
  bool gouraud{true};
  session_exit exit_requested{session_exit::none};
  std::uint16_t clock{0};
  std::uint16_t shield_deadline{0};
  bool mouse_started{false};
  double mouse_origin_x{0};
  double mouse_origin_y{0};

  darker::game::flight_controls_input input(GLFWwindow &window) {
    /// Supply wrapping relative mouse counters and held steering keys to the original control filter
    double x{0}, y{0};
    glfwGetCursorPos(&window, &x, &y);
    if(!mouse_started) {
      mouse_origin_x = x;
      mouse_origin_y = y;
      mouse_started = true;
    }
    auto const down{[&](int const key){ return glfwGetKey(&window, key) == GLFW_PRESS; }};
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
  namespace po = boost::program_options;
  po::options_description options{"Darker (current flight reconstruction milestone)"};
  options.add_options()
    ("help,h", "show usage")
    ("data-dir", po::value<std::string>()->default_value("."), "directory containing DARKER.00 through DARKER.04 (default: current working directory)")
    ("mute", "disable PCM sound output")
    ("cheat-level-x", "enable the Level X cheat: press X during flight to advance the mission")
    ("skip-intro", "start at game selection, skipping the startup presentation and title")
    ("scale", po::value<int>()->default_value(4), "initial window scale: positive integer multiple of 320 x 240")
    ("craft", po::value<std::string>()->default_value("caero"), "caero, skimma or upgraded; selects the corresponding city")
    ("seconds", po::value<double>()->default_value(0.0), "close after this many seconds; zero waits")
    ("output", po::value<std::string>(), "write RGB PPM without opening a window");
  po::variables_map arguments;
  try {
    po::store(po::parse_command_line(argc, argv, options), arguments);
    if(arguments.contains("help")) {
      std::cout << options << std::endl;
      return EXIT_SUCCESS;
    }
    po::notify(arguments);
  } catch(po::error const &error) {
    return startup_failure(error.what());
  }
  auto const scale{arguments["scale"].as<int>()};
  constexpr int display_width{framework::render::cockpit_framebuffer::width};
  constexpr int display_height{framework::render::cockpit_framebuffer::height};
  if(scale < 1 || scale > std::numeric_limits<int>::max() / std::max(display_width,display_height)) {
    return startup_failure("--scale must be a positive integer whose window dimensions fit in an int");
  }
  auto const name{arguments["craft"].as<std::string>()};
  if(name != "caero" && name != "skimma" && name != "upgraded") return startup_failure("unknown --craft");
  auto const type{name == "caero" ? darker::graphics::craft::caero : name == "skimma" ? darker::graphics::craft::skimma : darker::graphics::craft::upgraded_skimma};
  bool const caero{type == darker::graphics::craft::caero};
  auto const seconds{arguments["seconds"].as<double>()};
  if(!std::isfinite(seconds) || seconds < 0) return startup_failure("--seconds must be finite and non-negative");
  std::optional<darker::resources::archive_set> loaded_archives;
  try {
    loaded_archives.emplace(arguments["data-dir"].as<std::string>());
  } catch(std::runtime_error const &error) {
    return startup_failure(error.what());
  }
  auto const &archives{*loaded_archives};
  unsigned int const slot{caero ? 16u : type == darker::graphics::craft::skimma ? 17u : 18u};
  auto const bitmap{darker::graphics::decode_bitmap(archives.load({.archive{0}, .slot{slot}}))};
  auto const cache{darker::graphics::make_cockpit_cache(bitmap.image)};
  auto game_palette{bitmap.palette};
  auto cockpit{cache};
  darker::resources::geometry_bank bank{archives.load({.archive{0}, .slot{caero ? 30u : 31u}})};
  std::optional<darker::game::tunnel_network> tunnel_network;
  auto cells{darker::game::make_city_map(archives.load({.archive{0}, .slot{caero ? 68u : 69u}}), caero)};
  std::array<std::uint8_t, 256> variant_limits{};
  for(std::size_t i{0}; i < bank.city_types().size(); ++i) variant_limits[i + 1] = bank.city_types()[i].variant_limit;
  darker::game::assign_city_variants(cells, variant_limits);
  flight_host host;
  if(caero) {
    darker::game::initialise_caero_hangar(host.player, cells, host.hangar, bank.header_at(bank.special_models()[25]).height);
  } else {
    host.player.craft = darker::game::skimma_flight_state{
      .pose{.position{12672, 14976, 1536}, .speed{500}}, .damage{.shield_charge{0xbfff}}, .horizontal_velocity{500},
    };
    host.player.upgraded = type == darker::graphics::craft::upgraded_skimma;
    host.player.engine_flags = 0;
  }
  darker::resources::campaign_resources campaign{archives};
  auto const *scenario{&campaign.scenario(1)};
  auto mission{scenario->records().front()};
  darker::game::beacon_changes beacon_changes;
  darker::game::world_objectives objectives{.list{mission.objective_cell_list}};
  darker::resources::font_resource const font{archives.load({.archive{0}, .slot{29}})};
  auto text{scenario->language(0, darker::resources::scenario_language::english)};
  std::unique_ptr<darker::presentation::front_end> front;
  std::filesystem::path const save_path{"darker-cpp.sav"};
  darker::resources::save_file saves;
  try {
    if(caero && std::filesystem::exists(save_path)) saves = darker::resources::decode_save(darker::resources::read_binary_file(save_path,darker::resources::save_file_size));
  } catch(std::runtime_error const &error) {
    return startup_failure(error.what());
  } catch(std::invalid_argument const &error) {
    return startup_failure(error.what());
  }
  if(caero) {
    front = std::make_unique<darker::presentation::front_end>(archives,font,campaign,saves,arguments.contains("skip-intro"));
    if(arguments.contains("cheat-level-x")) front->enable_level_skip();
    host.front = front.get();
  }
  darker::game::mission_script initial_script{
    .continuation{*mission.player_program - mission.shared.offset}, .checkpoint{*mission.player_program - mission.shared.offset},
  };
  auto script{initial_script};
  darker::game::mission_context context{.program{scenario->bytes(mission.shared)}, .text{text}, .cells{cells}, .time_multiplier{mission.time_multiplier}, .text_cursor{0}};
  std::optional<darker::game::mission_message> message;
  host.briefing = caero;
  auto initial_actors{caero ? darker::game::make_scenario_group(mission.groups[0], bank, 1, 0, mission.shared.offset)
    : std::vector<darker::game::scenario_actor>{}};
  auto combat{std::make_unique<darker::game::mission_combat>(initial_actors)};
  if(caero) combat->reserves = darker::game::make_scenario_group(mission.groups[1],bank,static_cast<uint8_t>(1 + mission.groups[0].objects.size()),0,mission.shared.offset);
  auto const activate_reserves{[&](uint8_t const opcode, uint8_t const count){
    darker::game::activate_scenario_reserves(combat->actors,combat->reserves,static_cast<darker::game::actor_category>(opcode - 9),
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
    if(selection > 0 && selection < 4) combat->primary_weapon = selection;
    else if(selection >= 4) combat->secondary_weapon = selection;
  }};
  context.select_weapon = select_weapon;
  context.change_beacons = change_beacons;
  context.activate_reserves = activate_reserves;
  if(caero) host.combat = combat.get();
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
    auto const *watched{combat->missile_camera_enabled ? combat->camera_projectile : nullptr};
    bool const cockpit_visible{!watched && host.camera.visible_mode() == darker::game::camera_mode::cockpit};
    bool const external{watched || (host.camera.visible_mode() != darker::game::camera_mode::cockpit && host.camera.visible_mode() != darker::game::camera_mode::fullscreen)};
    int const height{cockpit_visible ? (caero ? 168 : 180) : 240};
    auto const &pose{host.player.pose()};
    auto const subject{!watched ? darker::game::camera_subject::player : (watched->flags & 8) ? darker::game::camera_subject::missile_effect : darker::game::camera_subject::missile};
    auto const camera{host.camera.view(watched ? watched->placement : pose,frame_step,(host.player.lifecycle.flags & 16) != 0,subject)};
    darker::graphics::city_view view{
      .column{camera.position[0]}, .row{camera.position[1]}, .column_fraction{camera.fractions[0]}, .row_fraction{camera.fractions[1]},
      .altitude{std::bit_cast<std::int16_t>(camera.position[2])},
      .angles{.heading{camera.angles[0]}, .pitch{camera.angles[1]}, .roll{camera.angles[2]}},
      .origin{.x{160}, .y{static_cast<std::int16_t>(height / 2)}}, .bottom{height},
    };
    view.underground = host.player.tunnel.has_value();
    view.radius = view.underground ? 8 : 15;
    view.beacon_lighting = caero && !view.underground;
    view.gouraud = host.gouraud;
    darker::graphics::model_animation animation;
    animation.parameters[0] = std::bit_cast<std::int16_t>(host.hangar.extension);
    darker::graphics::update_fountain_parameters(animation, clock);
    darker::graphics::draw_sky_ground(world, view.angles, view.origin, height);
    objects.clear();
    contacts.clear();
    auto const coverage{darker::game::make_radar_coverage(cells,{pose.position[0],pose.position[1]},view.underground)};
    for(auto const &actor : combat->actors) {
      if(actor.flags & 8) continue;
      objects.push_back({.model_offset{actor.parameters.model_token}, .pose{actor.pose}});
      if(actor.category != darker::game::actor_category::stationary) {
        contacts.push_back({.position{actor.pose.position[0],actor.pose.position[1]},
          .group{actor.category == darker::game::actor_category::air ? darker::graphics::radar_group::a : darker::graphics::radar_group::b},
          .covered{coverage.contains(static_cast<uint8_t>(actor.pose.position[0] >> 8),static_cast<uint8_t>(actor.pose.position[1] >> 8))}});
      }
    }
    for(auto const *pool : {&combat->projectiles,&combat->hostile_projectiles}) {
      for(auto *shot{pool->objects().head}; shot; shot = shot->next) {
        if(!(shot->flags & 8) && !(shot == watched && host.camera.visible_mode() == darker::game::camera_mode::fullscreen)) objects.push_back({.model_offset{shot->parameters.model_token}, .pose{shot->placement}});
      }
    }
    if(external) objects.push_back({.model_offset{bank.special_models()[host.player.tunnel ? 28 : caero ? 25 : host.player.upgraded ? 27 : 26]}, .pose{pose}});
    darker::graphics::particle_scene const particles{.effects{combat->effects}, .sheet{cache}, .clock{clock}};
    auto const count{scene.draw(world, bank, cells, view, caero && !host.player.tunnel ? 0x20 : 0x60, lighting, animation, objects, &particles)};
    display = cockpit;
    auto const components{darker::graphics::cockpit_components(type)};
    std::array<std::uint8_t, 9> instruments{};
    if(auto const *state{std::get_if<darker::game::caero_flight_state>(&host.player.craft)}) {
      auto const measured{darker::graphics::measure_caero_instruments(*state, clock)};
      host.engine_indicator = darker::graphics::caero_engine_indicator(host.engine_indicator, host.player.engine_flags & 1, state->pose.speed);
      instruments = {measured.altitude, measured.impact, measured.damage_lights, measured.power_cells, measured.charging,
        state->energy.incoming_display, state->energy.reserve_display, host.engine_indicator, 0};
    } else {
      auto const &skimma{std::get<darker::game::skimma_flight_state>(host.player.craft)};
      auto const measured{darker::graphics::measure_skimma_instruments(pose.position[2], skimma.damage.shield_charge,
        skimma.damage.shield_enabled, false, clock, host.shield_deadline)};
      host.shield_ready = measured.shield_ready_sound;
      darker::graphics::draw_skimma_shield_startup(cache, display, measured.shield_startup);
      instruments[0] = measured.low_altitude;
      instruments[1] = measured.shield;
      instruments[2] = darker::graphics::skimma_speed_instrument(pose.speed, host.player.upgraded);
    }
    for(std::size_t i{0}; i < components.size(); ++i) darker::graphics::update_instrument(cache, display, type, i, 0, instruments[i]);
    darker::graphics::copy_rectangle(world.pixels, display.pixels, {.x{0}, .y{0}}, {.x{0}, .y{cockpit_visible && caero ? 8 : 0}}, 320, height);
    auto const grid{darker::game::beacon_grid_coordinates({host.player.pose().position[0],host.player.pose().position[1]})};
    darker::graphics::radar_view_state const navigation{
      .player{.x{view.column}, .y{view.row}}, .heading{view.angles.heading},
      .row{grid[1]}, .column{grid[0]},
    };
    if(cockpit_visible && caero) {
      darker::graphics::update_caero_bitmaps(cache, display, {}, {.row{navigation.row}, .column{navigation.column}, .primary_weapon{combat->primary_weapon}, .secondary_weapon{combat->secondary_weapon}});
      darker::graphics::update_compass(display, 0, darker::graphics::compass_phase(view.angles.heading));
      auto const attitude{darker::graphics::calculate_attitude(view.angles.pitch >> 6, view.angles.roll >> 6, static_cast<std::int8_t>(view.angles.pitch >> 8), false)};
      darker::graphics::draw_screen_line(display, attitude.first, attitude.last, attitude.colour);
      darker::graphics::draw_attitude_surround(display, combat->primary_weapon == 0 ? 0xff19 : 0x0019);
      if(!host.player.tunnel) darker::graphics::draw_radar_beacons(display,cells,navigation.player,navigation.heading,coverage);
      darker::graphics::draw_radar_contacts(display, navigation.player, navigation.heading, contacts);
      darker::graphics::draw_caero_frame_edges(cache, display);
      if(enlarged) darker::graphics::draw_enlarged_radar(cache, display, navigation, contacts);
    } else if(cockpit_visible) {
      darker::graphics::update_skimma_bitmaps(cache, display, type, {}, {});
      darker::graphics::draw_target_marker(display, darker::graphics::target_marker::skimma_aim, {.x{164}, .y{90}}, 14, 14);
    }
    if(message) {
      darker::graphics::draw_text(display, font, darker::resources::font_face::compact,
        text.subspan(message->offset, message->length), {.x{static_cast<int16_t>((320 - message->width) / 2)}, .y{32}}, {.ink{255}, .edge{0}});
    }
    if(host.hangar.returning == darker::game::hangar_return_phase::complete) {
      std::string const complete{"Mission complete"};
      darker::graphics::draw_text(display, font, darker::resources::font_face::interface,
        std::as_bytes(std::span{complete}), {.x{104}, .y{72}}, {.ink{255}, .edge{0}});
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
  glfwSetInputMode(window.get(), GLFW_CURSOR, caero ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
  if(glfwRawMouseMotionSupported()) glfwSetInputMode(window.get(), GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
  glfwSetWindowUserPointer(window.get(), &host);
  glfwSetKeyCallback(window.get(), [](GLFWwindow *const window, int const key, int, int const action, int){
    if(action == GLFW_RELEASE) return;
    auto &host{*static_cast<flight_host *>(glfwGetWindowUserPointer(window))};
    if(host.front && host.front->active()) {
      using darker::presentation::front_key;
      if(action != GLFW_PRESS && key != GLFW_KEY_BACKSPACE) return;
      switch(key) {
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
      default: break;
      }
      return;
    }
    if(key == GLFW_KEY_F9 && action == GLFW_PRESS) host.gouraud = !host.gouraud;
    if(key == GLFW_KEY_ESCAPE) {
      if(host.front) host.exit_requested = session_exit::menu;
      else glfwSetWindowShouldClose(window, GLFW_TRUE);
      return;
    }
    if(action == GLFW_PRESS && key == GLFW_KEY_ENTER && host.hangar.returning == darker::game::hangar_return_phase::complete) {
      host.exit_requested = session_exit::completed;
      return;
    }
    if(action == GLFW_PRESS && key >= GLFW_KEY_F1 && key <= GLFW_KEY_F6) {
      auto const selected{static_cast<darker::game::camera_mode>(key - GLFW_KEY_F1)};
      if(key >= GLFW_KEY_F5) {
        if(!(host.player.lifecycle.flags & 16)) host.camera.drop(selected,host.combat && host.combat->missile_camera_enabled && host.combat->camera_projectile ? host.combat->camera_projectile->placement : host.player.pose());
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
    if(action == GLFW_PRESS && key == GLFW_KEY_COMMA && host.camera.distance_step > 0) --host.camera.distance_step;
    if(action == GLFW_PRESS && key == GLFW_KEY_PERIOD && host.camera.distance_step < 5) ++host.camera.distance_step;
    if(key == GLFW_KEY_ENTER && host.player.lifecycle.crashing) {
      if(action == GLFW_PRESS) host.exit_requested = session_exit::death;
      return;
    }
    if(host.player.lifecycle.crashing) return;
    if(key == GLFW_KEY_X && action == GLFW_PRESS && host.front && host.front->level_skip_enabled()
      && !(host.player.lifecycle.flags & 0x20)) {
      // B926 restores C610 into C81E and requests the ordinary successful mission exit.
      host.hangar.return_site = host.hangar.next_return_site;
      host.exit_requested = session_exit::completed;
      return;
    }
    using darker::audio::flight_sound;
    using darker::game::flight_command;
    switch(key) {
      case GLFW_KEY_1:
      case GLFW_KEY_2:
        if(host.combat && action == GLFW_PRESS && (host.available_weapons & (1u << (key - GLFW_KEY_1)))) host.combat->primary_weapon = static_cast<uint8_t>(key - GLFW_KEY_1 + 1);
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
  darker::audio::fm_stream audio{framework::platform::audio_output::sample_rate};
  glfwSetCharCallback(window.get(), [](GLFWwindow *window, unsigned int code){
    auto &host{*static_cast<flight_host *>(glfwGetWindowUserPointer(window))};
    if(host.front) host.front->character(code);
  });
  glfwSetMouseButtonCallback(window.get(), [](GLFWwindow *window, int button, int action, int){
    auto &host{*static_cast<flight_host *>(glfwGetWindowUserPointer(window))};
    if(!host.front || !host.front->active() || button != GLFW_MOUSE_BUTTON_LEFT || action != GLFW_PRESS) return;
    int width{0},height{0}; double x{0},y{0};
    glfwGetWindowSize(window,&width,&height); glfwGetCursorPos(window,&x,&y);
    auto const viewport{framework::render::fit_viewport(width,height,320,240)};
    if(viewport.width > 0 && viewport.height > 0) host.front->click(static_cast<int>((x - viewport.x) * 320 / viewport.width),static_cast<int>((y - viewport.y) * 240 / viewport.height));
  });
  std::unique_ptr<framework::platform::audio_output> audio_device;
  if(!arguments.contains("mute")) {
    if(caero) {
      std::array<std::vector<std::byte>,6> songs;
      for(unsigned int group{0}; group < songs.size(); ++group) songs[group] = archives.load({0,38 + group * 5});
      audio.configure_music(archives.load({0,33}),std::move(songs));
    }
    try {
      audio_device = std::make_unique<framework::platform::audio_output>([](void *const data, std::span<float> const output) noexcept {
        static_cast<darker::audio::fm_stream *>(data)->render(output);
      }, &audio);
    } catch(std::runtime_error const &error) {
      std::cerr << "WARNING: continuing without sound: " << error.what() << std::endl;
    }
  }
  std::cout << "Mouse/arrows steer; Ctrl adjusts arrow force; Backspace brakes; Enter boosts; E engine/shield; A altitude hold; -/= Skimma speed; Tab look around; F1 cockpit; F2/F3 following; F4 full-screen; F5/F6 drop camera; M missile view; ,/. camera distance; F9 shading; Insert/keypad 0 radar; Escape returns Caero to the menu (closes Skimma); Enter after a Caero crash shows the committal sequence; Skimma restarts." << std::endl;
  std::cout << (caero ? "Caero HQ launch: boost cells charge with the engine on; press Enter once to launch." : "Skimma airborne checkpoint.") << std::endl;
  if(caero) std::cout << "Space/Enter advances the briefing. Press 1 to select Pinner Direct; Space or left mouse fires. Complete the mission objectives, then approach HQ from the north to land. Docking saves progress and opens the next briefing." << std::endl;
  auto const start{std::chrono::steady_clock::now()};
  std::uint64_t previous_interrupts{0};
  darker::game::game_clock game_clock;
  // F15A's VGA timing gives 800 pixels per line and 527 lines at the mode-13h 25.175 MHz clock.
  // AF61 waits for retrace; host swap synchronisation alone can exceed this rate or provide no pacing.
  auto const display_interval{std::chrono::duration_cast<std::chrono::steady_clock::duration>(
    std::chrono::duration<double>{800.0 * 527.0 / 25'175'000.0})};
  auto next_frame{std::chrono::steady_clock::now()};
  while(!glfwWindowShouldClose(window.get())) {
    glfwPollEvents();
    auto const frame_time{std::chrono::steady_clock::now()};
    if(frame_time < next_frame) {
      glfwWaitEventsTimeout(std::chrono::duration<double>(next_frame - frame_time).count());
      continue;
    }
    next_frame = frame_time + display_interval;
    if(host.exit_requested != session_exit::none) {
      bool const died{host.exit_requested == session_exit::death};
      auto const completed{static_cast<uint8_t>(combat->completed_objectives)};
      bool const completed_mission{host.exit_requested == session_exit::completed};
      if(front) darker::game::commit_beacon_queue(cells,scenario->bytes(mission.beacon_sequence));
      if(completed_mission && front) {
        auto updated{front->selected_pilot()};
        if(!host.player.tunnel) {
          darker::game::pack_city_state(cells,bank.city_types(),updated.delphi);
          updated.return_site = host.hangar.return_site;
          updated.weapons = host.available_weapons;
        }
        ++updated.stage;
        auto updated_saves{saves};
        auto const slot_index{static_cast<size_t>(&front->selected_pilot() - saves.pilots.data())};
        updated_saves.pilots[slot_index] = updated;
        darker::resources::write_save(save_path,updated_saves);
        saves = updated_saves;
      }
      host.player = initial_player;
      host.camera = {};
      host.hangar = {};
      host.sounds = {};
      host.world_audio = {};
      host.shield_ready = false;
      if(audio_device) audio.publish({});
      cells = initial_cells;
      combat = std::make_unique<darker::game::mission_combat>(initial_actors);
      if(caero) host.combat = combat.get();
      host.primary_held = false;
      host.engine_indicator = 0;
      host.briefing = caero;
      if(front) {
        if(completed_mission) front->continue_campaign();
        else if(died) front->show_death(completed);
        else front->return_to_menu();
        glfwSetInputMode(window.get(),GLFW_CURSOR,GLFW_CURSOR_NORMAL);
      }
      script = initial_script;
      context.text_cursor = 0;
      context.messages.clear();
      message.reset();
      host.mouse_started = false;
      host.shield_deadline = 0;
      host.clock = 0;
      host.exit_requested = session_exit::none;
      game_clock = {};
      std::cout << (caero ? (completed_mission ? "Mission saved; continuing the campaign." : died ? "Showing the Kismet committal sequence." : "Returned to the run menu.") : "Restarted the airborne checkpoint.") << std::endl;
    }
    auto const now{std::chrono::steady_clock::now()};
    double const elapsed{std::chrono::duration<double>(now - start).count()};
    if(seconds > 0 && elapsed >= seconds) break;
    auto const interrupts{static_cast<std::uint64_t>(elapsed * (1193180.0 / 2386))};
    if(front) {
      front->advance(static_cast<uint32_t>(interrupts - previous_interrupts));
      if(front->save_requested) {
        darker::resources::write_save(save_path,saves);
        front->save_requested = false;
      }
      if(front->quit_requested) break;
      bool const active{front->active()};
      if(host.briefing != active) {
        host.briefing = active;
        host.mouse_started = false;
        glfwSetInputMode(window.get(),GLFW_CURSOR,active ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
        if(!active) {
          auto const &pilot{front->selected_pilot()};
          host.available_weapons = pilot.weapons ^ front->weapon_changes();
          scenario = &campaign.scenario(pilot.stage);
          auto const record{darker::resources::select_campaign_stage(pilot.stage).record};
          mission = scenario->records()[record];
          text = scenario->language(record,darker::resources::scenario_language::english);
          bool const underground{(mission.configuration & 15) == 4};
          bank = darker::resources::geometry_bank{archives.load({0,underground ? 32u : 30u})};
          cells = darker::game::make_city_map(archives.load({0,underground ? 70u + (mission.configuration >> 4) : 68u}),!underground);
          game_palette = underground ? darker::graphics::decode_palette(archives.load({0,19}),bitmap.palette).palette : bitmap.palette;
          scene = {};
          if(underground) {
            tunnel_network.emplace(archives.load({0,78}));
            variant_limits.fill(0);
            for(size_t i{0}; i < bank.city_types().size(); ++i) variant_limits[i+1] = bank.city_types()[i].variant_limit;
            darker::game::assign_city_variants(cells,variant_limits);
          } else {
            tunnel_network.reset();
            darker::game::restore_city_state(cells,bank.city_types(),pilot.delphi,pilot.stage);
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
          } else darker::game::initialise_caero_hangar(host.player,cells,host.hangar,bank.header_at(bank.special_models()[25]).height);
          initial_player = host.player;
          initial_cells = cells;
          std::optional<darker::game::tunnel_setup> const tunnels{underground ? std::optional{darker::game::tunnel_setup{*tunnel_network,cells}} : std::nullopt};
          initial_actors = darker::game::make_scenario_group(mission.groups[0],bank,1,underground ? 2 : 0,mission.shared.offset,tunnels);
          combat = std::make_unique<darker::game::mission_combat>(initial_actors);
          combat->reserves = darker::game::make_scenario_group(mission.groups[1],bank,static_cast<uint8_t>(1 + mission.groups[0].objects.size()),underground ? 2 : 0,mission.shared.offset,tunnels);
          combat->difficulty = static_cast<uint8_t>(pilot.stage * 2);
          host.combat = combat.get();
          initial_script = {.continuation{*mission.player_program - mission.shared.offset}, .checkpoint{*mission.player_program - mission.shared.offset}};
          script = initial_script;
          context = {.program{scenario->bytes(mission.shared)}, .text{text}, .cells{cells}, .time_multiplier{mission.time_multiplier}, .text_cursor{front->consumed_text()}};
          context.activate_reserves = activate_reserves;
          context.change_beacons = change_beacons;
          context.select_weapon = select_weapon;
          beacon_changes = {};
          message.reset();
          game_clock = {};
          host.clock = 0;
          host.primary_held = false;
          // Poll the cursor-capture warp before establishing the flight mouse origin; asset loading is not flight time.
          auto const loaded_at{std::chrono::steady_clock::now()};
          previous_interrupts = static_cast<uint64_t>(std::chrono::duration<double>{loaded_at - start}.count() * (1193180.0 / 2386));
          continue;
        }
      }
    }
    game_clock.running = !host.briefing && host.hangar.returning != darker::game::hangar_return_phase::complete;
    darker::game::advance_game_clock(game_clock, interrupts - previous_interrupts);
    previous_interrupts = interrupts;
    auto const step{darker::game::consume_game_frame(game_clock)};
    auto const *caero_state{std::get_if<darker::game::caero_flight_state>(&host.player.craft)};
    auto const previous_cells{caero_state ? caero_state->energy.boost >> 13 : 0};
    darker::game::city_collision_result contact;
    bool const primary_held{glfwGetKey(window.get(), GLFW_KEY_SPACE) == GLFW_PRESS || glfwGetMouseButton(window.get(), GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS};
    if(step != 0) {
      if(host.hangar.returning == darker::game::hangar_return_phase::none) {
        contact = host.player.advance(host.input(*window), glfwGetKey(window.get(), GLFW_KEY_BACKSPACE) == GLFW_PRESS,
          step, game_clock.frame_ticks, bank, cells,tunnel_network ? &*tunnel_network : nullptr);
      } else {
        darker::game::advance_hangar_return(host.player, host.hangar, step, game_clock.frame_ticks);
        if(host.hangar.returning == darker::game::hangar_return_phase::complete) host.exit_requested = context.objectives_complete ? session_exit::completed : session_exit::menu;
      }
      if(caero) {
        combat->update_difficulty((static_cast<uint32_t>(game_clock.wraps) << 16) | game_clock.frame_ticks);
        beacon_changes.advance(cells,game_clock.frame_ticks);
        auto const *previous_missile{combat->camera_projectile};
        combat->advance(host.player,cells,bank,(static_cast<uint32_t>(game_clock.wraps) << 16) | game_clock.frame_ticks,
          step,game_clock.frame_changes,primary_held && !host.primary_held,scenario->bytes(mission.shared),mission.time_multiplier,tunnel_network ? &*tunnel_network : nullptr);
        if(previous_missile && !combat->camera_projectile) host.camera.distance = 0x8000;
        context.clock = (static_cast<uint32_t>(game_clock.wraps) << 16) | game_clock.frame_ticks;
        objectives.advance(cells,mission,host.player.tunnel ? 0x60 : 0x20);
        context.objectives_complete = objectives.complete(mission) && combat->remaining_objectives() == 0;
        context.object_counter = static_cast<uint8_t>(combat->completed_objectives);
        context.counter = combat->world_damage_counter;
        context.suppress_messages = (host.player.lifecycle.flags & 0x20) != 0;
        context.messages.clear();
        darker::game::advance_mission_script(script, context);
        for(auto const &event : context.messages) message = event;
        if(message && std::bit_cast<int16_t>(static_cast<uint16_t>(game_clock.frame_ticks - message->expiry)) >= 0) message.reset();
        if(host.player.tunnel) darker::game::update_tunnel_portal(host.player,cells,host.hangar,step);
        else {
          darker::game::begin_hangar_return(host.player, cells, host.hangar, context.objectives_complete);
          darker::game::advance_hangar_departure(host.player, cells, host.hangar, step);
        }
      }
    }
    host.primary_held = primary_held;
    if(contact.contact != darker::game::city_contact::none
      && !(contact.contact == darker::game::city_contact::terrain && (host.player.lifecycle.flags & 16))) {
      std::cout << "Contact: " << (contact.contact == darker::game::city_contact::building ? "building" : "terrain")
                << "; position " << host.player.pose().position[0] << ',' << host.player.pose().position[1] << ',' << host.player.pose().position[2] << std::endl;
    }
    bool const enlarged{caero && (glfwGetKey(window.get(), GLFW_KEY_INSERT) == GLFW_PRESS || glfwGetKey(window.get(), GLFW_KEY_KP_0) == GLFW_PRESS)};
    host.camera.update_look(host.player.look_drive, glfwGetKey(window.get(), GLFW_KEY_TAB) == GLFW_PRESS, step, (host.player.lifecycle.flags & 16) != 0);
    auto const clock{game_clock.frame_ticks};
    host.clock = clock;
    auto const count{render(clock, enlarged, step)};
    if(caero_state && (caero_state->energy.boost >> 13) > previous_cells) host.sounds.trigger(darker::audio::flight_sound::charged, clock);
    if(audio_device) {
      auto const player_sounds{host.sounds.advance(host.player, clock, host.shield_ready,
        host.camera.visible_mode() == darker::game::camera_mode::cockpit || host.camera.visible_mode() == darker::game::camera_mode::fullscreen)};
      audio.select_music(front ? front->music_group() : -1);
      audio.publish(host.briefing ? darker::audio::fm_frame{} : host.world_audio.mix(player_sounds, *combat, host.player.pose()));
    }
    auto const status{host.briefing ? " - menu / presentation"
      : host.hangar.returning == darker::game::hangar_return_phase::complete ? " - mission complete"
      : host.player.lifecycle.crashing ? (caero ? " - crashed: Enter to continue" : " - crashed: Enter to restart") : " - flight"};
    std::string const title{"Darker - " + std::string{host.player.tunnel ? "Underground" : caero ? "Delphi" : "Halon"} + " - " + std::to_string(count) + " models - " + (host.gouraud ? "Gouraud" : "flat") + status};
    glfwSetWindowTitle(window.get(), title.c_str());
    presenter.present(output);
  }
  return EXIT_SUCCESS;
}

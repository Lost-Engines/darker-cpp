#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <boost/program_options.hpp>
#include <boost/scope/scope_exit.hpp>
#include <GLFW/glfw3.h>
#include "audio/flight_sounds.h"
#include "audio/fm_stream.h"
#include "game/city_map.h"
#include "game/flight_camera.h"
#include "game/game_clock.h"
#include "game/player_flight.h"
#include "graphics/bitmap_hud.h"
#include "graphics/city_scene.h"
#include "graphics/cockpit.h"
#include "graphics/flight_instruments.h"
#include "graphics/navigation_hud.h"
#include "graphics/palette_bitmap.h"
#include "graphics/procedural_hud.h"
#include "graphics/sky_ground.h"
#include "maths/sine_table.h"
#include "platform/audio_output.h"
#include "platform/framebuffer_presenter.h"
#include "resources/archive_set.h"
#include "resources/geometry_bank.h"

namespace {

struct flight_host {
  darker::game::player_flight player{};
  darker::audio::flight_sounds sounds;
  darker::game::flight_camera camera;
  bool shield_ready{false};
  bool gouraud{true};
  bool restart_requested{false};
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

auto main(int const argc, char const *const argv[])->int try {
  /// Run the reconstructed city flight path while scenario, actors and remaining presentation systems are recovered
  namespace po = boost::program_options;
  po::options_description options{"Darker (current flight reconstruction milestone)"};
  options.add_options()
    ("help,h", "show usage")
    ("data-dir", po::value<std::string>()->default_value("."), "directory containing DARKER.00 through DARKER.04 (default: current working directory)")
    ("mute", "disable PCM sound output")
    ("craft", po::value<std::string>()->default_value("caero"), "caero, skimma or upgraded; selects the corresponding city")
    ("seconds", po::value<double>()->default_value(0.0), "close after this many seconds; zero waits")
    ("output", po::value<std::string>(), "write RGB PPM without opening a window");
  po::variables_map arguments;
  po::store(po::parse_command_line(argc, argv, options), arguments);
  if(arguments.contains("help")) {
    std::cout << options << std::endl;
    return EXIT_SUCCESS;
  }
  po::notify(arguments);
  auto const name{arguments["craft"].as<std::string>()};
  if(name != "caero" && name != "skimma" && name != "upgraded") throw std::invalid_argument{"unknown --craft"};
  auto const type{name == "caero" ? darker::graphics::craft::caero : name == "skimma" ? darker::graphics::craft::skimma : darker::graphics::craft::upgraded_skimma};
  bool const caero{type == darker::graphics::craft::caero};
  auto const seconds{arguments["seconds"].as<double>()};
  if(!std::isfinite(seconds) || seconds < 0) throw std::invalid_argument{"--seconds must be finite and non-negative"};
  darker::resources::archive_set const archives{arguments["data-dir"].as<std::string>()};
  unsigned int const slot{caero ? 16u : type == darker::graphics::craft::skimma ? 17u : 18u};
  auto const bitmap{darker::graphics::decode_bitmap(archives.load({.archive{0}, .slot{slot}}))};
  auto const cache{darker::graphics::make_cockpit_cache(bitmap.image)};
  auto cockpit{cache};
  darker::resources::geometry_bank const bank{archives.load({.archive{0}, .slot{caero ? 30u : 31u}})};
  auto cells{darker::game::make_city_map(archives.load({.archive{0}, .slot{caero ? 68u : 69u}}), caero)};
  std::array<std::uint8_t, 256> variant_limits{};
  for(std::size_t i{0}; i < bank.city_types().size(); ++i) variant_limits[i + 1] = bank.city_types()[i].variant_limit;
  darker::game::assign_city_variants(cells, variant_limits);
  flight_host host;
  // Temporary airborne checkpoint replaces the inspection camera until original scenario initialisation is connected.
  if(caero) {
    host.player.craft = darker::game::caero_flight_state{
      .pose{.position{14976, 18816, 3072}, .speed{1000}},
      .energy{.buffer{8192}, .reserve{0xcfff}, .boost{0xbfff}}, .horizontal_velocity{1000}, .flying{true},
    };
  } else {
    host.player.craft = darker::game::skimma_flight_state{
      .pose{.position{12672, 14976, 1536}, .speed{500}}, .damage{.shield_charge{0xbfff}}, .horizontal_velocity{500},
    };
    host.player.upgraded = type == darker::graphics::craft::upgraded_skimma;
    host.player.engine_flags = 0;
  }
  auto const initial_player{host.player};
  auto const initial_cells{cells};
  darker::graphics::city_renderer scene;
  darker::graphics::distance_shading const lighting;
  framework::render::indexed_cockpit_framebuffer display{}, world{};
  framework::render::cockpit_framebuffer output;
  auto const render{[&](std::uint16_t const clock, bool const enlarged, std::uint16_t const frame_step = 0){
    bool const cockpit_visible{host.camera.visible_mode() == darker::game::camera_mode::cockpit};
    bool const external{host.camera.visible_mode() != darker::game::camera_mode::cockpit && host.camera.visible_mode() != darker::game::camera_mode::fullscreen};
    int const height{cockpit_visible ? (caero ? 168 : 180) : 240};
    auto const &pose{host.player.pose()};
    auto const camera{host.camera.view(pose, frame_step, (host.player.lifecycle.flags & 16) != 0)};
    darker::graphics::city_view view{
      .column{camera.position[0]}, .row{camera.position[1]}, .column_fraction{camera.fractions[0]}, .row_fraction{camera.fractions[1]},
      .altitude{std::bit_cast<std::int16_t>(camera.position[2])},
      .angles{.heading{camera.angles[0]}, .pitch{camera.angles[1]}, .roll{camera.angles[2]}},
      .origin{.x{160}, .y{static_cast<std::int16_t>(height / 2)}}, .bottom{height},
    };
    view.beacon_lighting = caero;
    view.gouraud = host.gouraud;
    darker::graphics::model_animation animation;
    darker::graphics::update_fountain_parameters(animation, clock);
    darker::graphics::draw_sky_ground(world, view.angles, view.origin, height);
    std::array<darker::graphics::scene_object, 1> const objects{{{.model_offset{bank.special_models()[caero ? 25 : host.player.upgraded ? 27 : 26]}, .pose{pose}}}};
    auto const count{scene.draw(world, bank, cells, view, caero ? 0x20 : 0x60, lighting, animation,
      std::span<darker::graphics::scene_object const>{objects}.first(external ? 1 : 0))};
    display = cockpit;
    auto const components{darker::graphics::cockpit_components(type)};
    std::array<std::uint8_t, 9> instruments{};
    if(auto const *state{std::get_if<darker::game::caero_flight_state>(&host.player.craft)}) {
      auto const measured{darker::graphics::measure_caero_instruments(*state, clock)};
      instruments = {measured.altitude, measured.impact, measured.damage_lights, measured.power_cells, measured.charging,
        state->energy.incoming_display, state->energy.reserve_display, static_cast<std::uint8_t>(host.player.engine_flags & 1), 0};
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
    darker::graphics::radar_view_state const navigation{
      .player{.x{view.column}, .y{view.row}}, .heading{view.angles.heading},
      .row{static_cast<std::uint8_t>(view.row >> 8)}, .column{static_cast<std::uint8_t>(view.column >> 8)},
    };
    if(cockpit_visible && caero) {
      darker::graphics::update_caero_bitmaps(cache, display, {}, {.row{navigation.row}, .column{navigation.column}, .primary_weapon{0}, .secondary_weapon{0}});
      darker::graphics::update_compass(display, 0, darker::graphics::compass_phase(view.angles.heading));
      auto const attitude{darker::graphics::calculate_attitude(view.angles.pitch >> 6, view.angles.roll >> 6, static_cast<std::int8_t>(view.angles.pitch >> 8), false)};
      darker::graphics::draw_screen_line(display, attitude.first, attitude.last, attitude.colour);
      darker::graphics::draw_attitude_surround(display, 0);
      if(enlarged) darker::graphics::draw_enlarged_radar(cache, display, navigation, {});
    } else if(cockpit_visible) {
      darker::graphics::update_skimma_bitmaps(cache, display, type, {}, {});
      darker::graphics::draw_target_marker(display, darker::graphics::target_marker::skimma_aim, {.x{164}, .y{90}}, 14, 14);
    }
    framework::render::expand_palette(display, bitmap.palette.colours, output);
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
  if(!glfwInit()) throw std::runtime_error{"GLFW initialisation failed"};
  boost::scope::scope_exit terminate_glfw{[]{ glfwTerminate(); }};
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
  std::unique_ptr<GLFWwindow, decltype(&glfwDestroyWindow)> const window{
    glfwCreateWindow(960, 720, "Darker", nullptr, nullptr), glfwDestroyWindow,
  };
  if(!window) throw std::runtime_error{"cannot create the GLFW window"};
  glfwMakeContextCurrent(window.get());
  glfwSwapInterval(1);
  glfwSetInputMode(window.get(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
  if(glfwRawMouseMotionSupported()) glfwSetInputMode(window.get(), GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
  glfwSetWindowUserPointer(window.get(), &host);
  glfwSetKeyCallback(window.get(), [](GLFWwindow *const window, int const key, int, int const action, int){
    if(action == GLFW_RELEASE) return;
    auto &host{*static_cast<flight_host *>(glfwGetWindowUserPointer(window))};
    if(key == GLFW_KEY_F9 && action == GLFW_PRESS) host.gouraud = !host.gouraud;
    if(key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(window, GLFW_TRUE);
    if(action == GLFW_PRESS && key >= GLFW_KEY_F1 && key <= GLFW_KEY_F6) {
      auto const selected{static_cast<darker::game::camera_mode>(key - GLFW_KEY_F1)};
      if(key >= GLFW_KEY_F5) {
        if(!(host.player.lifecycle.flags & 16)) host.camera.drop(selected, host.player.pose());
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
      if(action == GLFW_PRESS) host.restart_requested = true;
      return;
    }
    if(host.player.lifecycle.crashing) return;
    using darker::audio::flight_sound;
    using darker::game::flight_command;
    switch(key) {
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
  std::unique_ptr<framework::platform::audio_output> audio_device;
  if(!arguments.contains("mute")) {
    try {
      audio_device = std::make_unique<framework::platform::audio_output>([](void *const data, std::span<float> const output) noexcept {
        static_cast<darker::audio::fm_stream *>(data)->render(output);
      }, &audio);
    } catch(std::exception const &error) {
      std::cerr << "WARNING: continuing without sound: " << error.what() << std::endl;
    }
  }
  std::cout << "Flight checkpoint: mouse/arrows steer; Ctrl adjusts arrow force; Backspace brakes; Enter boosts; E engine/shield; A altitude hold; -/= Skimma speed; Tab look around; F1 cockpit; F2/F3 following; F4 full-screen; F5/F6 drop camera; ,/. camera distance; F9 shading; Insert/keypad 0 radar; Escape closes; Enter after a crash restarts the checkpoint." << std::endl;
  std::cout << "Original flight, charging, city collisions and flight cameras. Airborne checkpoint; missions, weapons, world sound and original death screens are not connected yet." << std::endl;
  auto const start{std::chrono::steady_clock::now()};
  std::uint64_t previous_interrupts{0};
  darker::game::game_clock game_clock;
  while(!glfwWindowShouldClose(window.get())) {
    glfwPollEvents();
    if(host.restart_requested) {
      host.player = initial_player;
      host.camera = {};
      host.sounds = {};
      host.shield_ready = false;
      if(audio_device) audio.publish({});
      cells = initial_cells;
      host.mouse_started = false;
      host.shield_deadline = 0;
      host.clock = 0;
      host.restart_requested = false;
      game_clock = {};
      std::cout << "Restarted the airborne checkpoint." << std::endl;
    }
    auto const now{std::chrono::steady_clock::now()};
    double const elapsed{std::chrono::duration<double>(now - start).count()};
    if(seconds > 0 && elapsed >= seconds) break;
    auto const interrupts{static_cast<std::uint64_t>(elapsed * (1193180.0 / 2386))};
    darker::game::advance_game_clock(game_clock, interrupts - previous_interrupts);
    previous_interrupts = interrupts;
    auto const step{darker::game::consume_game_frame(game_clock)};
    auto const *caero_state{std::get_if<darker::game::caero_flight_state>(&host.player.craft)};
    auto const previous_cells{caero_state ? caero_state->energy.boost >> 13 : 0};
    auto const contact{host.player.advance(host.input(*window), glfwGetKey(window.get(), GLFW_KEY_BACKSPACE) == GLFW_PRESS,
      step, game_clock.frame_ticks, bank, cells)};
    if(contact.contact != darker::game::city_contact::none) {
      std::cout << "Contact: " << (contact.contact == darker::game::city_contact::building ? "building" : "terrain")
                << "; position " << host.player.pose().position[0] << ',' << host.player.pose().position[1] << ',' << host.player.pose().position[2] << std::endl;
    }
    bool const enlarged{caero && (glfwGetKey(window.get(), GLFW_KEY_INSERT) == GLFW_PRESS || glfwGetKey(window.get(), GLFW_KEY_KP_0) == GLFW_PRESS)};
    host.camera.update_look(host.player.look_drive, glfwGetKey(window.get(), GLFW_KEY_TAB) == GLFW_PRESS, step, (host.player.lifecycle.flags & 16) != 0);
    auto const clock{game_clock.frame_ticks};
    host.clock = clock;
    auto const count{render(clock, enlarged, step)};
    if(caero_state && (caero_state->energy.boost >> 13) > previous_cells) host.sounds.trigger(darker::audio::flight_sound::charged, clock);
    if(audio_device) audio.publish(host.sounds.advance(host.player, clock, host.shield_ready, host.camera.visible_mode() == darker::game::camera_mode::cockpit || host.camera.visible_mode() == darker::game::camera_mode::fullscreen));
    std::string const title{"Darker - " + std::string{caero ? "Delphi" : "Halon"} + " - " + std::to_string(count) + " models - " + (host.gouraud ? "Gouraud" : "flat") + (host.player.lifecycle.crashing ? " - crashed: Enter to restart" : " - flight")};
    glfwSetWindowTitle(window.get(), title.c_str());
    presenter.present(output);
    glfwWaitEventsTimeout(0.01);
  }
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << std::endl;
  return EXIT_FAILURE;
}

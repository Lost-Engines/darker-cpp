#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include <boost/program_options.hpp>
#include <boost/scope/scope_exit.hpp>
#include <GLFW/glfw3.h>
#include "game/skimma_weapons.h"
#include "graphics/bitmap_hud.h"
#include "graphics/camera.h"
#include "graphics/cockpit.h"
#include "graphics/model_renderer.h"
#include "graphics/navigation_hud.h"
#include "graphics/palette_bitmap.h"
#include "graphics/procedural_hud.h"
#include "platform/framebuffer_presenter.h"
#include "resources/archive_set.h"
#include "resources/geometry_bank.h"

namespace {

struct inspector {
  darker::graphics::craft type;
  framework::render::indexed_cockpit_framebuffer cache;
  framework::render::indexed_cockpit_framebuffer screen;
  std::array<std::uint8_t, 9> states{};
  std::size_t selected{0};
  std::vector<int> keys;

  void set(std::size_t const component, unsigned int const value) {
    /// Apply a state transition against the original immutable cache
    if(value > 255) throw std::out_of_range{"instrument state must fit one byte"};
    auto const state{static_cast<std::uint8_t>(value)};
    darker::graphics::update_instrument(cache, screen, type, component, states.at(component), state);
    states[component] = state;
  }

  void handle(int const key) {
    /// Inspection controls supply display values without simulating the game's producers
    auto const count{darker::graphics::cockpit_components(type).size()};
    auto const limit{static_cast<unsigned int>(darker::graphics::instrument_limit(type, selected))};
    unsigned int const value{static_cast<unsigned int>(states[selected] & 127)};
    unsigned int const flag{static_cast<unsigned int>(states[selected] & 128)};
    switch(key) {
    case GLFW_KEY_UP: selected = (selected + count - 1) % count; break;
    case GLFW_KEY_DOWN: selected = (selected + 1) % count; break;
    case GLFW_KEY_LEFT: set(selected, (value ? value - 1 : 0) | flag); break;
    case GLFW_KEY_RIGHT: set(selected, std::min(value + 1, limit) | flag); break;
    case GLFW_KEY_HOME: set(selected, 0); break;
    case GLFW_KEY_END: set(selected, limit | flag); break;
    case GLFW_KEY_D:
      if(type == darker::graphics::craft::caero) set(7, 1 | ((states[7] ^ 128) & 128));
      break;
    case GLFW_KEY_R:
    case GLFW_KEY_F:
      for(std::size_t i{0}; i < count; ++i) set(i, key == GLFW_KEY_R ? 0 : static_cast<unsigned int>(darker::graphics::instrument_limit(type, i)));
      break;
    default: break;
    }
  }
};

// Temporary application milestone; world traversal will supply the models and camera.
struct model_inspection {
  darker::resources::geometry_bank bank;
  darker::graphics::camera_angles angles{.heading{8192}, .pitch{57344}};
  bool dragging{false};
  double last_x{0};
  double last_y{0};

  void mouse(double const x, double const y, bool const pressed) {
    /// Temporary model inspection until the flight loop supplies camera state
    if(pressed && dragging) {
      auto const dx{static_cast<int>(std::clamp(x - last_x, -512.0, 512.0) * 64)};
      auto const dy{static_cast<int>(std::clamp(y - last_y, -512.0, 512.0) * 64)};
      angles.heading = static_cast<std::uint16_t>(angles.heading + dx);
      int const pitch{angles.pitch > 32767 ? static_cast<int>(angles.pitch) - 65536 : angles.pitch};
      angles.pitch = static_cast<std::uint16_t>(std::clamp(pitch + dy, -12000, 12000));
    }
    dragging = pressed;
    last_x = x;
    last_y = y;
  }

  void draw(framework::render::indexed_cockpit_framebuffer &target, darker::graphics::craft const type) const {
    /// Exercise original geometry and camera coefficients at a safe fixed depth, then compose the cockpit view
    framework::render::indexed_cockpit_framebuffer view{};
    darker::graphics::projection_parameters const projection{
      .axes{darker::graphics::make_camera_basis(angles)},
      .depth{.whole{2048}}, .origin{.x{160}, .y{110}},
    };
    darker::graphics::model_colours colours{.dynamic{17}};
    for(std::size_t i{0}; i < colours.shades.size(); ++i) colours.shades[i] = static_cast<std::uint8_t>(i);
    int const top{type == darker::graphics::craft::caero ? 8 : 0};
    int const height{type == darker::graphics::craft::caero ? 168 : 180};
    darker::graphics::draw_flat_model(view, bank.model_pool(), bank.city_model_offset(30, 0, 0x20), projection, colours, height);
    darker::graphics::copy_rectangle(view.pixels, target.pixels, {.x{0}, .y{0}}, {.x{0}, .y{top}}, 320, height);
    if(type == darker::graphics::craft::caero) {
      auto const attitude{darker::graphics::calculate_attitude(0, 0, 0, false)};
      darker::graphics::draw_hud_line(target, attitude.first, attitude.last, attitude.colour);
      darker::graphics::draw_attitude_surround(target, 0);
    } else {
      darker::graphics::draw_target_marker(target, darker::graphics::target_marker::skimma_aim, {.x{164}, .y{90}}, 14, 14);
    }
  }
};

} // namespace

auto main(int const argc, char const *const argv[])->int try {
  /// Present original model bytecode and cockpit graphics through the single application
  namespace po = boost::program_options;
  po::options_description options{"Darker (current model and cockpit milestone)"};
  options.add_options()
    ("help,h", "show usage")
    ("data-dir", po::value<std::string>()->default_value("."), "directory containing DARKER.00 through DARKER.04 (default: current working directory)")
    ("craft", po::value<std::string>()->default_value("caero"), "caero, skimma or upgraded")
    ("fill", po::value<unsigned int>()->default_value(50), "initial strip-count percentage, 0 to 100 (inspection only)")
    ("field", po::value<std::size_t>()->default_value(0), "zero-based instrument selection")
    ("states", po::value<std::vector<unsigned int>>()->multitoken(), "apply successive raw state bytes to the selected instrument")
    ("static", "show the original cache without clearing the windscreen or drawing instruments")
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
  auto const fill{arguments["fill"].as<unsigned int>()};
  auto const seconds{arguments["seconds"].as<double>()};
  if(fill > 100) throw std::invalid_argument{"--fill must be between 0 and 100"};
  if(!std::isfinite(seconds) || seconds < 0) throw std::invalid_argument{"--seconds must be finite and non-negative"};
  darker::resources::archive_set const archives{arguments["data-dir"].as<std::string>()};
  unsigned int const slot{type == darker::graphics::craft::caero ? 16u : type == darker::graphics::craft::skimma ? 17u : 18u};
  auto const bitmap{darker::graphics::decode_bitmap(archives.load({.archive{0}, .slot{slot}}))};
  auto const cache{darker::graphics::make_cockpit_cache(bitmap.image)};
  inspector state{.type{type}, .cache{cache}, .screen{cache}, .selected{arguments["field"].as<std::size_t>()}, .keys{}};
  auto const components{darker::graphics::cockpit_components(type)};
  if(state.selected >= components.size()) throw std::invalid_argument{"--field outside craft instrument list"};
  darker::graphics::radar_view_state const navigation{.player{.x{60 * 256}, .y{60 * 256}}, .heading{0}, .row{1}, .column{1}};
  std::array<darker::graphics::radar_contact, 2> const contacts{{
    {.position{.x{50 * 256}, .y{70 * 256}}, .group{darker::graphics::radar_group::a}},
    {.position{.x{65 * 256}, .y{55 * 256}}, .group{darker::graphics::radar_group::b}},
  }};
  bool const static_view{arguments.contains("static")};
  if(!static_view) {
    std::size_t black{0};
    while(black < 256 && (!bitmap.palette.defined[black] || bitmap.palette.colours[black].red || bitmap.palette.colours[black].green || bitmap.palette.colours[black].blue)) ++black;
    if(black == 256) throw std::runtime_error{"cockpit palette has no defined black entry"};
    darker::graphics::clear_windscreen(state.screen, type, static_cast<std::uint8_t>(black));
    for(std::size_t i{0}; i < components.size(); ++i) state.set(i, static_cast<unsigned int>(darker::graphics::instrument_limit(type, i) * fill / 100));
    if(type == darker::graphics::craft::caero) {
      // Temporary display fixture until player position and weapon selection supply these fields.
      darker::graphics::update_caero_bitmaps(cache, state.screen, {}, {.row{navigation.row}, .column{navigation.column}, .primary_weapon{1}, .secondary_weapon{5}});
      darker::graphics::update_compass(state.screen, 0, darker::graphics::compass_phase(navigation.heading));
      darker::graphics::draw_radar_contacts(state.screen, navigation.player, navigation.heading, contacts);
    } else {
      darker::graphics::update_skimma_bitmaps(cache, state.screen, type, {},
        {.bearing{1}, .weapons{1, 2, static_cast<std::uint8_t>(type == darker::graphics::craft::upgraded_skimma ? 3 : 0)}});
      darker::game::weapon_ammunition ammunition;
      darker::game::refill_skimma_weapon(ammunition, 0);
      if(auto const ring{darker::game::calculate_weapon_ring(ammunition, {.reload_deadline{0}, .spread{252}}, 0, 1)}) {
        darker::graphics::draw_skimma_weapon_ring(cache, state.screen, type, 0, ring->radius, ring->remaining);
      }
    }
    if(arguments.contains("states")) {
      for(auto const value : arguments["states"].as<std::vector<unsigned int>>()) state.set(state.selected, value);
    }
  } else if(arguments.contains("states")) throw std::invalid_argument{"--states cannot be used with --static"};
  model_inspection model{.bank{archives.load({.archive{0}, .slot{type == darker::graphics::craft::caero ? 30u : 31u}})}};
  auto display{state.screen};
  if(!static_view) model.draw(display, type);
  framework::render::cockpit_framebuffer output;
  framework::render::expand_palette(display, bitmap.palette.colours, output);
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
  glfwSetWindowUserPointer(window.get(), &state);
  state.keys.reserve(64);
  glfwSetKeyCallback(window.get(), [](GLFWwindow *const window, int const key, int, int const action, int){
    if(action != GLFW_PRESS && action != GLFW_REPEAT) return;
    if(key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(window, GLFW_TRUE);
    else {
      auto &pending{static_cast<inspector*>(glfwGetWindowUserPointer(window))->keys};
      // Storage is reserved before callbacks are installed; bound the event queue.
      if(pending.size() < pending.capacity()) pending.push_back(key);
    }
  });
  framework::platform::framebuffer_presenter presenter{*window};
  std::cout << "Inspection only: isolated original model, no flight simulation. Left-drag: rotate model view. Up/down: instrument; left/right: count; Home/End: empty/full; R/F: all empty/full; D: Caero engine dimming; Escape: close.\n";
  for(std::size_t i{0}; i < components.size(); ++i) std::cout << i << ": " << components[i].label << " (max " << darker::graphics::instrument_limit(type, i) << ")\n";
  std::cout << "Hold Insert or keypad 0 for the Caero enlarged radar.\n";
  auto const start{std::chrono::steady_clock::now()};
  while(!glfwWindowShouldClose(window.get())) {
    glfwPollEvents();
    if(seconds > 0 && std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count() >= seconds) break;
    if(!static_view) {
      for(auto const key : state.keys) state.handle(key);
    }
    state.keys.clear();
    std::string const title{"Darker - " + name + " - " + (static_view ? std::string{"static cache"} : std::string{components[state.selected].label} + " " + std::to_string(state.states[state.selected] & 127) + "/" + std::to_string(darker::graphics::instrument_limit(type, state.selected)))};
    glfwSetWindowTitle(window.get(), title.c_str());
    bool const enlarged{!static_view && type == darker::graphics::craft::caero
      && (glfwGetKey(window.get(), GLFW_KEY_INSERT) == GLFW_PRESS || glfwGetKey(window.get(), GLFW_KEY_KP_0) == GLFW_PRESS)};
    display = state.screen;
    if(!static_view) {
      double x{0}, y{0};
      glfwGetCursorPos(window.get(), &x, &y);
      model.mouse(x, y, glfwGetMouseButton(window.get(), GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS);
      model.draw(display, type);
    }
    if(enlarged) {
      darker::graphics::draw_enlarged_radar(cache, display, navigation, contacts);
    }
    framework::render::expand_palette(display, bitmap.palette.colours, output);
    presenter.present(output);
    glfwWaitEventsTimeout(0.01);
  }
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << std::endl;
  return EXIT_FAILURE;
}

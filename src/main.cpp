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
#include "graphics/cockpit.h"
#include "graphics/palette_bitmap.h"
#include "platform/framebuffer_presenter.h"
#include "resources/archive_set.h"

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

} // namespace

auto main(int const argc, char const *const argv[])->int try {
  /// Assemble an original cockpit and expose masked instruments for inspection
  namespace po = boost::program_options;
  po::options_description options{"Darker (current cockpit milestone)"};
  options.add_options()
    ("help,h", "show usage")
    ("data-dir", po::value<std::string>()->required(), "directory containing original DARKER.00 through DARKER.04")
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
  bool const static_view{arguments.contains("static")};
  if(!static_view) {
    std::size_t black{0};
    while(black < 256 && (!bitmap.palette.defined[black] || bitmap.palette.colours[black].red || bitmap.palette.colours[black].green || bitmap.palette.colours[black].blue)) ++black;
    if(black == 256) throw std::runtime_error{"cockpit palette has no defined black entry"};
    darker::graphics::clear_windscreen(state.screen, type, static_cast<std::uint8_t>(black));
    for(std::size_t i{0}; i < components.size(); ++i) state.set(i, static_cast<unsigned int>(darker::graphics::instrument_limit(type, i) * fill / 100));
    if(arguments.contains("states")) {
      for(auto const value : arguments["states"].as<std::vector<unsigned int>>()) state.set(state.selected, value);
    }
  } else if(arguments.contains("states")) throw std::invalid_argument{"--states cannot be used with --static"};
  framework::render::cockpit_framebuffer output;
  framework::render::expand_palette(state.screen, bitmap.palette.colours, output);
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
  std::cout << "Inspection only: no world or gameplay simulation. Up/down: instrument; left/right: count; Home/End: empty/full; R/F: all empty/full; D: Caero engine dimming; Escape: close.\n";
  for(std::size_t i{0}; i < components.size(); ++i) std::cout << i << ": " << components[i].label << " (max " << darker::graphics::instrument_limit(type, i) << ")\n";
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
    framework::render::expand_palette(state.screen, bitmap.palette.colours, output);
    presenter.present(output);
    glfwWaitEventsTimeout(0.01);
  }
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << std::endl;
  return EXIT_FAILURE;
}

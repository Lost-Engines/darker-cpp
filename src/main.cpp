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
#include <boost/program_options.hpp>
#include <boost/scope/scope_exit.hpp>
#include <GLFW/glfw3.h>
#include "game/city_map.h"
#include "graphics/bitmap_hud.h"
#include "graphics/city_scene.h"
#include "graphics/cockpit.h"
#include "graphics/navigation_hud.h"
#include "graphics/palette_bitmap.h"
#include "graphics/procedural_hud.h"
#include "maths/sine_table.h"
#include "platform/framebuffer_presenter.h"
#include "resources/archive_set.h"
#include "resources/geometry_bank.h"

namespace {

struct inspection_camera {
  double column{58.5};
  double row{73.5};
  double altitude{2048};
  darker::graphics::camera_angles angles{.pitch{61440}};
  bool dragging{false};
  double last_x{0};
  double last_y{0};

  void update(GLFWwindow &window, double const seconds) {
    /// Temporary free-camera controls until the game's input and flight loop supply camera state
    double x{0}, y{0};
    glfwGetCursorPos(&window, &x, &y);
    bool const pressed{glfwGetMouseButton(&window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS};
    if(pressed && dragging) {
      auto const dx{static_cast<int>(std::clamp(x - last_x, -512.0, 512.0) * 64)};
      auto const dy{static_cast<int>(std::clamp(y - last_y, -512.0, 512.0) * 64)};
      angles.heading = static_cast<std::uint16_t>(angles.heading - dx);
      int const pitch{angles.pitch > 32767 ? static_cast<int>(angles.pitch) - 65536 : angles.pitch};
      angles.pitch = static_cast<std::uint16_t>(std::clamp(pitch + dy, -12000, 12000));
    }
    dragging = pressed;
    last_x = x;
    last_y = y;
    auto const down{[&](int const key){ return glfwGetKey(&window, key) == GLFW_PRESS ? 1 : 0; }};
    int const forward{down(GLFW_KEY_W) - down(GLFW_KEY_S)};
    int const strafe{down(GLFW_KEY_D) - down(GLFW_KEY_A)};
    double const sine{darker::maths::original_sine[angles.heading >> 6] / 32768.0};
    double const cosine{darker::maths::original_sine[((angles.heading >> 6) + 256) % 1024] / 32768.0};
    double const distance{std::min(seconds, 0.1) * 2};
    column = std::clamp(column + (strafe * cosine - forward * sine) * distance, 0.0, 127.99);
    row = std::clamp(row - (forward * cosine + strafe * sine) * distance, 0.0, 127.99);
    altitude = std::clamp(altitude + (down(GLFW_KEY_R) - down(GLFW_KEY_F)) * distance * 256, 64.0, 4096.0);
  }

  darker::graphics::city_view view(int const height) const {
    /// Convert only the inspection camera into original fixed-point position fields
    auto const x{static_cast<std::uint32_t>(column * 65536)};
    auto const y{static_cast<std::uint32_t>(row * 65536)};
    return {
      .column{static_cast<std::uint16_t>(x >> 8)}, .row{static_cast<std::uint16_t>(y >> 8)},
      .column_fraction{static_cast<std::uint8_t>(x)}, .row_fraction{static_cast<std::uint8_t>(y)},
      .altitude{static_cast<std::int16_t>(altitude)}, .angles{angles},
      .origin{.x{160}, .y{static_cast<std::int16_t>(height / 2)}}, .bottom{height},
    };
  }
};

} // namespace

auto main(int const argc, char const *const argv[])->int try {
  /// Assemble an original city and cockpit in the single application while the gameplay loop is reconstructed
  namespace po = boost::program_options;
  po::options_description options{"Darker (current city rendering milestone)"};
  options.add_options()
    ("help,h", "show usage")
    ("data-dir", po::value<std::string>()->default_value("."), "directory containing DARKER.00 through DARKER.04 (default: current working directory)")
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
  auto const components{darker::graphics::cockpit_components(type)};
  for(std::size_t i{0}; i < components.size(); ++i) {
    auto const value{static_cast<std::uint8_t>(darker::graphics::instrument_limit(type, i) / 2)};
    darker::graphics::update_instrument(cache, cockpit, type, i, 0, value);
  }
  darker::resources::geometry_bank const bank{archives.load({.archive{0}, .slot{caero ? 30u : 31u}})};
  auto cells{darker::game::make_city_map(archives.load({.archive{0}, .slot{caero ? 68u : 69u}}), caero)};
  std::array<std::uint8_t, 256> variant_limits{};
  for(std::size_t i{0}; i < bank.city_types().size(); ++i) variant_limits[i + 1] = bank.city_types()[i].variant_limit;
  darker::game::assign_city_variants(cells, variant_limits);
  inspection_camera camera;
  if(!caero) {
    camera.column = 49.5;
    camera.row = 58.5;
    camera.altitude = 768;
    camera.angles.pitch = 0;
  }
  darker::graphics::city_renderer scene;
  darker::graphics::distance_shading const lighting;
  framework::render::indexed_cockpit_framebuffer display{}, world{};
  framework::render::cockpit_framebuffer output;
  auto const render{[&](std::uint16_t const clock, bool const enlarged){
    int const height{caero ? 168 : 180};
    auto view{camera.view(height)};
    view.beacon_lighting = caero;
    darker::graphics::model_animation animation;
    darker::graphics::update_fountain_parameters(animation, clock);
    world.pixels.fill(0);
    auto const count{scene.draw(world, bank, cells, view, caero ? 0x20 : 0x60, lighting, animation)};
    display = cockpit;
    darker::graphics::copy_rectangle(world.pixels, display.pixels, {.x{0}, .y{0}}, {.x{0}, .y{caero ? 8 : 0}}, 320, height);
    darker::graphics::radar_view_state const navigation{
      .player{.x{view.column}, .y{view.row}}, .heading{view.angles.heading},
      .row{static_cast<std::uint8_t>(view.row >> 8)}, .column{static_cast<std::uint8_t>(view.column >> 8)},
    };
    if(caero) {
      darker::graphics::update_caero_bitmaps(cache, display, {}, {.row{navigation.row}, .column{navigation.column}, .primary_weapon{1}, .secondary_weapon{5}});
      darker::graphics::update_compass(display, 0, darker::graphics::compass_phase(view.angles.heading));
      auto const attitude{darker::graphics::calculate_attitude(view.angles.pitch >> 6, view.angles.roll >> 6, static_cast<std::int8_t>(view.angles.pitch >> 8), false)};
      darker::graphics::draw_screen_line(display, attitude.first, attitude.last, attitude.colour);
      darker::graphics::draw_attitude_surround(display, 0);
      if(enlarged) darker::graphics::draw_enlarged_radar(cache, display, navigation, {});
    } else {
      darker::graphics::update_skimma_bitmaps(cache, display, type, {}, {.bearing{1}, .weapons{1, 2, static_cast<std::uint8_t>(type == darker::graphics::craft::upgraded_skimma ? 3 : 0)}});
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
  glfwSetKeyCallback(window.get(), [](GLFWwindow *const window, int const key, int, int const action, int){
    if(key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) glfwSetWindowShouldClose(window, GLFW_TRUE);
  });
  framework::platform::framebuffer_presenter presenter{*window};
  std::cout << "City inspection: W/A/S/D move; R/F rise/lower; left-drag look; Insert/keypad 0 enlarged radar; Escape close." << std::endl;
  std::cout << "Free camera only: no flight, collisions or missions. Original distance shading in flat mode; gauges remain sample values." << std::endl;
  auto const start{std::chrono::steady_clock::now()};
  auto previous{start};
  while(!glfwWindowShouldClose(window.get())) {
    glfwPollEvents();
    auto const now{std::chrono::steady_clock::now()};
    double const elapsed{std::chrono::duration<double>(now - start).count()};
    if(seconds > 0 && elapsed >= seconds) break;
    camera.update(*window, std::chrono::duration<double>(now - previous).count());
    previous = now;
    bool const enlarged{caero && (glfwGetKey(window.get(), GLFW_KEY_INSERT) == GLFW_PRESS || glfwGetKey(window.get(), GLFW_KEY_KP_0) == GLFW_PRESS)};
    auto const clock{static_cast<std::uint16_t>(std::fmod(elapsed * (1193180.0 / 2386), 65536.0))};
    auto const count{render(clock, enlarged)};
    std::string const title{"Darker - " + std::string{caero ? "Delphi" : "Halon"} + " - " + std::to_string(count) + " models"};
    glfwSetWindowTitle(window.get(), title.c_str());
    presenter.present(output);
    glfwWaitEventsTimeout(0.01);
  }
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << std::endl;
  return EXIT_FAILURE;
}

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
#include "graphics/palette_bitmap.h"
#include "platform/framebuffer_presenter.h"
#include "platform/input.h"
#include "render/indexed_framebuffer.h"
#include "resources/archive_set.h"

auto main(int const argc, char const *const argv[])->int try {
  /// Present original source sheets through the platform-independent indexed image path
  boost::program_options::options_description options{"Original bitmap viewer"};
  options.add_options()
    ("help,h", "show usage")
    ("data-dir", boost::program_options::value<std::string>()->required(), "directory containing the original DARKER.00 through DARKER.04 files")
    ("slot", boost::program_options::value<unsigned int>()->default_value(16), "archive 00 source sheet: 15, 16 (Caero), 17 or 18 (Skimma)")
    ("seconds", boost::program_options::value<double>()->default_value(0.0), "close after this many seconds; zero waits until closed")
    ("output", boost::program_options::value<std::string>(), "write an RGB PPM instead of opening a window");
  boost::program_options::variables_map arguments;
  boost::program_options::store(boost::program_options::parse_command_line(argc, argv, options), arguments);
  if(arguments.contains("help")) {
    std::cout << options << std::endl;
    return EXIT_SUCCESS;
  }
  boost::program_options::notify(arguments);
  auto const slot{arguments["slot"].as<unsigned int>()};
  if(slot < 15 || slot > 18) throw std::invalid_argument{"supported source sheet slots are 15 through 18"};
  auto const seconds{arguments["seconds"].as<double>()};
  if(!std::isfinite(seconds) || seconds < 0.0) throw std::invalid_argument{"--seconds must be finite and non-negative"};
  darker::resources::archive_set const archives{arguments["data-dir"].as<std::string>()};
  auto const bitmap{darker::graphics::decode_bitmap(archives.load({.archive{0}, .slot{slot}}))};
  framework::render::framebuffer framebuffer;
  framework::render::expand_palette(bitmap.image, bitmap.palette.colours, framebuffer);
  std::cout << "Archive 00, slot " << slot << ": 320x200 source sheet; " << bitmap.palette.defined.count()
            << " defined source RGB colours. No runtime fades or VGA DAC conversion applied." << std::endl;
  if(arguments.contains("output")) {
    std::ofstream output{arguments["output"].as<std::string>(), std::ios::binary};
    output.exceptions(std::ios::failbit | std::ios::badbit);
    output << "P6\n320 200\n255\n";
    for(auto const &pixel : framebuffer.pixels) {
      output.put(static_cast<char>(pixel.red));
      output.put(static_cast<char>(pixel.green));
      output.put(static_cast<char>(pixel.blue));
    }
    output.close();
    return EXIT_SUCCESS;
  }
  glfwSetErrorCallback([](int const code, char const *const message){
    std::cerr << "ERROR: GLFW " << code << ": " << message << std::endl;
  });
  if(!glfwInit()) throw std::runtime_error{"GLFW initialisation failed"};
  boost::scope::scope_exit terminate_glfw{[]{
    glfwTerminate();
  }};
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
  framework::platform::input_state input;
  std::string const title{"Darker source bitmap - archive 00 / " + std::to_string(slot)};
  std::unique_ptr<GLFWwindow, decltype(&glfwDestroyWindow)> const window{
    glfwCreateWindow(960, 600, title.c_str(), nullptr, nullptr), glfwDestroyWindow,
  };
  if(!window) throw std::runtime_error{"cannot create the GLFW window"};
  glfwMakeContextCurrent(window.get());
  glfwSwapInterval(1);
  framework::platform::install_input_logging(*window, input);
  framework::platform::framebuffer_presenter presenter{*window};
  auto const start{std::chrono::steady_clock::now()};
  while(!glfwWindowShouldClose(window.get())) {
    glfwPollEvents();
    if(seconds > 0.0 && std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count() >= seconds) break;
    presenter.present(framebuffer);
    glfwWaitEventsTimeout(0.01);
  }
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << std::endl;
  return EXIT_FAILURE;
}

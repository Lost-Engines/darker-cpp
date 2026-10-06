#include <chrono>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <span>
#include <stdexcept>
#include <boost/program_options.hpp>
#include <boost/scope/scope_exit.hpp>
#include <GLFW/glfw3.h>
#include "audio/sine_wave.h"
#include "platform/audio_output.h"
#include "platform/framebuffer_presenter.h"
#include "platform/input.h"
#include "render/framebuffer.h"

auto main(int const argc, char const *const argv[])->int try {
  /// Exercise platform services without loading or implementing any Darker-specific state
  boost::program_options::options_description options{"Framework demo options"};
  options.add_options()
    ("help,h", "show usage")
    ("seconds", boost::program_options::value<double>()->default_value(0.0), "exit after this many seconds; zero runs until closed")
    ("no-audio", "explicitly skip opening an audio device")
    ("capture", "capture relative mouse input at startup");
  boost::program_options::variables_map arguments;
  boost::program_options::store(boost::program_options::parse_command_line(argc, argv, options), arguments);
  boost::program_options::notify(arguments);
  if(arguments.contains("help")) {
    std::cout << options << std::endl;
    return EXIT_SUCCESS;
  }
  double const seconds{arguments["seconds"].as<double>()};
  if(!std::isfinite(seconds) || seconds < 0.0) throw std::invalid_argument{"--seconds must be finite and non-negative"};

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
  std::unique_ptr<GLFWwindow, decltype(&glfwDestroyWindow)> const window{
    glfwCreateWindow(960, 600, "Armchair framework demo", nullptr, nullptr), glfwDestroyWindow,
  };
  if(!window) throw std::runtime_error{"cannot create the GLFW window"};
  glfwMakeContextCurrent(window.get());
  glfwSwapInterval(1);
  framework::platform::install_input_logging(*window, input);
  if(arguments.contains("capture")) framework::platform::capture_mouse(*window, true);
  framework::platform::framebuffer_presenter presenter{*window};
  framework::render::framebuffer framebuffer;
  framework::audio::sine_wave tone{framework::platform::audio_output::sample_rate, 220.0, 0.02f};
  std::unique_ptr<framework::platform::audio_output> audio;
  if(!arguments.contains("no-audio")) {
    audio = std::make_unique<framework::platform::audio_output>([](void *const userdata, std::span<float> const samples) noexcept {
      static_cast<framework::audio::sine_wave*>(userdata)->fill_stereo(samples);
    }, &tone);
  }
  std::cout << "Tab toggles mouse capture. Escape releases capture, then closes the window. Tone: 220 Hz at 2% peak amplitude." << std::endl;
  auto const start{std::chrono::steady_clock::now()};
  while(!glfwWindowShouldClose(window.get())) {
    glfwPollEvents();
    double const elapsed{std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count()};
    if(seconds > 0.0 && elapsed >= seconds) break;
    framework::render::draw_demo(framebuffer, elapsed);
    presenter.present(framebuffer);
    glfwWaitEventsTimeout(0.001);
  }
  return EXIT_SUCCESS;
} catch(std::exception const &error) {
  std::cerr << "ERROR: " << error.what() << std::endl;
  return EXIT_FAILURE;
}

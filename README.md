# Armchair platform proof of concept

A small C++23 program in preparation for the Darker project. The platform demo contains no game logic. A separate original-resource library and headless verifier now begin the faithful reconstruction.

- GLFW 3.4 owns the window and input events.
- A CPU-generated 320 × 200 RGBA framebuffer contains a checkerboard and moving green rectangle.
- An isolated OpenGL 2.1 presenter uploads that buffer to a nearest-filtered texture. It uses integer enlargement and letterboxing, with fractional reduction for small windows. This demo uses square pixels; no original-game aspect correction is assumed.
- miniaudio 0.11.23 provides only the low-level PCM device path. Its engine, resource manager, node graph, decoders, encoders and waveform generators are disabled. There is no silent null-device fallback.
- A separate oscillator supplies a 220 Hz sine at 2% peak amplitude (about −34 dBFS), stereo float at 48 kHz, with a 10 ms startup ramp. The callback preserves phase across buffers and performs no allocations, logging or locking.
- Boost.Program_options handles the small command line; Boost.Scope ensures GLFW cleanup on failure.
- Catch2 3.8.1 checks PCM continuity, amplitude, frequency, DC balance, input validation and viewport sizing.

## Build and run

Requires CMake 3.28+, a C++23 compiler, Boost 1.85+ with Program_options, and OpenGL/window-system development packages. On Linux, GLFW defaults to building both X11 and Wayland support; their development headers, xkbcommon and wayland-scanner must be installed. You can disable a backend with `-DGLFW_BUILD_WAYLAND=OFF` or `-DGLFW_BUILD_X11=OFF`.

GLFW, miniaudio and Catch2 are fetched from immutable GitHub commit archives with SHA-256 verification. Boost and system graphics/platform libraries are discovered locally. The first configure needs network access; sources remain in the build directory afterwards.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/framework_demo
```

Ninja is optional; omit `-G Ninja` to use CMake's default generator. Use `-DBUILD_TESTING=OFF` to omit Catch2 and tests. Build artefacts are ignored by Git.

## Controls and options

Keyboard presses, releases, repeats and native scancodes are printed to the terminal. Mouse movement deltas and button events are printed too. Scancodes are GLFW/platform values, not DOS keyboard codes.

- **Tab:** toggle captured relative mouse input; raw motion is enabled when supported.
- **Escape:** release mouse capture; press again while uncaptured to exit.
- **Close window:** exit.
- Losing focus releases capture; regaining focus does not capture automatically.

```sh
./build/framework_demo --help
./build/framework_demo --capture
./build/framework_demo --seconds 3
./build/framework_demo --no-audio --seconds 3
```

`--no-audio` is an explicit option for environments without an audio device. Otherwise an audio initialisation failure is reported and causes a failing exit status. The 2% signal level is relative to digital full scale, not a guarantee of physical loudness at a particular amplifier setting.

## Boundaries and lifetime

`src/audio` and `src/render` are independent of GLFW, OpenGL and miniaudio. `src/platform` implements their host adapters; `main.cpp` wires the demo together. Audio uses a fixed float-stereo callback contract: its user data must outlive the output object. The device is stopped and joined before that data is destroyed. The presenter dies before the window/context, and the window before GLFW termination.

The presenter deliberately uses a small OpenGL 2.1 textured quad rather than introducing a shader loader or Vulkan setup for this first test. No GL calls occur in CPU drawing. It can be replaced independently. The main loop uses a monotonic clock for demo animation; vsync only controls presentation. This is not a proposed game simulation timestep or input architecture.

The house conventions are in [style-guide.md](style-guide.md). First-party files follow its naming, brace, include, ownership and CMake conventions. External dependency sources retain upstream style.

Upstream references: [GLFW](https://www.glfw.org/docs/3.4/), [miniaudio low-level API](https://miniaud.io/docs/manual/index.html), [Catch2](https://github.com/catchorg/Catch2).

## Verification on this workstation

Built with GCC 16.2 in C++23 mode; the five platform Catch2 cases pass. With the resource milestone enabled, all ten unit cases and the original-pack integration test pass. A separate Xvfb/Mesa smoke run verified the displayed image, key press/release events, mouse deltas/buttons, capture/release, resizing and timed shutdown. PCM routed through a temporary PulseAudio-compatible sink measured approximately 220 Hz, 0.019989 peak and 0.014142 RMS. That sink was removed afterwards. The test measured the device stream, not acoustic speaker output. Native Wayland also opened successfully during runtime checking; the scripted input/screenshot checks used X11.

## Faithful reconstruction: first milestone

The [reconstruction contract](docs/reconstruction_contract.md) defines the fidelity target, platform boundary, arithmetic/clock rules, ownership and acceptance criteria. The [resource loader](docs/resource_loading.md) reads all five original packs using the verified executable-resident directory, independently of the graphics/audio demo.

```sh
./build/resource_check --install ../darker --reference ../analysis/resources
```

The application does not need the extracted references; they are comparison inputs for this command only. Ordinary unit tests also need no game assets. See the resource documentation for optional CTest integration and directory regeneration.

# Darker

A faithful C++23 reconstruction using the original game's data packs. The deliverable is one executable, `darker`. Development milestones evolve inside that application; superseded demonstrations and temporary controls are removed as work progresses.

## Build and run

Requires CMake 3.28+, a C++23 compiler, Boost 1.85+ with Program_options, and OpenGL/window-system development packages. GLFW builds X11 and Wayland support by default on Linux; disable an unwanted backend with `-DGLFW_BUILD_WAYLAND=OFF` or `-DGLFW_BUILD_X11=OFF`.

GLFW and miniaudio are fetched from pinned GitHub archives with SHA-256 verification. Test builds also fetch Catch2. Boost and system platform libraries are discovered locally.

From this directory:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
./build/darker --data-dir ../darker
```

The application looks for `DARKER.00` through `DARKER.04` in the **current working directory** by default. Run it from the directory containing those files without any flags, or use `--data-dir` to select another directory.

Use `-DBUILD_TESTING=OFF` for an application-only build. Build artefacts are ignored by Git.

## Current milestone

The application now flies the Caero over Delphi, or either Skimma over Halon, using the original fixed-point flight callbacks, keyboard/mouse steering, beacon charging, city collisions and crash response. Rendering retains the original software model drawing, Gouraud shading, distance ramps, beacon lighting and fountain animation.

This is an **airborne flight checkpoint**, not yet a complete mission. It temporarily starts above the city with a usable flight state. Original scenario/launch setup, other actors, weapons, sound, external cameras, landing and death/restart screens remain to be connected. Close and relaunch after crashing. Skimma shield logic is present, but its directional display is not yet driven; Caero stall dimming and Nayas activity also remain outstanding.

Use `--craft skimma` or `--craft upgraded` for the other craft. Controls:

- **Mouse / arrow keys:** steer; **Ctrl + arrows:** adjust control force.
- **Enter:** Caero boost; upgraded Skimma turbo setting.
- **Backspace:** brake.
- **E:** Caero engine / Skimma shields.
- **A:** Caero automatic altitude adjustment.
- **− / =:** Skimma low/high speed settings.
- **F9:** Gouraud shading; **Insert / keypad 0:** hold enlarged Caero radar.
- **Escape:** close and release the captured mouse.

The old W/A/S/D, R/F and drag-to-look inspection controls have been removed. Mouse capture requests raw motion where GLFW supports it; the original steering filter consumes wrapping relative counters. Host-to-DOS mouse sensitivity still needs an interactive comparison. Caero altitude, damage, boost cells, recharge and incoming-power displays now follow live state, as do compass/grid coordinates and Skimma engine-output strips. Weapon icons are empty until weapon integration.

The world view now draws the original 17 sky/ground colour bands, moving with pitch and bank. Halon’s distant grey building shades blend into its grey horizon; Delphi uses its own purple night palette. The original model distance-shading tables and draw radius are unchanged.

Source RGB and square-pixel presentation remain inspection conventions pending original palette/display handling. `--output /tmp/city.ppm` renders the initial checkpoint headlessly; `--seconds` permits timed window runs. These are development options, not proposed game controls.

## Structure

`src/game` contains platform-independent gameplay state calculations, including complete Caero and Skimma flight callbacks, capped game clocks, swept city collision and player crash handling, Skimma weapon state, typed object definitions, owned projectile creation and ordered direct/object/map guidance, expiry/reference repairs, object and player damage, shield recharge, beacon power and Caero energy accounting, intrusive object lists and the original random generator. `src/resources` reads the original packs. `src/graphics` contains game-specific palette, mask, cockpit, fixed-point camera/projection and original model-bytecode drawing logic. `src/render` contains platform-independent framebuffer types. `src/platform` isolates GLFW/OpenGL presentation and the low-level miniaudio PCM adapter. `src/main.cpp` is the sole application entry point. Current playback is silent until game audio is connected; the proof-of-concept oscillator lives only in test support.

No GL calls occur in CPU drawing. The presenter and audio device retain independent lifetimes and can be replaced without changing the game logic. Retired demo rendering, source-sheet entry points and input-logging callbacks have been removed.

The [reconstruction contract](docs/reconstruction_contract.md) defines fidelity and platform boundaries. [Resource loading](docs/resource_loading.md), [indexed images](docs/indexed_images.md) and [cockpit rendering](docs/cockpit_rendering.md) and [software polygons](docs/software_polygons.md) and [city/model rendering](docs/model_rendering.md) document implementation evidence and remaining work. [Skimma weapons](docs/skimma_weapons.md) records the first translated reload/state routines and remaining integration. [Caero flight](docs/caero_flight.md), [Skimma flight](docs/skimma_flight.md), [steering](docs/flight_controls.md), [clocks](docs/game_clock.md), [city collision](docs/city_collision.md) and [player integration](docs/player_flight.md) record the new runtime path and its native comparisons. [World primitives](docs/world_primitives.md) documents object allocation/recycling, definition expansion, projectile placement and the native random sequence. House conventions are in [style-guide.md](style-guide.md).

## Verification

```sh
ctest --test-dir build --output-on-failure
./build/resource_check --data-dir ../darker --reference ../analysis/resources
```

`framework_tests` and `resource_check` are test-only binaries, excluded by `BUILD_TESTING=OFF`. Unit tests require no game assets; optional resource integration checks use the original packs and previously verified extraction. Native-reference generators in `tools/` run the unpacked original executable with Unicorn. Their checked-in fixtures contain synthetic inputs and result fingerprints; original model and map bytes remain in the user's packs.

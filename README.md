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

The application draws Delphi or Halon directly from the original city map and geometry bank, inside the corresponding 320×240 cockpit. City traversal, model selection, placement, ordering, near-plane clipping, flat polygons, lines, beacon discs and fountain vertex animation now use reconstructed original routines. The camera can move through the scene; flight, collisions, moving objects and missions are not connected yet.

Use `--craft skimma` or `--craft upgraded` to explore Halon. Temporary inspection controls:

- **W / S:** move forwards / backwards.
- **A / D:** move sideways.
- **R / F:** rise / lower.
- **Left mouse drag:** look horizontally and vertically.
- **F9:** toggle original Gouraud shading.
- **Escape:** close.

The original **hold Insert / keypad 0** binding displays the enlarged Caero radar. Compass and grid coordinates follow the inspection camera; gauges and weapon displays remain sample values, and there are no radar contacts yet. The older instrument-adjustment controls have been removed.

The scene uses original Gouraud shading, distance ramps and Delphi beacon-state lighting, including the original distance-based flat fallback. Original palette/display handling and gameplay camera integration remain to come. Source RGB and square-pixel presentation are inspection conventions. `--output /tmp/city.ppm` renders the initial view headlessly; `--seconds` permits timed window runs. These are development options, not proposed game controls.

## Structure

`src/game` contains platform-independent gameplay state calculations, including the complete Caero flight callback, Skimma weapon state, typed object definitions, owned projectile creation and ordered direct/object/map guidance, expiry/reference repairs, object and player damage, shield recharge, beacon power and Caero energy accounting, intrusive object lists and the original random generator. `src/resources` reads the original packs. `src/graphics` contains game-specific palette, mask, cockpit, fixed-point camera/projection and original model-bytecode drawing logic. `src/render` contains platform-independent framebuffer types. `src/platform` isolates GLFW/OpenGL presentation and the low-level miniaudio PCM adapter. `src/main.cpp` is the sole application entry point. Current playback is silent until game audio is connected; the proof-of-concept oscillator lives only in test support.

No GL calls occur in CPU drawing. The presenter and audio device retain independent lifetimes and can be replaced without changing the game logic. Retired demo rendering, source-sheet entry points and input-logging callbacks have been removed.

The [reconstruction contract](docs/reconstruction_contract.md) defines fidelity and platform boundaries. [Resource loading](docs/resource_loading.md), [indexed images](docs/indexed_images.md) and [cockpit rendering](docs/cockpit_rendering.md) and [software polygons](docs/software_polygons.md) and [city/model rendering](docs/model_rendering.md) document implementation evidence and remaining work. [Skimma weapons](docs/skimma_weapons.md) records the first translated reload/state routines and remaining integration. [Caero flight](docs/caero_flight.md) records the complete startup and flight callback. [World primitives](docs/world_primitives.md) documents object allocation/recycling, definition expansion, projectile placement and the native random sequence. House conventions are in [style-guide.md](style-guide.md).

## Verification

```sh
ctest --test-dir build --output-on-failure
./build/resource_check --data-dir ../darker --reference ../analysis/resources
```

`framework_tests` and `resource_check` are test-only binaries, excluded by `BUILD_TESTING=OFF`. Unit tests require no game assets; optional resource integration checks use the original packs and previously verified extraction. Native-reference generators in `tools/` run the unpacked original executable with Unicorn. Their checked-in fixtures contain synthetic inputs and result fingerprints; original model and map bytes remain in the user's packs.

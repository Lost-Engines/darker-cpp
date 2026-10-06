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

The application currently assembles a 320×240 cockpit from the original artwork and implements its main masked instruments, plus Caero weapon icons, map-grid digits, compass and normal/enlarged radar contacts, Caero attitude/vector graphics, and Skimma bearing/weapon indicators, aim marks and weapon rings. The black windscreen is intentional: world rendering and gameplay are still to come. Source RGB and square-pixel presentation are inspection conventions pending original palette/display handling.

Use `--craft skimma` or `--craft upgraded` to inspect the other cockpits. Temporary controls:

- **Up / Down:** select an instrument; its name and count appear in the window title.
- **Left / Right:** decrease / increase its count.
- **Home / End:** empty / fill the selected instrument.
- **R / F:** empty / fill all instruments.
- **D:** toggle Caero engine dimming.
- **Escape:** close.

The original **hold Insert / keypad 0** binding displays the enlarged Caero radar; releasing it restores the normal view.

`--fill 0` starts with inactive strips; the default half-filled gauges are an inspection example, not game initial state. `--static` shows the underlying cockpit cache. `--field N --states 3 1` supplies a sequence of display states. `--output /tmp/cockpit.ppm` renders headlessly for verification; `--seconds` permits timed window runs. Cockpits currently use fixed sample navigation, contact and weapon states until their gameplay producers are connected. These are temporary development options, not proposed game controls.

## Structure

`src/game` contains platform-independent gameplay state calculations, including Skimma weapon state, typed object definitions, projectile launch placement, intrusive object lists and the original random generator. `src/resources` reads the original packs. `src/graphics` contains game-specific palette, mask and cockpit logic. `src/render` contains platform-independent framebuffer types. `src/platform` isolates GLFW/OpenGL presentation and the low-level miniaudio PCM adapter. `src/main.cpp` is the sole application entry point. Current playback is silent until game audio is connected; the proof-of-concept oscillator lives only in test support.

No GL calls occur in CPU drawing. The presenter and audio device retain independent lifetimes and can be replaced without changing the game logic. Retired demo rendering, source-sheet entry points and input-logging callbacks have been removed.

The [reconstruction contract](docs/reconstruction_contract.md) defines fidelity and platform boundaries. [Resource loading](docs/resource_loading.md), [indexed images](docs/indexed_images.md) and [cockpit rendering](docs/cockpit_rendering.md) document implementation evidence and remaining work. [Skimma weapons](docs/skimma_weapons.md) records the first translated reload/state routines and remaining integration. [World primitives](docs/world_primitives.md) documents object allocation/recycling, definition expansion, projectile placement and the native random sequence. House conventions are in [style-guide.md](style-guide.md).

## Verification

```sh
ctest --test-dir build --output-on-failure
./build/resource_check --data-dir ../darker --reference ../analysis/resources
```

`framework_tests` and `resource_check` are test-only binaries, excluded by `BUILD_TESTING=OFF`. Unit tests require no game assets; optional resource integration checks use the original packs and previously verified extraction. The development script `tools/verify_cockpit.py` compares the main application's output against the existing masked PNG references (requires Pillow).

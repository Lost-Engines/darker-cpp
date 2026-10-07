# Darker

A faithful C++23 reconstruction of the engine of the 1995 DOS game Darker by Psygnosis, using the original game's resources, building and running natively on modern platforms.

Read about the reverse engineering effort at https://lostengines.com/darker and browse a 3D map viewer, model viewer, and more.

## Data files

The original game's data files are required to run this.  Darker is considered abandonware at this point, and images of the original media are widely available: https://archive.org/details/darker-cdrom/ or https://www.myabandonware.com/game/darker-2dn#download.

## Build and run

Requires CMake 3.28+, a C++23 compiler, Boost 1.85+ with Program_options, and OpenGL/window-system development packages. GLFW builds X11 and Wayland support by default on Linux; disable an unwanted backend with `-DGLFW_BUILD_WAYLAND=OFF` or `-DGLFW_BUILD_X11=OFF`.

GLFW, miniaudio and Nuked OPL3 are fetched from pinned GitHub archives with SHA-256 verification. Test builds also fetch Catch2. Boost and system platform libraries are discovered locally.

From this directory:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
./build/darker --data-dir ../darker
```

The application looks for `DARKER.00` through `DARKER.04` (from the original game data) in the **current working directory** by default. Run it from the directory containing those files without any flags, or use `--data-dir` to select another directory.

Use `--skip-intro` to open game selection directly; mission briefings still play normally.

Use `--scale N` to set the initial window size to an integer multiple of the 320×240 display: `--scale 2` gives 640×480, `--scale 4` gives 1280×960. The default is 4× (1280×960). The window remains resizable.

Use `-DBUILD_TESTING=OFF` for an application-only build. Build artefacts are ignored by Git.

Unexpected runtime exceptions are deliberately uncaught, including failures inside the `noexcept` PCM callback. They terminate the process for debugging instead of becoming a normal error exit or silent audio. Expected startup failures retain short diagnostics. To stop at the original throw site, launch with `gdb --args ./build/darker --data-dir ../darker`, then use `catch throw` and `run`.

## Current milestone

The application now flies the Caero over Delphi, or either Skimma over Halon, using the original fixed-point flight callbacks, keyboard/mouse steering, beacon charging, city collisions and crash response. Rendering retains the original software model drawing, Gouraud shading, distance ramps, beacon lighting and fountain animation. City visibility now also follows the original 256-cell coordinate wrap, including scenery approaching across either seam.

The Caero begins with the original startup/title sequence, game selection and illustrated first-mission briefing, then starts at its **HQ launch site**: the engine starts enabled and boost cells charge while it remains on. Press **Enter** once to launch; the ship clears the first wall without steering. Gate, approach lights and hangar interior follow the original departure state changes. Skimma starts remain airborne checkpoints. The first mission now connects Pinner Direct combat, both aircraft objectives, the original return message and automatic HQ docking. Aircraft hits, fatal damage and ground impacts now produce original sprite bursts and damage trails, with their FM sound layers. Enemy gun endpoints flash and sound; Pinner projectile audio follows motion and distance. Remaining combat effects, world audio, object-target cameras and subsequent campaign progression remain outstanding. See [first mission](docs/first_mission.md) for scope and evidence. Enter after a Caero crash shows the original committal sequence before returning to the run menu. See [hangar launch](docs/hangar_launch.md) for verification and scope. Skimma shield startup/strength and low-altitude warning drive their cockpit displays; Caero stall dimming now follows the original speed thresholds; Nayas activity remains outstanding.

The original **Level X** cheat is available: on game selection, press **Shift+8**, release Shift, then press **3** on the number row. Enter **`Level X`** exactly and press Enter. **X** during flight then advances through the normal campaign transition and saves progression. Activation lasts until the program closes; it does not unlock unimplemented missions.

Use `--craft skimma` or `--craft upgraded` for the other craft. Controls:

- **Space / Enter:** advance briefing pages.
- **1:** select Pinner Direct; **Space / left mouse:** fire on each press.
- **Docking:** saves progress and opens the next supported briefing automatically.
- **Mouse / arrow keys:** steer; **Ctrl + arrows:** adjust control force.
- **Enter:** Caero boost; upgraded Skimma turbo setting.
- **Backspace:** brake.
- **E:** Caero engine / Skimma shields.
- **A:** Caero automatic altitude adjustment.
- **− / =:** Skimma low/high speed settings.
- **F1 / F2 / F3 / F4:** cockpit / following / level following / full-screen inside.
- **F5 / F6:** drop a tracking / fixed camera; **comma / period:** following-camera distance.
- **Tab:** hold to look around; steering controls the camera while held.
- **F9:** Gouraud shading; **Insert / keypad 0:** hold enlarged Caero radar.
- **Escape:** return to the Caero run menu and release the mouse; close in Skimma development starts.

The old W/A/S/D, R/F and drag-to-look inspection controls have been removed. Mouse capture requests raw motion where GLFW supports it; the original steering filter consumes wrapping relative counters. Host-to-DOS mouse sensitivity still needs an interactive comparison. Caero altitude, damage, boost cells, recharge and incoming-power displays now follow live state, as do compass/grid coordinates and Skimma engine-output strips. The Caero primary icon follows Pinner Direct selection.

The world view now draws the original 17 sky/ground colour bands, moving with pitch and bank. Halon’s distant grey building shades blend into its grey horizon; Delphi uses its own purple night palette. The original model distance-shading tables and draw radius are unchanged.

Source RGB and square-pixel presentation remain inspection conventions pending original palette/display handling. `--output /tmp/frame.ppm` renders the initial frame headlessly (startup presentation for Caero, flight for Skimma); `--seconds` permits timed window runs. These are development options, not proposed game controls.

## Structure

`src/game` contains platform-independent gameplay state calculations, including complete Caero and Skimma flight callbacks, capped game clocks, swept city collision and player crash handling, Skimma weapon state, typed object definitions, owned projectile creation and ordered direct/object/map guidance, expiry/reference repairs, object and player damage, shield recharge, beacon power and Caero energy accounting, intrusive object lists and the original random generator. `src/resources` reads the original packs. `src/graphics` contains game-specific palette, mask, cockpit, fixed-point camera/projection and original model-bytecode drawing logic. `src/render` contains platform-independent framebuffer types. `src/platform` isolates GLFW/OpenGL presentation and the low-level miniaudio PCM adapter. `src/main.cpp` is the sole application entry point. Player engine, boost, recharge and shield effects now use the original FM patches through a pinned Nuked OPL3 chip emulator. `--mute` disables PCM output. Combat sources now use native distance admission and Doppler; ambience, stereo positioning and complete native voice allocation remain outstanding; the proof-of-concept oscillator lives only in test support.

No GL calls occur in CPU drawing. The presenter and audio device retain independent lifetimes and can be replaced without changing the game logic. Retired demo rendering, source-sheet entry points and input-logging callbacks have been removed.

The [reconstruction contract](docs/reconstruction_contract.md) defines fidelity and platform boundaries. [Resource loading](docs/resource_loading.md), [indexed images](docs/indexed_images.md) and [cockpit rendering](docs/cockpit_rendering.md) and [software polygons](docs/software_polygons.md) and [city/model rendering](docs/model_rendering.md) document implementation evidence and remaining work. [Skimma weapons](docs/skimma_weapons.md) records the first translated reload/state routines and remaining integration. [Caero flight](docs/caero_flight.md), [Skimma flight](docs/skimma_flight.md), [steering](docs/flight_controls.md), [clocks](docs/game_clock.md), [city collision](docs/city_collision.md) and [player integration](docs/player_flight.md) record the new runtime path and its native comparisons. [World primitives](docs/world_primitives.md) documents object allocation/recycling, definition expansion, projectile placement and the native random sequence. House conventions are in [style-guide.md](style-guide.md).

## Verification

The [scenario reader](docs/scenarios.md) also loads all original mission/presentation records, preserving formatted multilingual text and setup groups for the upcoming mission runtime.
The [font renderer and formatter](docs/fonts_and_text.md) consume original bitmap fonts and page controls, with native comparisons for glyph coverage, layout and cursor state.
The [HQ launch](docs/hangar_launch.md) now supplies the original Caero start and gate departure. [Mission execution](docs/mission_execution.md) covers the deadline/checkpoint scheduler and conditional messages. [Actor construction and navigation](docs/actor_motion.md) cover placement, proximity response, targeting, clearance, manoeuvre selection and movement; the first mission’s two aircraft now fly in the live Delphi scene and appear on radar. Their combined callback matches 2,048 original updates, including pursuit. The connected first-mission combat and docking checks are described in [first mission](docs/first_mission.md).

```sh
ctest --test-dir build --output-on-failure
./build/resource_check --data-dir ../darker --reference ../analysis/resources
```

`framework_tests` and `resource_check` are test-only binaries, excluded by `BUILD_TESTING=OFF`. Unit tests require no game assets; optional resource integration checks use the original packs and previously verified extraction. Native-reference generators in `tools/` run the unpacked original executable with Unicorn. Their checked-in fixtures contain synthetic inputs and result fingerprints; original model and map bytes remain in the user's packs.

The Caero front end now includes the original startup animation, title and four-page briefing, with game selection and pilot-name entry. Pilot slots persist in `darker-cpp.sav` in the working directory using the original save format. Enter after a crash plays the original looping Kismet committal presentation. The first three missions now advance automatically after docking, saving city state, weapon availability and the return site. Stage four and later gameplay and exact retail menu composition remain outstanding. See [front end](docs/front_end.md) for controls and verification.

[Retail screenshot corrections](docs/visual_regressions.md) cover briefing composition, cockpit edges, indicators, radar coordinates and door interpolation. A [record/replay comparison harness](docs/comparison_harness.md) is proposed for broader visual verification.

The six original Sound Blaster [music groups](docs/sound_images_music.md) now play through the same OPL synthesiser in startup, menus, briefings and the committal presentation. Their timed register streams match the original driver across repeated playback.

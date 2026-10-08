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

During flight, **Pause** (or **Num Lock**) freezes the displayed frame and releases
the mouse. Press Pause/Num Lock again to advance one simulation frame while staying
paused (8 timer ticks, approximately 16 ms). Modifier keys alone and Alt/Windows/Meta shortcuts (including Alt+Tab) leave it paused.
Press an ordinary key to resume; that key
is ignored until release, including auto-repeat and held-key flight actions.
The console prints the paused position, attitude, destination hangar and mission-ready
state so a reported approach can be reproduced precisely.

For keyboard-only debugging, `--no-mouse` disables movement, buttons and menu
hover/click input, hides the pointer and leaves the mouse uncaptured. Arrow keys,
keyboard firing and menu navigation remain available.

`--noclip` skips player collisions with terrain, buildings and solid actors.
In tunnels it uses free Caero flight instead of route guidance or automatic portal
landing, and draws nearby tunnel cells without requiring a connected route
to the camera. Caero flight power and boosts stay replenished, with a constant beacon
supply while the engine is on, even outside the city or tunnel walls. Weapon
impacts and enemies remain active; combine with `--cheat-life` for impact-damage protection.
Both options persist across level changes and deaths and are not saved.
For example: `./build/darker --level=17 --no-mouse --noclip --cheat-lyndon`.

Use `--skip-intro` to open game selection directly; mission briefings still play normally.

Use `--level=4` to begin the briefing for a specific campaign level (1–116).
This creates a fresh debugging session with the original energised city templates, equipment and return sites reconstructed
from preceding briefings, rather than reproducing damage left by a particular playthrough.
It bypasses startup menus and does not write saves. The level selects its own craft;
do not combine it with `--craft`. Presentation-only levels play their interlude and continue.

Use `--scale N` to set the initial window size to an integer multiple of the 320×240 display: `--scale 2` gives 640×480, `--scale 4` gives 1280×960. The default is 4× (1280×960). The window remains resizable.

Use `-DBUILD_TESTING=OFF` for an application-only build. Build artefacts are ignored by Git.

Unexpected runtime exceptions are deliberately uncaught, including failures inside the `noexcept` PCM callback. They terminate the process for debugging instead of becoming a normal error exit or silent audio. Expected startup failures retain short diagnostics. To stop at the original throw site, launch with `gdb --args ./build/darker --data-dir ../darker`, then use `catch throw` and `run`.

## Current milestone

The application now flies the Caero over Delphi, or either Skimma over Halon, using the original fixed-point flight callbacks, keyboard/mouse steering, beacon charging, city collisions and crash response. Rendering retains the original software model drawing, Gouraud shading, distance ramps, beacon lighting and fountain animation. City visibility now also follows the original 256-cell coordinate wrap, including scenery approaching across either seam.

The Caero begins with the original startup/title sequence, game selection and illustrated first-mission briefing, then starts at its **HQ launch site**: the engine starts enabled and boost cells charge while it remains on. Press **Enter** once to launch; the ship clears the first wall without steering. Gate, approach lights and hangar interior follow the original departure state changes. Explicit `--craft` Skimma starts remain airborne checkpoints; campaign progression now switches into the Halon missions. The first mission now connects Pinner Direct combat, both aircraft objectives, the original return message and automatic HQ docking. Aircraft hits, fatal damage and ground impacts now produce original sprite bursts and damage trails, with their FM sound layers. Enemy gun endpoints flash and sound; Pinner projectile audio follows motion and distance. The connected campaign now reaches the final battle and ending, including the Halon flight stages and films, as well as Caero missions through ninety-seven, including reinforcements, Oppressors, the transfer to the tunnel-entry hangar and the all eight underground missions with their Wreckers and portal returns, and the return to surface combat with warehouse aircraft launches and Brent Hunter/Chargeable targeting and Brent Ground building objectives and timed Forbes Diffuser attacks, and the beacon-powered Caero Weapon. Broader mission playthrough checks and remaining weapon/world behaviours are still in development; see [campaign status](docs/campaign_status.md). See [first mission](docs/first_mission.md) for scope and evidence. Enter after a Caero crash shows the original committal sequence before returning to the run menu. See [hangar launch](docs/hangar_launch.md) for verification and scope. Skimma shield startup/strength and low-altitude warning drive their cockpit displays; Caero stall dimming now follows the original speed thresholds; Nayas activity now follows timed messages and the completed-mission return signal.

Mission 21 unlocks **Brent Hunter**: select it with **0**, acquire an aircraft in the target marker, and fire with **Alt / right mouse**. **Caps Lock** clears the lock for reacquisition. Mission 30 unlocks **Chargeable** on **9**: hold Alt / right mouse to charge, then release with an aircraft locked. Mission 50 adds **Brent Ground** on **6**, for designated building targets. Mission 57 adds **Dual Launch** on **3**; its native default-mask anomaly is documented in [weapon behaviour](docs/caero_weapons.md#dual-launch-and-a-native-input-mask-anomaly).

The original **Level X** cheat is available: on game selection, press **Shift+8**, release Shift, then press **3** on the number row. Enter **`Level X`** exactly and press Enter. **X** during flight then advances through the normal campaign transition and saves progression. Use `--cheat-level-x` to activate it at startup instead; combine with `--skip-intro` for quicker testing. **Shift+X** starts the previous playable level (or restarts level 1), using the same fresh setup as `--level`; from that point the session does not write saves. It has no effect in Nightmare. Activation lasts until the program closes; it does not unlock unimplemented missions.

The other original cheats can be enabled for the whole process with:

- `--cheat-lyndon`: **Z** toggles player-motion freeze. Enemies, weapons and external cameras remain active; this is separate from Pause. Each new flight starts unfrozen, with Z still enabled.
- `--cheat-brooke`: Jason Brooke's accelerated Caero boost replenishment. The original engine/beacon-energy gates still apply; it does not recharge indefinitely far outside the city.
- `--cheat-life`: the original incoming-impact damage patch. Impact kicks remain, scenery crashes are still lethal, and an unshielded Skimma remains vulnerable.

For tunnel inspection, for example: `./build/darker --level=17 --cheat-lyndon --cheat-life --cheat-level-x`.
These switches can be combined and remain enabled across deaths and level changes; they are not written to saves.


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

The Caero front end now includes the original startup animation, title and four-page briefing, with game selection and pilot-name entry. Pilot slots persist in `darker-cpp.sav` in the working directory using the original save format. Enter after a crash plays the original looping Kismet committal presentation. The supported missions advance automatically after docking, saving city state, weapon availability and the return site. Later campaign gameplay and exact retail menu composition remain outstanding; see [campaign status](docs/campaign_status.md). See [front end](docs/front_end.md) for controls and verification.

[Retail screenshot corrections](docs/visual_regressions.md) cover briefing composition, cockpit edges, indicators, radar coordinates and door interpolation. A [record/replay comparison harness](docs/comparison_harness.md) is proposed for broader visual verification.

The six original Sound Blaster [music groups](docs/sound_images_music.md) now play through the same OPL synthesiser in startup, menus, briefings and the committal presentation. Their timed register streams match the original driver across repeated playback.

Number-row 2 selects the Pinner Mimic after its mission-five introduction. M enables missile viewing for subsequent shots; F4 selects the missile-eye view. See [Mimic and cameras](docs/pinner_mimic.md) and [radar coverage](docs/radar_coverage.md).

See [campaign status and remaining work](docs/campaign_status.md) for the current playable boundary and next integration priorities.

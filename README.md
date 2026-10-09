# Darker

A faithful C++23 reconstruction of the engine of the 1995 DOS game Darker by Psygnosis, using the original game's resources, building and running natively on modern platforms.

Read about the reverse engineering effort at https://lostengines.com/darker and browse a 3D map viewer, model viewer, and more.

## Data files

The original game's data files are required to run this.  Darker is considered abandonware at this point, and images of the original media are widely available: https://archive.org/details/darker-cdrom/ or https://www.myabandonware.com/game/darker-2dn#download.

## Build and run

Requires CMake 3.28+, a C++23 compiler, Boost 1.85+ with Program_options, and OpenGL/window-system development packages. GLFW builds X11 and Wayland support by default on Linux; disable an unwanted backend with `-DGLFW_BUILD_WAYLAND=OFF` or `-DGLFW_BUILD_X11=OFF`.

GLFW, miniaudio, TinySoundFont, Munt, Nuked-SC55, Nuked OPL3 and the DOSBox DBOPL core are fetched from pinned GitHub archives with SHA-256 verification. Test builds also fetch Catch2. Boost and system platform libraries are discovered locally.

From this directory:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
./build/darker --data-dir ../darker
```

The default Linux installation is `~/.local/share/darker/` (or
`$XDG_DATA_HOME/darker/` when set). Game packs, `DARKER.SAV`, optional Roland ROMs,
manual, map and optional `soundfont.sf2` live together in that directory.
It can also hold the original DOS executable for use through DOSBox.

An explicit `--data-dir` takes precedence. Without it, an installation in the
current directory takes precedence over the per-user default. Saves always use
`DARKER.SAV` beside the selected packs, so DOSBox and this engine can share the
same installation. Run one version at a time to avoid competing save writes.
If `DARKER.SAV` is absent, an old `darker-cpp.sav` in the working directory is
read for migration; the next save writes `DARKER.SAV` and leaves the old file intact.


Optional asset helper (Bash, curl and sha256sum):

```sh
./scripts/fetch-assets.sh
./build/darker --music=roland-lapc
```

This fetches the five retail game packs, CM-32L ROM pair, five SC-55 v1.21 ROMs, multilingual manual
and city reference map directly into the destination directory. The destination defaults to the same per-user game directory. Pass a directory
argument to fetch into a different installation.

Downloads run in four phases: missing manuals and map first, game packs second,
then missing CM-32L ROMs and SC-55 ROMs. If `DARKER.00` exists, the game phase is skipped.
Otherwise all five packs are downloaded directly into the destination, replacing
any packs already there, then checked against SHA-256. Download or checksum
failures produce warnings; files are left in place and later phases continue.
A missing manual or map can be supplied separately. Missing Roland ROMs disable
Roland emulation only; other sound engines remain available.
The known retail SHA-256 values are listed in
[darker-retail-packs.sha256](scripts/darker-retail-packs.sha256). Download addresses are kept one per line in
[game_urls.txt](scripts/game_urls.txt), [roland_rom_urls.txt](scripts/roland_rom_urls.txt),
[sc55_rom_urls.txt](scripts/sc55_rom_urls.txt) and [manual_urls.txt](scripts/manual_urls.txt) (manual and map).
SC-55 files are checked against [sc55-v121.sha256](scripts/sc55-v121.sha256).
Keep these files and the checksum file beside the script when copying it
elsewhere. To update a download address, edit its line in the relevant text file;
no shell code needs changing.

Installed builds provide `darker-fetch-assets`. An installer can offer this as
an optional step, passing a writable user asset directory. Download and checksum warnings do not fail the installation; the helper exits
zero after all phases. Failure to create or enter the destination returns one. No downloads
run automatically during CMake installation or game startup. No save files are
fetched. Local copies work without the helper.

Archive availability does not grant redistribution rights: assets remain
external to our executable and release packages. See
[Internet Archive's rights guidance](https://help.archive.org/help/rights/).


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
This creates a debugging session with equipment and return sites reconstructed
from preceding briefings, plus cumulative target markings, authored cell-state
changes (including radio outages) and earlier missions' exit-time beacon failures.
Underground visits preserve the preceding surface hangar; tunnel portal coordinates
are used only within their own map.
It does not assume that previous targets were destroyed, or replay conditional
combat/script outcomes such as triggered citywide blackouts. Explicit damage in
scenario setup lists is retained; ordinary objective targets remain intact.
For debugging, entry at level 101 or later also unlocks the upgraded Skimma’s
third missile, as though its first pad service had finished. This convenience
also applies to Shift+X; ordinary campaign progression and X retain the original unlock.
It bypasses startup menus and does not write saves. The level selects its own craft;
do not combine it with `--craft`. Presentation-only levels play their interlude and continue.

Use `--scale N` to set the initial window size to an integer multiple of the 320×240 display: `--scale 2` gives 640×480, `--scale 4` gives 1280×960. The default is 4× (1280×960). The window remains resizable.

Use `-DBUILD_TESTING=OFF` for an application-only build. Build artefacts are ignored by Git.

Unexpected runtime exceptions are deliberately uncaught, including failures inside the `noexcept` PCM callback. They terminate the process for debugging instead of becoming a normal error exit or silent audio. Expected startup failures retain short diagnostics. To stop at the original throw site, launch with `gdb --args ./build/darker --data-dir ../darker`, then use `catch throw` and `run`.

## Playable scope

The application now flies the Caero over Delphi, or either Skimma over Halon, using the original fixed-point flight callbacks, keyboard/mouse steering, beacon charging, city collisions and crash response. Rendering retains the original software model drawing, Gouraud shading, distance ramps, beacon lighting and fountain animation. City visibility now also follows the original 256-cell coordinate wrap, including scenery approaching across either seam.

The campaign connects the original startup, menus, briefings and films through
the final battle and ending. It includes both cities, all eight tunnel missions,
Wreckers, vehicle routes, scripted attacks, weapons, hangar docking, supply pads
and Nightmare. Explosions, smoke and damage trails use the original effects and
FM sound layers. [Campaign status](docs/campaign_status.md) separates controlled
verification from natural playthrough coverage; this remains a playable alpha,
not a claim of complete retail equivalence.

The first Caero mission starts in its **HQ hangar**, with the engine enabled.
Allow a boost cell to charge, then press **Enter** once to launch. Follow each
briefing for objectives and the required return destination. Cockpit instruments,
Nayas messages, shields and radar reflect the corresponding craft and world.
See [hangar launch](docs/hangar_launch.md) and [first mission](docs/first_mission.md)
for the launch and combat checks. Explicit `--craft` starts remain development
free-flight checkpoints.

**F1–F6** select cockpit, following and dropped views. **F7 / backtick** selects
a pointed object or building camera; comma/period change the following distance.
**Tab** looks around. See [cameras](docs/flight_cameras.md) for original restrictions
and verification limits.

Mission 21 unlocks **Brent Hunter**: select it with **0**, acquire an aircraft in the target marker, and fire with **Alt / right mouse**. **Caps Lock** clears the lock for reacquisition. Mission 30 unlocks **Chargeable** on **9**: hold Alt / right mouse to charge, then release with an aircraft locked. Mission 50 adds **Brent Ground** on **6**, for designated building targets. Mission 57 adds **Dual Launch** on **3**; its native default-mask anomaly is documented in [weapon behaviour](docs/caero_weapons.md#dual-launch-and-a-native-input-mask-anomaly).

The original **Level X** cheat is available: on game selection, press **Shift+8**, release Shift, then press **3** on the number row. Enter **`Level X`** exactly and press Enter. **X** during flight then advances through the normal campaign transition and saves progression. Use `--cheat-level-x` to activate it at startup instead; combine with `--skip-intro` for quicker testing. **Shift+X** starts the previous playable level (or restarts level 1), using the same fresh setup as `--level`; from that point the session does not write saves. It has no effect in Nightmare. Activation lasts until the program closes.

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

`src/game` contains platform-independent gameplay state calculations, including complete Caero and Skimma flight callbacks, capped game clocks, swept city collision and player crash handling, Skimma weapon state, typed object definitions, owned projectile creation and ordered direct/object/map guidance, expiry/reference repairs, object and player damage, shield recharge, beacon power and Caero energy accounting, intrusive object lists and the original random generator. `src/resources` reads the original packs. `src/graphics` contains game-specific palette, mask, cockpit, fixed-point camera/projection and original model-bytecode drawing logic. `src/render` contains platform-independent framebuffer types. `src/platform` isolates GLFW/OpenGL presentation and the low-level miniaudio PCM adapter. `src/main.cpp` is the sole application entry point. Player engine, boost, recharge and shield effects now use the original FM patches through a pinned DOSBox DBOPL chip emulator by default. `--mute` disables PCM output. Combat sources use native distance admission, Doppler, ambience, stereo positioning and physical voice allocation. The proof-of-concept oscillator lives only in test support.

No GL calls occur in CPU drawing. The presenter and audio device retain independent lifetimes and can be replaced without changing the game logic. Retired demo rendering, source-sheet entry points and input-logging callbacks have been removed.

The [reconstruction contract](docs/reconstruction_contract.md) defines fidelity and platform boundaries. [Resource loading](docs/resource_loading.md), [indexed images](docs/indexed_images.md) and [cockpit rendering](docs/cockpit_rendering.md) and [software polygons](docs/software_polygons.md) and [city/model rendering](docs/model_rendering.md) document implementation evidence and remaining work. [Skimma weapons](docs/skimma_weapons.md) records the first translated reload/state routines and remaining integration. [Caero flight](docs/caero_flight.md), [Skimma flight](docs/skimma_flight.md), [steering](docs/flight_controls.md), [clocks](docs/game_clock.md), [city collision](docs/city_collision.md) and [player integration](docs/player_flight.md) record the new runtime path and its native comparisons. [World primitives](docs/world_primitives.md) documents object allocation/recycling, definition expansion, projectile placement and the native random sequence. House conventions are in [style-guide.md](style-guide.md).

## Verification

The [scenario reader](docs/scenarios.md) also loads all original mission/presentation records, preserving formatted multilingual text and setup groups for the mission runtime.
The [font renderer and formatter](docs/fonts_and_text.md) consume original bitmap fonts and page controls, with native comparisons for glyph coverage, layout and cursor state.
The [HQ launch](docs/hangar_launch.md) now supplies the original Caero start and gate departure. [Mission execution](docs/mission_execution.md) covers the deadline/checkpoint scheduler and conditional messages. [Actor construction and navigation](docs/actor_motion.md) cover placement, proximity response, targeting, clearance, manoeuvre selection and movement; the first mission’s two aircraft now fly in the live Delphi scene and appear on radar. Their combined callback matches 2,048 original updates, including pursuit. The connected first-mission combat and docking checks are described in [first mission](docs/first_mission.md).

```sh
ctest --test-dir build --output-on-failure
./build/resource_check --data-dir ../darker --reference ../analysis/resources
```

`framework_tests` and `resource_check` are test-only binaries, excluded by `BUILD_TESTING=OFF`. Unit tests require no game assets; optional resource integration checks use the original packs and previously verified extraction. Native-reference generators in `tools/` run the unpacked original executable with Unicorn. Their checked-in fixtures contain synthetic inputs and result fingerprints; original model and map bytes remain in the user's packs.

The Caero front end now includes the original startup animation, title and four-page briefing, with game selection and pilot-name entry. Pilot slots persist in `DARKER.SAV` beside the selected game packs using the original save format. A Caero crash leads to the original looping Kismet committal presentation; Enter can advance the crash view. The supported missions advance automatically after docking, saving city state, weapon availability and the return site. Remaining fidelity checks are tracked in [campaign status](docs/campaign_status.md). See [front end](docs/front_end.md) for controls and verification.

[Retail screenshot corrections](docs/visual_regressions.md) cover briefing composition, cockpit edges, indicators, radar coordinates and door interpolation. A [record/replay comparison harness](docs/comparison_harness.md) is proposed for broader visual verification.

The six original Sound Blaster [music groups](docs/sound_images_music.md) now play through the same OPL synthesiser in startup, menus, briefings and the committal presentation. Their timed register streams match the original driver across repeated playback.

`--music=none` disables music while keeping sound effects enabled.
For an optional listening experiment, `--music=roland-lapc --roland-gm-percussion-fallback`
adds General MIDI percussion for unmapped Roland keys, retaining Munt’s warnings.
Alternatively, `--music=roland-lapc --roland-gm-percussion-bank=path/to/MTGM.MID`
uses Roland’s own conversion bank on a separate emulated device; see the
[bank download and comparison instructions](docs/music_variants.md#rolands-own-general-midi-bank-experiment).
`--music=roland-lapc --roland-gm-bank=path/to/MTGM.MID` instead loads the whole bank
first, then Darker’s custom instruments, on a single device for comparison.

`--music=roland-sc55` plays the General MIDI arrangement through SC-55 v1.21 hardware emulation. It uses the five ROMs fetched beside the packs; `--sc55-rom-dir` overrides their location. This emulates an SC-55, not an SCC-1 card.

`--music=midi|roland-lapc|gravis|soundblaster_awe32` selects the other original music arrangements. Supply `--soundfont=path/to/bank.sf2`, or use an installed system bank. LAPC-I defaults to Munt using ROMs beside the packs; an explicit `--soundfont` selects its SoundFont rendition. `--mt32-rom-dir` can override the ROM location; see [music variants](docs/music_variants.md), especially the LAPC-I instrument distinction.

Number-row 2 selects the Pinner Mimic after its mission-five introduction. M enables missile viewing for subsequent shots; F4 selects the missile-eye view. See [Mimic and cameras](docs/pinner_mimic.md) and [radar coverage](docs/radar_coverage.md).

See [campaign status and remaining work](docs/campaign_status.md) for the current playable boundary and next integration priorities.

The default `--opl=dosbox` uses DOSBox 0.74-3's DBOPL
synthesiser at the reference 44.1 kHz rate, converted to the host PCM rate.
`--opl=nuked` selects the previous synthesis for comparison. Neither option changes the original
sound patches or game logic; music and effects both use the selected core.
See [FM audio](docs/fm_audio.md) for the measured differences and verification.

[Release readiness](docs/release_readiness.md) tracks the alpha boundary,
remaining fidelity questions and distribution requirements.

See [binary packaging](docs/binary_packaging.md) for a clean installation of the
single executable, documentation and dependency notices.

Use `--language=french` or `--language=german` for the original translated menus,
briefings and radio messages; English remains the default.

AWE32 music now uses the original driver synthesis library and EMU8000 emulation:
`--music=soundblaster_awe32`. Run `scripts/fetch-assets.sh` to obtain `awe32.raw`,
or supply `--awe32-rom=/path/to/awe32.raw`. An explicit `--soundfont` retains the
previous SoundFont comparison. See [music synthesis](docs/music_variants.md).

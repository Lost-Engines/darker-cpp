# Faithful reconstruction contract

This is the contract for the first C++ implementation, not the later modernised re-engine. Verified behaviour takes priority over cleaning up unusual game rules. Unresolved behaviour stays identified as unresolved until there is evidence for a choice.

## Initial reference target

- Native amd64 Linux, C++23, with the original DOS game as the reference executable in DOSBox/native-instruction probes.
- The supplied executable edition and its five original `DARKER.00`–`.04` packs. The supported unpacked executable SHA-256 is `7599201a01aa24b7e6ad1ae4295d493821cde7c3a43b247c464522626c0380a4`.
- First gameplay milestone: the normal Caero, Delphi, first mission (`04_000 / 0`), cheats disabled, from setup through completion, return and progression.
- Initial game rendering target: original indexed pixels and palette at the original 320 × 240 viewport (native right/bottom clipping constants 319/240). The current indexed cockpit is expanded to RGBA for square-pixel presentation; display-aspect handling remains separate.
- Audio will preserve explicit original hardware profiles. Existing OPL effect verification is the starting reference; the five music arrangements must not be collapsed into one supposedly canonical soundtrack. Full audio integration/profile selection is a later milestone.

The packed executable remains a reference, not a runtime execution dependency. Identified executable-resident data, beginning with its archive directory, can become generated, provenance-labelled C++ constants. We are not introducing converted model, map or music assets into this version.

## Host boundary

The game core must not depend on GLFW, OpenGL or miniaudio. The host supplies input events and elapsed time, reads original files and presents the game's finished output. The game owns interpretation of those inputs, clocks, rendering and sound events.

- GLFW key/scancode events are host input. They are not DOS scancodes. Translate them explicitly before applying reconstructed keyboard logic; text input remains a distinct concern.
- Mouse motion passes through the original scaling/smoothing/control logic once recovered. Neither desktop acceleration nor GLFW cursor coordinates define flight behaviour by themselves.
- Presentation consumes indexed pixels and palette state. Upload, output scaling and aspect correction do not change game projection, visibility or rasterisation.
- Audio output consumes PCM. Its asynchronous callback must not advance mission/flight state, allocate game objects or determine simulation timing.
- Recorded input and explicit clock values must be usable without a window or audio device. Reference comparisons must not depend on refresh rate or host execution speed.

The single application stays runnable while the game-specific systems are connected incrementally.

## Arithmetic and clocks

Use exact widths where recovered storage or arithmetic requires them. Preserve word/byte wrapping, signed interpretation, shifts, rounding, carry-dependent calculations and lookup tables explicitly. Do not rely on C++ signed overflow, implicit narrowing, floating-point interpolation or a host pointer's numeric value to emulate them.

Resource and object offsets are values within defined domains, not native addresses. Read little-endian fields explicitly; do not cast pack bytes to C++ structs. Recovered segment distinctions, especially SS-relative object state versus code/data state, become named data ownership rather than an emulated universal pointer space.

Original tick sources, modulo comparisons, self-modifying accumulators and update order need explicit state. The measured source frequency is approximately 500 Hz; that is not permission to impose a fixed 500 Hz simulation loop, equate one tick to one frame, or discard fractional clock behaviour. Recorded traces supply the exact timestep sequence used by each comparison. The interrupt cap and frame-accounting contract are now reconstructed; outer-loop input and mission ordering still require integrated fidelity checks.

Self-modifying code should become explicit parameters or state-machine transitions where understood. Startup changes, including the sine-table extension, are part of initialisation. A readable rewrite must preserve them even when the untouched executable bytes look different.

## Named representations

The reconstruction uses named records where the original stores several different
fields consecutively. This changes how the C++ expresses the data, not the
original arithmetic or update order:

- `maths::world_coordinates<T>` names column, row and height;
  `map_coordinates<T>` names the two horizontal components. Position words and
  fractional bytes remain separate, with their original widths. Indexed access
  remains available for algorithms that perform the same operation on each axis.
- `maths::attitude_angles` names heading, pitch and roll. Projectile angular rates
  name pitch and turn separately; their unused native word is retained when a
  pool record is reused.
- Object definitions expose named craft, projectile and player views of the
  eight overlapping role bytes. `std::bit_cast` preserves those bytes without
  accessing an inactive union member. Resource parsers still read individual
  fields explicitly rather than interpreting external bytes as host structures.
- `object_update` names the native callbacks while retaining their original
  numeric addresses. Reference comparisons use `std::to_underlying` at the
  boundary to captured native values.
- Scenario actors have named behaviour fields and active, reserve and free
  groups. Combat frame timing, input and scenario context are separate records;
  Skimma armament groups the state used by its weapon routines.

Arrays remain appropriate for actual sequences, lookup tables, fixed pools,
encoded payloads and reserved bytes. Native-reference fixtures retain their
captured representation. These changes do not alter ownership, object identity,
allocation order, random-number consumption, timing or the main program's flow.

## Ownership and failure behaviour

A decoded resource is an owning byte buffer. Parsers may create bounded views while its owner remains alive. Mutable game state must not alias immutable packed input by accident. Resource reload/reset behaviour will be recovered per consumer; no universal cache or object framework is assumed yet.

The first loader reads all five compressed archives into owned host memory and decodes resources on demand. This intentionally replaces DOS file handles and EMS/XMS caching. It does not claim to reproduce allocation timing, streaming pauses or original memory exhaustion behaviour.

Malformed input is rejected with bounds/output-limit checks. Those checks are reconstruction safety behaviour, not evidence that the DOS executable diagnoses corrupt data identically. Files are read-only. Saves will be written as explicit user-visible operations once that subsystem is implemented.

## Evidence and acceptance

Each milestone must state what it checks and what it leaves out:

1. Resource loading: all 164 resources compare byte-for-byte with independent exports previously checked against native decoder `437C`.
2. Arithmetic/state helpers: compare inputs, outputs and relevant state mutations with native-instruction fixtures, including boundaries and wrapping.
3. Rendering: compare indexed pixels and palette state before host scaling; viewer resemblance alone is insufficient. Clipping, ordering and pixel edge rules remain part of fidelity.
4. Audio: compare event/register streams and clocks before judging final PCM, whose output also depends on the chosen chip/device emulation.
5. Gameplay: compare complete initialised scenarios, input/tick sequences, actor ordering and transitions. Controlled fixtures are labelled as such, never as captured playthroughs.

Ordinary Catch2 tests require no proprietary assets. Original-pack comparison is an optional CTest integration test enabled by explicit local paths. Reference files never become build/runtime requirements for the resource library.

## Progression and deferred work

The executable now connects the original assets and software renderer to the
campaign through stage 116, including Delphi, tunnels, Halon, the final battle
and ending, plus Nightmare. Both craft families, weapons, scripted actors,
mission messages, supply pads, automatic docking, menus, original-format saves,
FM audio allocation and stereo have implementations and comparison coverage.
See [campaign status](campaign_status.md) for the scope of each controlled check.

Caero launch and tunnel entry/exit paths include their original setup and
conditional presentation behaviour. Pause supports single stepping and consumes
the resume key. Debugging options are explicit deviations, including noclip's
free tunnel steering and unlimited flight power; they are not baseline evidence.

A connected campaign is not yet a verified faithful game. Priorities are
integrated collision/update ordering, state persistence across unusual exits,
rendering and camera differences, and remaining audio gate/event behaviour.
Retail playtesting has exposed differences despite passing isolated native
fixtures. In particular, [runtime instruction patches](runtime_patches.md) must
be applied by their producers before treating a callback comparison as evidence.
The Dual Launch release-mask and Halon enemy-gun corrections document examples.

Converted assets, configurable gameplay controls, adjustable fog, alternative world renderers and the browser re-engine belong to the second project stage. They must not silently enter fidelity tests for this one.


## Optional enhancements after completion

First complete and verify the faithful reconstruction. Afterwards, this same
reconstruction may gain opt-in extensions which preserve the original game's
character and rendering mechanisms, separately from the converted-asset/browser
re-engine. Keep the original settings and arithmetic available as the reference
mode; enhancements must not change the baseline fidelity comparisons.

Deferred candidates:

- **Longer viewing distance:** widen coordinate arithmetic sympathetically across
  placement, projection, culling and sorting, and review traversal limits. The
  radius-only experiment caused nearby tiles to disappear through original word
  wrapping; increasing a constant is insufficient. Check continuity while moving
  and turning, as well as distant geometry and unchanged baseline frames.
- **Higher internal rendering resolution:** extend software drawing, clipping and
  framebuffer addressing coherently, including cockpit/HUD placement. This means
  more rendered pixels, beyond the existing host-window scaling.
- **Customisable field of view:** make projection and corresponding visibility
  bounds configurable together, preserving the original projection as default.
- **Other small optional presentation tweaks:** consider only once the complete
  game provides a reliable behavioural and visual reference.

Do not implement these extensions during reconstruction milestones. Defer their
API and implementation design until the working original mechanisms are in place.

See [rules, representations and extension points](engine_rules.md) for the owning
types, constants and relationships to consider when modifying this subsystem.

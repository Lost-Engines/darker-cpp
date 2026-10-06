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

Original tick sources, modulo comparisons, self-modifying accumulators and update order need explicit state. The measured source frequency is approximately 500 Hz; that is not permission to impose a fixed 500 Hz simulation loop, equate one tick to one frame, or discard fractional clock behaviour. Recorded traces supply the exact timestep sequence used by each comparison. The interrupt cap and frame-accounting contract are now reconstructed; outer-loop input polling, modal pauses and mission scheduling still need integration.

Self-modifying code should become explicit parameters or state-machine transitions where understood. Startup changes, including the sine-table extension, are part of initialisation. A readable rewrite must preserve them even when the untouched executable bytes look different.

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

Original indexed assets, model/city rendering, steering, both craft flight callbacks, city collisions, flight cameras, player FM effects and several live cockpit producers are connected. Caero now starts inside its original HQ with its engine enabled, automatic boost charging and animated gate departure; Skimma starts remain airborne checkpoints.

The scenario reader, bitmap fonts, mission timing/wait/message scheduler and actor construction/navigation components have native comparisons but are not yet a complete connected world. Next connect scenario activation and actor updates, firing/collision/lifecycle handling and briefing/message presentation, then complete the first mission's objective and return-to-HQ cycle. Preserve native update order when connecting these independently checked components; passing their isolated fixtures does not establish a working mission.

Enemy behaviour, remaining weapons, other craft/cities, exceptional missions, spatial audio and menu/save integration follow with their own reference evidence. Unknown save fields, the anomalous convoy, full visibility/raster contracts and clock/pause behaviour remain open; see the existing [analysis inventory](../../docs/reconstruction-evidence-inventory.md).

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

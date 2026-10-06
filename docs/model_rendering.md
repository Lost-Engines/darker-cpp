# Original geometry banks and projection

`resources::geometry_bank` owns decoded original bank bytes. Its typed city
records name the model offset, column/row fractions, collision marker and variant
limit; the two still-unexplained bytes retain explicit unknown names. Special
slots retain their shared pool offsets. Model bytecode and trailing world data
are bounded views into the owned resource, without conversion or relocation.

City model selection follows the alternate link first, then bank-masked damage
links. Special model headers are not interpreted as city link records. The
original-pack integration check compares native selection fingerprints for all
256 states of all 391 city types in banks 30–32. Ordinary tests use synthetic
headers to check traversal order, aliases and rejection of truncated/invalid data.
Regenerate the fingerprints with `tools/generate_geometry_bank_reference.py WORKSPACE`.

`graphics::model_projection` translates the far-path coordinate cache and signed
projection at `FC97`, with component set/zero/negate operations from the `FDxx`
and `FFxx` handlers. Coefficients, translation and screen origin are supplied by
the caller. Both direct projection and retained camera-space vertices share this cache.
Signed products discard their lowest byte before caching, and negation acts on
that cached result rather than recomputing the product. Depth is accumulated with
fractional carry before signed division. Original division faults are explicit
errors rather than unsafe host arithmetic.

There is a noteworthy shared field: component A's and component B's vertical
products both write the fractional byte at `FCD3`. The most recent operation on
either component replaces or negates it. Their whole-word contributions remain
separate. This is present in both the unpacked executable and the captured live
DOS memory (`convoy-live/attempt-1/briefing-state.bin.gz`, CS 01A2), so a conventional
fresh matrix multiply is not a faithful replacement for the stateful interpreter.
The C++ cache represents this shared byte explicitly.

`tools/generate_model_projection_reference.py WORKSPACE` executes 24 uninterrupted
coordinate streams (768 vertices), with varied coefficient matrices, translations,
fractions and origins. Set, zero and negate operations all execute as original
instructions. Tests compare both projected coordinates and depth after every
operation. No drawing or visibility commands are included in these streams.

## Flat drawing streams

`graphics::draw_model` executes original pool bytecode against the stateful
projection cache and the original flat polygon filler. It follows relative
calls/jumps and both visibility branches, preserves vertex cursor rewrites, and
resolves palette groups through a supplied distance shade table. Dynamic colour
codes use the original byte wrapping and clamp. The distance branch tests the
sign bit of a byte subtraction, including its wrap, rather than a host signed
comparison. Unsupported commands fail explicitly; this is not yet a complete
world renderer. Gouraud commands support both original interpolation and the
flat fallback (`312A`), which skips each vertex shade operand.

`tools/generate_model_renderer_reference.py WORKSPACE` captures 108 complete
synthetic drawing streams with the original interpreter, projection, colour
lookup, culling, clipping and edge walking. Only VGA planar span writes are
replaced with contiguous index writes. Tests compare all 76,800 pixels via a
64-bit fingerprint. No original model bytes are embedded in these fixtures.

`tools/generate_original_model_reference.py WORKSPACE` additionally captures 702
frames from actual city-bank models under two controlled coefficient matrices.
The optional original-pack integration check loads these models from the user's
archives and compares complete frames. It covers all three banks, including
shared drawing subroutines and visibility branches. These matrices are test
inputs, not a claim to have reconstructed camera setup.

Four initially-flat candidate views execute Gouraud instructions reached only
through conditional branches (bank 30 types 63, 64 and 77). The extraction
inspector's fall-through opcode inventory cannot identify every executed command.
The native capture rejects unsupported drawing paths explicitly. With the
original Gouraud-off dispatch installed, all four views now pass, alongside the
other models containing shaded faces.


## Camera coefficients and placement

`graphics::make_camera_basis` translates `1D63–1E76`, preserving each separately
rounded signed product, wrapping additions and the A/B swap when installing the
model coefficients. It accepts final camera angles, applies the caller's
fifteen-unit rounding bias and uses the original 1024-entry sine table. Camera
tracking, inverted-pitch folding, external-view choice and stereo offsets are
not part of this calculation.

`graphics::place_model` translates `2E21–2EAA`: fractional map position, camera
subtraction, selected model height, ordered whole/fraction products and the
approximate sorting distance. The sorting estimate complements negative words
(abs minus one), and uses camera altitude independently of model height. Its
units are explicit in the public records: model column/row are 1/256 cell; the
camera subtractors are 1/1024 cell. Nearby relative coordinates wrap as original
words, rather than expanding the original visibility range.

`tools/generate_camera_reference.py WORKSPACE` captures 256 camera bases and
placements, including quarter turns, table-index boundaries, negative heights
and wrapping positions. Tests compare all nine coefficients, all three whole
and fractional coordinate pairs, and the sorting distance (4,096 assertions).

The original-pack check also compares 96 complete perspective frames of the two
models used by the application milestone, covering a full heading turn and
three elevations. Camera coefficients come from the native setup, depth varies
per vertex, and clipping uses the Caero/Skimma view heights of 168/180 rows.
These checks exercise camera setup, model interpretation, projection and drawing
together; they do not substitute for scene traversal or near-plane handling.


## Near clipping, lines, discs and animation

The near path retains signed 24-bit camera coordinates and clips at depth 32.
`22B4` finds intersections through ordered arithmetic halvings, with asymmetric
rounding and distant-coordinate saturation. This is deliberately not replaced
by a floating-point intersection formula. Tests compare 512 native intersections
and 264 complete synthetic near-clipped frames in flat and Gouraud modes, including winding, visibility
branches and the Gouraud-off path.

World lines share the HUD line rasteriser after the original `A86D` viewport
clipping. The 1,024 endpoint cases include both distant rejection and crossings
close to the viewport. Disc drawing preserves `A5C4`'s overlapping span writes,
including the visibly asymmetric small radii, with 224 complete native frames.

Model commands `32/35/38` implement gate interpolation in both projection paths;
`3B` reads the cell-state-indexed animation parameter. The direct interpolation
path reads `FCD3/FCD4` together, accidentally including the adjacent `BA` MOV
opcode as a fractional contribution to vertical coordinates. The C++ path
preserves this extra 186/256 unit. A dedicated shallow-depth fixture exposes it:
96 complete synthetic gate frames match both original paths, including fractional
translation and almost-fully-open states.

Fountain parameters follow `DB1A`, retaining six amplitudes, signed products and
phase wrapping. A fingerprint compares every parameter byte over all 2,048 clock
steps. At the measured PIT divisor 2,386 (approximately 500.075 Hz), this repeats
in approximately 4.095 seconds. The application uses elapsed inspection time for
this clock until the original game scheduling and pause handling are connected.
The original-pack check also covers 176 complete beacon, line, gate and fountain
model frames at varied headings, animation states and projection paths.

## City setup and traversal

`game::make_city_map` expands type bytes into typed mutable cells. Delphi's fresh
beacon state follows `BB6D`: only type-one cells on the nine-cell lattice receive
`FF`. `assign_city_variants` reproduces `BBFC`'s per-type counter in map order.
Full initial map fingerprints match native setup for both cities.

`collect_city_cells` translates `26EE`'s circular row spans and heading half-map
selection. Steep pitches scan the full circle. Candidate order is retained, with
192 native cases covering empty cells, map edges, headings, pitch and radius.
The original setup at `BCE3–BCFB` installs radius 15 for map modes below four
(Delphi/Halon), and radius eight for underground modes. The application uses
15; its initial eight-cell inspection radius was a milestone mistake, corrected
after the short-distance pop-in was noticed. This is the ordinary Delphi/Halon
path; underground visibility propagation is not implemented here.

A temporary larger-radius option was withdrawn after interactive testing showed
nearby tiles popping out of existence. Merely increasing the scan radius retains
original coordinate wrapping: placement doubles the signed 1/1024-cell difference
into another 16-bit word (`2E21`), leaving about 16 cells of unwrapped range on
either map axis. Completing 2,000 camera renders without errors did not establish
visual correctness. The application now fixes the radius at the original 15.
A correct extension requires coordinated placement/projection arithmetic changes;
see [deferred enhancements](reconstruction_contract.md#optional-enhancements-after-completion).

`place_city_cell` translates `2A1A`: linked state selection, type-relative origin,
header height and extent, signed culling, near/direct path selection, and the
flat-distance threshold. There are 1,564 native placement/cull cases across all
391 types and four cell states. Background entries draw in reverse insertion
order; ordinary entries sort by decreasing distance with stable ties, matching
the original list and tree traversal.

Forty-eight complete scene frames execute original setup, traversal, placement,
ordering, beacon selection, distance-shade selection, bytecode and drawing in
Unicorn. Both Gouraud and flat modes are checked, including the native
distance-dependent switch to flat drawing.
Delphi cases include beacon strengths 255, 128 and zero; Halon uses its native
constant-strength path. These replace the earlier identity-table scene fixtures.
The original-pack integration check compares initial map bytes, accepted model
counts and all 76,800 framebuffer pixels via fingerprints. These are composition
checks in addition to the individual routine fixtures, not comparisons against
a separately approximated renderer.

Regenerate the additional fixtures with `tools/generate_near_clip_reference.py`,
`generate_near_model_reference.py`, `generate_screen_primitives_reference.py`,
`generate_model_animation_reference.py`, `generate_model_effect_reference.py`,
`generate_city_scan_reference.py`, `generate_city_placement_reference.py` and
`generate_city_frame_reference.py`, each taking the analysis workspace path.
The generators require Unicorn and access to the original unpacked image;
ordinary unit tests require neither.

## Distance shading and beacon lighting

`graphics::distance_shading` translates `B73C–B77F`: 60 rows of 28 shade entries
for city scenes, or 28 rows for underground. Each shade starts with fractional
value 128. Its signed step towards shade one is divided using IDIV truncation;
ordered subtraction generates each successive distance row. Tests compare every
byte of both complete native tables, plus 448 selections across light strengths,
distance boundaries, far saturation and negative near-path origin depths.

`2CE4–2D04` selects one table per model from its origin depth. For strength `L`,
the effective depth is `uint16(depth + 16*(255-L))`; its high byte selects the row,
clamped to the final row. Near-path negative origin depths first become zero.
The same rotation supplies `F0h | (L >> 4)` for special colour codes 28–31.
Normal face colours retain their upper three palette-range bits and replace the
lower shade through the selected table. This is palette-index arithmetic, not
RGB interpolation, a new fog equation or per-face illumination.

Delphi's `2D85` obtains `L` from the state byte of the nearest nine-cell lattice
position, using the same startup coordinate mapping as beacon charging. Unlike
the charging routine, this renderer does not check for type one. All valid city
coordinates map to `9*floor((coordinate+4)/9)`; native whole-scene comparisons
exercise the lookup and mutable state together. Halon's `2DAC` supplies strength
255 independently of beacon state. The city renderer now uses these paths for
every selected model, including linked alternate/damage models.

Consequently, a beacon outage changes the nearby buildings' shade selection and
special colours as well as charging capability. The new scene fixtures set
strengths directly to verify rendering; mission-driven outages are not connected
to the application yet. The table generator accepts both original scene sizes,
but this does not implement underground visibility.

Regenerate lighting fixtures with `tools/generate_model_lighting_reference.py
WORKSPACE` and composed scene fixtures with `tools/generate_city_frame_reference.py
WORKSPACE`. The scene capture now executes the original lighting routines without
the earlier identity-shading hook.

## Gouraud polygons

`graphics::draw_gouraud_polygon` reproduces `A99D–AF51`, including palette-index
accumulators, colour clipping, edge stepping and discrete colour-band lengths.
The original VGA masks are represented as contiguous indexed pixel spans. Narrow
spans distribute multiple shade steps per pixel; wider spans distribute repeated
colour bands with integer quotient/remainder error tracking. Edge colours advance
before drawing, just like edge coordinates, and their step includes the original
extra unit before signed division.

Model commands `0E/0F/11` resolve each vertex shade through the selected distance
table. The starting accumulator is `((range+shade)<<8)+shade+128`, retaining the
otherwise unusual repeated shade byte. Near-plane clipping at `2195` interpolates
colour using a ratio of **whole** depths, independently of the coordinate-halving
algorithm; replacing these with one shared floating-point interpolation changes
pixels. Both mechanisms are now implemented.

`tools/generate_gouraud_reference.py WORKSPACE` captures 256 synthetic polygons,
including gradients in both directions, palette ranges, thin spans, clipping and
winding. The near-model fixture now captures 264 whole model frames in both modes.
Complete city comparisons additionally verify per-model distance fallback and
restoration of Gouraud dispatch between models. Native capture follows VGA plane
masks and records CPU-written colour bytes when the original rasteriser enables
that mode; flat fills retain the earlier set/reset capture.

The common entry is now `draw_model`, with explicit path and shading arguments.
F9 changes only the application shading selection, preserving the original flat
path and all earlier reference tests. No alternative renderer or modernised
lighting model is introduced.

## Application milestone

The single `darker` application draws the city from the reconstructed player
pose. The temporary free-camera controls have been removed; see
[player flight](player_flight.md) for the airborne checkpoint and its limits.
The scene uses Gouraud mode with original distance shading and beacon-state
lighting. F9 selects the original flat fallback. The viewport is clipped before
the Caero's eight-row destination offset is applied.

Compass and map-grid coordinates follow the craft. Caero energy, damage and
altitude gauges and the Skimma speed chart now follow live state. Weapon icons
remain empty until weapon integration. Other actors, missions and underground
scenes are not connected yet.

Isolated Xvfb/Mesa checks exercise all three craft selections, steering, boost,
braking, engine/shield commands, shading, window resizing and Escape. Earlier
native renderer comparisons and the 2,000-position city sweep remain applicable;
no tests preserve the removed temporary controls.

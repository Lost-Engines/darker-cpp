# Cockpit background and masked instruments

`darker` reads source sheets 00/16, 00/17 and 00/18 from the original packs. It implements the background transfers and the main masked instrument updater in portable C++23. Rendering remains indexed until the existing host adapter expands the palette to RGBA.

## Implemented paths

- `BF72–BF94`: copy sheet rows 0–135 to cache rows 0–135, then sheet rows 96–199 to cache rows 136–239. The cache is immutable while instruments draw into a separate display surface.
- `460B`, directories `4615/4D70`: select nine Caero or four Skimma instruments. All 13 descriptors and 127 strip masks are generated from the supported executable, with address/field provenance retained.
- `51B8`: select strips and apply their independent per-row horizontal skip/width masks. Colour index zero is copied like any other colour; transparency is the absence of a mask write.
- `457B–45A6`: increasing counts copy new on strips; decreasing counts restore base strips from the cache; equal nonzero counts redraw the last strip. The engine's high-bit dim state selects the source word preceding its descriptor. Other high-bit states are rejected until their producers/meaning are established.
- `7919/23B1/AF57`: in the normal Caero view, logical rows 0–167 address physical rows eight pixels lower. Lower cache rows remain unshifted. Skimma uses no offset. This implements the native cause of the previously observed Caero source alignment correction.
- `582B–5844`: ordinary Skimma output is capped at 16 strips; the upgrade permits 20.

The host presenter now accepts both the 320×200 source sheet and the 320×240 cockpit, retaining nearest-neighbour scaling and letterboxing. `copy_rectangle` and `copy_mask` are bounded software operations over separate tightly packed index surfaces. Their clipping preserves corresponding source/destination pixels. This translates the logical mono copies; it does not emulate VGA planar memory, duplicated stereo destinations, every mode patch or the full original clipping system.

Executable constants are regenerated with:

```sh
python3 tools/generate_cockpit_tables.py ../analysis/unpacked/image.bin
```

The generator rejects an unsupported executable hash. It embeds masks and coordinates, not bitmap artwork. Unparsed descriptor tail bytes are recorded in comments. The word at 4D1A also supplies the following engine descriptor's alternate source. Runtime needs neither Python nor extracted PNG/JSON files.

## History-dependent instrument pixels

The Nayas receiver's three masks overlap, as do two incoming-power strips at one pixel. A decrease restores only the removed strips: it does not redraw all remaining strips. Therefore a state reached by decreasing can differ from the same count reached from zero. The existing browser's rebuild-from-background approach does not expose every such distinction.

The C++ instrument updater retains the original incremental behaviour. A native x86 probe confirmed receiver transitions 0→3, 3→1, 1→1 and 1→0. In particular, 3→1 restores strips 1 and 2 from source word 58DE; a subsequent equal-count redraw repaints strip 0 from source word 2C1D. These are display-updater semantics, not proof that every synthetic state sequence is produced during gameplay.

## Verification

Unit tests cover opaque copying, clipping correspondence, mask gaps, background layout, instrument restorations, craft limits and engine alternate-state redraw, alongside the original-pack integration check.

At the cockpit-only milestone, **381 exact RGB comparisons** against independently extracted background and masked-strip PNGs covered all three craft, every supported instrument count, decreases from maximum, zero restoration and dim/normal changes. The comparison replayed base-strip restoration in order, preserving receiver overlap. Its temporary application controls and comparison script have now been retired as the main application progresses to city rendering; the underlying instrument tests remain.

An Xvfb/Mesa smoke check compared actual window pixels with headless output at 3× scale and after resizing to 800×700 with 2× letterboxing. Keyboard selection, increase/decrease, engine dimming and Escape shutdown passed.

## Remaining boundary

The windscreen now contains the original city; see [city/model rendering](model_rendering.md). Gauges remain sample values until health, flight and charging state are connected. The following sections record the successive HUD reconstruction steps and their native verification.

Source RGB colours and square-pixel presentation retain the previous inspection convention; original runtime fades, DAC quantisation, display timing and stereo/VR modes remain separate. The cockpit inspector is the current milestone in the single application; its temporary controls will be replaced as gameplay arrives.


## Caero bitmap callbacks

`graphics/bitmap_hud` implements changed-field dispatch for the two weapon icons and two normal cockpit coordinate fields, using the same immutable indexed cache and opaque rectangle blitter. There is no input or windowing code in this module.

- `5429/542C`: row goes to (44,185), column to (56,185). The input is the native encoded position byte, not a displayed grid number. For bytes 1–127, the display is `floor((value - 1) / 9) + 1`, written as two digits including a leading zero. Both zero and bytes 128–255 take the cached-background restoration path: native CBW/DEC/JS makes the signed-byte distinction significant. Glyphs are 4×5 at source X=308, physical Y=`8 + 5*digit`.
- `5484/5487`: 8×12 weapon artwork at physical source `(140 + 8*selection, 8)` is copied to (260,195) or (268,195). Allowed primary IDs are 1,2,3,7; secondary IDs are 4,5,6,8,9,10. These restrictions come from weapon selection logic, rather than the low-level rectangle callback itself.
- Selection zero restores the corresponding cached panel. This follows the user-confirmed empty-slot behaviour. The exact original zero-selection clear/skip call chain is still unresolved; the low-level selector alone would incorrectly sample the altitude artwork. Do not treat this high-level restoration choice as newly proven native control flow.

The main program supplies fixed sample state until position and weapon producers are translated. No new inspection keys or command-line options were introduced. Skimma screens remain unchanged.

Three additional engine tests exercise native coordinate boundaries, allowed weapon slots, empty-state restoration, pixel extents and changed-field dispatch. Coordinate expectations at 0,1,9,10,81,82,127,128,135,255 were checked by executing the original routine. All 22 CTest cases pass. The existing 381 image comparisons now include the recovered Caero glyph/icon PNGs in their expected composites; they still exercise all prior gauge/restoration paths.

## Skimma and navigation follow-up

The main application now also draws the following from executable-resident constants and original cockpit cache pixels:

- **Skimma bearing (`520A/5227`)**: restore the previous symbol's mask, then draw the new symbol. Zero turns it off; symbols 1–7 are preserved. All 49 symbol-to-symbol transitions restore correctly.
- **Skimma weapon status (`52AB/52E0/5356`)**: four source patches per slot, with the third slot restricted to upgraded Skimma. State zero uses the native restoration source. Masks retain coverage, rather than copying their bounding rectangles.
- **Caero compass (`5384–53D2`)**: the 38 original signed-byte offset pairs are folded/reflected into 136 phases. Heading uses the native byte rotation and quantisation at `56C7–56CF`. Old pixels are erased with index zero; the five new colours are 9E/9C/9C/9C/9E.
- **Normal Caero radar (`5AC9–5B55`, normal contact callbacks)**: unsigned 16-bit position differences, the 42-cell candidate window, signed high-word products from the original 1,024-word sine table, word wrapping, arithmetic shifts, radius clipping and group-specific distance colour are retained. Positions include fractional cell bytes. Hidden and uncovered contacts are rejected. Contacts draw in supplied order.
- **Skimma weapon ring (`5D83–5DDC`)**: native byte increments 146/255/205 produce 14/8/10 positions. Signed sine high bytes determine placement; source selection preserves destination alignment and the pre-draw remaining-count decrement. Normal-play centre/baseline (160,88), radius 15–127 and valid working capacities are supported. Ordinary Skimma cannot select weapon index 2.

The main application supplies fixed demonstration state without new keys or options. This is not live radar: world-object traversal, coverage/interference production, contact classification, clearing between world frames and scheduling still belong to the game loop. The contact drawer only writes current contacts. Similarly, the ring renderer consumes radius/count inputs; it does not yet implement reload deadlines, spread smoothing or the non-play baseline. Enlarged radar is covered below; target outlines and attitude drawing are covered in the following section.

Production tables are recovered directly from the hash-checked executable by `generate_cockpit_tables.py`. Tests independently use captures from the earlier native probes, generated with:

```sh
python3 tools/generate_navigation_reference.py ..
```

Reference headers record source hashes and contain no runtime dependency on analysis JSON or Python. All 136 compass phases, 144 whole-cell radar cases, eight additional native fractional/wraparound cases, 19 Skimma callback source/mask cases and 140 ring-placement cases pass. The latter cases compare complete synthetic indexed surfaces against native-capture-derived checksums. Checks focus on permanent engine arithmetic and drawing behaviour, not inspection controls.

## Procedural line and vector drawing

`graphics/procedural_hud` now translates drawing code instead of using pre-rendered frames:

- `5E6F–5ED3` computes attitude endpoints from the original sine words using signed high-word products. Both table indices accept all 1,024 positions. The pitch-high-byte shade adjustment and alternate-colour branch are retained, including byte wrapping. The returned coordinates include the normal Caero eight-row viewport offset.
- `5F5D/A77B` orders endpoints and draws the original horizontal/vertical runs using quotient/remainder stepping. Its endpoint convention is unusual: X is inclusive, while non-horizontal lines span the Y difference; descending lines begin one row below the supplied start in the direction of travel. Half-error ties, zero-length lines, vertical lines and the original wide-span case are covered by native comparisons. This entry accepts endpoints already within the display; the original world-line clipping path is still separate.
- `5F39/E302/E305` draws the small target marker, large target marker and Skimma aim symbol by interpreting their executable-resident byte streams. Command bits 0/1 advance the horizontal and vertical positions; the arithmetic right-shifted signed byte supplies the colour increment for the next pair of pixels. This is why the native outlines have graded colours rather than one flat colour per half.
- `5ED6–5F0D` uses that same outline decoder for the fixed Caero surround. Its final pair begins on different rows and advances inward, preserving the original separate row-pointer behaviour. The colour parameter at `5F01` remains a supplied raw word; its gameplay producer is not inferred.

The main application now renders the level Caero attitude line and surround, and the Skimma aim mark at its native X=164 and normal-view centre Y=90. Target marker shapes are available to the renderer but are not drawn as fictitious acquired objects in the default display. No new application, input controls or command-line options were added.

Tests compare all 289 native attitude-frame rasterisations, 22 additional native line cases (reversals, slope boundaries, axes, point and 320-pixel spans), 12 additional native attitude endpoint/colour cases, all three target-marker captures and the fixed surround. Production code contains only lookup tables and original command streams; captured frames/checksums live in test references. Regeneration requires the existing Python reverse-engineering dependencies (Pillow and Unicorn):

```sh
PYTHONPATH=/tmp/darker-python python3 tools/generate_procedural_reference.py ..
```

The existing 381 whole-cockpit comparisons include these additions. The enlarged-radar and navigation-contact follow-up below completes those drawing consumers. Remaining HUD work includes live display producers and frame update ordering. General world clipping, camera coupling and target acquisition remain separate from these verified drawing primitives.


## Enlarged radar and height-coded navigation contacts

`graphics/navigation_hud` now assembles the normal mono Caero enlarged radar from drawing operations and original cockpit glyphs:

- `A5D6/A712`: the radius-63 disc uses integer scanline stepping, exclusive right edges and symmetry about two centre rows. The background, centre symbol and heading line reproduce all 256 captured heading views. This is the specific radar geometry, not a general replacement for every original circle/clipping path.
- The existing contact projection retains its candidate window and signed high-word products. Enlarged mode multiplies the rotated words by three **before** shifting, preserving word wrapping, rejects squared distance at 3965, and uses the original group-dependent distance colours. Contacts use the twelve-pixel rounded diamond.
- Large coordinates use 8×7 glyphs at source X=312 and physical Y=`8 + 7*glyph`. Unlike the small coordinate callback, an unavailable position draws blank glyph 10 rather than restoring the cache. Row, separator and column occupy physical Y=41.
- `5B92`: the navigation-contact drawer restores the six-row mask using one of four alignment-specific source positions, then draws the diamond. Height differences wrap to a signed byte before halving. Negative magnitudes use complement, not absolute value; magnitude is capped at ten and selects the original palette ramp. Position and reference height are supplied by callers; their live producers are not yet translated.

The application exposes the assembled Caero view through the original hold-Insert/keypad-0 binding. It draws onto a copy of the current cockpit surface, so release restores the normal view without disturbing instrument state. Contacts, heading and coordinates remain fixed sample inputs. Coverage, interference, allegiance classification, world traversal, update scheduling and stereo variants remain outside this milestone.

All 38 CTest cases pass, including 256 complete enlarged-surround checksums, 144 enlarged projection cases and 144 height/alignment contact checksums derived from native captures. The existing 381 whole-cockpit comparisons still pass. An isolated Xvfb/Mesa check confirmed that actual window pixels match headless output, both Insert and keypad 0 display the enlarged view, releasing either restores the original pixels exactly, and Escape exits cleanly.


## Live Skimma shield and warning producers

`measure_skimma_instruments` now follows `579C–5828`: the low-altitude warning
is **one strip** below height 1024, or zero otherwise. `579C` uses `ADC AL,AL`
after clearing AL, so its carry contributes one, correcting the earlier
analysis note that described value two. The optional warning blink follows
clock bit 0100 when its enabling mode is present.

Enabling shields starts the original wrapping deadline `clock + 06FF`.
The display passes through its startup phases, then limits the visible strength
by the shield reserve's high byte. Its deadline correction retains the original
mutated DX value, including the byte-only DH shift. `5845` maps strength to
shield strips and returns an alternating first-strip index for the startup
pulse. `5192` draws that pulse into the same shield mask; once ready, the normal
454C component takes over. The producer exposes a ready-sound request for the
future sound consumer; the application is still silent at this milestone.

1,024 native comparisons cover warning thresholds/blinking, enabled/disabled
shields, depleted reserves, startup phases, deadline wrap and mutation, and both
5845 strip-range outputs. Ordinary tests use captured reference data and do not
need Unicorn. Original directional-hit effects and weapon/mission indications
still require those systems to be integrated.

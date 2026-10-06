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

The C++ inspector retains the original incremental behaviour. A native x86 probe confirmed receiver transitions 0→3, 3→1, 1→1 and 1→0. In particular, 3→1 restores strips 1 and 2 from source word 58DE; a subsequent equal-count redraw repaints strip 0 from source word 2C1D. These are display-updater semantics, not proof that every synthetic state sequence is produced during gameplay.

## Verification

All 19 CTest cases pass, including the full original-pack decompression comparison. New unit tests cover opaque copying, clipping correspondence, mask gaps, background layout, all instrument restorations, craft limits and engine alternate-state redraw.

The reusable image comparison performs **381 exact RGB comparisons** against the independently extracted background and masked-strip PNGs. It covers all three craft, every supported instrument count, decreases from maximum, zero restoration and dim/normal changes:

```sh
PYTHONPATH=/tmp/darker-python python3 tools/verify_cockpit.py
```

This development-only check requires Pillow and the parent workspace's existing analysis exports. No reference artwork is installed into the application. The comparison replays base-strip restoration in order, preserving the receiver overlap rather than assuming a unique final image per count.

An Xvfb/Mesa smoke check compared actual window pixels with headless output at 3× scale and after resizing to 800×700 with 2× letterboxing. Keyboard selection, increase/decrease, engine dimming and Escape shutdown passed.

## Remaining boundary

The black windscreen is an explicit empty viewport, not a rendered scene. Slider/key values are inspection inputs, not health, flight or charging simulation. The Caero bitmap callbacks below now cover its weapon icons and small coordinate digits. The follow-up below adds the compass, normal radar contacts and Skimma bearing/weapon graphics. Enlarged radar, target markers and attitude-line rasterisation remain to be implemented. Those can now use the same indexed surfaces and recovered copying primitives.

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

The main application supplies fixed demonstration state without new keys or options. This is not live radar: world-object traversal, coverage/interference production, contact classification, clearing between world frames and scheduling still belong to the game loop. The contact drawer only writes current contacts. Similarly, the ring renderer consumes radius/count inputs; it does not yet implement reload deadlines, spread smoothing or the non-play baseline. Enlarged radar, target outlines and attitude-line rasterisation remain separate.

Production tables are recovered directly from the hash-checked executable by `generate_cockpit_tables.py`. Tests independently use captures from the earlier native probes, generated with:

```sh
python3 tools/generate_navigation_reference.py ..
```

Reference headers record source hashes and contain no runtime dependency on analysis JSON or Python. All 136 compass phases, 144 whole-cell radar cases, eight additional native fractional/wraparound cases, 19 Skimma callback source/mask cases and 140 ring-placement cases pass. The latter cases compare complete synthetic indexed surfaces against native-capture-derived checksums. Checks focus on permanent engine arithmetic and drawing behaviour, not inspection controls.

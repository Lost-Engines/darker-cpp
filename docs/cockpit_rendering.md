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

The black windscreen is an explicit empty viewport, not a rendered scene. Slider/key values are inspection inputs, not health, flight or charging simulation. This milestone does not draw the callback-owned weapon icons, coordinate digits, compass, radar, bearing symbols, target markers or other procedural HUD graphics. Those can now use the same indexed surfaces and recovered copying primitives.

Source RGB colours and square-pixel presentation retain the previous inspection convention; original runtime fades, DAC quantisation, display timing and stereo/VR modes remain separate. The cockpit inspector is the current milestone in the single application; its temporary controls will be replaced as gameplay arrives.

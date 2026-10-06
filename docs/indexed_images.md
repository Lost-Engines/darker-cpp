# Indexed image milestone

The original-pack path now reaches the window: `archive_set::load` decompresses a resource, `decode_bitmap` reads its palette prefix and index bytes, and `expand_palette` resolves those bytes for the existing platform presenter. No converted assets or Python code are needed at runtime. The generic framework demo remains separate.

## Original format

The palette stream follows executable routines `4185/4190`, documented in the parent project's `docs/model-colours.md` and independently exercised by `tools/verify_model_colours.py`:

- An odd command byte skips `(byte >> 1) + 1` entries, retaining their previous values.
- An even command byte is the red component of an RGB triple; the next two bytes are green and blue.
- Decoding ends after covering exactly 256 entries. The following bytes belong to the image or other resource payload.

The palette state includes a defined-entry bitset. A fresh state has no defined entries. Skipping does not make an entry defined; a supplied previous palette can provide its value. Reading a bitmap rejects use of any undefined entry. Unused undefined colours are harmless. Palette decoding is separately callable for palette-only resources and later inherited palettes.

The supported source sheets are archive 00 slots 15–18, each with 64,000 row-major pixel indices. Slot 16 is the Caero sheet; 17 and 18 are the Skimma sheets. Other resource dimensions are deliberately outside this decoder's contract. In particular, a 320×200 source atlas is not evidence that every game screen or the assembled cockpit has that height.

Stored RGB values are preserved unchanged. This milestone does not implement the original colour ramps, fades, lighting controls or VGA DAC conversion (`AFAD–B04E`). It therefore matches extracted source images, not necessarily photographed or emulated display colours. The current nearest-neighbour, square-pixel window remains an inspection display.

## Verification

- All 14 CTest cases pass, including comparison of all 164 decompressed resources against the existing extraction.
- New synthetic unit cases cover inherited colours, literal RGB decoding, stream consumption, malformed commands, undefined pixel colours, image length, and recolouring without changing indices.
- All 64,000 RGB pixels of each of the four sheets match their independently extracted `analysis/media/00_015.png` through `00_018.png` references exactly, using the viewer's headless PPM output.
- An Xvfb/Mesa window capture matches the Caero sheet exactly at 3× scaling. After resizing to 800×700, it matches a centred 2× image with black letterboxing. Timed shutdown succeeds.

Next work can build on the unchanged indexed representation: verified image dimensions and clipping, original blit/mask operations, then cockpit composition. Simulation timing, sprite animation, and palette changes should be driven by reconstructed game state rather than the host presentation rate.

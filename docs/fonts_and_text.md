# Original fonts and formatted text

`resources::font_resource` reads the three fonts in original resource 00/29: interface (107 glyphs), compact (97) and wide (96). It retains their four alignment-specific plane streams, top offsets and stored dimensions. The graphics layer converts their coverage and two-colour patterns directly into indexed pixels, preserving transparent pixels. Compact/wide advance by width plus one; interface advances by width; space advances by four. Clipping is confined to the destination framebuffer.

`lay_out_text` translates B292's commands 0–7 into positioned glyphs with original colour tokens. It preserves page termination, byte consumption, margins, signed line increments, tab boundaries, nested centring and the runtime number. Centring deliberately measures stored widths without adding the compact/wide spacing increment, matching E2C1. FFFF retains the previous colour. The caller supplies initial state and owns the returned continuation; subsequent pages and counted mission messages are not merged into the briefing.

The layout retains colour words rather than guessing their final palette indices. The final palette/latch setup, runtime font selection for each presentation, timing and screen composition still belong to presentation integration. `draw_glyph` and `draw_text` already draw using explicitly supplied ink/edge indices.

## Checks

- All **300 glyphs at four alignments**, plus spaces, match native E29C/E1FE coverage, plane-pattern selection and advances. This verifies the font decoder and software drawing; it does not assert the palette conversion performed by VGA latch setup.
- **544 extracted formatted pages** match B292 glyph positions, colour tokens, final cursor/margin state and consumed length, using the interface font as a controlled test configuration. Three additional native cases exercise centring, tabs, signed line spacing and runtime numbers.
- Two structurally exported German pages contain codes outside all three font directories: **04_009 / 3**, page offset 2796h, contains code 99h within `…rtlichkeiten`; **04_010 / 6**, offset 3A09h, is `97 00`. The safe reader rejects these explicitly. Their exact original visual behaviour and any intended correction remain unresolved; source bytes are retained. These are separate from the earlier French counted-message length anomaly.
- Focused tests check transparency, negative/right-edge clipping, missing font data, invalid codes, truncated controls and page boundaries.

Regenerate native references with:

```sh
PYTHONPATH=/tmp/darker-python python3 tools/generate_font_reference.py ..
PYTHONPATH=/tmp/darker-python python3 tools/generate_text_reference.py ..
```

The formatter probe intercepts glyph and colour calls: its results establish layout and token propagation, not a complete original presentation frame. The separate font probe executes the actual drawing path and records VGA plane choices. The application continues to start at the airborne checkpoint until mission presentation and launch are connected.

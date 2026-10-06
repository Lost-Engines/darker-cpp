# Flat polygon rasterisation

`graphics/flat_polygon` is the first world-polygon drawing primitive. It translates
native `A1B6` through the flat-fill scanline setup into the indexed 320×240
framebuffer. It consumes convex screen-space polygons and an already-resolved
palette index. Model bytecode, camera projection, near-plane clipping, distance
colour lookup, scene ordering and Gouraud shading are separate consumers/work.

The native path rejects off-screen bounds and back-facing winding, then clips
against bottom, top, right and left in that order. Coordinates and signed quotient
rounding remain integral. Bottom clipping anchors its intersection at the next
vertex; the other planes anchor at the inside endpoint. Replacing these with one
algebraically equivalent floating-point expression changes edge pixels.

Edge walkers retain the original fractional starting value of 128, advance before
the first scanline, and use different negative-slope rounding on the two sides.
Horizontal spans are half-open; top rows are included and bottom rows excluded.
The right walker starts one unit beyond its vertex. These details intentionally
differ from generic triangle rasterisers and GPU fill conventions.

The implementation uses two bounded stack buffers, with room for the maximum
byte-counted input plus viewport intersections. It performs no per-face heap
allocation. Input must be convex, as expected by the original edge walk. Bounds
and original division-fault conditions are checked before unsafe host accesses.

## Native reference

```
PYTHONPATH=/path/to/unicorn python3 tools/generate_flat_polygon_reference.py WORKSPACE
```

The hash-guarded probe runs original `A1B6` through clipping and both edge walkers.
At `A583`, it records the left/right endpoints and row, bypasses only VGA plane
writes, and resumes the shared row advance at `A566`. Row offsets are supplied
as `y*256`, matching the native scanline step. The original front/back decision,
clipping, edge selection, slope division and fractional carry all execute.

The 504 cases include triangles, quads, larger convex polygons, degenerate input,
fully clipped shapes, reversed winding and cyclic vertex starts, across full,
200-row and reduced viewports. Tests compare both nonzero-pixel count and a
fingerprint of every indexed framebuffer byte. This validates polygon coverage;
it is not a VGA register/latch emulator or a complete world-render comparison.

The default native clipping constants are right `319` (`A320`) and bottom `240`
(`A296`). They support the 320×240 reconstruction target already used by the
cockpit artwork; the earlier contract's 320×200 target was inherited from the
initial framework demonstration.

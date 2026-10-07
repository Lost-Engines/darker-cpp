# Underground visibility

The renderer now has the native underground traversal (2778–29B6), selected
separately from the surface-city circular scan. It seeds the containing cell,
visits eight cells in each cardinal direction, then processes the original
sequence of diagonal and eightfold outward passes. Model acceptance feeds the
low visibility bit used by neighbouring cells. Empty cardinal cells terminate
that ray and clear the visibility bits beyond them. The original repeated
seventh-ring side pass is retained.

`visit_tunnel_cells` keeps this bit separate from persistent gameplay damage
state. The renderer retains it between underground frames. The visitor performs
the existing native model placement/culling and appends accepted draw records;
normal painter ordering and polygon rendering follow. It does not replace the
scan with a flood fill or the ordinary city radius.

The native reference generator executes 2778–29B6 unchanged, substituting only
2A1A's model-acceptance result. Three successive scans on each of 128 maps compare
every visit, its result and all 16,384 visibility bits. These **384 frames** cover
empty cells, accepted/rejected models and retained state from earlier frames.
The fixture supplies CX=8 from the real 26EE caller; leaving CH from the preceding
scan would incorrectly lengthen the cardinal loops.

This is a renderer component, not a claim that the underground campaign is
playable. Tunnel navigation, transitions and the complete player update still
need connecting.

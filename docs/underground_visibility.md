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

## Moving-object ordering inside tunnels

Underground setup at BC85–BC96 patches the opcode at 2EDC from `01` (ADD) to
`29` (SUB). The moving-object placement at 2EBC therefore subtracts model extent
from its wrapping sorting key underground; surface objects add it. City tile
placement at 2A1A keeps its separate extent addition. The reconstruction had
incorrectly used the surface adjustment for both, allowing nearby floor strips
and sloping wall faces to overdraw the Wrecker.

`generate_object_sorting_reference.py` executes 2EBC with both opcode values
against all 33 special models in bank 32. The integration check compares all 66
keys, including the Wrecker, against actual C++ placement. This establishes the
missing mode-dependent bias; it does not establish pixel-perfect overlap for
every viewpoint. User examples were immediately after the first demolished gate
and left turn in level 17, and a later view from behind the roller where floor
markings and sloping wall panels appeared in front of it.

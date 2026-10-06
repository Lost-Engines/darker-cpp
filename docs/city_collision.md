# City collision geometry

`city_collision_boxes` decodes the native stream at `607A–60D2` from the
original bank's trailing world data. Model selection follows the existing
alternate/damage links. Directory marker `FF` disables collision for that type;
otherwise the marker supplies the base height, multiplied by 32.

The selected header's word at `+4` points into world data at original address
`8000`. Each six-byte primitive contains a five-bit category, an eleven-bit
height and four signed horizontal endpoint offsets. The type's fractional cell
anchor supplies the horizontal origin. Horizontal upper endpoints add one;
vertical upper endpoints do not. The optional expansion is applied to all six
bounds with word wrapping, as in the original routine.

An `F0–F7` prefix adds an eleven-bit lower-height offset to the following box.
It applies to that box only: the next record resets to the directory's base
height. `F8–FF` terminates the stream. The resulting coordinates are column,
row and height in collision units; height here is one eighth of the object's
vertical position word. These boxes are separate from rendered polygons.

The category is gameplay data, not a visual material or building type. In
particular, the player-building impact handler treats category 2 specially
when the model has a damage link, explaining the destructible streetlights.
Decoding does not itself mutate a building or decide a swept intersection.

Verification runs the executable's decoder over all 391 city types in banks
30–32, with four sampled states and two expansion values: 3,128 comparisons of
box counts, all bounds and categories. The native fixture skips intersection
tests after capturing their arguments; it does not replace stream decoding.
Synthetic tests additionally cover signed endpoints, height-prefix reset,
missing terminators, invalid pointers and non-colliding types.

## Swept primitive tests

`sweep_collision_box` tests a local movement segment against one decoded box,
then shortens its endpoint on a hit. Exact rational comparisons express the
original three projected intersection tests without floating point. Coordinates
are made relative to the starting word so local movements across signed and
unsigned word boundaries remain continuous.

Impact placement follows `662B–6704`, including the `extent + 1` divisors,
16-bit entry fraction, complement multiplication and zero-fraction special
case. An object already inside can therefore move back one unit on a positive
axis; replacing this with an ordinary interpolated ray hit would change the
original behaviour. Boxes include their lower bounds and exclude their upper
bounds during the initial rejection test.

The native fixture executes intersection and shortening instructions unchanged
for 4,096 cases (611 hits), including stationary, boundary, inside, reversed and
wrapping coordinates. Both hit decisions and all endpoint words agree. These
are local segments; ambiguous movements spanning half the wrapping coordinate
space are not a supported flight contract. Map-cell traversal and terrain clipping compose this primitive below;
gameplay responses remain separate consumers.

## Cell traversal

`swept_collision_cells` reproduces the ordered cell walk at `655E–662A`.
It retains the distinct equal-slope branch, the minor-axis crossing bias,
independent byte wrapping of row/column and rejection of coordinates outside
the 128×128 map. Testing only the start/end cells, or substituting a generic
line rasteriser, would miss the original neighbouring-cell visits.

A further 4,096 native traversals compare both the number and order of visited
cells, covering diagonals, axis-aligned movement, negative directions and map
boundaries. The fixture supplies empty cells so the entire walk is observed;
actual collision traversal stops at the first cell reporting a hit.

## Complete city sweep

`sweep_city` composes terrain clipping, the ordered cell walk, the model's
height rejection and its primitive stream. It stops at the first colliding
cell, preserving the final admitted primitive's category within that cell.
It returns the contact kind and cell alongside the shortened position, leaving
destruction/crash policy to the caller.

Terrain comparison uses half-height words; primitive comparisons use eighth-height
words. Consequently, the player threshold of 10 produces a quantised contact
height of 16, rather than 20. Horizontal terrain clipping retains signed
integer division. Misses leave the caller's endpoint unchanged.

The integration check compares 9,384 full native sweeps across all three banks,
all city types, base/damaged/alternate states, four heights, both horizontal
axes, reversed approaches and descending terrain contacts. It checks contact
kind, primitive category and all three final position words. Native selection,
cell traversal, broad-phase rejection, primitive tests and impact placement run
without stubbing; moving-object lists and gameplay responses are outside this
fixture's scope.

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

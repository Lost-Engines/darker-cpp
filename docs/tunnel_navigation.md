# Underground route geometry and object placement

`game/tunnel_network` reads original resource 00/78. D0BE expands its 200
10-byte records into a 3,200-byte runtime table: each cell type has a flag and
three edges with two perimeter endpoints, packed height levels, an integer
length and an orientation. The original packed offsets are retained because
junction selection also addresses overlapping/alternate portions of this table.

Perimeter coordinates describe a 32-unit square; positions scale those units
by eight within the 256-unit world cell. D31F interpolates each segment using
signed integer division, with the original special height values for levels
zero and one. D894 selects a traversal direction from the stored orientation
and the object's heading.

BEE7 uses two bits in the supplied heading as a route index, removes those bits
from the actual heading and places the object at the segment midpoint. Moving
underground scenario actors now use that placement, retain named tunnel state
and receive callback 8609. The model's height adjustment follows route placement.
Compact Wrecker/vehicle placements still use their separate route interpreter.

Evidence:

- 200 synthetic records compare every byte of D0BE's prepared table.
- 2,048 native D31F/D894 samples compare positions, heights and traversal flags.
- The original-pack integration check compares **all 158 shipped underground
  moving-object placements**, running native BEE7 with the actual route table
  and the appropriate original map. This replaces the earlier analysis harness
  that substituted the underground placement helper.

Route connections, underground steering and the player's D510 flight callback
remain separate work. Correct placement does not yet establish a playable
underground mission.

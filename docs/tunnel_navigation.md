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

The player’s D510 flight callback and campaign integration remain separate work. Correct actor movement does not yet establish a playable underground mission.

## Connections and lookahead

D136/D159 choose the adjoining segment by perimeter endpoint error, height
agreement and preferred heading. Equal scores retain the native traversal/tie
order. The special junction table uses a different offset calculation from
ordinary segments; its low two route bits and the additional junction selector
must be handled separately.

D284 projects a craft onto its current segment. Crossing behind that segment
updates its current cell/route; a steering target farther ahead can traverse
additional edges without changing the craft’s current route. Projection and
interpolation retain signed division. Flag 40h disables connected-route tracing.

Original-pack checks now compare 1,264 connection choices and 1,264 lookahead
samples with native execution. They cover both travel directions, varied heading
preferences, slightly displaced positions and targets spanning multiple cells.
The special junction addressing was caught by the lookahead comparisons: correct
connection flags alone did not establish that the subsequent segment was read
from the correct offset.

## Aircraft steering and motion

`game/tunnel_navigation` reproduces 860D–874D junction preferences, target-cell
selection, wandering and door-route changes. The original-pack check compares
2,528 native choices, including the retained route, progress and oscillation state.

The rest of callback 8609 combines awareness, route lookahead, direct heading and
pitch steering, nearby-player height adjustment, aircraft avoidance and fixed-point
movement. The avoidance callback can raise the *other aircraft’s* awareness, so
list order is retained. This underground callback does not itself fire weapons.

A further 1,536 native actor updates follow the three aircraft in the first tunnel
for 512 frames, change destinations, and bring the player close to exercise vertical
avoidance. Every position, angle, fractional coordinate, speed, angular rate,
awareness/cooldown and route-state field matches. Only script execution is stubbed
in that fixture; navigation, awareness, neighbour traversal and motion execute the
original instructions. The surrounding mission script/collision pipeline still
needs integration.

## Player route reacquisition

D39C searches the current cell, then one horizontal and one vertical neighbour
chosen by the player's half-cell position. D474 scores eligible segments using
height difference and separately truncated fixed-point projections. The search
retains the original asymmetric negative-distance rounding, strict acceptance
threshold and rejection of matching route indices. It is not a generic nearest
line search.

`reacquire` matches 5,214 native cases around the shipped moving-object starts,
with displaced positions, three heights and both retained and unrelated current
cells. The native fixture initialises the extended sine table as the game does;
raw executable bytes beyond the base table are not valid runtime sine values.
This helper supplies the player's off-route recovery path; D510 integration is
still required.

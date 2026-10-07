# Caero weapon firing and targeting

`fire_caero_weapon` composes native C9C2 with the CB01 projectile allocator for
Pinner Direct, Pinner Mimic, Brent Ground and Brent Hunter. Surface cost is the
definition's first role byte times 256, plus 255: Direct costs 6,399 units and
Mimic 7,423. Underground mode overrides that cost with **80FFh (33,023)**.
Lifetime is the next definition byte times 256. Boost and engine buffer are
separate from this weapon reserve.

The guards reject protected/crashing players, exhausted projectile pools and
insufficient reserve. Brent Hunter requires a negative object token other than
FFFF; Brent Ground requires a map-cell token. A valid weapon remains ready
without a trigger edge. Holding the button does not generate repeated launches.
Primary and secondary readiness now drive their original cockpit colours.

## Acquisition and retention

1E77/6D08 constructs a fixed-point forward ray from player heading and pitch,
clips it against terrain/city geometry, then checks the airborne list in native
order. CF94 retains an existing target rather than tracing again every frame.
It resolves the target bounds, checks wrapped world distance, transforms with
the current view basis and applies the original 55-unit rectangular and 2,916
squared-radius limits. Negative projected coordinates use one's-complement
magnitude, preserving the original asymmetry.

Object-only and building-only weapons reject the other target class. Caero
building locks additionally reject energy towers and require state bit 40.
The target marker follows the projected coordinates, switches size at native
radial bucket two and uses secondary readiness colours. Camera rendering and
lock projection share the same fixed-point basis construction. Caps Lock clears
the selection for reacquisition.

Hunter projectiles resolve their retained object token on every update. On
removal, 7A52's reference repair makes dependent missiles self-targeting and
clears the selected lock before a reusable aircraft record can launch again.
The existing CC61 guidance then continues from the missile's own attitude.

## Controls and connected campaign

1 and 2 select the unlocked Pinners; Space or left mouse fires the primary.
Mission 21 introduces Brent Hunter: **0 selects it; Alt or right mouse fires**.
6 selects Brent Ground when unlocked; its firing and map-guidance primitives
are connected, but its later mission introduction remains beyond the current
campaign gate. Remaining weapon handlers are still outstanding.

## Verification

- 1,640 native firing cases cover energy boundaries, trigger edges, target kinds,
  pool exhaustion, protected states and the underground cost override.
- 1,024 native CF4A cases compare retained/rejected locks and partial coordinate
  writes; 256 native ray cases compare fixed-point endpoints.
- 256 complete native 6D08 cases compare acquisition against original Delphi
  building geometry and two aircraft, including occlusion and list order.
- Original-pack mission 21 completes eight counted removals using Hunters,
  its messages and docking, with controlled player aim/position and charging.
  A separate retirement check verifies missile-reference repair before reuse.

These comparisons do not establish an uncontrolled retail-equivalent playthrough
or exact camera/input ordering in every exterior view. The live synchronised
DOSBox comparison tool remains deferred.

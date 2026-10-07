# Caero weapon firing and targeting

`fire_caero_weapon` composes native C9C2 with the CB01 projectile allocator for
Pinner Direct, Pinner Mimic, Dual Launch, Brent Ground, Chargeable and Brent Hunter. Surface cost is the
definition's first role byte times 256, plus 255: Direct costs 6,399 units and
Mimic 7,423. Underground mode overrides that cost with **80FFh (33,023)**.
Lifetime is the next definition byte times 256. Boost and engine buffer are
separate from this weapon reserve.

The guards reject protected/crashing players, exhausted projectile pools and
insufficient reserve. Brent Hunter requires a negative object token other than
FFFF; Brent Ground requires a map-cell token. A valid weapon remains ready
without a trigger edge. Ordinary trigger-edge weapons do not repeat while held; the Dual Launch follow-up has the separate mask condition described below.
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

## Dual Launch and a native input-mask anomaly

Mission 57 unlocks selection 3 (number key 3) and its automatic selection 7.
CA4A launches the primary capsule and changes primary selection to 7. CA99
requires the **oldest active player projectile** to have definition 1956h;
an absent or different oldest projectile returns selection to 3. A successful
follow-up substitutes that capsule as its target and returns selection to 3.

There is an important qualification to that description. The complete native
C8E9 dispatcher supplies DX from the configured primary action mask at C97A,
whose unpacked default is 4016h. CA99 tests **DX & 0080h**, rather than the
pressed-input mask. Consequently the default does not launch the follow-up,
regardless of pressed/released input, even with enough reserve and a valid
capsule. An action mask containing 0080h instead admits it without requiring a
new trigger edge. The reconstruction preserves this condition. Internal firing
requests retain the mask as an explicit argument; this does not add a control
option to the application.

This is evidence from executing the native dispatcher, not a confirmed retail
playthrough diagnosis. Command-line action-mask handlers can alter that value;
no later runtime patch has been established. Earlier isolated CA99 probes used
DX=0080h and therefore demonstrated the eligible path without discovering the
default-mask problem. Do not describe this as an ordinary second-click weapon
until the complete retail input path has been checked interactively.

When admitted, CBE7 homes on the capsule, with separation-dependent speed and
shared sound pitch. At a separation metric below 20 it makes three blast passes:
aircraft use a larger horizontal window and squared-distance threshold than
ground and stationary records. CD13 supplies distance-dependent strength to
the ordinary object-impact handler. Both projectiles enter the native 256-tick
retirement path, and recipe 71E8h appears at the capsule. A removed target causes
self-guided straight flight through the existing target-release path.

Native comparison covers 1,024 full-dispatch firing cases (including default
and alternative masks), 1,024 separation cases, 2,048 category/strength cases
and 2,048 complete movement updates. Separation preserves the unsigned
vertical ordering and horizontal one's-complement quirk. A controlled scene
also exercises the three blast categories, retained distant object, counted
removals, effects and both projectile deadlines. It constructs the pair directly
to test the admitted path independently of the default-mask anomaly.

## Forbes Diffuser

Key 5 selects the gas capsule (selection 5, definition slot 4). A successful
secondary shot switches to its trigger (selection 4, slot 3), and the trigger
switches back to gas. A pressed attempt with insufficient energy or a negative
target token resets to gas; early player/pool gates preserve the selection.
There is no separate key 4 binding.

CE84 accepts only collision category 3 with cell state bit 40h. Gas records one
shared target cell and a deadline 2800h ticks ahead, and emits recipe 75D4h at
the building origin. A trigger must hit that cell when the wrapped difference
`clock - deadline` is at least F000h: 6,144–10,239 ticks after gas impact, about
12.29–20.48 seconds. Success uses ordinary building destruction; a rejected
building hit emits 75A3h at the projectile. A later gas hit replaces the shared
target and timer. Object and terrain impacts retain their ordinary paths.

The successful trigger also expires the fixed sound record at 37F6h by writing
the current clock. The state retains that deadline; the general fixed ambient
sound-record scheduler is not yet connected to playback.

There are 768 full native firing comparisons and 1,024 native impact
comparisons. A controlled mission-80 test uses real projectile flight and
collision geometry to destroy three skylights with timed pairs, then the
storage tank with Brent Ground, and verifies the return message and docking.
It does not replace an interactive retail comparison of the gas appearance.

# Actor movement and proximity response

`actor_motion` translates the shared 8351 attitude controller and 8D75 speed controller. These are separate from player controls: actors approach pitch and bank targets with definition-specific response parameters, derive heading changes from midpoint bank, and accelerate or decelerate towards a byte speed request. The original wrapping arithmetic, fractional positions and effective frame-step changes are retained.

The 8597 displacement calculation is now shared by actors and direct projectiles through `advance_speed_motion`. This retains the original midpoint speed and sine-table projection rather than introducing a second movement approximation.

`actor_awareness` translates 8AA8–8AFC, the proximity response following an actor's script update. Distance controls a squared falloff scaled by the placement's strength byte. Rise and decay use separate rates; decay can undershoot the target. Weapon cooldown decreases independently with the definition's shift count. This response is an input to target selection, not a complete enemy controller.

The original horizontal distance helper is shared with hangar departure. Its negative X difference uses one's complement, while negative Y uses ordinary negation; replacing it with conventional Manhattan distance would change boundary results.

Native comparison fixtures cover 2,048 proximity/cooldown cases, 1,024 steering cases and 1,024 speed/displacement cases. They execute the corresponding original routines with varied state, including wrapping angles, signed speeds, fractional positions and large frame steps.

```sh
PYTHONPATH=/tmp/darker-python python3 tools/generate_awareness_reference.py ..
PYTHONPATH=/tmp/darker-python python3 tools/generate_actor_motion_reference.py ..
```

These components are not yet a complete actor simulation. Scenario activation, the navigation components below, firing and collision responses must be connected before enemy behaviour is playable. The proximity probe begins after the script call; mission execution has its own independent native comparisons.

## Scenario construction

`make_scenario_actor` translates the BE07/BF1A/C7BF construction paths for surface moving objects, compact special objects and absolute static objects. It expands the original definition, subtracts model height from nominal placement height, preserves objective/attribute bits, initial speed, behaviour bytes and cached cell/position, and resolves stored script or object-target references. Program pointers become offsets into the shared scenario program; the executable-resident stop script becomes explicit stopped state.

384 native cases exercise all three placement forms, varied definitions and model heights, both surface modes, static underground placement and both program/target operands. Moving underground actors deliberately require their separate BEE7 route initialiser. This constructor does not activate category lists or execute the seven embedded scenario setup blocks, and creating reserve objects does not imply activating them.

```sh
PYTHONPATH=/tmp/darker-python python3 tools/generate_scenario_actor_reference.py ..
```

## Manoeuvre decisions

`choose_actor_manoeuvre` translates 89A3–8A51 after target direction and obstacle clearance have been resolved. It preserves the close-target speed changes, heading-error thresholds, nominal-height recovery and requests to check firing eligibility. A firing request is not a shot: the later helper still checks target flags, aim, cooldown and projectile availability.

2,048 native decision cases include 364 calls to the firing helper. That helper is intercepted, and the probe captures the inputs at 8351 before steering executes; steering and displacement have separate comparisons above.

One surprising detail: the multiply at 8A43 appears to scale heading error with the engagement parameter, but its result is discarded. The subsequent 7BF4 call clamps **pitch** to ±1800h, overwrites DX, and leaves DI holding the original unscaled heading error. The reconstruction follows those resulting registers rather than applying the apparently intended turn scaling.

```sh
PYTHONPATH=/tmp/darker-python python3 tools/generate_actor_navigation_reference.py ..
```

## Targeting and clearance

`select_actor_target` preserves the original sticky player-pursuit rule: nonzero awareness retains an existing player target; only reaching zero restores the scripted target. New pursuit begins when awareness's high byte exceeds placement byte 51.

Object courses use the original direction table and horizontal-distance approximation. Within 511 horizontal units the aim height shifts 512 units above or below the target. The caller must resolve expired object targets to the player before requesting that course, as 88AF does.

City courses aim at the base type descriptor's position and geometry height, independently of damage variants. Negative collision markers use nominal altitude and cell centre instead. An out-of-map column also uses the nominal-height path, but retains the column byte in the low altitude byte. Slot 22 applies its additional speed-dependent height offset.

Clearance follows the original order:

1. Reset retained clearance when sufficiently far from its reference cell.
2. Scan nearby actors in active-list order. The wrapping square has asymmetric endpoints, and equal-height priority follows stable object order. Nearby lower actors can raise the retained floor and change the climb byte; positive neighbour pitch and speed also participate.
3. Scan the eight surrounding city cells, excluding the current cell. This path **does** follow alternate/damage model links, takes a signed maximum and starts at 1900 native height units.
4. Apply the city maximum to the climb byte and combine it with the retained actor floor to set recovery pitch.
5. Choose the manoeuvre, perform any eligible firing check, then steer and integrate speed.

512 native target-selection cases, 512 object courses, 512 city courses, 1,024 clearance adjustments, 1,024 neighbour scans and 256 city scans check these contracts. The neighbour probe runs original 6DB5 through 8C50; city probes run the original eight-cell scan and model-link traversal, with synthetic linked models under both damage masks. Clearance-adjustment probes substitute the supplied scan maximum, keeping their scope separate from those scan tests.

```sh
PYTHONPATH=/tmp/darker-python python3 tools/generate_actor_target_reference.py ..
```

The slot-23 close-target action, firing helper, actor update/lifecycle ordering and complete scenario/world integration remain outstanding. These comparisons establish individual routine contracts, not an end-to-end native enemy simulation.

## Combined surface flight

`advance_surface_actor` now composes awareness, target selection, object/city
courses, neighbour clearance, manoeuvre selection, steering and displacement in
native callback order. It returns a firing-check request; weapon allocation and
hit processing remain separate. Slot 23's special close-target action is rejected
until that callback is connected.

The application constructs the first mission's two slot-19 aircraft from
`04_000 / 0`, preserving source identities and reverse active-list order. They
fly in Delphi, appear as models and radar contacts, and reset with the player.
This does not yet enable combat or claim a completable mission. Actor lighting,
collision/death responses and weapon firing remain outstanding.

An integration comparison executes original `8823` on the original Delphi map
for **2,048 sequential actor updates**, checking 17 fields each time. Its
controlled player position starts stationary, then follows closely enough to
exercise pursuit. Script calls, firing and HUD threat selection are intercepted;
all movement, awareness and obstacle scans execute natively. Construction has
its separate native fixture. The comparison retains both actors' changing state
and active-list order, so it checks composition as well as individual routines.

```sh
PYTHONPATH=/tmp/darker-python python3 tools/generate_actor_flight_reference.py ..
```

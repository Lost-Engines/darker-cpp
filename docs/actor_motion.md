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

These components are not yet a complete actor simulation. Scenario activation, target selection, obstacle avoidance, firing decisions and collision responses must be connected before enemy behaviour is playable. The proximity probe begins after the script call; mission execution has its own independent native comparisons.

## Scenario construction

`make_scenario_actor` translates the BE07/BF1A/C7BF construction paths for surface moving objects, compact special objects and absolute static objects. It expands the original definition, subtracts model height from nominal placement height, preserves objective/attribute bits, initial speed, behaviour bytes and cached cell/position, and resolves stored script or object-target references. Program pointers become offsets into the shared scenario program; the executable-resident stop script becomes explicit stopped state.

384 native cases exercise all three placement forms, varied definitions and model heights, both surface modes, static underground placement and both program/target operands. Moving underground actors deliberately require their separate BEE7 route initialiser. This constructor does not activate category lists or execute the seven embedded scenario setup blocks, and creating reserve objects does not imply activating them.

```sh
PYTHONPATH=/tmp/darker-python python3 tools/generate_scenario_actor_reference.py ..
```

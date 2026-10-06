# First mission: combat and return

The default Caero session now runs `04_000 / 0` from its four original English
text pages through combat and HQ docking. Space or Enter advances the pages.
Wait for boost charge, press Enter to launch, select Pinner Direct with **1**,
and fire with **Space or left mouse**. Each press produces a trigger edge; holding
the button does not continuously fire. Weapon energy recharges through the
existing Caero energy system.

The two counted slot-19 aircraft pursue and fire at the player. Destroying both
allows the original player script to deliver “Well done- you can return to
base.” An aircraft remains counted while falling or awaiting removal, so the
message does not appear at the instant of a fatal hit. Return to the original HQ
at cell **(49,113)** from the north, flying south, low and aligned with the
entrance. The original capture checks hand control to automatic docking. The
current completion overlay stops the simulation; Enter restarts this mission.

## Reconstructed behaviour

- Pinner selection drives the cockpit icon. C9C2/CAC0 guards, energy spending,
  CB01 allocation, direct movement, city collision and model-extent hull sweeps
  determine hits. The projectile pool retains hit records for their original
  removal delay instead of recycling them immediately.
- Aircraft impacts use CE38's damage and attitude kicks. Awareness and impact
  accumulation share field 56; damage and recovery share field 64. Fatal hits
  switch to 8DAA's falling motion, then expiry removes the counted objective.
- The first mission's slot-19 close-range gun uses a randomised ray, not a
  conventional moving projectile. 8B65 checks aim and clock bits, traces against
  the player and applies original damage strength `15h` with kick amplitude 3.
  The aim helper consumes DX's speed/distance bytes even though its caller puts
  apparent intended bounds `180Ch` in BX. The reconstruction preserves the
  executed register behaviour.
- The existing mission scheduler handles the objective wait, message delay,
  counted message and stop. Formatted briefing pages use the compact original
  font, matching the original presentation's font selection at 3CCA–3CD2.
- C670, 7CEF and 7D32 implement return capture, approach and settling. See
  [hangar return](hangar_launch.md#mission-return) for the native trajectory.

## Evidence

`generate_aircraft_combat_reference.py` produces 512 native hull sweeps, 512
falling updates and 512 gun cases. The gun fixture admits 73 shots with 17 hits;
it compares eligibility, hit results and random state while intercepting damage
and effect spawning. Damage accounting has separate native comparisons.

The resource integration check uses the original map, models and first-mission
placements. It positions and recharges the player deliberately to isolate combat
from navigation, fires real projectiles to remove both objectives, checks the
original message and stopped script, then places the player at the HQ approach
and completes docking. This is **controlled integration**, not an autonomous
flown playthrough or a recording of retail play. A separate native trace compares
all 684 docking frames. The existing 2,048-update actor flight comparison remains
enabled, as do the launch and individual weapon/damage checks.

Window checks advance the briefing, charge and launch, select the weapon, fire
and close cleanly. Screenshots confirm that the briefing text fits and the Pinner
icon appears. The user has since completed the mission interactively and confirmed seamless
landing. Combat effects are now connected; see [particle effects](particle_effects.md)
for their separate native comparisons and remaining limits.

```sh
PYTHONPATH=/tmp/darker-python python3 tools/generate_aircraft_combat_reference.py ..
PYTHONPATH=/tmp/darker-python python3 tools/generate_hangar_return_reference.py ..
ctest --test-dir build --output-on-failure
./build/resource_check --data-dir ../darker
```

## Remaining scope

This is a first-mission gameplay milestone, not finished retail presentation.
Briefing artwork and transitions, music, gun flashes and combat sound effects,
Nayas activity, aircraft lighting, aircraft/player ramming, original death and
debrief screens, and loading the next mission remain outstanding. Player city
collision still occurs inside its flight update; the complete original global
collision dispatch order has not yet been reconstructed. Other scenario actor
callbacks, weapons and campaign setup blocks must not be inferred to work from
this first-mission path. Skimma sessions remain free-flight checkpoints.

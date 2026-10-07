# Ground vehicle routes

`game/vehicle_route` reconstructs the 8F3B movement callback: waits, straight
movement, curved turns, raised traversal, relative branches, door damage,
stationary cutting effects, timed visibility flags and removal scheduling.
It reads original shared scenario bytes. A command’s upper nibble repeats its
action in 4,096-tick units; FF branches relative to the displacement byte.
Clock arithmetic wraps at 16 bits; at most one route unit advances per call.

Movement preserves whole-cell updates, quarter-circle byte interpolation,
four-bit fractional coordinates, model-height offsets and triangular pitch
profiles. Ground route state is separate from aircraft script state, despite
the native object record sharing these fields. Ground movement bypasses
aircraft steering and aircraft city sweeps.

Commands 5 and 6 damage the current cell once per route unit. The shared 67BF
path adds 32 to its state, updates the native world-damage counter and emits
its type’s effect. Commands 6 and 7 use the original random schedule for sparks,
sounds and recipe 77C5 bursts. Command 9 establishes a 4,096-tick removal
deadline and changes visibility near the end of its movement phase.
Command 8 schedules removal after 256 ticks via 6EC7; it does not immediately
erase the vehicle. Existing destruction deadlines are retained.

`tools/generate_vehicle_route_reference.py` executes the original callback,
suppressing vehicle combat and intercepting external damage/effect calls.
Sixty traces cover the mission-four flatbed, synthetic turn and height routes,
the actual first-tunnel Wrecker and timed removal, in four directions at three
callback intervals. Fingerprints cover movement, flags, route fields, random
state, deadlines, damage cells and effect arguments. External effects are
checked separately by their own subsystem tests.

The original-pack integration check follows the actual fourth-mission flatbed
through its combat runtime. At 50-tick sampling the removal request occurs at
155,650 and removal at 155,950 (strictly after the 256-tick deadline). It neither
alters the city nor counts as a destroyed objective. Shooting it also retains
its route until its destruction deadline.

An isolated first-tunnel check follows the Wrecker’s original route through map
70. Doors at (61,56), (65,49) and (62,41) each change from state 0 to 32 to 64.
At 512-tick sampling those six changes occur at 57,344, 73,728, 155,648, 172,032,
253,952 and 270,336. It also requires both spark and burst output.

Vehicle firing and the connected underground campaign remain outstanding.
These route checks do not establish a playable tunnel mission or a complete
retail-equivalent campaign playthrough.

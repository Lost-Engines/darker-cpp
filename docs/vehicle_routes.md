# Ground vehicle routes

`game/vehicle_route` reconstructs the basic 8F3B ground callback: waits, straight
movement, both curved turns, relative branches and removal. It reads the original
shared scenario bytes. A command's upper nibble repeats its action in 4,096-tick
units; FF branches relative to the displacement byte. Clock arithmetic wraps at
16 bits, and the callback advances at most one route unit per invocation.

Movement preserves the original whole-cell updates, quarter-circle lookup and
byte interpolation, four-bit fractional coordinates, and model-height offset.
Ground route state is named separately from aircraft mission-script state even
though the original reused fields within its object records. Ground movement
bypasses aircraft steering and aircraft city sweeps.

`tools/generate_vehicle_route_reference.py` executes the original callback with
its combat helper suppressed. Twenty-four traces cover the fourth mission's
flatbed route and a repeating synthetic route, four initial headings and three
callback intervals. **35,444 callbacks** match every retained position, fraction,
angle, speed, flag, route cursor, command, clock origin and removal result.
These checks cover movement rather than vehicle combat.

The original-pack integration check also constructs the actual fourth-mission
flatbed from its scenario record and model, executes its route through the combat
runtime and checks removal at the native time: tick 155,650 with 50-tick sampling
(the nominal command boundary is 155,648). It does not alter the city or count as
a destroyed objective. This is an isolated route check, not a fourth-mission
playthrough.

Raised traversal, map damage, stationary effects, timed visibility flags and
vehicle firing are not connected yet. Unsupported route actions fail explicitly.
Mission four remains gated pending its reinforcement activation and placement
callbacks; later missions must not be considered supported merely because their
route bytes can be decoded.

# Pinner Mimic and missile cameras

Number-row 2 now selects the Pinner Mimic after its original mission-five briefing unlocks it. Both Pinners use C9C2/CAC0 firing guards, their own energy cost and lifetime, and the existing projectile pool and collision path. The Mimic costs 7,423 reserve units and has a nominal 1,536-tick lifetime; Direct costs 6,399 and lasts 1,024 ticks. Holding fire does not repeatedly launch either weapon: the original trigger edge is retained.

CBCE copies the player's pitch and bank each update. The roll difference determines a folded midpoint bank, which 8375 turns into heading motion. Its response is the remaining lifetime shifted left four bits, with native word truncation. Thus the Mimic follows player attitude continuously, and loses turning authority as its lifetime runs down. It does not seek an enemy automatically. Shared speed integration then advances the projectile with its original fractional coordinates.

M toggles the original missile camera request. A shot launched while this is enabled becomes the watched projectile (CACD–CAE7). F1/F2 follow it, F3 uses a level following view, and F4 looks from its nose. F5/F6 retain the shared dropped-camera controls. The view stays with the impact record until that record is removed, then returns to the player; no pointer survives recycling. The live missile and impact effect have different following-distance tables. F4 follows the effect after impact rather than remaining at its centre.

The first seven missions are now enabled. The fifth introduces Mimic, the sixth tests it in another reinforcement mission, and the seventh uses two timed reinforcement waves. Its coarse waits are 14h and 28h units of 2,048 ticks, separate from the short scaled-delay opcode. Mission eight remains gated because its scripted actor navigation, additional actor roles and subsequent combat interactions need integration.

## Verification

- 2,048 complete native CBCE calls match pitch, bank, heading, speed, position, fractions and response, including wrapped angles and arithmetic boundaries.
- 360 native C9C2/CAC0 cases cover both definitions, energy boundaries, trigger state, blocked player states and pool availability.
- 1,024 native 2409 camera cases cover all four attached missile modes, live/impact states, distance settings and smoothing, fractional coordinates and ground adjustment. Existing player-camera comparisons remain unchanged.
- Controlled original-pack combat completes missions one through seven, with Mimic used for five through seven. Mission seven removes eight counted aircraft across all its waves. Every mission reaches its final return message and docks. This controls player aim/position and charging; it is not a retail playthrough comparison.
- A runtime launch/expiry check ensures the watched projectile is registered and cleared before recycling.

F7 object selection, complete camera-mode transition timing and explosion-camera behaviour remain separate work. Existing camera limitations concerning exterior lighting and far actor dots still apply.

A real-window check completes Level X progression through all seven briefings and flight entries, selects Mimic in mission five, fires in M mode, captures following/nose views and expiry, and verifies the saved stage-eight gate and weapon mask 3. The screenshots were inspected. This check exposed the mission-six animation reset lifetime bug, now covered by interval-by-interval presentation drawing.

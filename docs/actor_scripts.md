# Scripted aircraft destinations

Airborne actors now execute their retained mission script before awareness, target selection, manoeuvring and motion, matching the 8823 → 8AA1 → BF97 call chain. The same bounded interpreter serves player and actor scripts; target mutations are supplied by the owning object. The runtime supplies the full elapsed clock, the record's interval multiplier and the actor's current cell.

Target opcodes 00/01 read a packed cell, 02/03 resolve an object index to its native identity, 04/05 select the current cell, and 06 selects the player. All write the retained target and bit 02 of the object's flags, then wait ten record intervals. This is an implicit delay even when the next instruction is a stop. The 896 native BF97 cases cover all seven operations, unrelated flag preservation, byte coordinates, object identities, interval variation and deadline wrap. Their token, flags, continuation and deadline all match.

Mission eight now runs its initial and reserve aircraft scripts, including the Stalker's alternating two-cell patrol, alongside its seven existing flatbed routes. Its player script admits one reserve aircraft and later four more. The controlled original-pack combat check removes all eight counted aircraft across these waves, consumes the final return message and completes docking. Later archives and mission behaviours remain gated; this does not claim all interpreter operations or all enemy roles are connected.

Invisible actors with a live flight callback still update; only their rendering is suppressed. Effect-only records continue to wait for removal without running flight navigation. This preserves the distinction between hiding a model and stopping an actor.

Opcode 08 now executes its original distance retirement loop: while the wrapped
squared cell distance from the player is below 0510h it retries itself after
eight script intervals. At or above that threshold it marks the actor 28h,
installs C002 and sets removal to the current clock. The current airborne call
still finishes; the changed callback applies to later updates. 512 native
BF97/C30A fixtures cover boundaries, byte wrapping, flags, timers and script
continuation. This is distinct from the explicit reserve queues.

# Mission reserves and reinforcement placement

Mission four now admits its two pairs of reinforcement Skimmas through player-script opcode 09. The initial aircraft, both subsequent waves, the original message sequence and the flatbed route run through the same runtime as missions one to three. Successful docking saves and advances to stage five, whose gameplay remains gated.

Scenario category follows BE44–BE7F: static placement header bit 6, otherwise ground for definition slots above 28, air for the remaining slots. Initial traversal and reserve selection preserve reverse source order within each category. C33E admission repeatedly pops the reserve head and prepends it to the corresponding active category, reversing each admitted batch. Objective counts include newly admitted objects immediately, before the script continues. Ground route origins receive the original clock minus 4,096; other script deadlines receive the current clock. A malformed request exceeding the reserve list is rejected before mutation; the original has no exhaustion guard.

Normal surface-air placement follows C39E, including its signed byte coordinate difference, 1,296-cell-squared player exclusion radius, selected-axis displacement of 36 cells, and the C313 scan of existing air objects in a wrapping 510-unit square. The height adjustment preserves the original signed word comparison, byte overflow and accumulation. Static and ground admissions have no placement adjustment. The separate configuration-4 and underground air callbacks are not connected; this path currently serves the supported Caero campaign only.

The mission script executes after actor movement, so new aircraft start moving on the next frame. Landing retains C670's direct C84E objective check: there is no additional final-wave or script-finished requirement in the original admission routine.

## Verification

`tools/generate_actor_activation_reference.py` executes 1,024 complete native C39E calls, including 8432, 6DB5 and C313, with three neighbours each. Every resulting coordinate matches the C++ implementation. Signed altitude boundaries, wrapping world coordinates, exclusion-axis selection and square-edge admission are covered. The category admission test also checks reserve order, head insertion and deadlines.

The original-pack integration check completes all four supported mission scripts through controlled real projectile combat, then docks. Mission four destroys five counted aircraft across its three waves, consumes its final return message, exhausts its air reserves and completes docking. This controls player position and energy to isolate combat and script behaviour; it is not a claim of a complete retail/native playthrough comparison. The independent flatbed route test remains in place.

A real-window check also enters mission four through Level X progression, advances its briefing, waits in the hangar, launches and advances to the saved stage-five gate. Its hangar screenshot was inspected.

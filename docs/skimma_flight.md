# Skimma flight callback

`advance_skimma_flight` reconstructs steady callback `8108–8207`, including its
shared tail at `8004`. Both Skimma variants use this update with their original
craft parameters. The upgraded craft's initial `7E49` transition remains a
separate lifecycle step, not an alternate steady flight equation.

The callback recharges shields first. Steering gain is the square of the
nonnegative speed deficit below 2047, shifted by seven. Bank response includes
a speed-dependent dead zone and additional centring; pitch response uses a
different scaling of the same gain. Altitude and low-speed assistance constrain
pitch before the shared attitude coupling feeds heading and lift.

Forward drive combines the selected setting with one eighth of measured speed.
Braking substitutes 130 for the setting. The original settings 248/500/640 are
parameters, not host velocity units. The vertical integrator receives twice the
horizontal timestep, and uses horizontal speed in its lift term. Speed is
measured before vertical integration, matching the Caero's ordering.

The craft definitions supply angular gains 352/384 and signed vertical biases
-106/-110. Shield enablement does not gate the recharge accumulator. Weapon,
speed-selection and shield-toggle key policy belongs to the input/event layer.

`flight_attitude` now owns the shared `8077` pitch projection and `802D` turn/lift
coupling used by both craft families. This keeps their common fixed-point rules
in one place while retaining distinct steering, assistance and energy systems.

The native fixture captures 32 sequences of 16 updates with both parameter
sets, all three speed settings, braking, full attitude ranges, varied heights,
shield accumulator boundaries and initial speeds on either side of 2047.
All sixteen persistent fields agree across 512 updates (8,192 assertions).
The existing full Caero traces also pass after extracting the shared helpers.
These are callback comparisons; scenario setup, collision responses and the
application flight loop remain separate integration work.

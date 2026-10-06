# Caero HQ launch

The default Caero start now uses the new-game return site **7162h**, cell **(49,113)**, instead of an invented airborne checkpoint. BD34–BD98 places the ship at `(12672,29080,0)` with pitch 0A20h, no speed or stored boost, and the landed/protected flag set. The original Caero model header contributes its −104 height adjustment. The engine starts enabled, matching retail play and the outer startup initialisation at 3D5C. The earlier engine-off default incorrectly overrode that state.

Boost cells charge automatically while waiting in the hangar, including with the engine switched off. Press **Enter** once to launch; no steering is required to clear the first wall. The regular flight callback takes over on the first boost. Waiting fills the starting boost cells; this is separate from airborne beacon charging.

C6D2 toggles the high state bit on the gate, neighbouring approach lights and hangar interior together. C5F9 grows the gate parameter while inside an entrance cell, derives it from distance while crossing the lights, and restores the three cells after departure. The parameter drives the existing model interpolation directly. The native negative-X distance asymmetry and the sound-level update skipped at extension saturation are preserved.

The startup path is currently restricted to type-17 return sites: all nine Delphi return locations explicitly assigned by the analysed mission scripts use that type. This is not an assertion that the similarly shaped entrances in other orientations are interchangeable start locations. Landing capture, docking callbacks, mission completion/return permission and Skimma supply-pad starts remain separate work.

**512 native startup cases** compare position, angles, landed flag and all three changed cells. **512 native departure cases** compare extension, sound level, landed flag and cell changes, including wrapping arithmetic and saturation. The GLFW application has also been exercised through charge, boost, pull-up and external flight view with simulated input.

```sh
PYTHONPATH=/tmp/darker-python python3 tools/generate_hangar_reference.py ..
```

The game still has no active mission actors or briefing flow. Enter after a crash restores the initial HQ state; this is a development retry, not yet the original campaign/death-screen path. Skimma starts remain airborne checkpoints.

## Retail charging correction

The user confirmed that hangar boost charging continues regardless of engine state. The reconstruction now follows that observation. Isolated native callback 7E7F tests bit 0 of 4552 before adding charge; the surrounding retail behaviour responsible for the discrepancy remains unresolved. Native comparisons retain all other outputs, but exclude the startup accumulator and boost reserve for engine-off, pre-launch samples. A separate regression checks equal charging with the engine enabled and repeatedly toggled, full pips and boost availability. Airborne charging still uses its original engine-state gate.

## Hands-off launch trace

A comparison following the user's report found another omitted outer-startup write: **3D50–3D52 sets energy buffer 7F8F to 6000h (24,576)**. This buffer contributes forward drive independently of the visible boost pips and weapon-charge reserve. Initialising only placement and boost cells left it at zero, causing the ship to lose speed and descend before reaching the wall. HQ initialisation now includes that buffer.

The new resource integration check starts at the actual Delphi HQ, waits 3,000 timer ticks, presses boost once, then supplies no further input. **All 27 recorded fields match for all 500 flight updates**, plus initial and charged state: whole and fractional coordinates, angles, velocities, energy/boost state, damage/repair, engine state, gate extension and lifecycle flags. This uses fixed eight-tick updates over approximately eight seconds, with the original map, geometry, beacon lookup and model variants.

[Coordinate/speed comparison CSV](evidence/hangar_launch_comparison.csv) and [comparison plot](evidence/hangar_launch_comparison.svg) retain the original, pre-fix C++ and corrected C++ trajectories. At tick 2,000 (about four seconds), original/corrected position is `(12672,28044,670)` at speed 298; the previous C++ position was `(12672,28348,315)` at speed 164. The previous version crashes at tick 2,808, approximately 5.6 seconds. Original and corrected traces remain airborne through tick 4,000.

```sh
PYTHONPATH=/tmp/darker-python python3 tools/generate_hangar_flight_reference.py .. --csv /tmp/darker-native-launch.csv
./build/resource_check --data-dir ../darker --launch-trace /tmp/darker-cpp-launch.csv
```

This is controlled execution of the original startup (including 3D1A–3D60), boost, flight, collision and gate routines, **not a recorded DOSBox session**. Rendering, sound/effect spawning, mission actors and outer-loop input processing are omitted. Steering is explicitly zero, the engine is enabled, and the harness schedules the gate callback only while the landed flag remains set. It does not resolve the separate engine-off charging discrepancy above.

### Display pacing

The windowed test exposed a second issue: Xvfb/Mesa did not provide useful swap pacing, allowing approximately 100 physics updates per second. The original routines also crash with fixed five-tick updates (at tick 520 in the controlled probe); their ground-contact quantisation makes this launch sensitive to update intervals. Changing the translated physics to conceal that would lose the original behaviour.

The original AF61 waits for VGA vertical retrace. F15A's CRTC table programs total register 06 to 0Dh with overflow register 07 set to 3Eh: 525 plus two scan lines, or 527. With mode-13h's 800-dot line and 25.175 MHz clock this is approximately 59.71 Hz. The host now enforces that minimum frame interval even on faster displays or without functioning swap synchronisation. It retains elapsed-time physics steps and the original pending-time cap; this is not a complete emulation of the original page-flip/display scheduler.

A windowed Xvfb/Mesa test now waits for charge, presses and releases Enter once, and remains airborne for eight seconds with no mouse or steering input. No physics coefficients were changed.

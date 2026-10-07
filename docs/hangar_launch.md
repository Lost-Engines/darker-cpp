# Caero HQ launch

The default Caero start now uses the new-game return site **7162h**, cell **(49,113)**, instead of an invented airborne checkpoint. BD34–BD98 places the ship at `(12672,29080,0)` with pitch 0A20h, no speed or stored boost, and the landed/protected flag set. The original Caero model header contributes its −104 height adjustment. The engine starts enabled, matching retail play and the outer startup initialisation at 3D5C. The earlier engine-off default incorrectly overrode that state.

Boost cells charge while waiting in the hangar with the engine enabled. Switching it off pauses charging; switching it back on resumes from the stored charge. Press **Enter** once to launch; no steering is required to clear the first wall. The regular flight callback takes over on the first boost. Waiting fills the starting boost cells; this is separate from airborne beacon charging.

C6D2 toggles the high state bit on the gate, neighbouring approach lights and hangar interior together. C5F9 grows the gate parameter while inside an entrance cell, derives it from distance while crossing the lights, and restores the three cells after departure. The parameter drives the existing model interpolation directly. The native negative-X distance asymmetry and the sound-level update skipped at extension saturation are preserved.

The startup path is currently restricted to type-17 return sites: all nine Delphi return locations explicitly assigned by the analysed mission scripts use that type. This is not an assertion that the similarly shaped entrances in other orientations are interchangeable start locations. Landing capture and docking now use that same type-17 site after the first mission’s objectives clear. Skimma supply-pad starts remain separate work.

**512 native startup cases** compare position, angles, landed flag and all three changed cells. **512 native departure cases** compare extension, sound level, landed flag and cell changes, including wrapping arithmetic and saturation. The GLFW application has also been exercised through charge, boost, pull-up and external flight view with simulated input.

```sh
PYTHONPATH=/tmp/darker-python python3 tools/generate_hangar_reference.py ..
```

The first mission now has briefing text, active aircraft and combat. Enter after a crash restores the initial HQ state; this is a development retry, not yet the original campaign/death-screen path. Skimma starts remain airborne checkpoints.

## Retail and demo charging

Retail callback 7E7F tests bit 0 of 4552 before adding charge. The engine starts enabled, so charging begins immediately; switching it off pauses charging. This now agrees with the user's corrected retail observation. All native startup output comparisons are enabled, including engine-off accumulator and boost reserve values. A separate regression checks pause/resume without losing accumulated charge.

The user reports that the demos instead start with the engine disabled and charge in the hangar regardless of engine state. The earlier contradictory observation described those demos, not retail; it is no longer an unresolved retail discrepancy. The reconstruction follows retail behaviour.

## Hands-off launch trace

A comparison following the user's report found another omitted outer-startup write: **3D50–3D52 sets energy buffer 7F8F to 6000h (24,576)**. This buffer contributes forward drive independently of the visible boost pips and weapon-charge reserve. Initialising only placement and boost cells left it at zero, causing the ship to lose speed and descend before reaching the wall. HQ initialisation now includes that buffer.

The new resource integration check starts at the actual Delphi HQ, waits 3,000 timer ticks, presses boost once, then supplies no further input. **All 27 recorded fields match for all 500 flight updates**, plus initial and charged state: whole and fractional coordinates, angles, velocities, energy/boost state, damage/repair, engine state, gate extension and lifecycle flags. This uses fixed eight-tick updates over approximately eight seconds, with the original map, geometry, beacon lookup and model variants.

[Coordinate/speed comparison CSV](evidence/hangar_launch_comparison.csv) and [comparison plot](evidence/hangar_launch_comparison.svg) retain the original, pre-fix C++ and corrected C++ trajectories. At tick 2,000 (about four seconds), original/corrected position is `(12672,28044,670)` at speed 298; the previous C++ position was `(12672,28348,315)` at speed 164. The previous version crashes at tick 2,808, approximately 5.6 seconds. Original and corrected traces remain airborne through tick 4,000.

```sh
PYTHONPATH=/tmp/darker-python python3 tools/generate_hangar_flight_reference.py .. --csv /tmp/darker-native-launch.csv
./build/resource_check --data-dir ../darker --launch-trace /tmp/darker-cpp-launch.csv
```

This is controlled execution of the original startup (including 3D1A–3D60), boost, flight, collision and gate routines, **not a recorded DOSBox session**. Rendering, sound/effect spawning, mission actors and outer-loop input processing are omitted. Steering is explicitly zero, the engine is enabled, and the harness schedules the gate callback only while the landed flag remains set.

### Display pacing

The windowed test exposed a second issue: Xvfb/Mesa did not provide useful swap pacing, allowing approximately 100 physics updates per second. The original routines also crash with fixed five-tick updates (at tick 520 in the controlled probe); their ground-contact quantisation makes this launch sensitive to update intervals. Changing the translated physics to conceal that would lose the original behaviour.

The original AF61 waits for VGA vertical retrace. F15A's CRTC table programs total register 06 to 0Dh with overflow register 07 set to 3Eh: 525 plus two scan lines, or 527. With mode-13h's 800-dot line and 25.175 MHz clock this is approximately 59.71 Hz. The host now enforces that minimum frame interval even on faster displays or without functioning swap synchronisation. It retains elapsed-time physics steps and the original pending-time cap; this is not a complete emulation of the original page-flip/display scheduler.

A windowed Xvfb/Mesa test now waits for charge, presses and releases Enter once, and remains airborne for eight seconds with no mouse or steering input. No physics coefficients were changed.

## Mission return

After both counted aircraft are removed, C670’s position and attitude checks admit
an approach from the north, flying south towards the HQ. The capture window is
608–863 horizontal units from the entrance reference, below 2,304 altitude units,
with the original narrow roll, pitch and heading tolerances. Capture opens the
site and transfers control to the 7CEF approach and 7D32 settling callbacks.

**684 consecutive return frames** match the original execution on the real Delphi
map, from `(12672,28380,500)` facing south through docking at tick 5,472. Fifteen
fields per frame include fractional position, attitude, speed, gate extension
and return phase. The integration check also rejects return before objectives
clear. This is a controlled native routine trace, not a recorded DOSBox flight.

```sh
PYTHONPATH=/tmp/darker-python python3 tools/generate_hangar_return_reference.py ..
```

The current application stops at a simple completion overlay after docking;
original debrief presentation and loading the next mission are still pending.

## Scripted departure destination

Presentation opcode 29 supplies the C610 destination operand. When the ship
leaves its starting hangar, the old gate/interior/light states are closed before
that nonzero destination becomes the return site. Mission sixteen uses 3064h,
cell (50,48), to direct the player to Communications HQ for the tunnel mission.
The native departure fixtures include this handoff. Zero-destination automatic
hangar selection remains a limitation; see [campaign status](campaign_status.md).

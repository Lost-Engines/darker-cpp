# Caero HQ launch

The default Caero start now uses the new-game return site **7162h**, cell **(49,113)**, instead of an invented airborne checkpoint. BD34–BD98 places the ship at `(12672,29080,0)` with pitch 0A20h, no speed or stored boost, and the landed/protected flag set. The original Caero model header contributes its −104 height adjustment. The engine starts enabled, matching retail play and the outer startup initialisation at 3D5C. The earlier engine-off default incorrectly overrode that state.

Boost cells charge automatically while waiting in the hangar, including with the engine switched off. Press **Enter** to boost and pull up (Down arrow). The regular flight callback takes over on the first boost. Waiting fills the starting boost cells; this is separate from airborne beacon charging.

C6D2 toggles the high state bit on the gate, neighbouring approach lights and hangar interior together. C5F9 grows the gate parameter while inside an entrance cell, derives it from distance while crossing the lights, and restores the three cells after departure. The parameter drives the existing model interpolation directly. The native negative-X distance asymmetry and the sound-level update skipped at extension saturation are preserved.

The startup path is currently restricted to type-17 return sites: all nine Delphi return locations explicitly assigned by the analysed mission scripts use that type. This is not an assertion that the similarly shaped entrances in other orientations are interchangeable start locations. Landing capture, docking callbacks, mission completion/return permission and Skimma supply-pad starts remain separate work.

**512 native startup cases** compare position, angles, landed flag and all three changed cells. **512 native departure cases** compare extension, sound level, landed flag and cell changes, including wrapping arithmetic and saturation. The GLFW application has also been exercised through charge, boost, pull-up and external flight view with simulated input.

```sh
PYTHONPATH=/tmp/darker-python python3 tools/generate_hangar_reference.py ..
```

The game still has no active mission actors or briefing flow. Enter after a crash restores the initial HQ state; this is a development retry, not yet the original campaign/death-screen path. Skimma starts remain airborne checkpoints.

## Retail charging correction

The user confirmed that hangar boost charging continues regardless of engine state. The reconstruction now follows that observation. Isolated native callback 7E7F tests bit 0 of 4552 before adding charge; the surrounding retail behaviour responsible for the discrepancy remains unresolved. Native comparisons retain all other outputs, but exclude the startup accumulator and boost reserve for engine-off, pre-launch samples. A separate regression checks equal charging with the engine enabled and repeatedly toggled, full pips and boost availability. Airborne charging still uses its original engine-state gate.

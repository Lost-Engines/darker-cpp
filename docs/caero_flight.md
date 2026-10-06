# Caero flight callback

`game::advance_caero_flight` translates the initial callback `7E7F` and steady
callback `7EB6–802C`, including the coupling helpers through `80EE`. It composes
the previously verified angular response, movement, beacon charging, damage
repair and energy routines in their native order. This is a complete craft
update, not yet a complete game frame: collision handling, controls, camera
tracking and the object-list dispatcher remain separate integration work.

The named state owns position/fractions/attitude, velocity components, angular
response rates, the pitch-assistance accumulator, active boost drive, carried
forward target contribution, startup accumulator, damage-repair phase and energy
accounting. The impact field previously called `heading` is now `turn`: the
Caero uses this same `BP+28` channel for bank response. This avoids treating the
shared object field as a universal heading rate.

## Ordering and preserved details

- Startup accumulates seven times the frame step while the engine's low flag is
  set, caps the accumulator's high byte and exposes quantised boost reserve.
  An active boost transitions to steady flight, reproducing the callback swap.
- Bank response is integrated first. Mid-step bank feeds manual/assisted pitch,
  low-speed pitch assistance and bank-related heading change.
- Altitude assistance uses the control layer's height-reference term plus the
  configured desired height. Manual pitch is scaled by bank/pitch orientation.
- The frame step is transformed into the original energy/movement interval;
  buffer and active boost are spent before horizontal movement.
- Beacon strength is sampled at the new horizontal position and old height.
  Speed is measured before vertical integration. Braking changes the target
  contribution carried into the following frame.
- Charging then updates buffer, reserve and future boost pips. Lift and vertical
  demand combine the saved steering/projection terms with current beacon power,
  configured vertical bias and altitude. Attitude folding happens last.

`activate_caero_boost` reproduces `B963–B973`: require and spend one complete
32-unit pip in the high reserve byte, then set the active boost's high byte to
56 while preserving its low byte. Sound/UI effects remain the event caller's
responsibility. Craft definition values are explicit parameters rather than
universal constants; the supplied Caero definition yields gain 416, drive 1656
and signed vertical bias -90.

The caller supplies the original frame-step value. Zero drive divisors and
quotient overflow are reported explicitly, not hidden by host floating-point
arithmetic. This callback does not impose a fixed simulation frequency or
substitute elapsed seconds for the original clock contract.

## Verification

`tools/generate_caero_flight_reference.py WORKSPACE` records 32 native sequences
of 16 steps, including startup transitions, manual and assisted pitch, braking,
engine flags, partial/zero beacon strengths and the boost cheat. Each comparison
checks all 26 persistent fields, including display quantities and fractional
positions: 13,312 assertions. Twelve additional native handler cases check boost
activation thresholds and retriggering with nonzero low bytes.

There is one emulator accommodation: Unicorn loses carry when `851F` modifies
its active translated block. The generator executes the original repair helper
in a separate native harness with mirrored DS, exactly as the existing repair
fixture does, then returns its updated damage/phase to the callback. No flight
arithmetic is replaced with Python. This limitation is explicit in the generated
fixture and does not alter the C++ repair implementation.

The application still uses its inspection camera until native input, collision
and game-frame ordering can supply a coherent flight loop.

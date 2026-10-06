# Player flight composition

`player_flight` owns a variant of the two craft states, steering history,
engine/altitude/speed settings and crash lifecycle. It consumes explicit input,
frame step and clock values with no GLFW or rendering dependency. Original
object definitions supply each craft's response gain and vertical/drive terms.

The update preserves the previous position, advances the craft callback, then
performs the city/terrain sweep. An admitted building hit crashes the player;
category 2 advances the cell's damage state when its current model has a damage
link. Terrain contacts respect player flag `10`. Crash transition `6F4F` retains
unrelated flags, sets `08|20`, stops speed, sets pitch `0205` and records the
wrapping `clock + 1536` deadline. Later updates use `6EF7`'s heading rotation and
signed pitch approach to `EC00`.

Flight commands preserve the craft-dependent E, A, Enter, minus and equals
behaviour. Caero boost spends the existing reserve; upgraded Skimma Enter selects
640. Shields cannot be enabled while the Skimma's landing flag is set. Sound,
messages and external crash-camera actions remain separate event consumers.

## Evidence

- 512 native crash transitions/attitude updates compare flags, expiry, speed and
  angles, including already-expired objects and wrapping times.
- 1,050 native integrated frames across 24 controlled traces compare complete
  craft movement followed by the original `6F0F` collision pass. They include
  Caero and both Skimmas, two heights, engine/shield settings, streetlights and
  houses, final position/attitude/velocity, building state and crash transition.
- The integrated traces use zero processed steering, an isolated real Delphi
  building and empty other-object lists. Separate steering fixtures cover held
  inputs. Visual-effect spawning is omitted from the native collision harness;
  geometry and gameplay response execute unchanged.
- 1,024 native instrument samples compare Caero altitude/damage/boost fields and
  Skimma speed strips. Incoming/reserve displays come from the independently
  verified Caero accounting callback.

## Current application boundary

The single executable now uses this flight composition instead of its free
camera. It starts from an explicitly temporary airborne checkpoint above each
city; those coordinates, initial speed and reserves are not represented as the
original new-game setup. The next integration must replace them with scenario
initialisation and its landing/gate state.

The host captures relative mouse movement (raw where supported), forwards
arrow/Ctrl state and flight key events, converts elapsed time into the recovered
PIT-rate interrupt count and consumes the capped frame clock. Input-event timing
and mouse sensitivity require interactive comparison; the complete original
50 Hz button polling and modal input paths are not claimed here.

First-person rendering follows the craft pose. Several gauges are now live;
weapon icons are blank. Skimma shield startup/strength and the low-altitude
warning are live; directional hit effects, Caero stall dimming and Nayas activity
remain separate producers. Other actors,
weapons, world sound, landing, alternate camera modes, explosions and death/retry
screens are not yet connected. Enter after a crash restores the temporary airborne checkpoint, including the
initial city state and neutral steering. This is a development checkpoint retry,
not the original campaign death/retry flow. The reconstructed deadline is retained for the future scene
transition; no automatic retry policy has been invented.

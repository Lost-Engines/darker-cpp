# Player flight composition

`player_flight` owns a variant of the two craft states, steering history,
engine/altitude/speed settings and crash lifecycle. It consumes explicit input,
frame step and clock values with no GLFW or rendering dependency. Original
object definitions supply each craft's response gain and vertical/drive terms.

The standalone update preserves the previous position, advances the craft callback,
then performs the city/terrain sweep. Campaign flight separates `advance_motion`
from the common collision phase, after other moving-object callbacks. An admitted building hit crashes the player;
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

Campaign flight uses the scenario's original craft profile and launch/pad setup.
Explicit free-flight checkpoints retain the composed `advance` operation;
campaign frames call `advance_motion` and pass the saved starting position into
`mission_combat::advance`. The common collision phase resolves the player before
aircraft and both projectile lists. It applies a city response only after the
ordered object lists have had their chance to supersede that contact.

The host captures relative mouse movement (raw where supported), forwards
arrow/Ctrl state and flight key events, converts elapsed time into the recovered
PIT-rate interrupt count and consumes the capped frame clock. Input-event timing
and mouse sensitivity require interactive comparison; the complete original
50 Hz button polling and modal input paths are not claimed here.

Cockpit, full-screen, following, level-following and dropped cameras are live.
Tab redirects steering to look-around; released offsets return through the
original vector reduction. Several gauges are now live;
weapon icons are blank. Skimma shield startup/strength and the low-altitude
warning are live; directional hit effects, Caero stall dimming and Nayas activity
remain separate producers. Other actors,
weapons, world sound, landing, object/missile cameras, explosions and death/retry
screens are not yet connected. Enter after a crash restores the temporary airborne checkpoint, including the
initial city state and neutral steering. This is a development checkpoint retry,
not the original campaign death/retry flow. The reconstructed deadline is retained for the future scene
transition; no automatic retry policy has been invented.

## Player ramming

`6ED4` first applies strength 5C to the other object, including the separate
zero-resistance vehicle path. It then consumes a random byte, sets its high bit
and applies player damage with kick amplitude 3C. Caero damage and Skimma shield
responses share their existing native arithmetic. Recipe 70F0 appears at the
retained player endpoint. The ordinary lethal-damage gate runs after the other
collision passes, as in 6F45. A player carrying flag 20 is excluded from this
collision-owner pass; target flags are not a universal collision exclusion.

512 native cases execute the full player admission/response pass, with original
model extents and visual effect spawning intercepted. 171 produce ramming
responses. They cover all three ordinary player craft, shield states, varied
initial damage, actor flags, aircraft and zero-resistance vehicles, underground
damage response and clock wrapping. Both objects' resulting damage/motion fields
and the final random state agree. The 500-frame native hands-off hangar launch
now also runs through the separated campaign motion/collision phases.

Number-key weapon selection now shares the native C8A0 supplementary-context
gate and triggers B903's craft-specific confirmation sound on accepted input.
Automatic script selections remain independent of that keyboard gate. The
windowed Halon transition check passes all eight flight entries and saves
through stage 114 after the collision-phase change; this uses Level X and does
not replace the normal objective fixtures or manual combat testing.

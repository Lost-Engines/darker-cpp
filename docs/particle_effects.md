# Combat particle effects

Aircraft hits now emit the original `72DF` burst; damage overflow selects `7319`.
Damaged and falling aircraft leave stationary sprites through the `79C2/676E`
trail path. Aircraft collisions with terrain/buildings select `7199/716C`.
Pinner terrain and non-destructive building impacts select `721C/7386`, and
eligible building destruction follows the type's effect-directory index.

The artwork comes directly from the current cockpit sheet in the original packs.
No converted images, alpha fades, interpolated sprite sizes or new effects assets
are used. `generate_effect_tables.py` retains the executable's 21 emitter recipes,
seven building bindings, five sprite selectors, row masks and distant point colours.
Sound layers now retain their original timing and parameters and feed the live
FM path; see [combat audio](combat_audio.md).

`effect_system` owns separate pools of up to 25 recipe emitters and 20 trail sprites,
as initialised by `1D28/1D32`. At capacity the oldest active record is replaced,
matching `1CBF`'s tail-recycling policy. New recipes retain their individual start
delays, wrapping radius/angle words and signed rates. Height motion carries a
fractional byte between updates. An emitter's remaining lifetime selects phases
in reverse; bright sequences splice from phase 16 into phase 9. The exact final
tick still draws phase zero.

Trails use the saved aircraft position and one word from the **shared original
random generator**. Consequently enabling trails changes subsequent randomised
combat kicks; the controlled integration check's old shot count is not a stable
acceptance criterion. The check still requires two removed objectives, the original
return message, completed docking and both burst/trail emission.

The renderer inserts effects into the existing city/model ordering and uses the
same camera basis. Recipe rings follow `689F`'s two-arc traversal, including its
redundant nearest sample. `6AA6` chooses 16/12/8/6/4-pixel sprites by depth, then
single pixels at long distance. Destination alignment chooses the corresponding
source variant; opaque pixels follow the original scanline masks. Buildings can
therefore obscure effects through ordinary scene ordering.

## Verification

- **1,091 native projected emitter groups:** compare ordered sprite-call counts
  and fingerprints, including clipped rings and tied depths.
- **512 native motion updates:** compare height/fraction, radius and angle with
  signed rates, wrapping values and signed frame-step bytes.
- **220 native sprite selections:** compare complete C++ framebuffer fingerprints
  against the source coordinates and masks captured at the original blitter entry,
  using a synthetic position-dependent source pattern. This checks copying and
  alignment; it does not emulate VGA writes.
- **152 phase cases and 192 trail cases:** retain the independently verified
  extraction fixtures, including delays, expiry and bright-phase transitions.
- Original-pack integration still completes the first mission with effects enabled.
- A temporary render harness exercised a fatal-damage recipe at four ages over
  Delphi; visual inspection checked its progression and source-sheet alignment.

```sh
python3 tools/generate_effect_tables.py ../analysis/unpacked/image.bin
PYTHONPATH=/tmp/darker-python python3 tools/generate_effect_reference.py ..
ctest --test-dir build --output-on-failure
```

## Remaining fidelity work

The native comparisons establish per-emitter projection, source selection, motion
and ageing, not a complete original scene trace. Visibility admission currently
occurs while drawing; the original also rejects some spawns before allocating.
Pool-pressure behaviour therefore still needs comparison during overlapping,
off-screen effects. Exact render-list tie ordering, single-sprite subpixel details
and first-update scheduling need a combined native scene fixture.

Enemy gun endpoints now emit the original short sprite and timed patch-22 sound.
Player-crash recipes and other weapon callbacks remain to be connected. Combat
audio has native spatial admission and Doppler comparisons, with remaining
allocation and stereo limits documented separately.

The user completed the preceding effects-free first mission and confirmed that
landing was seamless. This change adds the missing aircraft-hit feedback to that
playable loop; it does not mark the whole reconstruction complete.

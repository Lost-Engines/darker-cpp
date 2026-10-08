# Energy beacons and radio coverage on radar

The normal Caero radar now draws lit energy towers through the original 5898–5928 path. It samples a five-by-five nine-cell lattice, beginning at the original lookup of player position minus eighteen cells. One rotated origin and two fixed-point increments determine all dot positions; independently projecting each tower would change rounding. Each dot requires type 1, state bit 80h and radio coverage, and uses colour 22 minus squared pixel radius divided by 32. The radius limit is 21 pixels.

Radio coverage follows 3AFD, 5A75 and 59A3: the nearby type-12 grid's state bytes enable a three-by-three region mask. State 20h and above disables a region. The lookup deliberately tests the state, not the model type. Both aircraft and ground contacts now use this mask, and the two categories select their respective original palettes. Static objects are not included by the normal surface radar path. The enlarged surface radar retains object contacts only, as in 5A3A–5A5A; the separate energy-tower loop belongs to the normal display.

`tools/generate_radar_beacons_reference.py` executes 512 native coverage setups and complete tower loops, with varied player position, orientation, sixteen radio-beacon states and tower states including off, lit and damaged. Tests compare every mask, eight independently probed contact decisions per setup, and every resulting framebuffer pixel. The tower drawing sink is intercepted; lattice traversal, rotation, map-state checks, coverage filtering and colour calculation execute natively. Inputs cover the 128-by-128 city, plus the underground coverage bypass in isolation. Full off-map scratch-memory behaviour and underground cockpit composition are not established by these fixtures.

A window test uses `--skip-intro`, enters the first mission, captures normal and enlarged radar and exits through the menu. The normal hangar screenshot now shows the missing tower dots. The full test suite passes. Uncovered regions now use the native interference path described below.

## Uncovered-region interference

`5941–59A2` attempts 17 random points per normal surface-radar frame whenever
the coverage mask differs from `0777h`. Each point consumes the shared `92D2`
generator, even when coverage or circular clipping rejects it. Signed byte
shifts choose offsets near the player; the retained low bytes participate in
the subsequent fixed-point rotation. Points in covered regions are rejected.

The surviving points use `5C04`, the same single-pixel drawing path as energy
beacons: radius 21, palette index `22 - (squared_radius >> 5)`. Thus static is
grey and becomes darker towards the rim. It is drawn after ordinary contacts,
and the restored cockpit background clears the previous frame's dots. Full
coverage consumes no random numbers. Underground bypasses this surface path;
the enlarged-radar routine has no separate interference loop.

512 original-code fixtures compare complete snow pixel buffers and final random
states for absent, partial and full coverage, arbitrary headings and wrapped
positions outside the city. Only the final VGA pixel sink is substituted; the
original random generator, coverage predicate, rotation, clipping and shade
calculation execute. The live renderer shares the combat random state rather
than introducing an independent cosmetic generator.

```sh
PYTHONPATH=/tmp/darker-python python3 tools/generate_radar_noise_reference.py ..
```

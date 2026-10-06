# Combat sound and gun endpoints

Effect recipes now retain their original six-byte sound records alongside the
visual emitters. All layers start at recipe onset; visual start delays do not
delay sound. Duration is stored in 16-tick units, level gains the original FF low
byte, and pitch, patch and flags remain unchanged. Recipe sounds and gun sounds
have independent 16-record pools, following the original `6F90/6F98` capacities
and oldest-record replacement. Expiry uses the signed wrapping deadline check.

The first-mission enemy gun does not draw a tracer beam or muzzle billboard.
`6730/6742` creates an endpoint sprite and sound after the ray test. A hit has
sprite flags 3 and sound level DE30; a miss has flags 6 and level CE30. Both use
patch 22, pitch 0203 and a 256-tick sound lifetime. The endpoint height retains
the collision routine's eight-unit quantisation.

Live Pinner projectiles use their executable definition's patch and pitch.
`3969` scales their level by the existing expiry fade. Their FM note follows the
projectile until it hits or expires, while the impact recipe supplies separate
sound layers. All sound is synthesised from the original FM patches on the
existing OPL stream; no exported audition recordings are played.

## Spatial calculations

`audible_level` translates `3488`'s early distance rejection and squared-distance
attenuation, including wrapped coordinate differences, altitude weighting and
nonspatial flag 2. The result participates in selecting the strongest sources.

`doppler_factor` translates `3ACD`'s two cosine products and wrapping speed.
`spatial_pitch` follows `39BA`'s bearing calculation and listener/source factor
ratio. Flag 4 bypasses this pitch adjustment, as in the original; ordinary recipe
layers commonly use flags 5. Gun endpoint sources are stationary, while Pinner
sources retain projectile motion.

Simulation-owned records feed immutable nine-voice snapshots through the existing
queue. The audio callback neither advances gameplay nor accesses mutable game
objects. Voice identities keep continuing sounds on their current channel;
replacement sources increment that channel's generation to retrigger the patch.

## Evidence and limits

Native fixtures execute **512 admission cases**, **512 directional velocity
cases**, **256 complete pitch-ratio cases**, and both **gun endpoint constructors**.
The gun probe substitutes only visibility admission and otherwise uses the native
pools and constructors. Existing FM-driver and PCM fingerprints remain enabled.
A combined test runs overlapping destruction recipes and gun effects through
voice selection and actual PCM synthesis, checks finite non-clipping output and
verifies that all timed voices expire. The original-pack first mission continues
to complete with these effects active.

The current channel manager selects up to nine strongest sources and preserves
continuing assignments. This is a deliberate integration step, **not yet an exact
translation of the entire native voice allocator and its tie/priority rules**.
Player sound callbacks still produce their previously reconstructed nonspatial
records. Listening position and velocity currently follow the player, including
external views; native camera-dependent sound ownership remains outstanding.
OPL output retains the existing mono-compatible presentation; flag-1 stereo
placement is not reconstructed here. Ambient sources, enemy engine loops, music,
player-crash recipes and other weapon callbacks also remain separate work.

```sh
python3 tools/generate_effect_tables.py ../analysis/unpacked/image.bin
PYTHONPATH=/tmp/darker-python python3 tools/generate_world_sound_reference.py ..
ctest --test-dir build --output-on-failure
```

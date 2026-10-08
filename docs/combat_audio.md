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

The channel manager now follows native candidate admission and persistent
active/free-list allocation; details and the consecutive-frame fixture are below.
Player engine and boost sounds now follow their world-space source. Other cockpit
notifications retain their nonspatial flags. Camera-dependent listening and
flag-1 stereo placement are described below. Music uses the separate original
sequencer; remaining event bindings still need auditing.

```sh
python3 tools/generate_effect_tables.py ../analysis/unpacked/image.bin
PYTHONPATH=/tmp/darker-python python3 tools/generate_world_sound_reference.py ..
ctest --test-dir build --output-on-failure
```

## Aircraft and vehicle engine callbacks

Non-player objects now run their definition's original sound callback before
spatial admission. Aircraft callback 391D adds speed and a clock/object-dependent
triangle modulation whose amplitude increases with damage. 393D suppresses
hidden or destroyed aircraft. Ground-vehicle callback 3942 changes pitch with
inclination and halves it when stationary. Callback 3957 raises a projectile's
pitch while its signed high-byte remaining lifetime is positive; 3969 applies
the original fade, including its distinct flags behaviour.

The native fixture covers 1,024 cases across every non-player definition,
including wrapped clocks, speeds, damage, flags and fade values. An integration
test checks that a moving engine retains its voice, loses level with distance,
and becomes silent when hidden, destroyed or too distant. Live sources retain
the existing Doppler calculation and native channel manager below.

```sh
PYTHONPATH=/tmp/darker-python python3 tools/generate_object_sound_reference.py ..
```

## Delphi fixed ambience

The live mixer now receives the ten fixed ambient records selected by Delphi's
BDEC range. Halon and underground ranges exclude these sources. 3599 executes
each callback before checking its timer and previous-frame voice ownership;
rejection resets the deadline to the current clock. The reproduction feeds back
its actual channel assignments from the native allocation policy below.

* 37A6 selects the radio-beacon lattice and rejects state bits E0. Clock-change
  bit 0200 starts its 112-tick beep; there is one selected source, not sixteen.
* 37BA selects the nearest type-1 energy tower and derives volume as 218 times
  its mutable state byte.
* 380A/381E select industrial proxy positions by listener quadrant and are
  suppressed in the supplementary mission context. 380A repeats every 1,920
  ticks; 381E is continuous.
* 3832 uses its separate factory-chimney regions and original clock-driven
  pitch modulation.
* 3846 follows the current hangar site while the gate sound flag is nonzero.
* 385A–3896 retain four bell-tower positions, pitches and distinct periods.
  An inaudible or displaced timed source loses its retained voice; this affects
  its subsequent deadline and cannot be replaced by an unconditional loop.

A further 1,024 native cases execute the original callbacks **and** timer/voice
checks, substituting only the final spatial-admission call. Comparisons cover
all updated coordinates, pitch, volume, flags, deadlines and admission results.
Integration tests check note retention, bell retriggering, world exclusion and
several cycles through the real OPL PCM renderer. Listener coordinates now follow the rendered camera.

```sh
PYTHONPATH=/tmp/darker-python python3 tools/generate_ambient_sound_reference.py ..
```

## Persistent physical voice allocation

3488 inserts candidates weakest first, before equal levels. With nine candidates,
a new level no stronger than the weakest is rejected; a stronger insertion drops
the weakest. 33F1 then traverses the previous active list, retains matching source
identities, and prepends released channels to the free list. Remaining candidates
consume that free list weakest first. Both active and free list order persist
between frames. This differs from sorting strongest first and finding the first
unused channel, especially for equal-volume effects and competing sources.

`voice_allocation` reproduces these rules. A fixture executes native 3488/33F1
for 256 consecutive frames with ties, empty frames and more than nine candidates,
substituting only hardware note writes. Every physical channel owner matches.
The world mixer visits player projectiles, hostile projectiles, ground vehicles,
player engine, aircraft, ordered fixed records, effects and gun sounds. Stationary
objects are excluded by the native list range. Transient effect pools now follow
363C: a previously submitted sound that lost its voice is retired rather than
restarting later if a channel becomes available.

Fixed records retain their identity when retriggered; note generation is tracked
separately from physical ownership. The host uses wider source identities instead
of native byte-sized pool identities. This preserves retention within their
lifetimes but does not establish identical transient pool reuse or all note-gate flags. Low-level gate behaviour and remaining event coverage are still open.

```sh
PYTHONPATH=/tmp/darker-python python3 tools/generate_voice_allocation_reference.py ..
```

## Stereo and camera-dependent listening

3A2A projects wrapped source displacement through the native camera basis. The
bearing feeds two sine-table curves, with separate signed intermediate rounding,
before producing left/right OPL carrier attenuation. The second curve uses the
native half-table wrap, rather than substituting a mathematically equivalent
cosine: signed rounding makes a one-step difference in some cases. All 512 native
reference cases match, including arbitrary basis coefficients and saturation.

The stream now runs two synchronised OPL2 paths. Patch, pitch and gate writes
are shared; only the final carrier-level write differs between ears. This
reproduces independent FM attenuation, rather than panning a mixed PCM signal.
Music continues to receive identical writes on both paths. PCM checks exercise
hard-left, hard-right and centred notes; the existing synthesis checks remain.

The host retains the rendered camera pose for distance admission, ambient proxy
selection and stereo orientation, without advancing the camera a second time.
Doppler separately retains the watched object's motion, following 264F's source
pointer. The player's engine and boost are positioned at the player rather than
at the listener. Stationary recipe/gun effects and moving actors/projectiles use
their original spatial flags. Fixed-record visitation now places the message
notification at 38BE, after the Caero switch at 38AA and before Skimma switches.

This does not claim hardware-identical analogue output or exhaustive validation
of every dropped-camera/missile-camera combination. Low-level note-gate flags,
transient pool reuse and remaining event bindings still warrant review.

```sh
PYTHONPATH=/tmp/darker-python python3 tools/generate_stereo_reference.py ..
```

## Timed player-record retention

The 3599 previous-voice rule also applies to timed player records, not just world
ambience. The mixer now returns their ownership mask to `flight_sounds`. A new
boost, charged-cell, switch, message or shield trigger gets its first admission
attempt; once submitted, losing its channel cancels it until another trigger.
Continuous engine/weapon-charging records remain eligible to retry. The shield
callback also follows 390E's exact engine-state comparison with one. Tests cover
first admission, eviction, a later free channel, explicit retriggering and the
continuous charging exception.

## Message notifications by craft

Native C427 requests fixed record 38BE for each nonempty, unsuppressed mission
message. B906 keeps that address for the Caero (B91F=19h), but adds 28h for either
Skimma, selecting 38E6. The latter now has its own notification binding and is
visited after the Skimma switch record in fixed-record allocation order.

Caero uses patch 31, pitch 13056, level C000h and 160 ticks; Skimma uses patch 38,
pitch 3464, level D800h and 112 ticks. Both use the same logical notification
slot, so subsequent messages retrigger it. A regression test passes each sound
through fixed-record lifetime handling, world voice allocation and the queued
OPL stream, checking that it emits audible PCM and expires at the original time.

A live mission-80 trace confirmed the existing Caero notification is triggered
and remains admitted for its lifetime. The reported missing chirp has therefore
not yet been reproduced; the craft selection correction addresses the separate
Skimma discrepancy, not an established cause of the Caero report.

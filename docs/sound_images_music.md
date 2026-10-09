# Sound Images music playback

The reconstruction plays the six **Sound Blaster** music variants directly from
00/38, 43, 48, 53, 58 and 63. Instrument definitions, percussion mapping,
fractional pitch and attenuation tables are read from the original driver
00/33. No MIDI export, SoundFont, external synthesiser process or converted
music asset is required. The other hardware arrangements have a separate
[SoundFont playback path](music_variants.md).

Presentation opcode 40 selects a group. Startup honours the delay before its
first music command; menus use group zero. Flight stops music and returns the
same OPL synthesiser to the procedural sound-effects driver. The committal
sequence starts its music at the script's command rather than immediately on
entry. `--mute` disables both music and effects.

## Original driver behaviour

The modern sequencer follows 0295/02F0/030C: sixteen possible track records,
source-order dispatch, variable-length deltas and a shared Q16 tempo accumulator.
The six shipped SB songs use at most nine FM channels. It handles notes,
releases, instruments, tempo, pitch bend, channel volume, percussion mode,
loop checkpoints and looping/parked endings. Digital sample channels are
explicitly rejected; they are not used by these music resources.

The FM path follows B94, C72, D2C/D71 and E67. Register-cache suppression matches
0856. Percussion changes the instrument through the 128-byte table at 23C0;
its pitch path differs from ordinary transposed notes. Instrument records begin
at 2440, fractional pitch words at 14BA and attenuation bytes at 1B7A. This is
why a generic MIDI interpretation cannot reproduce the SB variant's timbres.

The game supplies period 5D24 to driver function 2 at 9CA4; 065A uses it in
its integer tempo calculation. Actual calls occur every ten game interrupts
(0C0F–0C2E), or 23,860 PIT cycles. The audio adapter retains both quantities
rather than rounding the scheduler to exactly 50 Hz. Device-buffer boundaries
do not affect event timing. Chip writes and PCM generation remain on the audio
thread; song storage is loaded and reserved before the device starts.

## Verification and remaining limits

`tools/generate_music_reference.py` runs the original 00/33 driver under Unicorn,
substituting OPL register output for hardware I/O. It uses the original channel
setup and register-initialisation table, then executes 0295 for 16,384 calls per
song: roughly 328 seconds each, including repeats. The C++ integration test
matches the complete ordered register/tick fingerprints: **191,083 writes**
across the six groups, including initialisation. This verifies these shipped
sequences, not arbitrary Sound Images files or the driver's digital mixer.

The integration check also renders twenty seconds with two different callback
block sizes and requires identical PCM, finite samples and nonzero output.
The windowed test exercises unmuted menus, briefing, flight, death and retry
through the real audio device.

Twelve additional native traces cover direct song changes and stop/resume
transitions. These verify retained fractional timing and residual track fields,
including the extra driver tick performed when stopping music. Hardware analogue
output, exact device latency and synchronisation with DOS presentation drawing
are not established by these tests.
OPL emulation uses the existing pinned Nuked implementation.

## Other hardware arrangements

See [music variants](music_variants.md) for the sampled-driver sequencers and
SoundFont playback. Sound Blaster remains the default native FM path.

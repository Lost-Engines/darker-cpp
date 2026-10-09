# Music arrangements and synthesis

`--music=soundblaster|scc1|lapc1|gus|awe32` selects one of the five original
arrangements for all six music groups. Selection applies to startup, menus,
briefings and films; it does not change procedural flight sound effects.
Sound Blaster remains the default, using the original FM instruments and the
selected `--opl` emulator.

The four sampled arrangements use TinySoundFont with a SoundFont 2 bank:

```sh
./build/darker --music=scc1 --soundfont=/path/to/instruments.sf2
./build/darker --music=awe32 --soundfont=/path/to/instruments.sf2
```

Without `--soundfont`, the game searches for `soundfont.sf2` in the working
directory, then `/usr/share/sounds/sf2/FluidR3_GM.sf2` and
`/usr/share/sounds/sf2/TimGM6mb.sf2`, in that order. No SoundFont is included in
the installed game. A missing or unreadable bank produces an error rather than
silently substituting the Sound Blaster arrangement. `--mute` skips audio setup.

## What is reproduced

Each choice loads its own six native Sound Images sequences directly from the
retail packs. The sequencer preserves part order, tempo arithmetic, original
PIT scheduling, looping, channel selection, program changes, note releases,
pitch bends, volume, panning and controller messages. It plays continuously;
these are not the finite MIDI/Ogg previews in the analysis gallery.

SCC-1 command 99 releases the last note only on channels 0–11. LAPC-I, GUS and
AWE32 release it on all 16 sequence channels. GUS omits the initial channel
volume writes made by the other three drivers. AWE32 calls its synthesis
library directly; its calls are represented by equivalent MIDI messages at
this boundary. Command 94 takes no operand in these four drivers, unlike the
FM variant. Command 97's stored auxiliary field is not used by their note path.

## SoundFont rendition versus original hardware

The arrangement selector chooses the original composition and driver event
behaviour. SoundFont playback does **not** emulate the SCC-1, LAPC-I, GUS or AWE32 synthesis
hardware. Instrument numbers are interpreted by the supplied SoundFont; its
samples, envelopes and supported controllers determine the sound. [TinySoundFont](https://github.com/schellingb/TinySoundFont)
does not implement the original units' reverb and chorus processing.

LAPC-I is particularly dependent on its hardware: its driver uploads custom
Roland timbres and patch assignments during device initialisation. Those SysEx
uploads cannot be represented by selecting GM programs in a SoundFont. Its
SoundFont rendition is useful for comparing arrangements, but is not faithful
MT-32/LAPC-I instrumentation. Use the Munt backend below to hear the original custom instruments. Choosing a SoundFont
for GUS or AWE32 does not reproduce their original sample banks or voice engines.

The terminal identifies sampled playback as a SoundFont rendition explicitly.
The backend is isolated in `audio/soundfont`; original event decoding lives in
`audio/midi_music`, independently of the renderer and platform audio device.

## Verification

`tools/generate_midi_music_reference.py` executes all four original drivers,
intercepting their MIDI byte output or AWE32 synthesis calls. The integration
suite compares timed event fingerprints over 16,384 driver updates per
arrangement, including repeated loops. It also renders all 24 arrangements
with a GM SoundFont supplied using CMake’s `DARKER_TEST_SOUNDFONT`, checks finite non-silent PCM,
changes groups and verifies that OPL flight effects resume after music stops.
If no test bank is supplied, PCM checks are explicitly skipped; native event
comparisons still run. The test SoundFont is not installed with the game.

## Roland emulation with Munt

```sh
./build/darker --music=lapc1 --mt32-rom-dir=/path/to/roms
```

Munt is built automatically. Supply a compatible control ROM and PCM ROM in
that directory; neither is included with the game. CM-32L ROMs are preferred
for the LAPC-I arrangement: a 64 KiB control ROM (1.00 or 1.02) and the 1 MiB
PCM ROM in Munt format. Compatible MT-32 pairs also work. Files are identified
by contents rather than names; CM-32L is preferred when several pairs exist.
The [Munt ROM catalogue](https://github.com/munt/munt/blob/master/mt32emu/src/ROMInfo.cpp)
records recognised versions and checksums. `--mt32-rom-dir` requires
`--music=lapc1` and cannot be combined with `--soundfont`.

The game expands the original driver's custom instruments and uploads twelve
Roland DT1 SysEx messages before playback. Six messages define memory timbres
0–5; six assign them to zero-based programs 28–31 and 66–67. The upload is
824 bytes, including addresses and checksums. Five timbres enable two partials;
one enables a single partial. The driver leaves the ten-byte timbre names
untouched. Each patch uses neutral transposition and fine tuning, a twelve
semitone bend range and disables reverb. These are synthesis parameters,
not replacement PCM samples.

Music changes retain these instruments. Munt handles the device synthesis,
short-message serial delay, analogue output stage and conversion to the output
sample rate. Procedural flight effects continue through the OPL backend.

The integration test compares the entire upload byte-for-byte with native
07DF execution, captured at 07A7. Set CMake's `DARKER_TEST_MT32_ROM_DIR` to
exercise six groups through Munt and check finite, audible PCM. Without ROMs,
that PCM check is explicitly omitted; the native upload comparison still runs.

The optional `scripts/fetch_data.py roland ./roms` helper downloads the CM-32L
1.02 control and complete PCM ROM using direct Internet Archive member URLs.
It checks Munt's SHA-1 identifiers and refuses to replace existing files.
ROMs are never embedded or included in installed packages. Both MT-32 1.07
and CM-32L 1.02 control/PCM pairs have passed the six-group finite, non-silent
PCM test.

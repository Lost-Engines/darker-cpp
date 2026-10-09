# Music arrangements and synthesis

`--music=none|soundblaster_fm|midi|roland-lapc|roland-sc55|gravis|soundblaster_awe32` disables music or selects one of the five original
arrangements for all six music groups. Selection applies to startup, menus,
briefings and films; it does not change procedural flight sound effects.
The Roland option names identify the hardware family; the native driver names are:

| Option | Original driver |
| --- | --- |
| `none` | No music; sound effects remain enabled |
| `soundblaster_fm` | Sound Blaster FM |
| `midi` | Roland SCC-1 / General MIDI, SoundFont rendition |
| `roland-sc55` | Same original General MIDI arrangement, emulated SC-55 v1.21 |
| `roland-lapc` | Roland LAPC-I / MT-32 family |
| `gravis` | Gravis UltraSound |
| `soundblaster_awe32` | Sound Blaster AWE32 |

`roland-lapc` names the original LAPC-I driver, emulated with CM-32L or MT-32
ROMs. The former name `roland` remains accepted as a compatibility alias.
The existing ROM-directory and percussion-bank switches retain their names.

Sound Blaster FM remains the default, using the original FM instruments and the
selected `--opl` emulator. `--music=none` needs neither a SoundFont nor Roland
ROMs. Use `--mute` to disable all sound output.

SCC-1 and GUS use TinySoundFont with a SoundFont 2 bank. AWE32 now uses its native synthesis library and EMU8000 emulation by default; an explicit `--soundfont` retains the comparison rendition. LAPC-I uses
Munt by default, or TinySoundFont when an explicit `--soundfont` is supplied:

```sh
./build/darker --music=midi --soundfont=/path/to/instruments.sf2
./build/darker --music=soundblaster_awe32 --soundfont=/path/to/instruments.sf2
```

Without `--soundfont`, the game searches for `soundfont.sf2` beside the selected packs, then in the
working directory, then `/usr/share/sounds/sf2/FluidR3_GM.sf2` and
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
./build/darker --music=roland-lapc
```

Munt is built automatically. Supply a compatible control ROM and PCM ROM beside
the game packs, or override their location with `--mt32-rom-dir=/path/to/roms`; neither is included with the game. CM-32L ROMs are preferred
for the LAPC-I arrangement: a 64 KiB control ROM (1.00 or 1.02) and the 1 MiB
PCM ROM in Munt format. Compatible MT-32 pairs also work. Files are identified
by contents rather than names; CM-32L is preferred when several pairs exist.
The [Munt ROM catalogue](https://github.com/munt/munt/blob/master/mt32emu/src/ROMInfo.cpp)
records recognised versions and checksums. `--mt32-rom-dir` requires
`--music=roland-lapc`. It cannot be combined with `--soundfont` unless the percussion
fallback below is enabled.

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
the complete native initialiser at 0713 (including uploader 07DF), captured at 07A7. Set CMake's `DARKER_TEST_MT32_ROM_DIR` to
exercise six groups through Munt and check finite, audible PCM. Without ROMs,
that PCM check is explicitly omitted; the native upload comparison still runs.

The optional `scripts/fetch-assets.sh ./data` helper downloads the CM-32L
1.02 control and complete PCM ROM using direct Internet Archive member URLs.
It also fetches the game packs, manual and map. Existing ROMs and documents
are kept; game packs are replaced only when DARKER.00 is absent.
ROMs are never embedded or included in installed packages. Both MT-32 1.07
and CM-32L 1.02 control/PCM pairs have passed the six-group finite, non-silent
PCM test.

## Unmapped percussion diagnostics

The original LAPC-I sequences emit rhythm-channel keys 52, 55, 57 and 58,
which are OFF in the CM-32L 1.02 factory rhythm map. Munt reports these as
`Rhythm: Attempted to play unmapped key` and ignores them. Full native
initialisation at 0713 emits only our existing twelve custom-instrument
messages; no percussion-map upload is missing. The native timed-event tests
include these note events. We retain this behaviour rather than inventing
replacement drums; these diagnostics do not indicate a ROM loading failure.

## Optional General MIDI percussion fallback

```sh
./build/darker --music=roland-lapc --roland-gm-percussion-fallback
```

This experimental enhancement mixes General MIDI percussion into Munt playback
for rhythm keys explicitly marked OFF in the loaded Roland mapping. It reads the
mapping after the original instrument upload and restricts supplementation to
GM percussion keys 35–81. This includes the soundtrack's missing keys 52 (Chinese
cymbal), 55 (splash cymbal), 57 (crash cymbal 2) and 58 (vibraslap).
Mapped Roland notes and custom melodic instruments continue through Munt.
All original messages still reach Munt, so its “Attempted to play unmapped key”
diagnostics remain visible when these notes occur.

The fallback uses the normal SoundFont search. With this switch, `--soundfont`
selects only the additional percussion bank, rather than replacing Munt for the
whole arrangement; it may also be combined with `--mt32-rom-dir`. Percussion
uses the existing SoundFont gain of −6 dB and is added to the Roland output.
It follows rhythm-channel controllers and is stopped on music changes.

This is a listening experiment, not an established reconstruction of the intended
instrumentation. It is disabled by default; omit the switch for unmodified Roland
playback.

## Roland's own General MIDI bank experiment

Roland still supplies [MT-32 to General MIDI](https://www.roland.com/de/support/by_product/all/general_apps_tools/508451ba-ab7a-44bb-979c-a4097dfe1142/)
as [MT2GM.EXE](https://static.roland.com/assets/media/exe/MT2GM.EXE).
Despite the filename, this download can be extracted with `unzip`; it need not
be executed. Extract `MTGM.MID` and place it beside the ROMs or elsewhere locally.
No additional ROM is required. The bank is an external asset, not bundled with
the engine.

```sh
./build/darker --music=roland-lapc --roland-gm-percussion-bank="$HOME/.local/share/darker/MTGM.MID"
```

This is an alternative to `--roland-gm-percussion-fallback`, not an additional
switch to use with it. It does not use a SoundFont. The engine loads the MIDI
file's checked Roland SysEx messages into a second Munt instance using the same
ROMs. Only otherwise-unmapped GM-range percussion notes and rhythm controllers
reach that instance. The main instance still receives the original events,
retains Darker's custom instruments, and prints the unmapped-key warnings.

Isolation matters: MTGM.MID replaces all 64 writable timbres, remaps melodic
programmes and receive channels, and changes system settings. Loading it over
Darker's own device would change much more than the missing percussion. The
second device runs at normal Munt output gain, so balance may differ from the
SoundFont experiment. Its reverb tails may continue briefly after note release.

The bank's standard rhythm map assigns keys 52, 55 and 57 to writable timbres
63, 61 and 60 respectively; key 58 uses rhythm timbre 92. These are zero-based
internal timbre numbers. All four produce sound in the integration checks;
mapped notes remain unchanged with either fallback. This demonstrates a viable
alternative sound source, not evidence that Darker's composer used this bank.

The downloaded MTGM.MID is 27,607 bytes, SHA-256
`0d0308e00f9045241013f4943c21ba97cb59c3e4e723251ee05f9431b96fdc3c`.
Its accompanying MT-TO-32.WRI describes the setup as the 7 December 1993 version.
To include this optional local asset in audio integration checks, configure
`DARKER_TEST_ROLAND_GM_BANK` with its path alongside `DARKER_TEST_MT32_ROM_DIR`.

## Full GM bank followed by Darker's instruments

```sh
./build/darker --music=roland-lapc --roland-gm-bank="$HOME/.local/share/darker/MTGM.MID"
```

This separate experiment loads **every** SysEx message from MTGM.MID into the
main Roland device, then applies Darker's complete native instrument upload.
There is one synthesiser, no SoundFont, and no selective percussion routing.
The order models starting Darker with a synthesiser already configured by the
GM utility. Darker's six custom timbres and six patch assignments take precedence;
other bank settings remain in place, including its melodic mapping, percussion,
channel assignments and partial reserves. Music changes preserve this setup.

In particular, the bank receives melodic channels 1–8 rather than the factory
2–9. This can change which parts sound, not just their timbre. We deliberately
retain that setting in this experiment. Missing-key warnings naturally disappear
for keys the bank now maps; any remaining unmapped notes still generate Munt's
diagnostics. Omit the switch to return to the original startup state. The full-bank
option cannot be combined with either percussion fallback or `--soundfont`.
The original bank also contains writes outside Munt’s recognised device memory
regions; Munt reports and ignores those writes. They are passed through unchanged.

This is a test of a possible pre-existing hardware configuration, not evidence
that it was the composer's setup. Compare it with the original and isolated
percussion variants above.


## SC-55 v1.21 hardware playback

```sh
./scripts/fetch-assets.sh
./build/darker --music=roland-sc55
```

This mode runs the original SCC-1/General MIDI event stream through the
Nuked-SC55 backend, pinned to J. C. Moyer's fork at
`f3464753f64a7da5f5fd3a96fd72197628369589`. It emulates an **SC-55 v1.21**;
it does not claim to emulate an SCC-1 card. The arrangement in the game packs
is unchanged. Firmware, samples, envelopes, effects and voice allocation now
come from the emulated module rather than a modern SoundFont.

Five external ROMs are needed: `sc55_rom1.bin`, `sc55_rom2.bin`, and
`sc55_waverom1.bin` through `sc55_waverom3.bin`. The fetch helper downloads
individual files from the Internet Archive collection and checks the separate
SHA-256 list. Existing files are preserved. Runtime identification uses hashes
of `.bin` files in the selected directory, accepting only the complete v1.21
set; incompatible or incomplete sets produce an actionable error.

ROMs default to the game data directory. Use `--sc55-rom-dir=/path/to/roms`
for a separate location. `--soundfont` cannot be combined with this mode.
The existing `--music=midi` retains its SoundFont rendition, and `roland-lapc`
retains LAPC-I/MT-32 emulation and its optional percussion experiments.

Only the emulator backend is built; no SDL frontend or external MIDI service
is used. Firmware boots before the audio device starts. Its oversampled 64 kHz
output is converted to the host PCM rate with continuous linear interpolation,
retaining fractional phase across callbacks. Music changes send channel sound-off
and controller-reset messages without rebooting the device. ROMs are runtime
assets and are not bundled in the executable. The backend's GPL-2.0-or-later
notice is included in installed dependency notices.

Set `DARKER_TEST_SC55_ROM_DIR` at CMake configuration to enable real-ROM checks
for note output, stopping and callback-size independence. Without external ROMs,
that integration test is explicitly skipped.

## AWE32 / EMU8000

`--music=soundblaster_awe32` uses the original embedded synthesis library from
resource `00/37`, the AWE32 sample ROM, and the DOSBox-X EMU8000 core (derived
from 86Box). Supply `awe32.raw` beside the packs, run `fetch-assets.sh`, or use
`--awe32-rom=/path/to/awe32.raw`. The ROM is loaded at runtime, never built in.

The verified C++ sequencer calls the original library's note, program,
controller and pitch functions. A small isolated Unicorn x86 instance executes
that library against emulated I/O ports; it does not execute the game, its DOS
services or its sequencer. This preserves the native ROM preset tables, voice
allocation, envelopes and register programming while the EMU8000 supplies
samples, filtering, chorus and reverb. It is an emulated original library,
not yet a C++ translation of that library.

The device reports revision `0x0c`. The upstream emulator reports `0x1c`, but
Darker's library at `2B8E` tests for exactly `0x0c` and otherwise returns success
without initialising the synthesis hardware. The adapter changes only that
identification value. Hardware advances during port reads so native sample
counter waits can finish. Output is resampled from 44.1 kHz to the host rate.

With `DARKER_TEST_AWE32_ROM` and `DARKER_REFERENCE_DIR` configured, tests exercise
all six arrangements and consecutive selections, verify note output and release,
and compare identical PCM rendered in differently sized host buffers. Listening
against a physical AWE32 remains the final reference for the emulator's analogue
and effects behaviour.

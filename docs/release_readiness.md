# Alpha release readiness

The target is a complete, faithful native Linux x86-64 reconstruction using
the original retail packs. The browser engine and optional rendering extensions
remain separate work. This page tracks current implementation and verification
work; it does not define a release procedure or CI workflow.

## Implemented scope

- Campaign entry and progression through stage 116, both surface cities, tunnels
  and Nightmare; original-format pilot saves and retained world state.
- Caero and both Skimmas, weapons, enemies, scripted ground attacks, vehicles,
  Wrecker door demolition, docking, tunnel handoffs and supply pads.
- Original software rendering, cockpits, menus, briefings, films and fonts,
  with English, French and German text selected using `--language`.
- Five selectable music arrangements (native FM or SoundFont renditions),
  procedural effects, nine-voice allocation, spatial sound
  and stereo. DOSBox 0.74-3 DBOPL is the default; `--opl=nuked` retains the
  previous synthesis for comparison.

See [campaign status](campaign_status.md) for evidence. Many checks position
the craft and aim shots deliberately; they do not constitute a natural full
playthrough. The old subsystem checkpoint notes describe their original test
boundaries, not an authoritative list of currently missing game features.

## Remaining implementation

- All five arrangements are selectable. Sound Blaster uses native FM patches;
  the other four currently use SoundFont synthesis. Exact SCC-1, LAPC-I, GUS and
  AWE32 synthesis remains separate work, including LAPC-I custom timbre uploads.
  See [music variants](music_variants.md) for the implemented boundary.

## Verification in progress

- F1–F7/backtick, Tab and missile views are connected; combinations of object,
  missile and temporary look views still benefit from retail comparison.
- Transient sound-record reuse and retained-voice retriggering are checked.
  Exhaustive event coverage and integrated register timing remain open; a
  DOSBox-matching chip does not prove an identical full mixer.
- Controlled fixtures cover selected runtime patches, update ordering and
  mission exits; indirect writes and every death/load/exit combination are not
  exhaustively verified. Endgame playtesting is underway; record its results
  here as testing progresses.
- The original fixed-width coordinate and painter-order behaviour is retained.
  Mission 80's convoy passing through buildings is confirmed retail behaviour.

## Build and package checks

1. Run the complete unit/native-reference and original-pack integration suite
   in the proposed release build. Keep the results with the release notes.
2. Smoke-test the actual executable through ordinary menus and a Caero launch,
   tunnel entry, Skimma entry and Nightmare, using a separate save directory.
3. Check the default DBOPL audio through the release build. It matches the
   preferred playback references; physical-hardware accuracy remains unverified.
   Do not compensate for chip differences by editing game patches.
4. Preserve dependency notices and exact source/build references. Project
   licensing and ownership decisions remain with the project maintainers; this
   preparation does not add a first-party licence or ownership claim.
5. Record the supported modern Linux runtime dependencies and test the packaged
   executable in that environment. Older-distribution compatibility is not a target.
6. Package only the engine and its required notices/instructions, with no retail
   packs, reference executable, analysis captures or personal saves. Check the
   archive contents and test from an unpacked copy before publishing.

No public release, tag or deployment is produced by these preparation notes.

## Useful focused checks

```sh
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/darker --data-dir ../darker --level=80 --opl=dosbox
./build/darker --data-dir ../darker --level=80 --opl=nuked
./build/darker --data-dir ../darker --level=17 --no-mouse
./build/darker --data-dir ../darker --level=105
```

`--level` does not write pilot saves. For normal menu/save testing, launch from
a temporary working directory with `--data-dir` pointing to the retail packs.

See [binary packaging](binary_packaging.md) for the repeatable local staging procedure.

## Validation completed on 9 October 2026

- Optimised Linux x86-64 build: all **233 CTest checks passed**, including the
  original-pack integration test (about 27 seconds for the full release suite).
- Earlier windowed staged-binary checks: Caero, underground, upgraded Skimma, final-battle
  entry and Nightmare launch/death, with DBOPL PCM output and a separate working
  directory. F7/backtick and return-to-cockpit were also exercised after launch.
- Native runtime-patch inventory: 78 direct writes, seven world-profile writes
  in each of three worlds, and six return decisions checked.
- Installation contents checked against an allowlist: one game executable,
  documentation, its validation reports and dependency notices/source references. No game
  packs, pilot saves or verification executables were included.

Current interactive testing covers busy-combat audio, natural late-game
objective completion and the ending, and Nightmare scoring/retry. Save/load
playtesting has been successful; the additional automated retry checks are
recorded in [campaign status](campaign_status.md#release-preparation-validation).

# Alpha release readiness

The intended first release is a native Linux x86-64 **playable reconstruction
alpha**, using the original retail packs. It is not yet a certified pixel- or
hardware-perfect replacement. The browser engine and optional rendering
extensions remain separate work.

## Implemented scope

- Campaign entry and progression through stage 116, both surface cities, tunnels
  and Nightmare; original-format pilot saves and retained world state.
- Caero and both Skimmas, weapons, enemies, scripted ground attacks, vehicles,
  Wrecker door demolition, docking, tunnel handoffs and supply pads.
- Original software rendering, cockpits, menus, briefings, films and fonts.
- Sound Blaster music, procedural effects, nine-voice allocation, spatial sound
  and stereo. DOSBox 0.74-3 DBOPL is the default; `--opl=nuked` retains the
  previous synthesis for comparison.

See [campaign status](campaign_status.md) for evidence. Many checks position
the craft and aim shots deliberately; they do not constitute a natural full
playthrough. The old subsystem checkpoint notes describe their original test
boundaries, not an authoritative list of currently missing game features.

## Known fidelity limits

- F1–F7/backtick, Tab and missile views are connected; combinations of object,
  missile and temporary look views still benefit from retail comparison.
- Distant-object points have a supplied-zero initial AL boundary; original
  traversal residue can shift a point by one pixel. See [cameras](flight_cameras.md).
- Only the Sound Blaster music arrangement plays in this executable. The other
  original hardware arrangements remain available in the separate analysis.
- Transient sound-record reuse and retained-voice retriggering are checked.
  Exhaustive event coverage and integrated register timing remain open; a
  DOSBox-matching chip does not prove an identical full mixer.
- Controlled fixtures cover selected runtime patches, update ordering and
  mission exits; indirect writes and every death/load/exit combination are not
  exhaustively verified. There is no completed unassisted retail comparison of
  the entire campaign.
- The original fixed-width coordinate and painter-order behaviour is retained.
  Mission 80's convoy passing through buildings is confirmed retail behaviour.
- Direct level entry reconstructs authored setup history, not a hypothetical
  player's combat damage. Debugging cheats and noclip are explicit deviations.

## Release gates

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
5. Build and test on the oldest supported Linux distribution. A local build
   currently links the workstation's Boost Program_options, libstdc++, glibc and
   OpenGL; it is not a portable Linux binary merely because it is x86-64.
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

- Optimised Linux x86-64 build: all **228 CTest checks passed**, including the
  original-pack integration test (about 27 seconds for the full release suite).
- Windowed staged-binary checks: Caero, underground, upgraded Skimma, final-battle
  entry and Nightmare launch/death, with DBOPL PCM output and a separate working
  directory. F7/backtick and return-to-cockpit were also exercised after launch.
- Native runtime-patch inventory: 78 direct writes, seven world-profile writes
  in each of three worlds, and six return decisions checked.
- Installation contents checked against an allowlist: one game executable,
  documentation, its validation reports and dependency notices/source references. No game
  packs, pilot saves or verification executables were included.

The remaining interactive priorities are busy-combat audio, natural late-game
objective completion and the ending, and Nightmare scoring/retry. Save/load
playtesting has been successful; the additional automated retry checks are
recorded in [campaign status](campaign_status.md#release-preparation-validation).

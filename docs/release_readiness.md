# Release status

The reconstruction implements the original campaign through stage 116, both
cities, tunnels and Nightmare, using the original retail packs. Optional
rendering extensions and the later browser engine are separate projects.

## Implemented scope

- Caero and both Skimmas; weapons, enemies, scripted ground attacks, vehicles,
  Wrecker demolition, docking, tunnel handoffs and supply pads.
- Original software rendering, cockpits, menus, presentations and effects.
- English, French and German menus, credits, briefings and radio messages.
- Original-format saves, campaign progression and retained world state.
- Sound Blaster FM, AWE32/EMU8000, Gravis/GF1, LAPC-I/Munt, SC-55 and SCC-1A
  hardware paths, plus General MIDI SoundFont playback. AWE32 is the default
  music selection. Procedural effects use DOSBox DBOPL by default; Nuked OPL
  remains selectable. See [music variants](music_variants.md).

There is no currently identified missing gameplay subsystem. Earlier subsystem
notes describe checkpoints in development, not a current list of absent
features. [Campaign status](campaign_status.md) records the evidence and scope
of individual comparisons.

## Recorded exceptions and deferred testing

The German level 76 briefing contains an undefined glyph. Retail displays
flickering corruption; the reconstruction substitutes `?` where the lookup
would leave the font resource. A proper Ö is deferred to the expanded version.
Mission 80's convoy passing through buildings is confirmed original behaviour
and is retained.

Save/load playtesting has been successful. Natural endgame and Nightmare
completion comparisons are deferred: neither has yet been completed in the
retail reference during this round of testing. Controlled ending and Nightmare
checks are separate evidence, not a claim that those playthroughs happened.
These comparisons can resume later without blocking CI setup.

`--level` is a development convenience with documented cumulative unlock/state
behaviour and suppressed save writes, not a limitation of normal progression.
Requiring separately supplied original game data is inherent to this engine
reconstruction.

## Automated builds and releases

[Build and release CI](../.github/workflows/build.yml) builds Debug and Release
for Linux x86-64, Windows x86-64 and macOS arm64. Every job runs the available
asset-independent CTest suite and checks its installed executable's `--help`.
CI does not download game packs or synthesiser ROMs to run private-asset tests.

A code-changing push to `master` publishes the six tested packages after every
job succeeds. Documentation-only pushes do not run the workflow. Workflow,
test and helper-script changes can exercise the builds but do not alone publish
a release. See [binary packaging](binary_packaging.md) for exact filters,
package contents, prerequisites and reruns.

The workflows define the checks; their presence does not itself establish that
a platform passed. Use the run attached to the release commit as the build and
test record. No endgame playtest result is inferred from a successful CI run.

## Existing validation evidence

On 9 October 2026, the optimised Linux x86-64 build passed 233 CTest checks,
including original-pack integration (about 27 seconds for that run).
Earlier staged-binary checks covered Caero, underground, upgraded Skimma,
final-battle entry and Nightmare launch/death, with DBOPL PCM output and isolated
save data. F7/backtick and return-to-cockpit were exercised after launch.

The native runtime-patch inventory covers 78 direct writes, seven world-profile
writes in each of three worlds, and six return decisions. Installation contents
were checked to exclude game packs, saves and verification executables.
Following the German glyph fix, all 116 campaign briefings were rendered and
advanced in all three languages; native text comparison fixtures also passed.

For a local check with separately available reference assets:

```sh
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Use a separate copy of the packs for save-writing smoke tests: saves live beside
the selected packs, so changing only the working directory does not isolate them.

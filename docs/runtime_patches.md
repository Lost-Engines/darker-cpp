# Runtime instruction-patch audit

The Dual Launch correction showed why executing a callback from the unpacked
image is not enough: gameplay setup and the frame loop can replace its operands
before the callback runs. Reference generators must reproduce those producers,
or explicitly document which producer output they supply.

`tools/audit_runtime_patches.py` inventories 78 direct absolute-memory writes
in the bounded mission/frame setup, renderer world selection, world-profile and
player-parameter, mission-exit and automatic-return ranges. It also executes BCA4–BCC8 for Delphi, Halon and
underground, recording all seven profile writes. The generated
[runtime-patch-audit.json](runtime-patch-audit.json) retains addresses, widths,
values and the input-image checksum. Some inventoried destinations are ordinary
data; the list is not a claim that all 78 modify executable instructions.

```sh
PYTHONPATH=/tmp/darker-python python3 tools/audit_runtime_patches.py ..
```

## World profiles

Values below are hexadecimal. BEA7 is a relative CALL operand, not an absolute
callback address: 003A selects BEE3; 003E selects BEE7.

| Destination | Delphi | Halon | Underground | Consumer / reconstruction |
| --- | --- | --- | --- | --- |
| 3597 | 3792 | 38D2 | 38AA | First fixed audio record; world audio/ambient admission |
| 35F5 | 390E | 390E | 390E | Exclusive end of fixed audio records |
| C841 | 20 | 60 | 60 | Mission cell-objective damage mask |
| 2E10 | 40 | C0 | C0 | Shifted geometry damage-state traversal mask |
| 192E | 34 | 00 | 3B | Pinner Direct strength; Halon does not fire this Caero weapon |
| 8B6A | 30 | 20 | 20 | Enemy gun target-protection flags; corrected by this audit |
| BEA7 | 003A | 003A | 003E | Surface placement versus tunnel route snapping |

The audio, objective, geometry, Pinner and actor-setup consumers already express
these differences. The enemy gun still used the unpacked 30h in every world.
It now receives the actual world mask: flag 10h prevents enemy gunfire in
Delphi, but does not provide that immunity in Halon. Flag 20h still prevents
firing in both. This concerns the gun branch, not every form of damage or the
separate missile branch. The final Skimma battle in Delphi keeps mask 30h;
the decision is not simply Caero versus Skimma.

`generate_aircraft_combat_reference.py` now executes BCA4 before each gun case.
Its 1,024 existing input cases run under all three profiles (3,072 comparisons),
including ray intersections and random-state consumption. Forty-one input cases
change between Delphi and Halon, including admitted hits against flag-10h players.
The existing aircraft, projectile and controlled campaign checks remain active.

## Other reviewed setup/frame consumers

- BC85–BC96 changes the moving-object sorting opcode at 2EDC from ADD to SUB
  underground. The earlier tunnel overlap fix already represents both paths.
- BD19/BD22 supply the selected player definition's signed vertical bias and
  forward setting. Flight parameters and craft initialisation represent these;
  noclip's deliberate free-Caero override is outside baseline fidelity.
- 3D79 supplies primary/secondary press events to C9F7. 3D7C–3D87 supplies
  **release** events to CAAC. The corrected Dual Launch fixture executes that
  patch before C8E9. Its unpacked immediate is not a configuration constant.
- 3D44 resets clock wraps; 3DB1 updates difficulty pressure. The runtime has
  explicit clock/difficulty state, with script setup taking precedence over
  the stage-derived initial difficulty.
- 3C27 resets Chargeable accumulation; 3C65 clears target lock; the setup loop
  resets weapon/HUD selections, and 3D47/3D4A/3D4D reset time, startup energy
  and player damage. Runtime mission reconstruction creates fresh corresponding
  state. These are not all persistent pilot-save fields.
- 3D50–3D52 supplies 6000h initial thrust energy, already represented separately
  from visible boost cells after the launch-glide correction.

## Limits and next coverage

This pass follows direct writes in specified ranges and the complete small
world-profile producer. It does not discover indirect writes, all keyboard or
command-line handlers, loaded scenario native blocks, renderer inner-loop
patching, or sound-driver patches. The inventory is a starting point for those
passes, not a full-program verification claim.

The remaining audio gate/reuse audit and reported rising-pitch death sound are
still open. Existing recipe voice submission checks cannot establish perceptual
parity. Likewise, controlled campaign tests do not replace natural playthroughs
or certify every death/load/transition combination.

## Automatic-return outcome and retry

`7D63–7D6F` calls `C84E` through its patched objective-list cursor at `C84F`.
Only an `FF` building-list terminator together with zero outstanding objects
produces outcome 1. An `FE` boundary or nonzero object counter produces outcome
3. Six native cases now record this decision in the audit output.

The application previously treated the unsuccessful return as an ordinary menu
exit. It now shows original presentation `04_015/3` (mission aborted), then
returns to the run menu. Outcome 1 alone advances the campaign and commits
surface city state, weapons and the return site. Outcomes 2/3 retain the last
committed pilot; a retry reads the full load briefing. Underground success
advances the stage without replacing either surface map or the saved surface
hangar.

The presentation integration check compares 128 abort frames with the original
record, preserves the complete encoded save, then dismisses and retries mission
67, checking its load briefing and unchanged history. Original `3EC8` exit and
`BBC6` reload probes separately confirm the commit policy; this is not a natural
end-to-end tunnel playthrough.

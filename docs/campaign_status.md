# Connected campaign and remaining work

The playable campaign currently covers missions 1–8 (04/0), using the original packs. It includes the original startup/title/briefing path, saved pilot progression, Pinner Direct and Mimic, enemy ray fire and homing missiles, aircraft destruction/effects, reinforcement waves, scripted aircraft destinations, flatbed routes, completion messages and automatic HQ docking. Mission eight saves stage nine and returns to the menu; later stages are deliberately not advertised as playable. Skimma starts remain development free-flight checkpoints rather than the connected Halon campaign.

`--skip-intro` starts at game selection; it keeps briefings. `--scale` defaults to 4. The original Level X command provides mission skipping. After mission five unlocks Mimic, number-row 2 selects it. M enables the missile camera for subsequent launches; F4 gives the live missile-eye view. The normal radar includes energy towers and applies the small radio-beacon coverage grid to tower and vehicle contacts.

## Next integration priorities

1. **Campaign across archive boundaries:** select subsequent 04 resources and records, retain their lifetimes correctly across presentation and flight, apply nonempty scenario cell lists and beacon queues, and connect remaining actor/player script operations with their native ordering.
2. **World interactions and enemy roles:** actor-to-actor and building attacks, ground weapons, vehicle destruction/raised-route events, Wrecker door destruction, remaining aircraft callbacks, ramming, and the complete collision/update ordering.
3. **Weapons and targeting:** original target acquisition and lock indicators, remaining primary/secondary weapon selection and firing, Dual Launch, Diffuser timing, charged weapons and their distinct damage paths. Existing homing/placement primitives are useful but do not by themselves establish these behaviours.
4. **Tunnels and Halon progression:** original transitions, underground navigation and map-state rules, connected Skimma combat, upgrades, supply-pad capture/release and endgame progression.
5. **Remaining presentation and fidelity:** exact menus and score/debrief screens, Nightmare entry, Nayas activity, radar interference, remaining camera transitions, actor lighting/distant dots, complete audio voice allocation/stereo, palette fades and presentation ordering.

Continue native comparisons and focused interactive checks as these are connected. The standalone live-sync DOSBox comparison tool remains deferred. Resolution, view-distance/FOV extensions, converted resources and browser work remain outside this baseline reconstruction.

## Evidence and limits

The full suite currently has 158 passing tests, including an optional original-pack integration test. The latter completes all eight supported scripts with controlled aim/position and beacon charging, checks objective removals and final messages, and docks. It does not prove a complete uncontrolled retail-equivalent campaign playthrough. Native fixtures cover the individual arithmetic, placement, activation, targeting, camera and rendering paths described in their subsystem documents.

A real-window check uses the ordinary menus and Level X to traverse every supported briefing and flight entry, verifies original-format save checksums and weapon unlocks, and exercises Mimic follow/nose views and expiry. Manual retail/native playtesting remains valuable for integrated behaviour that isolated fixtures cannot establish.

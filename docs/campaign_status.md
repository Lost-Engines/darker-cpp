# Connected campaign and remaining work

The playable campaign currently covers missions 1–27 (04/0 through 04/2 and the first three records of 04/3), using the original packs. It includes the original startup/title/briefing path, saved pilot progression, Pinner Direct, Mimic and Brent Hunter, enemy ray fire and homing missiles, aircraft destruction/effects, reinforcement waves, scripted aircraft destinations, flatbed routes, completion messages and automatic HQ docking. Mission twenty-seven saves stage twenty-eight and returns to the menu; later stages are deliberately not advertised as playable. Skimma starts remain development free-flight checkpoints rather than the connected Halon campaign.

`--skip-intro` starts at game selection; it keeps briefings. `--scale` defaults to 4. The original Level X command provides mission skipping. After mission five unlocks Mimic, number-row 2 selects it. M enables the missile camera for subsequent launches; F4 gives the live missile-eye view. The normal radar includes energy towers and applies the small radio-beacon coverage grid to tower and vehicle contacts.

## Next integration priorities

1. **Scenario transitions and world state:** connect supplementary-script activation and remaining actor/player script operations with their native ordering.
2. **World interactions and enemy roles:** actor-to-actor and building attacks, ground weapons, remaining aircraft callbacks, ramming, and the complete collision/update ordering.
3. **Weapons and targeting:** original target acquisition and lock indicators, remaining primary/secondary weapon selection and firing, Dual Launch, Diffuser timing, charged weapons and their distinct damage paths. Existing homing/placement primitives are useful but do not by themselves establish these behaviours.
4. **Tunnels and Halon progression:** original transitions, underground navigation and map-state rules, connected Skimma combat, upgrades, supply-pad capture/release and endgame progression.
5. **Remaining presentation and fidelity:** exact menus and score/debrief screens, Nightmare entry, Nayas activity, radar interference, remaining camera transitions, actor lighting/distant dots, complete audio voice allocation/stereo, palette fades and presentation ordering.

Continue native comparisons and focused interactive checks as these are connected. The standalone live-sync DOSBox comparison tool remains deferred. Resolution, view-distance/FOV extensions, converted resources and browser work remain outside this baseline reconstruction.

## Evidence and limits

The full suite currently has 174 passing tests, including an optional original-pack integration test. The latter completes the first fifteen combat scripts and missions nineteen through twenty-two, twenty-five through twenty-seven with controlled aim/position and beacon charging, checks objective removals and final messages, and docks. It separately checks missions sixteen, eighteen and twenty-three’s destination handoffs and docking. It does not prove a complete uncontrolled retail-equivalent campaign playthrough. Native fixtures cover the individual arithmetic, placement, activation, targeting, camera and rendering paths described in their subsystem documents.

A real-window check uses the ordinary menus and Level X to traverse all twenty-seven supported briefings and flight entries, verifies original-format save checksums and weapon unlocks, and exercises Mimic follow/nose views and expiry. Manual retail/native playtesting remains valuable for integrated behaviour that isolated fixtures cannot establish.

## Distant moving-object visibility

Mission two exposed a missing renderer integration step: native `26EE` patches the byte-coordinate window consumed by `2F35` before projecting any aircraft, projectile or other moving object. The reconstruction passed these objects straight into the narrow fixed-point transform. Far-away coordinates could therefore wrap into the visible scene. Those false nearby images retained an unrelated sorting distance, so local shots could overdraw them and buildings could hide them. Their real positions remained outside radar range and projectile collision range.

Moving objects now pass the original window first: for radius 15, each wrapped cell delta must be between -14 and +14 inclusive. Particle emitters share that check (their previous independent check was one cell too wide). This preserves original arithmetic and range rather than extending the view distance or changing radar/collision rules.

`generate_object_window_reference.py` executes original window setup and acceptance instructions for 1,024 cases covering every byte delta on both axes, both native radii and wrapping camera positions. A resource integration regression renders the actual second-mission aircraft from HQ at sixteen headings and requires an empty scene, then places an aircraft nearby and requires it to be admitted. Existing combat checks continue to exercise real projectile impacts. These checks establish the false-image fix; they do not establish pixel-perfect painter ordering in every overlapping-model situation.

## Ground-vehicle impacts

The fourth mission's flatbed (definition slot 31) has zero resistance and an active route callback. Native `CE26`/`CE38` dispatches it through `CDFB`, not the ordinary aircraft damage calculation: `6EC7` sets flag 20h and a deadline of current clock plus 256, then recipe `7247` is spawned at the vehicle origin. The route callback remains active until removal; no aircraft damage accumulation or random angular kick occurs. The truck is not an objective, so shooting it does not credit an enemy kill.

`hit_actor` now handles this dispatch before calling the ordinary damage routine. It also distinguishes the zero-resistance definition flag 02h (impact-only recipe `721C`) and callback-zero static objects (recipe `7296` at the object origin, followed by immediate removal). The ordinary damage routine retains its precondition checks.

Seventy-two native reference cases cover these branches, existing expiry flags and clock wrapping. The integration check fires a real Pinner at the fourth-mission truck, verifies the destruction effect and retained route callback, and runs through its removal deadline without changing the objective count. The fixture aims within the upper part of the original collision cube to keep the shot above ground.

## Campaign archive boundary

`campaign_resources` lazily retains each normal archive-04 scenario at a stable address. Briefing and flight use the same owner; selecting another resource leaves earlier borrowed byte spans valid. The saved stage selects `(stage - 1) >> 3` and `(stage - 1) & 7`, matching `BB12`. Actual record counts are checked before use. The campaign gate remains explicit while later worlds are incomplete.

Both player and airborne scripts now receive the completed-object counter used by opcode `1F`. Missions nine through fifteen run their original reinforcement, timed-message and patrol scripts. The controlled combat fixture uses Pinner Direct throughout, independently of Mimic steering, and checks counted removals, the final message and docking for each mission. Longer briefing tests include opcode `41`'s palette/image preload and opcode `43`'s cached-image blit; `42` remains the combined load-and-display operation.

## Wrecker routes and effects

Raised movement, door damage, cutting sparks/bursts and timed route removal now match native route fixtures. An original-pack integration check follows all six door-state changes in the first underground section. The first tunnel renderer and campaign transition are connected; see [vehicle routes](vehicle_routes.md).

## Transfer to the tunnel-entry hangar

Mission sixteen (04/1 record 7) is a Delphi flight to the east hangar of
Communications HQ, cell (50,48). Its four aircraft are not counted objectives;
its player script stops immediately. Briefing opcode 29 stores destination
3064h in the immediate operand C610. Departure closes the old hangar’s three
linked cells before installing the new return site. Level X uses the same
retained destination when skipping this flight; the stage-seventeen save
therefore retains 3064h.

The 512 native departure fixtures now compare destination handoff as well as
gate state. Original-pack checks execute the briefing, advance its four actors,
close the departure site and dock at the destination. A real-window check
traverses all sixteen briefings and flight entries using Level X, checking the
save checksum and final return site. Controlled repositioning in the docking
check does not prove the entire manually flown transfer.

Automatic selection of other return hangars when the native destination is
zero is still incomplete: the reconstruction retains the starting site in that
case. Explicit nonzero scripted destinations now take precedence correctly.

## Underground foundations

The native [visibility scan](underground_visibility.md) and [route geometry/placement](tunnel_navigation.md) are implemented. Visibility compares against 384 native frames, and all 158 shipped underground moving-object starts now match the actual BEE7 helper. Underground actor steering and movement additionally match 2,528 native route choices and 1,536 actor updates. Player route recovery matches 5,214 native cases, and tunnel player flight matches 2,048 updates. These callbacks are connected to world setup and the first underground campaign transition.

## First underground mission

Mission seventeen now loads bank 32, map 70 and the underground palette, enters
at the briefing's opcode-28 site, and uses definition 28 with native tunnel
flight, route assistance, visibility propagation and the two-step damage mask.
Aircraft retain underground route placement when activated as reinforcements.
The player script selects Pinner Direct and waits for the Wrecker's door changes
before admitting the next waves. Returning through the entrance uses the native
portal speed/lookahead changes and automatic docking. Successful tunnel exits
advance the stage without replacing saved Delphi damage, weapons or return site.

The integration check runs the Wrecker, all thirteen reinforcements and sixteen
counted removals with controlled Pinner aiming and replenished weapon energy,
then captures and completes portal return. This is not an uncontrolled mission
playthrough. A real-window check covers briefing, flight entry, Level X exit,
save checksum and preservation of surface state.

Native entry fixtures include the startup resistance, lookahead and off-route
timer, rather than relying on their executable-image defaults. A thousand
consecutive hands-off updates at timestep eight match native movement,
collision, energy and portal state. At timestep ten the original itself hits
the entrance geometry at tick 2080; the reconstruction matches all 208 updates,
including the crash. Replaying a variable-timestep window trace in the native
executable also reproduced its entrance collision. Hands-off survival here is
therefore sensitive to frame timing; it is not used as a universal acceptance
criterion. Briefing resource loading is excluded from elapsed flight time, and
cursor capture is processed before establishing the flight mouse origin.

Mission eighteen restores the surface bank, Delphi palette and saved city state
and names Hemmersan (7162h) as the next return site. Its timed script includes
two visible warnings and an empty message entry. The transfer check follows the
destination handoff from Communications HQ and completes docking at Hemmersan.
Mission nineteen's controlled combat check removes its three counted aircraft,
consumes the concluding messages and docks. A real-window Level X traversal now
covers all twenty-seven supported briefings and flight entries, including both
surface/underground bank changes and original-format save checksums.

## Warehouse launches and mission twenty

The Delphi warehouse spawner now uses the original eight launch cells, compact
eligible-site timers, wrapped distance arithmetic and reusable aircraft pool.
Scenario opcode 31 controls admission. Protected take-off uses callback 8DDD,
then hands the aircraft to ordinary AI after 1024 ticks. Distant script-08
retirement returns eligible records to the free pool, retaining their identities
and script checkpoints. Warehouse platform animation slots follow native 8EEE.
See [aircraft spawning](aircraft_spawning.md) for evidence and remaining scope.

Mission twenty now connects its building-attack flag, bomber drops, timed
reinforcements and warehouse aircraft. The controlled combat check verifies five
counted removals, its return message and docking with spawning enabled. The
windowed campaign check traverses stages 1–20 using Level X and verifies the
stage-21 save; that is a transition check, not twenty manual combat playthroughs.

## Hunter introduction and mission twenty-two

Mission 21 now introduces and unlocks Brent Hunter through the original briefing.
Object acquisition, lock retention/markers, secondary fire, homing impacts and
reference repair are connected; see [Caero weapons](caero_weapons.md). Its
controlled combat check uses Hunters for all eight counted removals and completes
docking. Mission 22 completes seven removals and its timed messages while its
opcode-31 instructions disable and re-enable warehouse admission. The windowed
Level X check covers both entries and verifies the saved Hunter unlock.

## Administration HQ and the second underground mission

Mission 23 transfers from Hemmersan to Administration HQ (4C64h). Mission 24
loads map 71 and enters its underground portal at 3F64h. Its Wrecker's door-state
changes release fourteen reinforcements in groups of nine, three and two; the
controlled test completes twenty counted removals and automatic portal return.
This route exceeds the first tunnel test's original time budget, so the test
allows its complete native door sequence rather than accelerating the Wrecker.

Missions 25 and 26 restore surface combat, including their timed waves and
messages, and complete seven and twelve counted removals respectively. Mission
25's initial object-flag wait now reads player slot zero. Airborne scripts and
the player interpreter receive stable indexed flags from active, reserve and
free records; removed records retain their final flags, as DOS records do.
The retirement test also checks this retained state.

The real-window Level X traversal covers stages 1–26 and verifies that the
Administration return site and Hunter unlock survive the second tunnel, then
mission 25 selects Hemmersan as its return destination. Controlled aiming/energy and Level X checks
remain distinct from an uncontrolled gameplay comparison.

## Power-station failure and supplementary contexts

Mission 27 connects the first owner-registered blackout path. Its enemy patrol
scripts can register opcode 24 after the generator-state waits. The late frame
gate then installs shared record 04/15/7, including its separate language cursor.
The blackout continues indefinitely after its nine messages and tower fades;
it does not return to the interrupted player script. Successful combat still
finishes the ordinary mission and docks.

The normal controlled combat check removes six counted attackers. A separate
failure check supplies destroyed generator states, executes the original enemy
patrol scripts and waits, enters the supplementary context, and verifies nine
messages and 224 extinguished towers. Aircraft movement and the actual attacks
causing those supplied generator states are outside that failure fixture.
Native comparisons additionally cover twelve owner registrations and four
consecutive context exchanges, including a displayed message surviving return
to the other text stream. See [mission execution](mission_execution.md).

Mission 28 next needs its explicit objective-count adjustment: that script can
allow withdrawal while some counted aircraft remain. A derived count of live
actors cannot reproduce that operation. Mission 30 introduces Chargeable and
will require its held/released trigger, changing power and impact path.

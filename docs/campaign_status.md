# Connected campaign and remaining work

The playable campaign currently covers missions 1–97 (04/0 through the first record of 04/12), using the original packs. It includes the original startup/title/briefing path, saved pilot progression, Pinner Direct, Mimic, Brent Hunter, Chargeable, Brent Ground, Forbes Diffuser and the Caero Weapon, enemy ray fire and homing missiles, aircraft destruction/effects, reinforcement waves, scripted aircraft destinations, flatbed routes, completion messages and automatic HQ docking. Mission ninety-seven saves stage ninety-eight and returns to the menu; later stages are deliberately not advertised as playable. Skimma starts remain development free-flight checkpoints rather than the connected Halon campaign.

`--skip-intro` starts at game selection; it keeps briefings. `--scale` defaults to 4. The original Level X command provides mission skipping. After mission five unlocks Mimic, number-row 2 selects it. M enables the missile camera for subsequent launches; F4 gives the live missile-eye view. The normal radar includes energy towers and applies the small radio-beacon coverage grid to tower and vehicle contacts.

## Next integration priorities

1. **Scenario transitions and world state:** connect supplementary-script activation and remaining actor/player script operations with their native ordering.
2. **World interactions and enemy roles:** actor-to-actor and building attacks, ground weapons, remaining aircraft callbacks, ramming, and the complete collision/update ordering.
3. **Weapons and targeting:** original target acquisition and lock indicators, remaining primary/secondary weapon selection and firing, Dual Launch, Diffuser timing, charged weapons and their distinct damage paths. Existing homing/placement primitives are useful but do not by themselves establish these behaviours.
4. **Tunnels and Halon progression:** original transitions, underground navigation and map-state rules, connected Skimma combat, upgrades, supply-pad capture/release and endgame progression.
5. **Remaining presentation and fidelity:** exact menus and score/debrief screens, Nightmare entry, Nayas activity, radar interference, remaining camera transitions, actor lighting/distant dots, complete audio voice allocation/stereo, palette fades and presentation ordering.

Continue native comparisons and focused interactive checks as these are connected. The standalone live-sync DOSBox comparison tool remains deferred. Resolution, view-distance/FOV extensions, converted resources and browser work remain outside this baseline reconstruction.

## Evidence and limits

The full suite currently has 187 passing tests, including an optional original-pack integration test. The latter completes the first fifteen combat scripts and missions nineteen through twenty-two, twenty-five through thirty-five and thirty-eight through forty-four and forty-eight through forty-nine and fifty-two through fifty-three with controlled aim/position and beacon charging, checks objective removals and final messages, and docks. It separately checks missions sixteen, eighteen, twenty-three, thirty-seven, forty-five, forty-seven and fifty-six’s destination handoffs and docking. It does not prove a complete uncontrolled retail-equivalent campaign playthrough. Native fixtures cover the individual arithmetic, placement, activation, targeting, camera and rendering paths described in their subsystem documents.

A real-window check uses the ordinary menus and Level X to traverse all fifty-nine supported briefings and flight entries, verifies original-format save checksums and weapon unlocks, and exercises Mimic follow/nose views and expiry. Manual retail/native playtesting remains valuable for integrated behaviour that isolated fixtures cannot establish.

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
covers all fifty-nine supported briefings and flight entries, including both
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


## Withdrawal with surviving aircraft

Mission 28's opcode 33 adds CEh (-50) to the outstanding-objective byte, then
clamps a negative signed result to zero. This permits withdrawal after its
six-removal threshold and subsequent messages even if other aircraft remain.
The counter is now explicit: reserve admission adds counted records with byte
wrapping; removal and script adjustments use native C16E. Completed removals and
live actors remain separate quantities. Projectile expiry shares that arithmetic.

The counter matches 1,280 native cases covering every initial byte and boundary
operands. Mission 28's controlled check currently removes eight of eleven counted
aircraft, receives the withdrawal message and docks with three survivors.
Its assertion allows the original minimum rather than requiring all eleven to
die. Mission 29 completes nine removals and docks. Windowed checks cover both
briefing/flight entries and the stage-30 save boundary.

## Chargeable and missions thirty to thirty-two

Mission 30 introduces Chargeable on key 9. A secondary-fire edge pays the initial
cost; holding accumulates energy, saturating at FFFFh. Maintaining full charge
still drains energy at 1/32 of the charging rate. Releasing clears stored charge
and launches only with a valid aircraft target. Projectile lifetime is charge
shifted right four bits. CC68 steers like Hunter while deriving extra speed and
visible spin from remaining lifetime; its retained steering roll is separate.
CF37 derives impact strength from remaining lifetime, and makes the last timer
page harmless. The common collision caller still stops the projectile.

The firing request now names its inputs, including distinct pressed and held
states. Charge survives weapon selection and clears on mission setup or player
crash, matching CA80's native resets. The rising charging tone uses effect 37E2
and callback 370F, including timer modulation. The underground initial cost's
signed extension immediately saturates charge: this oddity is preserved.

Native fixtures cover 2,048 firing/charging cases, 2,048 complete flight updates,
1,024 impact-boundary cases and 512 charging-tone samples. The controlled mission
30 check completes twelve counted removals using Chargeable; missions 31 and 32
complete eleven and six respectively. These checks supply aim/position and
beacon energy, rather than simulating an unaided player. Windowed checks cover
the three briefings, key 9, held/released secondary fire, the weapon unlock and
stage-33 save boundary.

## Assassins, Stalkers and Communications network

Missions 33 and 34 complete four and two counted Assassin removals using charged
shots. Their controlled aim follows the target's heading: launching northwards
across its lateral motion made the native-speed missiles repeatedly overshoot.
The weapon steering was not changed to make that test pass. Mission 35 removes
the Stalker and docks at Communications (3060h); its player script stops without
a return message. Mission 36 places the player at that tunnel's corresponding
entry and completes 15 admissions, 19 objective removals and portal return.
Mission 37 changes the destination back to Hemmersan. Missions 38–40 complete
eight, four and six removals respectively with Chargeable.

The third tunnel also introduces two stationary aircraft in the reserve list.
Unlike routed aircraft, they have no tunnel connection. Their category is
preserved on activation, and the ordinary callback-zero hit path removes them.
Native 6F0F calls collision sweeps for owner lists 6FD0, 6FD8, 6FB8 and 6FC0;
it does not walk static list 6FB0 as a collision owner. Static records are now
excluded from the reconstruction's moving-aircraft sweep. A regression check
uses the real stationary definitions with a forced terrain intersection and
requires them to remain unchanged until hit.

All eight briefings and flight entries passed windowed Level X checks, including
the Communications destination, tunnel return, retained weapon mask and stage-41
save boundary. As elsewhere, controlled combat and windowed traversal are
complementary checks, not proof of a complete unaided playthrough.

## Radio-tower defence and Storage HQ

Missions 41–44 complete seven, six, eight and twelve counted removals, including
the reinforcement messages and return-to-base scripts. Mission 43 exercises the
existing beacon queue/fade consumer for a partial blackout; its supplementary
full-blackout path uses the already-connected shared record 04/15/7.

Mission 45 transfers to Storage HQ at 0C84h. The fourth underground mission has
its own entry coordinate 1784h, which does not replace the saved surface return
site. Mission 46 completes its Wrecker route, ten reinforcements, fourteen
removals and portal return; mission 47 transfers back to Hemmersan. Windowed
checks traverse these seven entries and verify both return-site handoffs, the
weapon mask and stage-48 save boundary.

Mission 48's mobile missile launcher now uses optional route-combat callback
9185. Brent Ground building objectives are now connected; the next integration is the
later generator defence and mixed building/aircraft objectives.

## Mobile missile launchers

The compact-route callback invokes 9185 after actions 0, 1, 6 and 7, but not while
turning, climbing or expiring. Definition flag 01h enables the firing path.
The launcher checks player distance/height and a narrow rearward cone, followed
by two cells behind its travel direction. Type zero and collision marker FFh
are clear. Native 9210 also returns clear outside the map: its TEST clears carry
before returning, and the caller tests carry. That edge behaviour is preserved.

An admitted attempt complements byte +50 and records the shot time even if the
hostile projectile pool is full. The pair alternates a difficulty-dependent long
delay with a three-page delay. It launches definition 18 at the player through
the ordinary homing pipeline; zero-resistance emitter placement reverses the
vehicle heading and supplies pitch 0ABEh.

The guards/allocation accounting match 4,096 native cases, including 112 launches;
160 native route samples establish the call/no-call decision and cardinal
argument. A real-resource check runs mission 48's six launchers with a controlled
rearward player position until the route pipeline emits a missile. Missions 48
and 49 complete twelve counted removals each. Windowed Level X checks verify
both entries, saves and the stage-50 boundary.

## Brent Ground and marked buildings

Missions 50 and 51 introduce key 6 and complete five and three marked storage
tank/chimney objectives. Mission 54 destroys seven tank/house targets before
directing the player to Storage HQ’s east hangar (0D88h). Each check fires the
actual weapon with ordinary target acquisition and world collision, advances
the cell-list objective cursor, checks the final message and completes docking.
Player aim, position and beacon charging are controlled. The denser Cudsall
layout is approached from multiple sides to avoid neighbouring buildings
obstructing acquisition. Missions 52 and 53 additionally complete three and five
aircraft objectives using Chargeable.

Native CDB9 requires a destruction-model link. Category-one surfaces reject
shots; category-two components accept any projectile. Other surfaces require
a projectile among definitions 0–18 with role flag 02h. Player shots additionally
require category zero and cell marker 40h; hostile shots do not. This restores
the building-attack path used by enemy bombing as well as the player’s marked
targets. Ground-impact effects also use the underground recipe 7386h when
appropriate. The decision matches 2,880 native cases spanning real Delphi
model links, state masks, projectile definitions and collision categories.

Windowed Level X checks cover all five briefings, flight entries, the Ground
unlock (saved weapon mask 0323h), destination handoff and stage-55 boundary.

## Storage east and underground Pinner strength

Native world setup BCA4 patches definition zero’s strength from the BDEC
profile table: 52 in Delphi, 59 underground (and zero in Halon, where the
Caero Pinner is not used). The Caero impact path now uses the appropriate
value. Native setup samples verify both Caero modes, and the earlier tunnel
combat checks run with the corrected strength.

Mission 55 uses entry 1088h, independently of the saved surface site 0D88h.
Its Wrecker events admit 18 reinforcements; 23 counted objects are removed
before portal return. Six of these are stationary Assassin models, whose
callback-zero impact dispatch removes them directly. Their ordinary airborne
resistance is therefore not the applicable damage path. Mission 56 supplies
four warning messages and sends the player back to Hemmersan. Both stages
pass controlled integration and windowed briefing/Level X save checks.

## Dual Launch introduction and the north-east missions

Mission 57 unlocks the two Dual Launch selections and completes seven aircraft
objectives. Mission 58 completes five marked tank objectives. Mission 59
admits its reinforcement waves and completes fifteen removals. Aircraft checks
use the already available Chargeable; building checks use Brent Ground. These
are controlled combat/script/docking checks, supplemented by real-window
briefing/flight/Level X traversal and saved weapon-mask checks (0367h).

The original Dual Launch firing, paired homing, blast-category passes, effects,
retirement and sound pitch are connected. The native dispatcher exposes an
apparent default input-mask defect that prevents the follow-up launch; this is
preserved and documented with the limits of the evidence in
[Caero weapons](caero_weapons.md#dual-launch-and-a-native-input-mask-anomaly).
The working pair is verified separately in a controlled scene.

## Hemmersan attacks and replacement objectives

Missions 60–63 complete six, eleven, eleven and eight aircraft removals with
Chargeable. Mission 64 clears nine ground vehicles with Pinner Direct. These
controlled checks exercise the real actor scripts, reinforcement admissions,
weapons, impacts, completion messages and docking.

Mission 65 changes its building-objective list during play. Opcode 34 calls
native C858: it marks the first embedded list with 40h, then selects that list
on FE or the following unmarked list on FF. Objective advancement still checks
one cell per frame, including a separate FE transition frame. Sixty-four native
replacement cases and 512 subsequent objective frames cover this behaviour.
The mixed integration check clears fourteen counted aircraft and eight office
components, follows the completion message and docks. Brent Ground must reach
the offices’ recessed category-zero entrances; their category-one walls resist
shots. The test controls the approach to those entrances and uses ordinary
acquisition, guidance and collision rather than injecting destroyed states.

Mission 66 transfers from Hemmersan (7162h) to Administration (4D62h). Its
briefing, world actors and docking are checked; the return transfer in mission
68 is also exercised independently. Mission 67 now completes too; its earlier stalled check exposed a missing
underground-specific angular kick, described below. Native startup 3D1C–3D27
clears HUD/weapon fields 4541–4553, so retaining a stronger secondary weapon
across entry was not an explanation.
Opcode 27’s write to the actor’s tunnel oscillation byte is connected to the
already verified navigation consumer.

All 188 tests pass. Windowed Level X traversal additionally checks missions
60–66, their briefings and flight entries, save checksums and the Administration
return-site handoff. This does not replace manual combat playtesting.

## Underground hit reactions and Administration return

Native 854E tests world mode 2 (underground), not Skimma mode. It halves the
ordinary impact kick and supplies an awareness-dependent minimum for weak
hits. The primitive was already tested against native execution, but the live
actor-impact caller always requested the surface path. Projectile, paired-blast
and aircraft-contact consumers now pass the correct world mode; the misleading
parameter name has been corrected too. The full aircraft-contact comparisons
now cover surface and underground modes.

That connection resolves the Administration tunnel’s stalled Pinner test:
mission 67 admits 24 reinforcements, removes all 36 counted targets and completes
portal return, with 1,101 controlled launches. The five earlier tunnel missions
also pass with their changed impact trajectories. Mission 68 completes the
return transfer from Administration to Hemmersan. Player-to-object collision
remains separate outstanding work; neither enemy resistance nor Pinner strength
has been weakened to achieve these results.

All 188 tests pass with the corrected underground response. Windowed Level X
checks cover mission 67’s underground entry, mission 68’s return briefing and
flight, both saved transitions and the final Hemmersan return site.

## Western supply craft and industrial targets

Missions 69–79 are connected through their original scripts and resources.
Mission 69 clears eight Teale houses and three counted aircraft. Missions 70–72
clear six, eight and fourteen objects; their Terrablitz mix includes parked
stationary records as well as flying craft. The controlled pilot uses Pinner
Direct on parked craft and ground vehicles, which cannot be acquired by the
air-targeting Chargeable. It attacks vehicles from the front rather than
remaining in the missile launchers’ rearward firing sector.

Missions 73–76 complete nine office components, five storage targets, nineteen
power-site components and sixteen processing targets. Office approaches use
the actual vulnerable entrance bounds, varying distance to avoid neighbouring
buildings. Mission 77 clears 39 initial office/house cells and then four new
factory targets supplied by opcode 34. These later demolition checks also
engage defending aircraft; they do not disable enemy weapons or restore player
health. They still control player position, aim and beacon power, so this is
objective/combat integration evidence rather than an unaided playthrough.
Missions 78 and 79 clear three and ten counted craft/vehicles respectively.
All checks follow completion messages and finish docking.

Mission 76 additionally exercises reserve admission from an **actor’s** script.
Actor update traversal now records stable identities separately for each native
category pass. Admissions at the head of the current category wait until the
next pass; a later category sees its new objects immediately. The executing
script retains its actor independently while insertion moves vector elements,
and writes it back by identity before flight continues. Four native two-frame
traces compare actor/player admission into air/ground lists and establish the
scheduling rule independently of the mission test.

All 188 tests pass. Windowed Level X traversal covers all eleven new briefings
and flight entries, saved weapon masks/checksums and the stage-80 boundary.

### Forbes Diffuser and mission 80

The final record of 04/9 is connected. The original presentation unlocks both
Diffuser stages; key 5 selects gas, and secondary fire alternates gas/trigger.
Native firing and timing checks cover 1,792 cases. The real-map combat check
clears all four building objectives, observes the final return message and
completes docking. See [weapon behaviour](caero_weapons.md#forbes-diffuser)
for timing, shared-target limitations and the remaining sound-scheduler scope.

### Missions 81–87

The next group now connects mixed building and ground-vehicle objectives,
Gesic defence, the Deeds fighting, the seventh underground sortie, and
Wilston Estate. Controlled combat verifies:

- 81: six building objectives and thirteen counted ground vehicles.
- 82: four warehouse roofs and seventeen counted missile launchers.
- 83–85: six, four and fifteen counted aircraft, with original final messages.
- 86: ten admitted reserves, eighteen objectives and the tunnel portal return.
- 87: nine buildings, the final message and the Hemmersan return destination.

The mixed tests use Pinner Direct against vehicles, Chargeable against aircraft,
Brent Ground against ordinary buildings and timed Diffuser pairs against
warehouses. During the gas delay the controlled pilot leaves the defended
roof; this is a position-controlled integration test, not an automated
navigation/playthrough claim. No health restoration or invulnerability is
used. Uncounted aircraft defenders are included, and mission 84's controlled
run also clears its defending aircraft before the final bombing targets.

### Final Delphi missions, 88–97

Key 8 and the Caero Weapon's beacon-powered hit are connected. Its impact
calculation matches 2,048 native samples, including the original use of the
victim token as coordinate fractions. Mission 88's controlled run uses it to
complete 28 objectives. Later tests use Chargeable when low beacon output
makes the Caero Weapon ineffective against a target's resistance.

The remaining surface combat, transfer briefings, and eighth tunnel sortie
are connected. Mission 92 admits 29 reserves, removes 41 objectives and
returns through the original portal. Mission 94 verifies five building
objectives plus two parked aircraft; mission 96 verifies nine buildings.
All checks use the original messages and normal docking/portal completion.

Objective totals are path-dependent. In the controlled mission-89 run,
23 initial counted aircraft are removed before actor scripts admit another
ten from reserve. Mission 97 admits its waves after seven and fourteen
removals, delivers the withdrawal messages, then executes `33 9C` to subtract
100 from the outstanding counter. The test reaches that deliberate early
return after seventeen removals; it must not require destroying every
remaining enemy. These counts describe controlled runs, not universal
requirements for every playthrough.

Stage 98 is a presentation-only transition into Halon. It and the connected
Skimma campaign remain the next boundary; free-flight Skimma starts still do
not constitute that campaign.

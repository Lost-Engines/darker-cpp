# Object lists and random sequence

The shared world primitives live in `src/game`. The lists and random generator have no rendering, window, audio or resource dependencies and are used by the running game. The later sections record their reconstruction and native-reference verification.

## Intrusive object lists

`object_list<Record>` retains active head, active tail and free-list head, matching the header words at native offsets +0, +2 and +4. Records supply typed `next` and `previous` pointers. This replaces segment offsets with ordinary C++ pointers while retaining stable identity and native ordering. The list privately owns its three roots and exposes read-only root accessors. Records must remain at fixed addresses for the lifetime of their links; `projectile_pool` owns their storage. The list cannot be copied or moved, preventing two independently mutable sets of roots for the same records. `add_free_record` attaches previously unused storage during pool construction.

| Member function | Original routine | Behaviour |
| --- | --- | --- |
| `allocate` | `1C95` | Pop the free head, prepend to active head, repair previous/tail; return null on exhaustion without changing links. |
| `allocate_or_reuse` | `1CBF` | Try allocation first; on exhaustion detach the active tail and prepend it to the active head. |
| `unlink` | `7AB4` | Repair active neighbours/head/tail without adding the record to the free list; return the old next record for list traversal. |
| `recycle` | `1CD4` | Unlink and prepend the removed record to the free list, returning the next active record. |

These operations do not clear payloads. Free records retain stale previous links, which allocation overwrites. Unlinking alone leaves the removed record's links untouched. Callers must supply an active member to either removal function and must not insert the same record into multiple lists. There is no ownership scan in these low-level operations.

The native tail-reuse fallback assumes at least two active records. The C++ implementation reports a logic error if that precondition is violated instead of dereferencing the native equivalent of a null record. The ordinary allocation path has no such minimum.

Projectile launch `CB01` calls `1C95`, **not** `1CBF`; therefore the existence of tail reuse does not establish that firing evicts the oldest projectile. The reused-payload contract also means the subsequent definition expansion and constructor writes must be translated separately. `projectile_pool` implements the fixed-pool setup at `1CFB–1D62` with stable native IDs; its typed records preserve the relevant native fields without requiring the original 112-byte memory layout.

## Original random generator

`next_random` translates `92D2–92E5`, mutating a caller-owned 16-bit state and returning the same new value. It wraps `state + 1`, multiplies by 75, subtracts the multiplication's high word from its low word, then adds the subtraction borrow. When the increment wraps to zero, the multiply is skipped and the high operand stays 75. This special case is retained.

No standard random engine is substituted. Initial seeding and the order of calls across weapons, collision, effects and other systems remain to be connected; matching the recurrence alone does not guarantee a matching gameplay sequence. The function has no hidden global state.

## Native verification

`tools/generate_world_reference.py` runs the original routines from the hash-checked unpacked executable. It generates test-only fixtures; production code contains no captured results.

```sh
PYTHONPATH=/tmp/darker-python python3 tools/generate_world_reference.py ..
```

The 56 list fixtures cover active lengths zero through six, every active removal position, free-list exhaustion and tail reuse. Tests compare returned identity, header links, and every record's next/previous link, including stale free links. C++ tests also check that payload values survive these operations.

The random check executes **all 65,536 input states**, checking both native return value and stored state, then compares the C++ sequence using checksums grouped by input high byte. All **47 CTest cases** pass. No application controls or additional deliverable executable were introduced.

## Typed definitions and parameter expansion

`object_definitions.h` contains the 33 original 24-byte records, generated from the hash-checked executable. The C++ representation separates angular/motion seeds, impact and speed parameters, eight role-dependent bytes, and sound parameters. Callback entries remain native identifiers for future dispatch translation; they are not machine-code pointers called by C++. The eight role-dependent bytes are deliberately not assigned universal names because their meanings differ between craft and projectiles.

`apply_object_definition` translates `BF41–BF6E`. It writes the following subset, represented by `object_parameters` rather than an opaque 112-byte memory image:

| Native runtime offset | C++ field | Source |
| --- | --- | --- |
| 44h | `definition` | Non-owning pointer to the supplied typed definition |
| 22h | `model_token` | Explicit geometry-bank binding |
| 66h | `update_entry` | Definition +2 word |
| 4Ch | `flags_4c` | C0C0h |
| 30h | `angular_response` | Definition +4 unsigned byte × 8 |
| 32h | `motion[0]` | Definition +5 unsigned byte × 256 |
| 34h | `motion[1]` | Definition +6 unsigned byte × 64 |
| 36h | `motion[2]` | Definition +7 unsigned byte × 128 |

The definition must outlive the resulting parameters; the generated static array provides stable storage. Definitions retain their original zero model words. The original loader patched those words per geometry bank, so the C++ function instead accepts a separate model binding and leaves shared definitions immutable. This token is still a native-format model reference pending geometry loading, not a host pointer.

The function represents only these eight writes. Clearing at `BF25–BF3E`, complete runtime object construction and the projectile placement transforms are separate. In particular, `CB01` stores a launch deadline, inherited roll and target before calling `BF41`; a whole-record reset there would erase required state. The current code does not pretend to complete that launch using guessed coordinates or an artificial successful allocation.

Regeneration:

```sh
python3 tools/generate_object_definitions.py ../analysis/unpacked/image.bin
PYTHONPATH=/tmp/darker-python python3 tools/generate_definition_reference.py ..
```

The independent native probe checks every definition with three model bindings and twenty isolated seed-boundary cases, for **119 expansions**. It seeds the rest of each original runtime record with a sentinel and confirms that only the eight expected words change. C++ tests check the expanded values and definition identity, including unsigned seeds 128 and 255. All **48 CTest cases** pass.

## Projectile launch placement

`place_projectile` translates the position, angle and speed results of `CB1F–CBCD`, including its two placement branches. Input position is three native coordinate words plus three fractional bytes; output retains that representation. All calculations use integer arithmetic and the original sine words, now shared by gameplay and graphics in `src/maths/sine_table.h`.

The branch is selected by the **emitter definition's byte +8**, not the projectile's definition:

- Zero uses `CB25–CB66`: quantise the wrapping heading with a 2000h bias into four quadrants, transform offsets 6 and 36 through the original XOR/complement rules at `8FBD`, and choose the side from emitter byte +50 bit 80h. Fraction bytes are copied unchanged, altitude gains 160, heading flips by 8000h and pitch becomes 0ABEh. Roll is inherited. Complement operations are retained rather than replaced by negation, which would introduce one-unit placement errors.
- Nonzero uses `CB67–CBA0`: `207A/2096` derives a direction from heading, pitch and roll. It reflects quantised indices using XOR 1023 and computes signed products with truncation and 16-bit wrapping between stages. The resulting components are scaled by 46, 46 and -184. Their fractional offsets carry into the three coordinate words exactly as the original byte additions do. Heading, pitch and roll are copied unchanged.

Both paths inherit speed from emitter +3E. The function is a pure placement calculation, not a complete projectile/world record. The full native constructor also stores flags 20h at +7, FEh at +46, clears words +26/+28, and writes inherited roll, target and deadline. Those writes are checked by the native probe but are not claimed as C++ placement outputs or silently folded into unrelated state.

The probe executes **the complete original `CB01` constructor**, including actual free-list allocation and definition expansion, using controlled emitters and a free projectile record. It captures 160 cases spanning both branches, heading-quadrant boundaries, pitch/roll changes, side flags, fractional carries and coordinate wraparound. The C++ outputs match every captured placement. The probe additionally verifies inherited roll at +56, target at +6C, and wrapping launch deadline at +68.

```sh
python3 tools/generate_sine_table.py ../analysis/unpacked/image.bin
PYTHONPATH=/tmp/darker-python python3 tools/generate_placement_reference.py ..
```

All **49 CTest cases** and **381 cockpit comparisons** pass following the shared-table move. The next integration boundary is assembling these verified operations into owned world/projectile records, then translating movement and collision. There is still no live firing simulation in the main application.

## Owned projectile pool and assembled creation

`projectile_pool` now owns twelve records at stable addresses, matching the projectile list at `6FB8` and count 0Ch supplied to `1D49`. It cannot be copied or moved, because active/free links point into its storage. Initialisation prepends records in storage order, so allocation starts at slot 11 and proceeds backwards. The native probe now executes `1D49` directly and checks that free-list order.

`launch` combines the verified allocation, definition expansion and placement functions with the remaining constructor writes:

- Deadline +68 is the wrapping sum of supplied clock and lifetime delta. This is a projectile deadline, not a firing interval.
- +56 receives the emitter's original roll, even if a placement branch subsequently changes projectile orientation.
- +6C receives the supplied target token.
- Flags +7 become 20h and lifecycle +46 becomes FEh.
- Angular-motion words +26/+28 are cleared. Word +24 is **retained**, including after recycling.

There is no blanket record reset. Definitions are non-owning references to stable data; callers must keep them alive. The pool's `recycle` operation accepts an active member and returns its next active neighbour, preserving the list-walker convention.

If the pool is exhausted, C++ `launch` returns null without mutation or eviction. Original callers check capacity before entering `CB01`, whose inner allocator has no safe constructor-level exhaustion branch. The C++ check is an explicit boundary safeguard, not a claim that executing native `CB01` without a free record is valid.

The typed `projectile` currently contains the fields required for creation and the retained angular-motion words. It is not yet the complete native runtime object; further movement, collision and lifecycle fields will be introduced with their consumers. Target tokens retain their native encoding until object/cell target resolution is implemented. This pool does not yet provide firing input, ammo consumption, target resolution, clock advancement or projectile movement, and the main application remains the cockpit milestone.

The 160 complete native launch fixtures now include fifteen constructor metadata values as well as placement. An assembled C++ test checks all of them after recycling a record with nonzero angular state. A separate pool integration test fills all twelve slots, checks identities/order, attempts an exhausted launch and recycles/relaunches a middle record. All **51 CTest cases** pass. No new executable or inspection controls were added.

## Straight-projectile motion and timed fade

`advance_direct_projectile` translates `CC64/CC87/858F–8608`. Target speed is the definition's unsigned base-speed byte multiplied by 16. The native signed approach-to-target helper uses four times the frame-step word, with word wrapping. Travel uses the wrapping midpoint of old/new speeds, multiplied by a signed word formed from the step's **low byte** shifted left eight. These distinct time operands are preserved.

Pitch projects vertical displacement and horizontal distance through the original sine table. Heading then projects horizontal displacement, including the native three-bit shift before subtracting from X/Y. Fraction bytes carry or borrow into their wrapping coordinate words. Angles remain unchanged. This entry implements the direct callback, not homing, collision, impact effects or a generic player flight update.

`update_projectile_deadline` translates the clock/fade section `79E5–7A18`, stopping at the expiry-handling boundary `7A52`. It preserves these details:

- Flags 60h select timed behaviour; otherwise the fade and deadline stay unchanged.
- A negative signed `deadline - clock` clears the timed bits. Bit 20h requests expiry handling; a bit-40h-only transition instead sets fade to 255 and continues.
- In the final 256 ticks, fade follows the remaining low byte (or its complement for the bit-40h mode).
- With more time remaining, a signed altitude high byte of at least 50h shortens the deadline to `clock + 255` and enables bit 20h. Heights whose high byte has its sign bit set do not take this branch.

The return value requests expiry handling; it does **not** recycle the projectile. Original expiry also clears target references, updates mission counters and applies lifecycle-specific unlink/recycle rules. Visibility gates before this section, the previous-position snapshot and callback dispatch after it, and those expiry side effects remain separate integration work. The newly represented fade byte is retained by launch, as the constructor does not write it.

`tools/generate_motion_reference.py` runs the actual direct movement path and the isolated deadline section. Its 480 movement cases cover heading/pitch projection, fractional wrap, speed boundaries and low-byte/full-word step behaviour. Another 210 cases cover timed flags, signed clock wrap and altitude boundaries. All **53 CTest cases** pass. These routines still require frame-loop integration; the main application has no live projectile simulation yet.

```sh
PYTHONPATH=/tmp/darker-python python3 tools/generate_motion_reference.py ..
```

## Angular response and homing with supplied target angles

`calculate_angular_response` translates `83BF–8411`. It bounds angular error using the original signed complement convention, applies response and frame-step products, updates/damps the stored angular rate, clamps a sign crossing, and integrates the midpoint rate. Word truncation, byte-level rounding and the high step-byte contribution are retained. The original helper doubles CX then halves it logically before returning, so it also returns the resulting step word; callers must not silently restore the original high bit.

`advance_homing_projectile` translates `CCDB–CD12` followed by the shared `CC64` movement path. It accepts target heading/pitch explicitly. Pitch uses the shortest wrapping word difference; heading retains the original quarter-turn test, ±192/193-unit correction and alternative negated-error branch. Pitch and heading angular rates update separately, then position/speed integration uses the step left by those helpers. Roll is unchanged. A valid definition reference is required before any state mutation.

The target lookup/direction calculation before `CCDB` is not yet translated here. The native homing probe intercepts `CC9C` only to supply DI/DX target angles and resumes at `CCDB`; all subsequent steering and movement instructions execute normally. This is evidence for the steering consumer, not for map-height lookup, object target resolution or acquisition.

`tools/generate_steering_reference.py` captures **384 angular-response cases** and **252 homing updates**. Cases exercise response/rate boundaries, error clamping, the quarter-turn heading branch, negative/wrapping errors and frame-step truncation. Tests compare the resulting rates, angles, position words, fraction bytes and speed. All **55 CTest cases** pass. The main application still does not run a projectile world; target-angle production and the object update loop remain integration work.

```sh
PYTHONPATH=/tmp/darker-python python3 tools/generate_steering_reference.py ..
```

## Object target angles and complete homing trajectories

`maths::object_target_direction` now translates `9250/925C/927F` and the object-homing heading adjustment at `CCD7`. It uses the original 256-byte ratio lookup at `934A`, wrapping coordinate differences, quadrant branches and signed minimum-word behaviour. Pitch uses the maximum absolute horizontal component rather than Euclidean distance; coincident positions retain the native zero-vector result.

`advance_object_homing_projectile` accepts a resolved target placement and now runs direction calculation, steering and movement together. A reference to the projectile's own placement takes the original self-target branch and supplies its current angles. Map-cell targets and native-token resolution remain separate.

`generate_direction_reference.py` checks 405 direction cases against `9250`, then executes complete `CC61` updates **without intercepting target-angle production** for six 64-step trajectories. Four targets move, one starts coincident with the projectile, and one is the projectile itself. C++ matches all 384 steps, including positions, fractions, angles, angular rates and speed. All 57 CTest cases pass.

```sh
python3 tools/generate_direction_table.py ../analysis/unpacked/image.bin
PYTHONPATH=/tmp/darker-python python3 tools/generate_direction_reference.py ..
```

## Ordered per-projectile update

`update_projectile` now composes the post-visibility part of the object walker (`79E5–7A2D`) for direct and object-homing callbacks:

1. Update the clock-dependent flags/fade. If this requests expiry, return immediately without looking up a target, taking a position snapshot or running motion.
2. Copy the three current coordinate words to `previous_position` (native +38/+3A/+3C). Fraction bytes are not included in this snapshot.
3. Dispatch the mutable update entry: `CC64` runs direct motion; `CC61` runs homing against the supplied resolved object placement, including the self-target case.

The caller still performs visibility/update eligibility, resolves any native target token, and processes expiry's target-reference and mission effects. Map-target homing and other callbacks are not silently treated as direct motion: unsupported callbacks or missing required inputs report an error. Clock/fade processing precedes that validation, as expiry can bypass the callback entirely. The function does not recycle records or advance a global clock.

`generate_update_reference.py` executes the original walker from `79E5`, stopping either at `7A52` (before expiry side effects) or at `7A30` (after the actual callback returns). No steering, direction, motion or deadline code is stubbed. **Thirty multi-frame sequences / 426 updates** cover both callbacks, fixed/moving/self targets, ordinary expiry, high-altitude deadline shortening, fade-in completion, untimed objects, immediate expiry, and clock wraparound. Tests compare old/current positions, fractions, angular rates, angles, speed, flags, fade, deadline and expiry outcome at each step. All **58 CTest cases** pass.

The native step lives in the immediate operand at `7A2B`. After patching it between frames, the probe invalidates Unicorn's translated block and asserts the actual CX value at callback dispatch. Without that invalidation, cached code can retain an earlier step and produce misleading sequence captures. Production C++ uses an explicit step argument and has no such instruction patching.

```sh
PYTHONPATH=/tmp/darker-python python3 tools/generate_update_reference.py ..
```

## Banked map-target guidance

`advance_map_homing_projectile` implements the other `CC9C` branch and `831E–8390`: adjust the resolved target height by the definition's guidance shift, steer pitch, derive a bounded bank target, steer roll, and turn heading through the original folded mid-roll calculation. It preserves the ±7-unit horizontal near-target gate and the asymmetric signed-overflow heading gate (-4000h accepted, +4000h rejected). Motion still runs when steering is skipped.

The update API now accepts a typed variant of no target, resolved object placement, or `map_guidance_target`. That map structure carries the coordinates, height and height extent supplied by native `D089`; map/geometry lookup itself remains external. The angular response and all following position integration are translated, including x86 shift-count masking and wrapping intermediate products.

`generate_map_guidance_reference.py` executes complete `CC61` updates, intercepting only `D089` to supply geometry lookup results. C++ matches **960 sequential updates** across heading quadrants, near-target boundaries, guidance shifts and extreme step words. The tests run through the public per-projectile dispatcher. All **59 CTest cases** pass.

```sh
PYTHONPATH=/tmp/darker-python python3 tools/generate_map_guidance_reference.py ..
```

## Expiry reference repair and objective counters

`expire_projectile` now translates `7A52–7A82`, including `7A92`, `7A97` and the selected-target clear at `CFCB`. It repairs active projectile targets that reference the expired record by making each one self-targeting; clears matching selected, 2449h and missile-view references; and sets flag 20h. Selecting no target also sets the ring's mutable target spread to 508.

Lifecycle bit zero increments the completed byte and decrements the outstanding byte. The latter uses the native signed-byte clamp, including wraparound for unusual starting values. Upper lifecycle bits select unlink without recycling (zero), decrement by two then recycle (02h–FCh), or recycle unchanged (FEh, preserving FFh too). The next active record is returned for safe traversal. Callers supply an active pool member and must invoke expiry exactly once.

The pool now exposes stable native IDs `D1A6 + 112*slot` and a bounded ID lookup. These are identity tokens, not dereferenced host addresses; resolving a slot does not by itself establish active membership. `generate_expiry_reference.py` executes the full native pool initialiser with its probe stack outside the cleared region, verifies all twelve IDs, then runs **42 full expiry cases** without stubbing target repair, counter changes or list operations.

The ring state now includes the mutable target word at `5E03`. Smoothing approaches that target rather than an assumed zero. Reload leaves it intact. The expanded ring probes check **1,008 full updates**, including targets zero, 508 and FFFFh. All **60 CTest cases** pass.

```sh
PYTHONPATH=/tmp/darker-python python3 tools/generate_expiry_reference.py ..
PYTHONPATH=/tmp/darker-python python3 tools/generate_weapon_reference.py ..
```

## Object impacts and delayed destruction

`object_impact` translates the ordinary nonzero-resistance, active-callback branch
of `CE26`, including `854E`/`8568` and the original random generator. It retains
byte/word overflow, the minimum one-unit damage increment, complemented angular
kicks, mode-2 kick scaling, saturation of the separate impact accumulator, and
fatal-hit scheduling of callback `8DAA`. Already-expiring objects use the original
signed deadline-difference test. The returned effect identifies the native hit
(`72DF`) or fatal-hit (`7319`) definition; effect spawning remains the caller's job.

The state is a typed projection of the fields consumed by this handler, not a new
object pool or a substitute for collision detection. Zero resistance and inactive
callbacks take different original effect/removal paths and are rejected before
mutation here. Integration must dispatch those branches separately.

`tools/generate_impact_reference.py WORKSPACE` records 576 native responses across
resistance/strength boundaries, damage overflow, both mode paths and existing
expiry flags/deadlines. Random kick instructions execute uninterrupted; only the
final effect-spawning boundary is intercepted. Tests compare angular rates,
accumulator, damage, callback, deadline, flags, RNG state and selected effect.

The ordinary aircraft update also reduces this damage word towards zero:
`8AC0–8AD5` subtracts `frame_ticks << definition.role_data.craft().cooldown_shift`
(native role byte 3). The field called
`actor.awareness.cooldown` is therefore a recovering damage accumulator, not
merely a firing timer. See the analysis [enemy damage and recovery explanation](https://lostengines.com/darker/docs/weapon-behaviour.html#enemy-resistance-damage-and-recovery)
for the exact formula, definition values and [minimum-hits chart](https://lostengines.com/darker/docs/weapon-behaviour.html#minimum-hits-to-destroy-an-enemy).

## Player damage, shield recharge and repair

`player_damage` implements `84D0`, using the same `8568` angular-kick helper as
object impacts. The kick consumes a random word before the damage-cheat patch.
Caero damage carries each 68 peripheral units into a major unit; incoming bit 7
adds two major units. The `6F45` lethal threshold is exposed separately from the
not-yet-integrated crash transition. Skimma damage consumes only the high byte of
its fractional shield charge. Exact depletion is lethal, as is an unshielded hit,
even with zero damage. The cheat does not bypass those branches.

The `8108..8117` recharge prefix adds twice the timestep and corrects the high
byte at 192. It retains the original word wrap and single correction for extreme
steps. `8514` repairs one Caero peripheral unit on phase carry, without repairing
major damage. Its original `8523` operand is zero; no extra configurable repair
rate is introduced.

`tools/generate_player_damage_reference.py WORKSPACE` captures 1,536 player hits
(every incoming byte for all three craft, with/without the cheat), 63 recharge
boundaries and 90 repair boundaries. Player hit tests compare angular kick,
damage, fractional shield charge and RNG state. The repair probe uses a mirrored
DS separate from CS: Unicorn otherwise loses carry when `851F` writes its own
translated block, although the real x86 MOV preserves carry. Both operand copies
are initialised before each call, and outputs are read from DS. No arithmetic
instructions are replaced. These are isolated routine comparisons, not full
flight, collision or crash-sequence tests.

## Beacon power and Caero energy accounting

`beacon_light` accepts a 128×128 expanded cell view (type and mutable state),
position words and horizontal fractional bytes. It reproduces the original
`0396` lookup construction and `8450` attenuation, including signed altitude
comparison and word arithmetic. It reads only the selected lattice cell and
requires type 1. A cell's state supplies its output strength, so dimming can
produce partial charging. This routine does not sum nearby streetlights or
other models; other potential light sources are outside this function's scope.
Invalid arithmetic that would fault the original DIV is reported explicitly.

`caero_energy` reproduces the engine flag gate, incoming-power display and
`7F6C`/`8529` accounting. It retains distinct buffer, reserve and boost words,
reserve caps and the Jason Brooke boost patch. Its timestep is the accounting
value already transformed by the flight callback (1028 for an ordinary input
step of 8); it does not transform the input a second time. Reserve spending and
full flight callback ordering are now implemented in [Caero flight](caero_flight.md);
application/game-frame integration remains pending.

`tools/generate_beacon_energy_reference.py WORKSPACE` captures 768 lighting cases
and 400 energy updates, including map boundaries, non-beacon cells, fractional
positions, threshold strengths, engine flags, full buffers and wrap boundaries.
No instructions within the probed functions are substituted. Native startup
constructs the lookup used to choose each fixture's cell. The C++ lookup is
constructed independently from the same original wrapping loop.

## Shared flight movement

`flight_motion` translates horizontal integration (`8208`), vertical integration
(`826E`) and integer speed measurement (`8254`/`92E6`). Positions retain their
16-bit coordinate plus fractional byte. Velocity smoothing preserves the
original one-unit increment when the signed error product is zero, including
zero-error cases. Midpoint integration, trigonometric table quantisation and
word wrap are unchanged; integer square root avoids floating-point conversion.

The caller supplies the already-transformed horizontal and vertical timesteps
and mid-step heading/pitch. These are not complete craft callbacks: control
processing, angular motion, lift, engine demand and update ordering still need
integration. `tools/generate_flight_motion_reference.py WORKSPACE` captures 512
steps in 32 native state sequences, including negative velocities, coordinate
wrap and extreme timestep arithmetic. C++ tests retain state between steps and
compare all position words/fractions, both velocities and measured speed.

The formerly projectile-specific placement record is now `object_pose`, shared
by flight and projectile motion. Launch construction still lives in
`projectile_placement`; there is no compatibility alias or second copy of the
coordinate representation. Both paths use `displace_object` for fractional carry.

## Angular motion shared by craft and guidance

`angular_motion` now owns the response previously embedded in projectile steering.
It also exposes the original driven-gain (`83D4`) and direct-impulse (`83DF`)
entries, bank folding (`83A4`) and attitude normalisation (`23A0`). Guidance calls
the same implementation; its earlier native trajectory tests remain unchanged.
Attitude normalisation retains the pitch XOR rather than substituting negation.

`tools/generate_angular_motion_reference.py WORKSPACE` records 640 gain/impulse
responses and a compact fingerprint of both folds for every 16-bit angle.
The existing bounded-error and projectile trajectory fixtures continue to cover
the third response entry and its callers. No platform input scaling is implied by these shared helpers. The complete
[Caero callback](caero_flight.md) now composes them in native update order.

## State ownership and semantic types

`clock_tick` names the wrapping 16-bit timer and its deadlines; `game_duration`
names native tick counts, and `campaign_clock` names the extended mission timer.
These are integer aliases, preserving the original promotions and truncations,
not unit-enforcing wrappers. Explicit casts remain at native byte/word boundaries.

Optional impact strengths, damage severities, particle frames, actor indices and
object-definition indices have names for their numeric meaning. Absence remains
distinct from a valid zero; these quantities are not enumerations.

Small state owners expose the operations that coordinate their members:
`steering_axis_state` filters its keyboard/mouse history, `flight_controls_state`
retains keyboard priority, `caero_energy_state::charge` updates its reserves and
displays together, and `weapon_target` applies craft-specific projection and
clearing rules. Their native state remains directly representable for setup and
reference fixtures. Calculations involving separate actors, maps or projectile
pools remain free functions; the main update sequence retains its original order.

See [rules, representations and extension points](engine_rules.md) for the owning
types, constants and relationships to consider when modifying this subsystem.

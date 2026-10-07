# Enemy aircraft missiles

The surface aircraft callback now runs the original object-target projectile
branch after its Skimma ray-gun check. Mission two's nonzero behaviour byte enables
this branch. Native 8AFD/8B65 apply target/protection and aim gates; 8B28 selects
the weapon through the aircraft definition and calculates the wrapping firing
delay from weapon data, behaviour and difficulty. CAF0 records the attempt time
even when there is no free hostile projectile.

The hostile list has six slots at D6E6–D916, separate from the player's twelve
slots at D1A6–D676. Existing native-checked placement and homing code constructs and
moves the missiles. Both pools feed world rendering and spatial FM sound.
The hostile collision target list at 7010 contains the player, not the aircraft
list. Scenery contacts share the existing building-state path, with the hostile
list's separate terrain effect. Player impact follows 6E95: effect 70C3, half the
projectile definition's damage strength and angular kick `(amount >> 1) - 7`.
The normal post-impact 256-tick removal state retains the record until expiry.

Scenario entry seeds difficulty from twice the stage byte. 3DB1–3DCB raises it
monotonically after four fine-clock wraps and reaches 255 after eight. This is
separate from the mission time multiplier and presentation timing.

## Evidence and limits

`tools/generate_aircraft_fire_reference.py` captures 2,048 native firing cases
(384 admitted launches, including building targets) and 96 difficulty cases. Ray geometry and allocation are
intercepted in this firing fixture; they retain their separate comparisons.
The pool test checks six-slot exhaustion, native identities and independence from
the player's pool. The original-pack test uses mission two's aircraft definition
and firing settings, launches a missile through the runtime and checks homing,
45 damage and the impact effect against a stationary player in an empty city.
This controlled case is not a complete mission flight.

Current runtime target resolution connects enemy aircraft missiles aimed at the
player and buildings. Attacks on other scenario objects, missile-warning HUD,
all global collision ordering and shared effect/random scheduling remain separate
work. Ground-vehicle firing is still unconnected. Existing player projectile
weapons beyond Pinner Direct retain their own integration backlog.

## Building attacks and bomb drops

Opcode 32 controls the building-attack flag (8B13), initially zero on each
scenario start. Cell targets require this flag, an unprotected attacker and a
distance below ten units of the manoeuvre's distance byte. They use the weapon
definition immediately after the object-target weapon. The common native aim
and wrapping cooldown checks remain intact.

Definition 23 additionally calls 8BE6 before obstacle avoidance when its target
is within 100 world units. With a cell target, flag 02 clear and building attacks
enabled, it attempts a definition-14 drop after 1536 ticks. An exhausted pool
still updates the attempt time. Successful drops start at pitch C800h. Existing
projectile placement, the hostile pool and map homing then handle the shot.

D089 resolves a cell's actual alternate/damaged model, descriptor origin,
vertical marker and extent. All 1,564 real bank/type/state target lookups match
native results. A separate 512-case native probe covers bomb admission,
cooldown wrapping, exhausted allocation and forced pitch; allocation itself is
intercepted and retains its existing independent verification. The controlled
mission-twenty combat script now runs with warehouse spawning enabled. Mission
twenty is connected; actor-to-actor missile dispatch remains outstanding.

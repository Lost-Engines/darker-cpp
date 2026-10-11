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
player and buildings. Attacks on other scenario objects,
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

## Incoming-missile HUD

Native `5CE0/5E6A` draws the hostile-projectile overlay in the Caero cockpit and
forward F4 view. `5A75/5AC9/5B76` uses player-relative horizontal positions and
heading, rather than perspective projection or missile lock state. Admission uses
a seven-cell half-open square followed by a squared-radius limit of `0x0c30`
after eightfold radar magnification. Hidden projectiles and contacts outside
radio coverage are suppressed; underground coverage is complete. All visible
hostile projectiles qualify, including shots aimed at buildings.

`5B92` supplies the existing six-pixel navigation sprite and height-coloured
centre, relative to camera height. The marker moves within the central attitude
surround, so missiles behind the player appear below its centre. External and
missile-follow cameras omit this overlay, as does the Skimma HUD. The four
cockpit pursuit lamps are a separate aircraft-aim indication.

`generate_missile_warning_reference.py` executes the original projection for
512 cases spanning heading, fractional coordinates, wrapping, range boundaries,
hidden flags and coverage gates. These complement the existing native sprite-mask
and height-colour comparisons.

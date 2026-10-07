# Skimma ammunition and ring state

`src/game/skimma_weapons` implements original state transitions independently of graphics, GLFW and wall-clock time. Both primary and secondary weapons now run in the combat core. Halon campaign startup, input selection and live cockpit integration remain to be connected.

## Implemented routines

- `5E44–5E58`: resupply restores working capacities **14, 8, 10** and reserve capacities **5, 3, 4**, in weapon-index order. It does not alter the shared ring state.
- `5E1F–5E43`: a nonempty working counter prevents automatic reload. Otherwise the reserve byte is decremented and tested for a negative signed result. A successful reload stores that reserve, restores working capacity, sets spread to 508 and sets the deadline to `clock + 1024`, wrapping to 16 bits. An unsuccessful attempt leaves state unchanged. The byte-sign test is retained even for out-of-game counter values; it is not replaced by a generic greater-than-zero test.
- `5D66–5DF6`: ring calculation tests enable bit zero and the signed, wrapping 16-bit difference between clock and deadline. During reload, radius is the high byte of the wrapped difference shifted left six, then halved. Values below 15 suppress drawing; visible reload frames use zero remaining shots. At or after the deadline, radius comes from wrapped `spread << 6`, with a minimum of 15, and the working count supplies the remaining-shot symbols.

The ring state is shared across selected weapons, matching the single native deadline and spread words. Ammunition is per slot. The caller is responsible for selecting a craft-available weapon: ordinary Skimma has two slots, upgraded Skimma three.

The calculation deliberately retains the full byte radius for raw inputs. The existing ring rasteriser supports the verified normal gameplay radius range 15–127; arbitrary out-of-range spread fixtures verify arithmetic but are not passed to that rasteriser.

## Evidence and verification

`tools/generate_weapon_reference.py` executes the original reload routine and existing native ring-producer harness. It checks the unpacked executable hash before capturing fixtures. Run from this project with the established reverse-engineering Python environment:

```sh
PYTHONPATH=/tmp/darker-python python3 tools/generate_weapon_reference.py ..
```

Tests compare 72 native reload transitions and 120 native ring decisions. Reload cases include empty/nonempty counters, signed-byte boundaries and deadline overflow. Each ring case is also translated to three deadline positions to check clock wrapping. Capacities match executable tables `5E19` and `5E1C`. All 41 CTest cases and 381 whole-cockpit comparisons pass.

## Stateful ring and weapon-status update

`update_weapon_ring` translates `5D56–5E0F` around the existing pure radius calculation. Its return value describes what to draw **before** the state mutation. A negative signed clock/deadline difference returns without modifying ring state. Otherwise the routine sets the deadline to the current clock and moves spread towards zero by the supplied native frame step (`7A2B`), using the signed comparisons and wrapping arithmetic of `7CDB`. This update happens even when enable bit zero is clear and no ring is drawn. The caller supplies the game-clock step, not a floating-point wall-clock interval.

`update_skimma_weapon_status` translates `C90A–C94E`, stopping before firing logic:

1. Attempt automatic reload on the selected weapon, regardless of its enable bit.
2. Clear reserve display `454E`, then scan available slots in descending order (two for ordinary Skimma, three for upgraded Skimma).
3. Clear status bit 1, preserving other flag bits.
4. For the selected, enabled slot, publish the newly updated reserve. If target word `5F0F` is -1 or target-list count `6FBC` is zero, leave bit 1 clear. Otherwise the original CBW replaces the reserve temporary with the sign extension of the flag byte before the working-ammunition test.
5. For other slots, set bit 1 if either reserve or working ammunition is nonzero. For the selected/enabled target branch, use the CBW temporary and working ammunition instead.

This preserves the native flag behaviour without assigning an unsupported general meaning such as “ammunition available” to bit 1. Invalid craft slot counts or unavailable selected indices are rejected before mutation. Target acquisition and the production of `5F0F`/`6FBC` remain outside this function.

The reference generator now also runs the complete ring producer while intercepting only its draw call, and the complete status producer while stopping at `C950`. **336 stateful ring cases** cover both sides of the deadline, disabled weapons, signed spread boundaries, truncation and large frame-step wraparound. **480 status cases** cover both craft slot counts, every selection, reserve-triggered reload, flag preservation, targets and empty/nonempty target lists. Tests compare counters, all slot flags, reserve display, deadline, spread and draw inputs. All **43 CTest cases** pass. These are engine-state checks; no new inspection controls or executable were added, and the current application remains a fixed-state cockpit milestone.

## Recoil and firing-path boundary

`calculate_skimma_recoil` translates `C950–C96A` with `C9FE–CA08`. The stored recoil is a signed byte (`C9FF`). The calculation consumes only the low byte of the frame step, moves the recoil towards zero with byte wrapping, and clamps a sign crossing to zero. Two separate outputs are important:

- The shot-direction offset is the negated arithmetic right shift by four of the **packed pre-clamp word**: old recoil in the high byte, wrapped new recoil in the low byte. It is not merely a multiple of the final recoil.
- The aiming offset, stored by the original at `5D4B`, is the post-clamp signed recoil shifted right by two.

`kick_skimma_recoil` translates `C984–C98E`: the supplied random byte becomes an impulse of 64–95 via `(byte & 31) + 64`, which the same byte arithmetic adds or subtracts according to the current sign. The random generator and its call ordering remain external inputs.

The native probe executes these exact code ranges. It records checksums for every one of the 256 stored recoil bytes at eleven frame-step boundaries (2,816 updates), and for all 256×256 recoil/random input pairs (65,536 kicks). Runtime code uses no captured outputs. Regenerate with:

```sh
PYTHONPATH=/tmp/darker-python python3 tools/generate_recoil_reference.py ..
```

These arithmetic checks cover the production combat routines.

### Mutable spread target

Expiry's selected-target clear (`CFCB`) writes 508 to the target operand at `5E03`. This is now explicit `weapon_ring_state::target_spread`; the post-draw update approaches it through the same signed/wrapping `7CDB` rules. Earlier zero-target descriptions above describe the initial probe configuration only. Reload changes current spread and deadline without resetting target spread. The full ring reference set now has 1,008 cases across three target values.

## Primary gun and craft-aware combat

The core combat update now accepts Skimma players. CD84's shortened ray,
recoil pitch offset and three random spread components are reproduced;
2,048 native endpoint/random-state comparisons cover arbitrary angles and
word wrapping. Enemy Skimma guns share that same verified ray calculation.

C950's recoil update runs every frame. A primary trigger edge outside the
crash flag traces against the city and the aircraft list, damages the last
intersecting aircraft with strength 32h, emits the original gun impact and
applies the next random recoil impulse. Ground and stationary lists are not
added to this gun's native aircraft-only trace. Caero projectile collision
still has its own broader actor traversal.

Player damage now dispatches through the actual craft: Skimma hits deplete
an enabled shield and are fatal without one. A controlled original-Halon
scene clears four aircraft with sixteen gun shots, retaining enemy movement
and return fire. Separate projectile-impact scenes check shielded survival
and unshielded death. This is combat-core integration; connected Halon
campaign startup and cockpit/input wiring remain separate.

## Secondary weapons and targeting

C90A–C9BF now runs through ammunition/status update and actual projectile
allocation. Only status exactly 3 admits a shot. Successful launch consumes
one working round. Slot 1 requires a map target; slots 0 and 2 require an
object target. Definitions 10–12 supply the original models, guidance,
strength and lifetime. A full firing-path fixture compares 1,536 native
cases, including full pools, disabled slots, empty magazines and clock wrap.

Reload gating belongs to the later target update, not an invented firing
deadline check. CF4A releases locks while reloading or disabled. Skimma
projection uses the wider 62-coordinate/3721-squared cone; ring spread is
four times the integer square root of four times squared screen distance.
Map locks require a nonzero damage-model link, without the Caero's marked
objective requirement. All 2,048 native projection cases match, including
partial screen-coordinate writes on rejection.

The controlled Halon actor scene also clears its four aircraft with both
air-target secondary weapons (13 shots for slot 0; 10 for upgraded slot 2).
It retains actor movement, return fire, normal ammunition and reload rules;
only player positioning/aim are controlled. This does not assert completion
of a Halon mission or verify the supply-pad campaign flow.

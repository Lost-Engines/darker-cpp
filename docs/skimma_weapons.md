# Skimma ammunition and ring state

`src/game/skimma_weapons` begins the gameplay layer with original state transitions, independent of graphics, GLFW and wall-clock time. The application still supplies sample state; firing, weapon selection and the simulation clock are not connected yet.

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

## Remaining integration

The stateful follow-up below now implements the post-draw update and the weapon-status producer. Firing, target acquisition, global frame scheduling and connection to live cockpit state remain separate. The alternate-view ring baseline is also separate. The main program now obtains its existing sample ring through the new state calculation, with identical pixels and no added keys or options.


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

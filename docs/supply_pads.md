# Skimma supply pads

`game/supply_pad` reconstructs mode-one starting placement, the C6FB
entry gate, and the 7D9A/7E49 approach and departure callbacks. The caller
owns the supplementary-script exchange; movement does not run scripts.

Entry requires shields off, no existing docking flag, downward velocity
with high byte FF, horizontal output below 320 and altitude 264–1023.
Signed heading-table offsets project the lookup point; both subcell
coordinates must be 36–219 and the projected cell must be type 3. Its state
byte is not tested. The accepted cell and subcell become the approach target.

Approach first converges to that captured point at altitude 328, rotates
towards the centre, then repeats for subcell 8080. Horizontal convergence
preserves the original major-axis ratio and fractional step carry. The
production callback uses the previously reconstructed attitude stabiliser.

The shared tail approaches the script-controlled output and stores half
as displayed speed. Departure requires sufficient output, an inactive
supplementary context and positive processed pitch input of at least 0C00.
It halves output, writes vertical velocity 200, clears docking and resumes
ordinary Skimma flight on the next frame.

Starting placement uses cell centre and nominal altitude 264 minus model
height. It starts unshielded, with output setting 256. Shield restoration
writes BF to the resource's high byte while retaining its low byte. The
ordinary Skimma definition starts in flight; the upgraded definition starts
in the docked callback. Inline scenario setup can override that placement.

## Verification

- 1,024 native entry cases cover heading projection and eligibility bounds.
- 512 native movement updates cover convergence and departure gates.
- Three complete approaches reach the original centre on the original frame.
- 256 native starts cover site, heading, model height and retained shield bits.
- Both original supply scripts run with movement, messages, ammunition,
  selection and shield restoration. Controlled 16-tick runs depart at
  22,176 and 26,416 ticks respectively; the upgraded script emits all 21
  messages. These are fixture timings, not promised in-game docking times.
- Both scripts repeat successfully on a second visit, resetting their text
  cursor and preserving the primary script's stopped state where necessary.

The isolated native movement fixture substitutes the attitude helper and
starts level with stationary attitude rates. The C++ implementation runs
the actual reconstructed stabiliser; those particular inputs leave it
unchanged. These tests do not constitute interactive flight/collision
validation at a pad. Connected Halon campaign integration remains separate.

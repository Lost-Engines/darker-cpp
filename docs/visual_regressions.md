# Retail screenshot comparisons

The first hangar and briefing comparison exposed differences outside the
previous isolated routine fixtures. These corrections retain original indexed
artwork and arithmetic; they do not adjust meshes to hide rendering errors.

## Cockpit edges and display producers

Runtime-patched 5575–559F supplies three final overlays after world rendering:

- A 76-pixel strip at logical (122,0), colour 152. The Caero row table places
  it at physical Y=8, completing the upper cockpit edge.
- Seven masked rows from logical source (72,43) to (48,169), using mask 55C1.
  Source Y becomes 51; the destination is below the shifted viewport range.
  This restores the raised instrument silhouette.
- A 53-pixel strip at (182,175), colour 21.

The original rectangular world viewport remains unchanged. These overlays
supply the missing shape at its boundaries. The native-call fixture uses the
captured running executable because its blitter call destinations have been
patched since the unpacked executable image.

56B2–56C5 dims the engine indicator below speed 200, restores it at 410 or
above, and retains the previous brightness between those thresholds. The
high bit chooses the already reconstructed alternate instrument source. This
change connects the Delphi display producer; underground-specific display
conditions and propagation of the display byte into other engine consumers
remain separate work.

An unselected primary weapon takes C9C7→CA47, producing FF at the crosshair
colour immediate's high byte. With the original low byte 19, 5F00–5F05 gives
palette index 3, rather than the ready colour 234. The current integration
covers unselected/selected Pinner states; full weapon readiness feedback is
still part of the remaining combat implementation.

5C65 maps player position bytes through the same A800 nearest-beacon lookup
used for lighting. 573F validates both coordinates and adds the display
encoding increment. Feeding raw cell numbers into the digit renderer caused
13:06 at the launch site; the original lookup produces 14:06. Both readouts
are blanked if either lookup coordinate is invalid. External camera position
no longer determines the displayed player grid coordinates.

`generate_hud_integration_reference.py` captures 32 native engine states,
512 coordinate pairs spanning every cell byte on both axes, and the three
edge-blit selections. The mask fixture verifies source sampling, coordinates
and coverage with a synthetic patterned sheet; it is not a full VGA emulation.

## Animated door interpolation

At the initial hangar view, the native and C++ flat-shaded city frames differed
at 39 pixels. All captured near-face coordinates agreed except four vertices
on two door panels: their inner corners were two screen pixels too high.

32DC/330E reads each endpoint starting at byte one, discarding its low eight
fractional bits **before subtraction**. Our expression subtracted complete
coordinates first and then shifted. A one-unit difference in the quantised
input became 928 internal coordinate units after multiplying by the gate
interpolation factor, enough to move the projected edge by two pixels.

The corrected operation order matches the native frame. The complete city
fixtures now include the launch camera at (12672,29080,0), pitch 2560, opened
gate parameter E800 and the three alternate hangar cells. Flat and Gouraud
frames are checked at three beacon-light settings. These are world-renderer
comparisons; sky, cockpit composition, input timing and palette presentation
are verified separately, not covered by that frame hash.

# Flight steering controls

`update_flight_controls` reconstructs the ordinary keyboard/mouse path at
`7AD6–7BC4` and its response helpers `7C04`, `7C50` and `7C73`. It produces the
bank and pitch drives consumed by the flight callbacks. Platform event capture,
joystick and VR orientation are separate integration work.

Each axis retains separate keyboard and mouse targets, the previous mouse
counter and the published midpoint reference. Arrow keys initially select a
force of 3510; Ctrl changes the retained force with the original frame step.
Opposite arrows with Ctrl reduce it towards zero. Opposite arrows without Ctrl
retain the stored target but filter towards zero for that update. Releasing
keys clears their target through the same midpoint filter.

Nonzero keyboard drive takes precedence over mouse processing. Mouse history
therefore remains pending while keyboard steering is active. Mouse input uses
wrapping accumulated counters, an inverted vertical counter and the original
sensitivity (default 12). Its decaying target preserves the asymmetric rounding
for negative values. Both sources publish a signed midpoint multiplied by the
frame step and shifted by eight; no floating-point smoothing is substituted.

`tools/generate_flight_controls_reference.py WORKSPACE` captures 1,024 updates
from the executable, covering held-key combinations, Ctrl, release, keyboard
handover, counter wrapping and sensitivities 1/12/25. Tests compare both output
drives and all eight persistent words. The fixture selects ordinary mouse mode
and suppresses the separate joystick fallback through its native input flags.

The application retains its inspection controls until collision handling and
frame ordering can support a coherent flight loop.

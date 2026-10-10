# Flight steering controls

`flight_controls_state::update` reconstructs the ordinary keyboard/mouse path at
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

The application now supplies GLFW relative mouse counters and held arrow/Ctrl
keys to this filter. The former free-camera controls have been removed. Host
mouse sensitivity still needs comparison with the original DOS input path.

## Underground large mouse sweeps

A reported difference remains under investigation: large captured-mouse sweeps
can pull the reconstruction into a tunnel wall more readily than retail under
DOSBox, particularly when travelling beyond where an uncaptured cursor would
have reached the window edge. No speculative steering-strength reduction has
been applied.

The tunnel fixture now has 3,072 native D510/D5C9 updates. The additional 1,024
feed sustained horizontal or vertical sweeps, holds and reversals through native
7AD6 before the tunnel callback, over frame steps 1/2/8/16. All reconstructed
motion fields match. This exercises D86B's 3072 steering-demand cap and its
asymmetric approach rate, rather than only the earlier ±2048 synthetic inputs.
It verifies controller arithmetic for supplied counters, not equivalence between
physical mouse motion on two host input stacks.

Inspection of the unmodified DOSBox 0.74-3 source archive
(`https://deb.debian.org/debian/pool/main/d/dosbox/dosbox_0.74-3.orig.tar.gz`)
shows another possible source of differences. `src/gui/sdlmain.cpp` passes
captured relative movement through DOSBox sensitivity; `src/ints/mouse.cpp`
applies its own sensitivity and mickey conversion. Its PS/2 path bypasses cursor
bounds, but `DoPS2Callback` reduces accumulated differences modulo 256 and marks
overflow. Darker's 1259 callback accumulates the delivered byte and sign without
recovering overflow distance. The reconstruction instead uses GLFW raw motion
where available. Packet loss/wrapping and host acceleration are therefore
plausible explanations for fast-sweep differences, not evidence of a
window-edge clamp in Darker's tunnel controller. Which input path and delivered
counts explain the observed playtest still needs a matched live trace. A slow
and fast sweep over the same physical distance would help distinguish speed-
dependent packet delivery from a position limit.

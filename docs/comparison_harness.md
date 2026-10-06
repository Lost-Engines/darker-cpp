# Proposed retail/reconstruction comparison harness

A side-by-side viewer would make whole-frame regressions much easier to find.
This is a proposed next testing tool, not an implemented facility.

Start with **recording retail and replaying C++**, rather than attempting to
keep two windowed processes aligned by wall-clock time. DOSBox cycles affect
how much emulated work finishes between timer interrupts; identical real-time
key events alone will not guarantee identical game updates.

A retail capture should record:

- Original executable/data identity, initial scenario/save state and random
  generator state.
- Each consumed simulation frame's clock, elapsed step and changed-clock mask.
- Keyboard state and mouse deltas at the original input-consumption boundary,
  plus raw injected events where device/input translation is under test.
- Selected object/player state, script cursors and relevant world-state changes.
- Indexed framebuffer and palette at completed presentation boundaries.

Replay the same consumed inputs and frame steps through the C++ game/platform
boundary. A development-only viewer can show both frames, a flickering overlay
and differing pixels, and locate the first state divergence. Compare the
original 320 × 240 indexed pixels before GLFW/DOSBox window scaling; compare
palette updates separately. Audio can initially remain outside the visual
comparison, with its own timestamped events and PCM checks later.

A later DOSBox debugger bridge could pause at those boundaries and permit
controlled stepping in both implementations. Presentation/menu loops have
their own scheduling and need separate capture points. Determinism of timer,
input, random state and any device-dependent reads must be established before
claiming that this is a fully synchronised lockstep harness.

The existing native x86 routine fixtures and HQ trajectory traces remain
useful: when a complete frame diverges, they help distinguish simulation,
projection, rasterisation, composition and palette errors. The hangar-door
rounding regression in [visual comparisons](visual_regressions.md) is an
example of the kind of discrepancy this broader harness should reveal early.

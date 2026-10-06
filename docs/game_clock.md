# Interrupt and frame clocks

`game_clock` reconstructs the time-accounting instructions at `0BEF–0C0E`
and frame consumption at `B0CE–B0E9`. The caller supplies timer interrupts;
this component has no window, renderer, audio or wall-clock dependency.

The interrupt adds the running flag's increment, subtracts the last consumed
frame time, caps that pending interval, then reconstructs the current word.
Gameplay sets the cap to **80 ticks** at `3D30`; leaving gameplay restores
`FFFF` at `3ECE`. Thus a slow frame can stop game-clock advancement rather than
accumulate an arbitrarily large physics step. The menu/presentation setting
allows normal word wrapping. Pause disables the increment, while retaining
the native cap operation.

Each interrupt ORs changed clock bits into a pending mask. Frame consumption
publishes the elapsed word, advances the consumed clock, increments a byte on
word carry and exchanges that pending mask into the frame snapshot. This
preserves timing information used by other systems rather than reducing the
clock to a floating-point seconds counter.

`advance_game_clock` can coalesce many interrupts, preserving all intermediate
bit transitions. Its first interrupt still applies the native cap even if a
caller has changed the limit or supplied an inconsistent starting state.
No catch-up physics substeps or host framerate policy are introduced here.

The fixture compares 1,024 native interrupt/frame sequences, including pause,
wrap, caps 0/1/80/FFFF, changed limits and pending transition masks. A separate
long-delay check compares batched advancement with individual interrupts over
more than two clock periods. Hardware port access and the interrupt's keyboard
polling tail are outside this component.

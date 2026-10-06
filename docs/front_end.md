# Original presentations and front end

The default Caero launch now passes through the original startup animation,
title image, game selection, pilot-name entry and first-mission briefing before
entering the hangar. All images, animation frames and fonts are read directly
from the original packs. Skimma development starts still enter flight directly.

`presentation::player` interprets the presentation portion of the scenario
bytecode: scene continuations, retained language cursors, background and inset
image loads, retained palettes, two animation tables, twelve animation pairs,
frame selection and record-scaled delays. It composites these into the indexed
320 × 240 software framebuffer. No exported PNGs or rendered videos are used.

Startup selects record 1 of 04/15 (native entry 9CBD), followed by title 00/14
at (16,65) and menu background 00/21. The four briefing pages come from record 0
of 04/0. They use the wide font, original backgrounds 00/22–24, inset images
03/12 and 03/9, and animation 01/2. After the fourth page, the retained English
text cursor is 1085, where the in-flight message interpreter resumes.

## Controls and current scope

Space, Enter or a click dismisses the startup/title and advances the briefing.
Select a slot using 1–4, arrows and Enter, or the mouse. Empty slots ask for a
pilot name. The run menu supports Enter to run, S to select another slot, E to
erase the name, and Escape to confirm quitting. Confirmation uses up/down and
Enter, or clicking Yes/No. Escape during flight returns to the run menu; Enter
after docking or a crash also returns there.

The four pilot slots are **session-only names**. They do not read or write
DARKER.SAV, retain campaign progress, or provide four independent game states.
Each starts mission one. Menu text uses original wording and fonts, but its
composition is provisional: exact borders, score fields and retail positioning
remain to be reproduced. Nightmare mode is not exposed yet.

Music-selection opcodes are decoded but music is not played. Palette fades,
original input-policy details, exact presentation tick/display ordering,
debriefing, death presentations and campaign progression remain outstanding.
The interpreter rejects unsupported opcodes rather than silently treating
unimplemented presentations as complete. It currently selects English.

## Evidence and verification

`tools/generate_presentation_reference.py` checks all 35 frames in startup 01/1
and briefing 01/2 against the original DF36 routine under Unicorn, including
VGA plane-mask writes. Its checked-in fingerprints are compared with the C++
decoder by `resource_check`. This establishes pixel decoding, not full native
presentation timing or exact menu composition.

The same integration check executes the startup to completion and all four
briefing pages, draws before and after animation advancement, and verifies the
post-briefing message cursor. A windowed Xvfb/Mesa check exercised name entry,
briefing, mouse capture on entry to flight, an eight-second hands-off launch,
and return to the menu followed by confirmed quit.

Native presentation references: BFA9 (dispatch), C003 (background/wide font),
C07A (image/palette), D938 (presentation timing), DAA7 (animation pairs),
DADA (frame tables), DF36 (scanline animation drawing). See the parent analysis
repository's `docs/presentations-and-interface.md` for the wider opcode table.

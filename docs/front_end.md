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
erase the slot, and Escape to confirm quitting. Confirmation uses up/down and
Enter, or clicking Yes/No. Escape during flight returns to the run menu; Enter
after docking also returns there. Enter after a Caero crash opens the original
Kismet committal presentation; Enter, Space, Escape or its back button then
returns to the run menu. Escape directly from flight still leaves immediately.

The four pilot slots now persist in `darker-cpp.sav` in the current working
directory. Creating a named pilot or confirming erasure writes the file through
a temporary sibling and rename. It uses the original 6,600-byte format,
CRC-16/XMODEM checksum, four records and two-byte Nightmare trailer. The typed
codec preserves both packed city streams, weapon/return-site fields, opaque
record tails and trailer bytes. A corrupt or wrongly sized file is reported,
never silently reset. The retail `DARKER.SAV` is not automatically read or written.

This does **not yet save mission completion**: playable slots still start stage
one. If a later-stage retail record is copied into the reconstruction save, its
stage is displayed but Run reports that the stage is not implemented. Other
slots remain usable and the unsupported record is preserved. Campaign state
commit, city restoration and stage selection are the next integration work.

Menu text uses original wording and fonts, but its
composition is provisional: exact borders, score fields and retail positioning
remain to be reproduced. Nightmare mode is not exposed yet.

Music-selection opcodes are decoded but music is not played. Palette fades,
original input-policy details, exact presentation tick/display ordering,
debriefing and campaign progression remain outstanding.
The interpreter rejects unsupported opcodes rather than silently treating
unimplemented presentations as complete. It currently selects English.

## Evidence and verification

`tools/generate_presentation_reference.py` checks all 47 frames in committal 01/0, startup 01/1
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

## Screenshot comparison corrections

The second quotation page starts with a newline and inherits the first page's
margin 8 and line step 11. DA2F reads the persistent B2BD/B2C1 formatter state;
resetting it to margin 0/step 16 clipped the final attribution. Scene changes
now retain those two fields while resetting the text origin and colour as the
original wrapper does.

BFD3 writes image Y into the immediate at DABA. Animation frame Y therefore
adds the scene's image Y, including 24 for the first briefing portrait. Earlier
standalone frame decoding correctly recovered pixels, but omitted this caller
adjustment during composition.

DA48 draws the navigation controls using glyphs 60 and 62 of the current font,
at (287,226) and (305,226). Input-policy bits 4 and 1 select their visibility.
The left control exits to the run menu; the right advances. Exact hover colours
and the remaining original input policies are still outstanding.

## Mission death and save verification

Native exit code 3F15–3F3E forwards outcome 2 to D8D6, which selects record 2
of 04/15. This uses still 03/7 and animation 01/0. It shows the committal text
and repeats its animation sequence; opcode 1A saves a checkpoint, and 1F/FF
rewinds there with an eight-interval delay while completed counter C21B is less
than 255. This is a continuing presentation, not a movie that ends automatically.
The presentation player now implements that checkpoint/counter wait. The host
supplies the completed-object counter before resetting the failed mission.

The underground abort screen is record 3; Nightmare victory is record 4.
Neither is substituted for ordinary Escape or successful first-mission docking.
These will connect when their corresponding gameplay outcomes exist.

A windowed Xvfb/Mesa check created a pilot, restarted the executable and reloaded
it, launched with the engine off to trigger a crash, ran the committal scene for
31 seconds, returned to the menu and erased the slot. Its saved checksum was
independently checked with Python; death left the save byte-for-byte unchanged.

Tests cover 47 native-verified animation frames, two minutes of committal script
advancement, and dismissal back to the menu. Save tests exercise original field
offsets, opaque-byte retention, CRC, malformed lengths and corruption. The
patterned 6,598-byte test payload `(offset * 37 + 11) & 255` produces CRC 7584;
the original A110 writer produces the identical file and 0364 accepts it under
the existing parent `tools/verify_save_file.py::Save` harness. These tests
intercept DOS I/O and leave the retail save untouched.

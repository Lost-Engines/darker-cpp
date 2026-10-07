# Original presentations and front end

The default Caero launch now passes through the original startup animation,
title image, game selection, pilot-name entry and first-mission briefing before
entering the hangar. The first seven campaign missions now run consecutively. All images, animation frames and fonts are read directly
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
after docking is no longer required: a completed docking saves progress and
opens the next supported briefing automatically. Enter after a Caero crash opens the original
Kismet committal presentation; Enter, Space, Escape or its back button then
returns to the run menu. Escape directly from flight still leaves immediately.

The four pilot slots now persist in `darker-cpp.sav` in the current working
directory. Creating a named pilot or confirming erasure writes the file through
a temporary sibling and rename. It uses the original 6,600-byte format,
CRC-16/XMODEM checksum, four records and two-byte Nightmare trailer. The typed
codec preserves both packed city streams, weapon/return-site fields, opaque
record tails and trailer bytes. A corrupt or wrongly sized file is reported,
never silently reset. The retail `DARKER.SAV` is not automatically read or written.

Successful docking now commits the Delphi city stream, current weapon mask and
return site, increments the saved stage and opens the next briefing. The first
three missions are connected. Completing the third saves stage four and returns
to the menu with an explicit unsupported-stage notice; the next mission needs
ground-vehicle logic. Loading stages beyond three preserves the record but
does not launch it. Death and Escape leave the previous committed record intact.

Each flight loads a fresh city, restores saved bits for stages after one, and
recreates actors/scripts from its own scenario record. BB90/BBC6 packing and
restoration match native fingerprints for both Delphi and Halon, including
high-bit beacon templates and variant rebuilding. Stage one bypasses the whole
BBC6 path, including variant rebuilding. Only Delphi is connected to campaign
play at this milestone. All three supported missions have empty beacon queues;
queued-outage exit handling remains necessary for later missions.

Briefing opcode 30 now retains weapon-range toggles for the flight session;
the first briefing grants Pinner Direct. Save commits retain that runtime mask.

Menu text uses original wording and fonts, but its
composition is provisional: exact borders, score fields and retail positioning
remain to be reproduced. Nightmare mode is not exposed yet.

Original Sound Blaster [music playback](sound_images_music.md) is connected. Palette fades,
original input-policy details, exact presentation tick/display ordering,
later campaign records and their scripted events remain outstanding.
The interpreter rejects unsupported opcodes rather than silently treating
unimplemented presentations as complete. It currently selects English.

## Evidence and verification

`tools/generate_presentation_reference.py` checks all 113 frames in committal 01/0, startup 01/1,
briefings 01/2, 01/3, 01/5 and cutscene 03/2 against the original DF36 routine under Unicorn, including
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

## Campaign verification

`tools/generate_city_persistence_reference.py` executes native BB90/BBC6 against
both real maps/banks and emits full-stream and full-map fingerprints. Tests
compare source patterns with all four state codes, beacon templates, the first
stage bypass and later-stage restoration. No DOS save is modified.

Controlled combat now completes all three supported missions with their own
actors, source briefings, message cursors and original completion messages,
then performs automatic docking. A separate windowed GDB fixture injects the
completion outcome at the host boundary to exercise actual slot commits and
transitions 1→2→3→4; it verifies CRC, Pinner availability and return-site bytes.
This boundary test does not claim an unaided interactive playthrough.

Mission two's second scene disables input and finishes automatically. C06E
(opcode 4C) toggles its repeat flag, rewinds itself once and delays by its operand
multiplied by the record interval. The player implements this delay, and the
front end honours no-input scene completion. Framebuffer transition/fade details
remain outside the pixel-decoder checks.

## Level X hidden command

Game selection recognises the original asterisk/three sequence (0751–078D).
Shift+8 followed by number-row 3 opens the separate STAR THREE editor. Exact
`Level X` submission enables the process-wide B92A patch equivalent and replaces
the selection prompt with the accepted phrase. Case changes, trailing characters
and entering the phrase as an ordinary pilot name do not enable it. The prompt
returns to game selection after submission or cancellation and never writes a
pilot record. Only Level X is connected; the other three hidden commands remain
future work.

X during live, non-dying flight follows B926: copy the retained C610 destination
into the C81E return site and request outcome 1. The existing successful exit
commits city state and weapons, advances the selected pilot, writes the save and
opens the next supported briefing. This deliberately does not mark remaining
actors destroyed or run docking. `hangar_state::next_return_site` names C610;
the first four supported mission scripts leave it at its initial zero value.
Future scripted destination changes and Skimma supply-pad landings must update
both destination fields as C23B/C764 do. Existing zero-site setup currently uses
the supported Caero HQ start; later return-site setup is not established here.

The resource integration check covers exact matching, ordinary-name isolation,
unchanged save bytes and persistence through death. A window test uses actual
Shift+8, number-row 3, typed text and X events, checks X is inactive beforehand,
and advances 1→2→3→saved 4 without debugger injection. Save checksum, weapons and
copied destination are checked. The unsupported-stage gate now begins at mission eight; mission four includes its original reinforcement waves.

## Background transitions and text lifetime

Both background opcode 3B and clear-screen opcode 3D call BFE4, which clears the
rendered-text pointer DA30. The player now clears its glyph list at those commands
while retaining text cursor and paragraph layout state. This prevents mission
two's briefing from remaining over its subsequent launch clip. The regression
check observes the original 3D/22 initial blank interval and confirms no briefing
pixels remain and no in-flight message text is consumed.

## Animation table reset

Opcodes 46/47 call DADA, which resets the two append descriptors but retains the existing frame-pointer entries and animation pairs. The presentation player now retains decoded frames across this reset and overwrites them as subsequent resources load. Mission six has a timed interval between reset and reload, so discarding all frames immediately left a live pair with no image to draw. An isolated native DADA call confirms that only the three-byte descriptors change, while later entries and all twelve pairs remain untouched. Briefing checks now draw at 25-tick intervals through missions two to seven, rather than observing only their final frames. Exact aliasing of overwritten packed animation memory remains a separate issue if a later scene deliberately reuses stale pointers into replaced data.

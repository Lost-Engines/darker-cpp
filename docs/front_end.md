# Original presentations and front end

The default Caero launch now passes through the original startup animation,
title image, game selection, pilot-name entry and first-mission briefing before
entering the hangar. The campaign interpreter now covers all 116 stages, including the Halon sequence and ending; see `campaign_status.md` for the distinction between controlled checks and playtesting. All images, animation frames and fonts are read directly
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
Select a slot using 1–4, arrows and Enter, or the mouse; N selects Nightmare. Empty slots ask for a
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

Successful docking commits the city state, weapon mask and return site, then
advances the selected pilot into the next briefing. The connected campaign runs
through stage 116, including Halon and its intervening presentations. Death and
Escape retain the previous committed campaign record. Each flight restores its
city and actors from the relevant scenario; queued beacon changes and retained
city state are covered in `campaign_status.md`.

Briefing opcode 30 now retains weapon-range toggles for the flight session;
the first briefing grants Pinner Direct and the fifth grants Pinner Mimic. Save commits retain that runtime mask.

Menu composition now follows the native row positions, palette shading, score
formatting and original interface font. Nightmare is available through N, with
its independent high-score erase action. Original Sound Blaster
[music playback](sound_images_music.md) is connected. The interpreter rejects
unsupported opcodes and currently selects English. Remaining limits include
some input-policy details and exact presentation tick/display ordering.

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
The left control exits to the run menu; the right advances. DA75 hover regions now select the FD/FC colour pair, using row 225 and the
284/302 column boundaries. Other original input-policy details remain outstanding.

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
Both are now connected to their corresponding gameplay outcomes.

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
pilot record. Only Level X is recognised by the hidden text editor. The other three cheats
are available through the command-line switches documented in the README.

X during live, non-dying flight follows B926: copy the retained C610 destination
into the C81E return site and request outcome 1. The existing successful exit
commits city state and weapons, advances the selected pilot, writes the save and
opens the next supported briefing. This deliberately does not mark remaining
actors destroyed or run docking. `hangar_state::next_return_site` names C610;
the first eight supported mission scripts leave it at its initial zero value.
Future scripted destination changes and Skimma supply-pad landings must update
both destination fields as C23B/C764 do. Existing zero-site setup currently uses
the supported Caero HQ start; later return-site setup is not established here.

The resource integration check covers exact matching, ordinary-name isolation,
unchanged save bytes and persistence through death. A window test uses actual
Shift+8, number-row 3, typed text and X events, checks X is inactive beforehand,
and advances 1→2→3→saved 4 without debugger injection. Save checksum, weapons and
copied destination are checked. All campaign stages now have script coverage; mission four includes its original reinforcement waves.

## Background transitions and text lifetime

Both background opcode 3B and clear-screen opcode 3D call BFE4, which clears the
rendered-text pointer DA30. The player now clears its glyph list at those commands
while retaining text cursor and paragraph layout state. This prevents mission
two's briefing from remaining over its subsequent launch clip. The regression
check observes the original 3D/22 initial blank interval and confirms no briefing
pixels remain and no in-flight message text is consumed.

## Animation table reset

Opcodes 46/47 call DADA, which resets the two append descriptors but retains the existing frame-pointer entries and animation pairs. The presentation player now retains decoded frames across this reset and overwrites them as subsequent resources load. Mission six has a timed interval between reset and reload, so discarding all frames immediately left a live pair with no image to draw. An isolated native DADA call confirms that only the three-byte descriptors change, while later entries and all twelve pairs remain untouched. Briefing checks now draw at 25-tick intervals through missions two to seven, rather than observing only their final frames. Exact aliasing of overwritten packed animation memory remains a separate issue if a later scene deliberately reuses stale pointers into replaced data.

## Nightmare

N selects the original fifth menu entry (native key table 054F). It uses record
0 of 04/15 and a transient pilot, rather than treating the two-byte save trailer
as a fifth pilot record. Native 3F23 updates only trailer byte 0 when the wrapping
byte score exceeds the existing best. Normal pilots and trailer byte 1 remain
untouched. Leaving, dying or skipping returns to the challenge menu; outcome 4
plays its dedicated victory presentation.

The launch uses site 4258, heading C4 and the existing Nightmare inline player
setup: an airborne downward launch near Kismet Square, rather than a hangar.
Briefing and mission opcodes 36/37 set difficulty and score checkpoints. Opcode
38 toggles the independent scripted altitude-hold bit; it consumes a height
word only when enabling it, and does not cancel the player's manual hold bit.

The integration check runs the entire original challenge with live actor and
beacon updates and controlled removal of counted objectives: outcome 4 at tick
162456, 15 reserve activations, 50 removals, 24 score checkpoints and final score
100. This verifies script progression, not the combat difficulty. Menu checks
cover all five exit outcomes, best-score updates and byte-for-byte preservation
of all four pilot records. An Xvfb/Mesa run enters through N, advances into live
flight, fires, exits with Level X and verifies the ordinary save is unchanged.

## Native menu panels, title fade and credits

0849/0850 place selection rows at Y=20,56,92,128,164; the selected game's run
panel uses Y=60. The row begins with Game at X=112 and Level at X=162, followed
by a centred pilot name. Empty slots use the original centred new-game label.
Nightmare's two percentage scores use the formatter's tab and runtime-number
operations, so their spacing follows the original glyph widths.

08E9 copies rectangles with VGA XOR bit mask 80. The corresponding upper
palette is reconstructed by DF1D/AFB1: copy entries 0..127, use component gains
24/25/26 and DAC offsets 8/9/9. This produces the shaded panels seen in the
existing unmodified DOSBox captures, rather than black rectangles or an invented
RGB transparency effect. Run, erase, confirmation and name-entry layouts use
the native coordinates; Y/N confirmation and Nightmare high-score erasure are
connected. The latter changes only the score byte and save checksum.

The title includes the compact-font TM overlay. AFA2 derives brightness from the
sine table, and AFAD maps each source component into six-bit DAC values. The
9CFE title path uses CX=0803: 1,024 ticks to full brightness, followed by a
4,000-tick interruptible hold. The credits page then fades in over 128 ticks and
waits for input. Its 80×17 logo comes directly from 00/28 at (120,48); its English
text reproduces the executable's formatted block at 9D80. `--skip-intro` bypasses
all these startup pages and still enters game selection immediately.

Native fixtures cover all 512 fade phases and all 256 component values at all
65 brightness coefficients. Windowed captures exercise the title fade, credits,
selection, high-score erase menu, Nightmare entry and external camera controls.
The earlier DOSBox captures confirm the selection and credits composition;
this is not a claim that all menu pixels or display timings have been compared.
Ordinary briefing transitions are not given an invented universal fade.

## Continuous entry versus loading a mission

Presentation opcode `2C` (`C2BC–C2E3`) branches on the preceding outcome at
`3E9A`. A fresh run reads past the language displacement word and shows the
briefing. With a nonzero previous outcome, it follows that displacement to the
in-flight text, clears input policy and stops the presentation immediately.

The front end now supplies this distinction when continuing the campaign,
including across presentation-only records. Earlier setup opcodes still execute:
level 17 retains its `28 80 64 30` entry at the eastern Comms HQ hangar. Docking
there in level 16 therefore enters the tunnels without a briefing screen.
Loading the saved tunnel level, or using `--level=17`, still shows its briefing.
This uses the script's conditional branch rather than a special case for level 17.

### Campaign-wide entry-path audit

All 116 campaign records were checked for the same conditional presentation.
There are 18 occurrences, all reached before drawing a briefing page. These are
campaign stage numbers, matching `--level`, and archive record indices are zero-based.

| Stage | Record | Role | Setup retained before the conditional |
| --- | --- | --- | --- |
| 17 | 04/002, 0 | Tunnel entry | Entry site 3064, heading 80 |
| 18 | 04/002, 1 | Surface return | Return destination 7162 |
| 24 | 04/002, 7 | Tunnel entry | Entry site 3F64, heading 80 |
| 25 | 04/003, 0 | Surface return | Return destination 7162 |
| 36 | 04/004, 3 | Tunnel entry | Entry site 3060, heading 80 |
| 37 | 04/004, 4 | Surface return | Return destination 7162 |
| 46 | 04/005, 5 | Tunnel entry | Entry site 1784, heading 80 |
| 47 | 04/005, 6 | Surface return | Return destination 7162 |
| 55 | 04/006, 6 | Tunnel entry | Entry site 1088, heading 80 |
| 56 | 04/006, 7 | Surface return | Return destination 7162 |
| 67 | 04/008, 2 | Tunnel entry | Entry site 4362, heading 80 |
| 68 | 04/008, 3 | Surface return | Return destination 7162 |
| 86 | 04/010, 5 | Tunnel entry | Entry site 693A, heading 80 |
| 87 | 04/010, 6 | Surface return | Return destination 7162 |
| 92 | 04/011, 3 | Tunnel entry | Entry site 693E, heading 80 |
| 93 | 04/011, 4 | Surface return | Return destination 7162 |
| 99 | 04/012, 2 | First Halon Skimma stage | Weapon toggles A9 and 21; entry site 16FC, heading 80 |
| 115 | 04/014, 2 | Final Delphi battle | Entry site 4258, heading C4 |

Sites, headings and opcode operands in this table are hexadecimal. Opcode 28
sets an entry pose and departure destination; opcode 29 sets only the latter.
In particular, surface returns do not gain an invented new entry pose when their
briefings are skipped. Stage 99's weapon changes happen before the branch, so
skipping its text must still apply them.

For all 18 stages, successful continuation omits the load-only briefing;
fresh entry from the run menu or `--level` shows it. The preceding presentation-only
stages 98 and 114 still play: their completion then skips the additional load-only
pages in 99 and 115. The ending at 116 has no such branch and is not skipped.

This follows native control flow as well as the script census. Successful exit
at 3EC8 advances the stage and returns to setup at 3C14 with the previous outcome
intact. The presentation runs before 3CBB clears that outcome. Fresh entry at
3C0F clears it first. C2BC is the presentation reader that makes the distinction;
this audit does not imply that loading and continuing have identical persistent
world state, equipment or player state.

The resource integration check constructs both entry paths for all 116 stages.
For each conditional stage it also completes the fresh-load presentation and
compares the resulting weapon toggles, entry pose, destination, difficulty and
score with the immediate continuation. The continuation text cursor is checked
against the stored displacement, rather than assumed equal to the cursor after
reading the briefing: stage 17 has displacement zero and no in-flight messages,
so its skipped cursor remains zero while reading the briefing consumes 269 bytes.
This guards against both losing setup and accidentally consuming mission messages. The
existing interstitial checks cover progression through all nine Halon story
records. These are script/engine checks; only the first tunnel transition has
also been checked live, and this is not a claim of a retail playthrough of every
listed transition.

## Underground cockpit differences

The native 571E branch skips altitude and coordinate updates for player
configuration 4. The left altitude arc therefore stays off and the small
coordinate fields retain the cockpit bitmap's dashes. The host now preserves
those defaults instead of displaying surface beacon-grid coordinates.

Normal underground radar takes the 586F/5876 branch: aircraft, ground vehicles
and both projectile lists share the grey 5C04 ramp, palette index
`22 - (squared pixel radius >> 5)`. Surface aircraft and ground contacts retain
their separate 249/242 bases. The common projection and clipping are unchanged.
B8C1 rejects the enlarged-radar command underground; Insert/keypad 0 now respect
that gate. A level-17 window capture checks the grey contacts, unlit arc and
dashed coordinates; unit checks cover the grey ramp over the existing native
radar projection fixtures. See `flight_cameras.md` for the separate underground
following/death-camera branch.

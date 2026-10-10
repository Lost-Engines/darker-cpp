# Player cameras and visible craft

F1–F6 now select the original attached and dropped player-camera modes:

- F1: cockpit; F4: full-screen inside, both hiding the player model.
- F2: following, retaining bank; F3: following with a level camera.
- F5: drop at the player's whole-word position and track the moving craft.
- F6: drop at the player's position with its current heading/pitch, keeping that
  camera fixed. Released look offsets remain in this mode.
- Comma/period: six following distances, using native table 24BB.
- Tab: redirect the steering device into look offsets, without steering the
  craft. Starting from the cockpit temporarily selects the following view.
  Releasing Tab recentres with the maximum-axis vector reduction at 7CA4,
  returning to the cockpit when the offsets reach zero. F4 keeps its inside-craft
  viewpoint while looking: native 78FE–7900 increments camera mode only when
  the selected mode is zero and the look flag is one.

`game::flight_camera` translates ordinary paths in 2409–2680, including angle
rounding, following-distance smoothing, byte-fraction position carries, the
camera-ground adjustment and the landed following offset. Captured native
comparisons cover 512 attached-camera cases, 256 dropped cameras and 1,024
look/return cases. The camera module has no GLFW dependency.

`graphics::orient_model` translates 1E9E's separate fixed-point products and
camera composition. Its nine coefficients agree in 512 random native cases.
The current player uses the original bank's special-model slot 25, 26 or 27.
Objects enter the same ordered draw list as city geometry, with their own
attitude axes and fractional origins. The common near/direct extent cull is
shared rather than duplicated. Background records retain their separate order.
Existing city-only native frame comparisons remain unchanged.

The world viewport is the original 240 rows outside the cockpit. Sky/ground
reference coverage now includes that height. Camera-mode changes do not alter
software projection or expand the radius. Window smoke checks exercised all
six modes, distance changes, banked flight and Tab look/recentring on Caero and
Skimma. These are reconstruction checks, not a claim of complete campaign
camera coverage.

[Missile-follow modes](pinner_mimic.md) now use the native live/impact distance tables. F7/backtick object selection is connected; further camera-transition comparisons
remain useful. Explosion/death cameras are connected,
including the underground exception below. Actor and projectile drawing now
consumes the native lifecycle fade byte at object +6F: the renderer advances BP
by 0E at 2EC8, then reads +61 at 2CA1/2D22. This is lifecycle brightness, not
regional beacon strength. The exterior player retains full strength. Distant-point rendering now follows the model thresholds, as described below.

City candidate scans wrap row and column arithmetic independently at 256 cells,
then reject the empty half outside the 128-by-128 city. This lets buildings
appear across the coordinate seam before the camera crosses it, in both Delphi
and Halon. The native comparison now covers 768 scans, including outside-map
centres, both seams and their corner, without changing the original view radius.

## Missile-follow indicator

The M toggle also draws the original green marker: a 4×3 rectangle at (300,0),
palette index BEh, after cockpit composition. Native `54A3–54EE` draws it steadily
in missile camera modes (7 and above). In ordinary views it requires the enabled
flag at `54CA` and nonzero clock bits `0180h`: 128 ticks off, then 384 ticks on,
repeating every 512 ticks (roughly one second). No rectangle is drawn during the
off phase; the newly rendered scene remains visible underneath.

Isolated execution of `54A3` confirmed the rectangle arguments and timer boundaries
for modes 0, 3, 6, 7 and 8, with the toggle both off and on. Regression checks cover
those timer boundaries, steady following and unchanged surrounding pixels.

## Underground attached views

Native 24A7 tests player configuration F003 against 4. Underground behind/level
views jump to 24E6, the full-screen pose path, before selecting a following
distance. This preserves the player's position and roll and does not update
255D. Death still selects level view and seeds distance 0205, but underground
that distance is not used to displace the camera. This is not a reduced zoom
radius or a collision clamp against tunnel walls. Dropped and missile views
retain their separate branches.

An isolated original-code check of 128 poses confirmed identical position,
fractions, angles and retained distance for underground behind/level and the
ordinary full-screen path. The C++ regression checks reuse native full-screen
samples for both underground modes. Native 2BD0 also omits the separately drawn
player craft underground; the host follows that restriction.

## Distant objects and lifecycle brightness

`2EF8` selects a single point when the projected depth minus 32 reaches the
model header's point threshold, after the flat-shading threshold. This is only
a moving-object path; city buildings retain their geometry. The point uses
header byte +5 and the shade table retained from the last mesh draw, rather
than applying its own distance/fade lookup again. Mesh draws use lifecycle
brightness from +6F, including aircraft fade-in and projectile fade-out.

512 native `2D32` comparisons cover all 256 incoming low-byte values, including
rounding boundaries, projection and viewport rejection. The draw traversal at
`2C38` supplies zero after a farther-child descent, or the current draw-record
address after returning from its farther subtree. The renderer reconstructs
that tree property using native object identities, then preserves projected Y's
low byte for the X division. An original-code tree insertion/traversal fixture
also checks the register value at each callback.

Insertion follows the native city, particle, player, air, static, ground and
projectile order before distance sorting. Equal-distance entries retain insertion
order. The shade latch persists between renderer calls; before any mesh draw
its table starts at zero.

## Pointed-object camera (F7 / backtick)

`257D` casts a camera-space ray through the existing city collision sweep, then
visits the player, ground vehicles, static objects and aircraft in native list
order. It retains the last intersecting object; this is not a nearest-hit pick.
The previously selected object is excluded, even after switching back to F1–F4.
F5/F6 clear that selection and take their drop origin from the player, including
when missile viewing is enabled. Both the existing camera and the new
selection must pass `841C`'s wrapping horizontal-distance check. Landed craft
and underground configuration reject the action.

A moving-object selection follows that object using mode 6. Its live distance
table is shared with ordinary following views; after flag 08h is set it uses
10, 11, 13, 18, 29 or 34 units. Removal clears the reference before the record
can be reused, as at `7AA2`. With no selected record, mode 6 reuses the stored
fixed anchor while retaining its own look-recentring behaviour. Selecting a building places a fixed camera at its
model origin, at `D089`'s height plus four model extents, facing the previously
watched object or player. Empty terrain alone does not select a camera.

512 native comparisons cover the F7 ray and asymmetric range arithmetic, and
512 more cover live/destroyed object camera positions and distance smoothing.
Original-pack integration checks exercise category ordering, exclusion, the
player as a target, a beacon camera and reference retirement. These do not yet
establish every combination of F7, missile viewing and Tab against live retail.

## Enlarged radar across camera modes

Insert/keypad 0 displays the enlarged Caero radar in F4 and external views as
well as the cockpit. Native 54F0–55BE checks the craft type and held key, but
does not require cockpit camera mode; nonzero camera modes branch directly to
the enlarged-radar check at 55A5. Underground flight remains excluded. The
overlay is composed after the scene and cockpit, independently of cockpit
visibility.

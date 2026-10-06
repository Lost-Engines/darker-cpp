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
  returning to the cockpit when the offsets reach zero.

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

Remaining camera work includes F7's object selection, missile-follow modes,
original explosion/death-camera transitions and external actor lighting updates.
The exterior player currently receives full beacon strength; actor light updates
will replace this with the original per-object field. Far actor point rendering
is also pending, so a dropped camera still uses the mesh until extent culling.

City candidate scans wrap row and column arithmetic independently at 256 cells,
then reject the empty half outside the 128-by-128 city. This lets buildings
appear across the coordinate seam before the camera crosses it, in both Delphi
and Halon. The native comparison now covers 768 scans, including outside-map
centres, both seams and their corner, without changing the original view radius.

# Warehouse aircraft spawning

Native 8EEE prepares Delphi launch cells 0355, 0B70, 1B33, 3754, 3757,
5E6A, 6612 and 7052. It increments their state bytes and reserves animation
parameter zero for HQ; warehouse parameters one through eight become -1 when
the resulting cell state has bit 40 set.

8E3F processes cells with either C0 bit and without bit 20. Timer indices advance
only for eligible cells. Wrapped byte Manhattan distance has a minimum of four;
its value limits the countdown and scales the random replacement interval. An
expired timer admits an aircraft only within sixteen cells, with spawning enabled
and an airborne free-list record available. Scenario opcode 31 changes that
permission. Timers survive scenario setup; admission is re-enabled on entry.

Delphi helper 8E00 places the aircraft at cell fractions (128,248), height
100 minus model height, heading 8000 and speed 100. It retains the record's
identity, fractional position, definition and script checkpoint, resets steering
and awareness, and installs callback 8DDD with flags 50. Movement precedes steering
towards pitch 0C00 during departure. After 1024 ticks, callback 8823 takes over
and protection bit 10 clears.

The shared native 79E5 deadline path now handles actor fade-in and expiry as well
as projectile expiry. Eligible airborne records return to the free pool after
removal, decrementing finite reuse counts by two; FE remains unlimited. Rendering
of actor fade brightness is not yet connected. Halon's different launch helper
8E10 and its connected campaign remain outstanding.

## Verification

`generate_aircraft_spawning_reference.py` executes the original code for 384
admission/timer cases, 32 warehouse preparation cases and 128 consecutive
protected-departure updates. An original-pack mission-twenty test launches an
actual free aircraft, follows its transition to ordinary AI, retires it using
its original script, and launches the same identity again. The controlled
mission-twenty combat test also runs the spawner while checking objectives and
docking. These establish the exercised paths, not a full native campaign trace.

## Halon sites and departure

C178/35 supplies the site list and marks each referenced cell's high state
bit. The list replaces the fixed Delphi warehouse list; it does not clear
other cell state or existing timer values. Timer storage now grows with the
list: one original Halon script contains nine sites.

Halon launches at the cell centre, nominal height 1160 minus model height,
speed 300 and pitch 2C00. The first departure heading is 4000; each admitted
launch advances it by 2800 with word wrapping. Admission, timer updates,
random sequencing and the common protected departure callback otherwise
share the existing path. There are 384 additional native comparisons using
bank 31, including refused launches and heading wrap. Connected campaign
startup must select the Halon list before its first spawning update.

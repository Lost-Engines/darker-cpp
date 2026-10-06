# Caero weapon firing

`fire_pinner_direct` composes original C9C2/CAC0 with the existing CB01 projectile
allocator. The first mission's direct weapon costs **6,399 reserve units**
(`18FFh`), rather than a rounded 24-unit high-byte amount. A successful shot has
an original lifetime of 1,024 clock ticks. Both come from the executable-resident
object definition; they are not tuning constants introduced by the reconstruction.

The original guard refuses firing while landed or crashing, when the twelve-slot
projectile pool is exhausted, or when reserve is insufficient. A valid weapon
remains ready without a trigger edge; holding a button does not itself generate
new edges. Reserve is spent only on an admitted launch. Boost reserve and the
internal engine buffer are separate and are not spent by this weapon.

120 native comparisons cover exact cost boundaries, player flags, trigger edges
and free-list exhaustion. The probe intercepts CB01 after the guards and energy
accounting, capturing its lifetime argument. Actual allocation, placement and
movement retain their separate native comparisons. Selection, input edges,
projectile hit processing and presentation must be connected before this weapon
is exposed in the playable application.

```sh
PYTHONPATH=/tmp/darker-python python3 tools/generate_caero_weapon_reference.py ..
```

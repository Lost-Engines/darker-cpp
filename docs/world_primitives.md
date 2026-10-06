# Object lists and random sequence

The first shared world primitives live in `src/game`. They have no rendering, window, audio or resource dependencies. They are not yet a running world or a complete projectile constructor.

## Intrusive object lists

`object_list<Record>` retains active head, active tail and free-list head, matching the header words at native offsets +0, +2 and +4. Records supply typed `next` and `previous` pointers. This replaces segment offsets with ordinary C++ pointers while retaining stable identity and native ordering. Records must remain at fixed addresses for the lifetime of their links; ownership and pool construction belong to the caller.

| Function | Original routine | Behaviour |
| --- | --- | --- |
| `allocate_object` | `1C95` | Pop the free head, prepend to active head, repair previous/tail; return null on exhaustion without changing links. |
| `allocate_or_reuse_object` | `1CBF` | Try allocation first; on exhaustion detach the active tail and prepend it to the active head. |
| `unlink_object` | `7AB4` | Repair active neighbours/head/tail without adding the record to the free list; return the old next record for list traversal. |
| `recycle_object` | `1CD4` | Unlink and prepend the removed record to the free list, returning the next active record. |

These operations do not clear payloads. Free records retain stale previous links, which allocation overwrites. Unlinking alone leaves the removed record's links untouched. Callers must supply an active member to either removal function and must not insert the same record into multiple lists. There is no ownership scan in these low-level operations.

The native tail-reuse fallback assumes at least two active records. The C++ implementation reports a logic error if that precondition is violated instead of dereferencing the native equivalent of a null record. The ordinary allocation path has no such minimum.

Projectile launch `CB01` calls `1C95`, **not** `1CBF`; therefore the existence of tail reuse does not establish that firing evicts the oldest projectile. The reused-payload contract also means the subsequent definition expansion and constructor writes must be translated separately. The original fixed-pool setup at `1CFB–1D62` and the full 112-byte runtime object are not implemented here.

## Original random generator

`next_random` translates `92D2–92E5`, mutating a caller-owned 16-bit state and returning the same new value. It wraps `state + 1`, multiplies by 75, subtracts the multiplication's high word from its low word, then adds the subtraction borrow. When the increment wraps to zero, the multiply is skipped and the high operand stays 75. This special case is retained.

No standard random engine is substituted. Initial seeding and the order of calls across weapons, collision, effects and other systems remain to be connected; matching the recurrence alone does not guarantee a matching gameplay sequence. The function has no hidden global state.

## Native verification

`tools/generate_world_reference.py` runs the original routines from the hash-checked unpacked executable. It generates test-only fixtures; production code contains no captured results.

```sh
PYTHONPATH=/tmp/darker-python python3 tools/generate_world_reference.py ..
```

The 56 list fixtures cover active lengths zero through six, every active removal position, free-list exhaustion and tail reuse. Tests compare returned identity, header links, and every record's next/previous link, including stale free links. C++ tests also check that payload values survive these operations.

The random check executes **all 65,536 input states**, checking both native return value and stored state, then compares the C++ sequence using checksums grouped by input high byte. All **47 CTest cases** pass. No application controls or additional deliverable executable were introduced.

## Typed definitions and parameter expansion

`object_definitions.h` contains the 33 original 24-byte records, generated from the hash-checked executable. The C++ representation separates angular/motion seeds, impact and speed parameters, eight role-dependent bytes, and sound parameters. Callback entries remain native identifiers for future dispatch translation; they are not machine-code pointers called by C++. The eight role-dependent bytes are deliberately not assigned universal names because their meanings differ between craft and projectiles.

`apply_object_definition` translates `BF41–BF6E`. It writes the following subset, represented by `object_parameters` rather than an opaque 112-byte memory image:

| Native runtime offset | C++ field | Source |
| --- | --- | --- |
| 44h | `definition` | Non-owning pointer to the supplied typed definition |
| 22h | `model_token` | Explicit geometry-bank binding |
| 66h | `update_entry` | Definition +2 word |
| 4Ch | `flags_4c` | C0C0h |
| 30h | `angular_response` | Definition +4 unsigned byte × 8 |
| 32h | `motion[0]` | Definition +5 unsigned byte × 256 |
| 34h | `motion[1]` | Definition +6 unsigned byte × 64 |
| 36h | `motion[2]` | Definition +7 unsigned byte × 128 |

The definition must outlive the resulting parameters; the generated static array provides stable storage. Definitions retain their original zero model words. The original loader patched those words per geometry bank, so the C++ function instead accepts a separate model binding and leaves shared definitions immutable. This token is still a native-format model reference pending geometry loading, not a host pointer.

The function represents only these eight writes. Clearing at `BF25–BF3E`, complete runtime object construction and the projectile placement transforms are separate. In particular, `CB01` stores a launch deadline, inherited roll and target before calling `BF41`; a whole-record reset there would erase required state. The current code does not pretend to complete that launch using guessed coordinates or an artificial successful allocation.

Regeneration:

```sh
python3 tools/generate_object_definitions.py ../analysis/unpacked/image.bin
PYTHONPATH=/tmp/darker-python python3 tools/generate_definition_reference.py ..
```

The independent native probe checks every definition with three model bindings and twenty isolated seed-boundary cases, for **119 expansions**. It seeds the rest of each original runtime record with a sentinel and confirms that only the eight expected words change. C++ tests check the expanded values and definition identity, including unsigned seeds 128 and 255. All **48 CTest cases** pass.

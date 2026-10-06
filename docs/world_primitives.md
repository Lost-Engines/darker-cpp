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

# Mission execution foundations

`mission_script` now translates the original BF97 deadline/checkpoint scheduler and the timing, wait and message operations used by the connected campaign. The resource reader remains separate: it owns original bytes, while the scheduler receives bounded shared-program and language views.

Deadlines use wrapping 16-bit subtraction and its sign bit. Delays advance the previous deadline rather than restarting from the current clock; overdue scripts can execute several instructions in one update. Failed waits rewind to the checkpoint and add eight times the record multiplier. Checkpoint timestamps retain the original rounded coarse clock, while coarse-time waits require strictly passing their target. Stop state ends script progression without removing the actor or stopping its movement.

Implemented operations are 00–0E, 11–14, 1A–26, 2F, 31 and 32, plus high-bit animation-parameter selection. Object/world flag waits preserve both all-set and all-clear masks. World waits address the original packed type/state bytes. Completion and target-position conditions are supplied by the world; they are not inferred from message text.

Counted messages preserve the shared text cursor, empty-entry behaviour, suppression, centre/left/right selection and independent display expiry. Script continuation and display duration run concurrently. Emitted messages retain their own source text view across context changes; the presentation owner must consume the events and manage the three display slots. Unknown opcodes currently raise an explicit diagnostic rather than being treated as no-ops. A bounded instruction budget also diagnoses non-yielding malformed programs.

**656 native cases** compare deadlines, saved continuations, checkpoint clocks, consumed messages, display expiry and dispatch counts. These include all 256 flag operands for both object and world waits. A separate stateful test reproduces the first mission's objective wait and delayed return-to-base message across seven clock samples. The application derives completion from actual actor/world state; controlled tests separately supply aim and energy.

```sh
PYTHONPATH=/tmp/darker-python python3 tools/generate_mission_script_reference.py ..
```

The probe executes the original scheduler and handlers, intercepting only message-notification sound. Synthetic scripts/text and controlled world/object states isolate their contracts. Activation, targeting, world mutation, weapon selection and surface/underground campaign transitions are now connected within the documented campaign boundary. Remaining handlers and the complete campaign remain in progress.

## Ownership and supplementary exchange

Opcode 24 exchanges the current object's identity with the shared owner word,
stores the old owner in the object's checkpoint field, and stops its script.
The late main-loop gate installs the supplementary context on the player when
an owner exists and that context is not already active. Archive 04/15 is retained
separately from normal campaign resources, so borrowed script/text spans stay valid.

`mission_exchange` swaps program/message cursors while retaining the object's
checkpoint and the global interval. Opcode 26 continues immediately in the
incoming program; it does not force a frame yield. Twelve native owner fixtures
and four double-exchange traces match registration, cursor restoration, deadline
and stop state. The exchange traces also emit a secondary-context message and
return before drawing: the message retains its original source despite restoration
of the primary text cursor.

The blackout entry's outgoing SI is still not established as a usable player
continuation. It is represented as unknown rather than inventing a return point.
The shipped blackout never consumes it: its terminal relative-delay loop remains
in that context. Explicit opcode exchanges preserve known continuations and can
return; Skimma supply-pad entry remains a separate integration task.

The current flight presentation still displays the most recently emitted message.
Simultaneous composition of all three native message channels remains outstanding.

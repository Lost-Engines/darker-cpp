# Mission execution foundations

`mission_script` now translates the original BF97 deadline/checkpoint scheduler and the timing, wait and message operations needed by the first mission. The resource reader remains separate: it owns original bytes, while the scheduler receives bounded shared-program and language views.

Deadlines use wrapping 16-bit subtraction and its sign bit. Delays advance the previous deadline rather than restarting from the current clock; overdue scripts can execute several instructions in one update. Failed waits rewind to the checkpoint and add eight times the record multiplier. Checkpoint timestamps retain the original rounded coarse clock, while coarse-time waits require strictly passing their target. Stop state ends script progression without removing the actor or stopping its movement.

Implemented operations are 07, 0C–0E, 1A–23 and 25, plus high-bit animation-parameter selection. Object/world flag waits preserve both all-set and all-clear masks. World waits address the original packed type/state bytes. Completion and target-position conditions are supplied by the world; they are not inferred from message text.

Counted messages preserve the shared text cursor, empty-entry behaviour, suppression, centre/left/right selection and independent display expiry. Script continuation and display duration run concurrently. Emitted messages refer to the supplied text view; the presentation owner must consume the events and manage the three display slots. Unknown opcodes currently raise an explicit diagnostic rather than being treated as no-ops. A bounded instruction budget also diagnoses non-yielding malformed programs.

**656 native cases** compare deadlines, saved continuations, checkpoint clocks, consumed messages, display expiry and dispatch counts. These include all 256 flag operands for both object and world waits. A separate stateful test reproduces the first mission's objective wait and delayed return-to-base message across seven clock samples. Combat supplies no artificial completion in the application: actor integration remains outstanding.

```sh
PYTHONPATH=/tmp/darker-python python3 tools/generate_mission_script_reference.py ..
```

The probe executes the original scheduler and handlers, intercepting only message-notification sound. Synthetic scripts/text and controlled world/object states isolate their contracts. Activation, context exchange, targeting, world mutation, weapons, presentation instructions and a complete campaign runner remain to be connected.

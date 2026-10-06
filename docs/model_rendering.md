# Original geometry banks and projection

`resources::geometry_bank` owns decoded original bank bytes. Its typed city
records name the model offset, column/row fractions, collision marker and variant
limit; the two still-unexplained bytes retain explicit unknown names. Special
slots retain their shared pool offsets. Model bytecode and trailing world data
are bounded views into the owned resource, without conversion or relocation.

City model selection follows the alternate link first, then bank-masked damage
links. Special model headers are not interpreted as city link records. The
original-pack integration check compares native selection fingerprints for all
256 states of all 391 city types in banks 30–32. Ordinary tests use synthetic
headers to check traversal order, aliases and rejection of truncated/invalid data.
Regenerate the fingerprints with `tools/generate_geometry_bank_reference.py WORKSPACE`.

`graphics::model_projection` translates the far-path coordinate cache and signed
projection at `FC97`, with component set/zero/negate operations from the `FDxx`
and `FFxx` handlers. Coefficients, translation and screen origin are supplied by
the caller; camera-matrix construction and near-plane clipping are not included.
Signed products discard their lowest byte before caching, and negation acts on
that cached result rather than recomputing the product. Depth is accumulated with
fractional carry before signed division. Original division faults are explicit
errors rather than unsafe host arithmetic.

There is a noteworthy shared field: component A's and component B's vertical
products both write the fractional byte at `FCD3`. The most recent operation on
either component replaces or negates it. Their whole-word contributions remain
separate. This is present in both the unpacked executable and the captured live
DOS memory (`convoy-live/attempt-1/briefing-state.bin.gz`, CS 01A2), so a conventional
fresh matrix multiply is not a faithful replacement for the stateful interpreter.
The C++ cache represents this shared byte explicitly.

`tools/generate_model_projection_reference.py WORKSPACE` executes 24 uninterrupted
coordinate streams (768 vertices), with varied coefficient matrices, translations,
fractions and origins. Set, zero and negate operations all execute as original
instructions. Tests compare both projected coordinates and depth after every
operation. No drawing or visibility commands are included in these streams.

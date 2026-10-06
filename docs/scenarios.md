# Original scenario resources

`resources::scenario_resource` owns a decompressed archive-04 resource and reads its length-prefixed record directory. Shared programs, three formatted language sections, beacon-change queues and inline setup blocks retain their original resource-relative addresses. Copies and moves do not invalidate internal pointers: records store ranges, and callers obtain borrowed spans from the owning resource.

The setup reader follows BAC1–BAF5 and BE07's stored formats. It distinguishes moving, compact-route and absolute placements; preserves counted-object flags, attributes, six motion bytes and special target words; and resolves forward program references. The three groups remain separate: initially active, reserve, and remaining free-list templates. The selected objective cell list follows the second list's FE/FF terminator. No beacon queue is applied merely because it occurs in a scenario header.

All seven FE-prefixed native setup blocks are retained with their length, source location and current-object index. They are **not executed or silently treated as no-ops** by a mission runner: this module only reads source records. Their eventual named C++ effects must preserve modifications to later placements and the cross-block model assignment. Similarly, stored placement coordinates have not yet undergone model-height adjustment or underground route snapping.

The resource integration check compares all **124 records**, **1,857 placements**, **seven native blocks**, and exact bytes of all **372 language sections** with the independently checked analysis export. This includes directory boundaries, group membership, object parameters, references, cell lists and native-block context. Focused tests cover truncated resources, invalid language ranges, invalid entries, ownership and language selection. Original script execution, mission activation, scene transitions and formatted text drawing remain separate consumers.

Regenerate the structural reference with:

```sh
python3 tools/generate_scenario_reference.py ..
```

The generator records the analysis JSON and original executable hashes. It checks stored shared/object bytes against the extracted resources; its fixtures describe source decoding, not a claim of complete native mission execution.

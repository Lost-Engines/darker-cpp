# Rules, representations and extension points

The reconstruction keeps the original algorithms and arithmetic. Named rules
make their relationships visible; they do not turn every native constraint into
an independently adjustable setting. A format field, a gameplay choice and an
arithmetic boundary can share a number without being the same rule.

## Where definitions belong

| Subject | Owning definitions | Consumers and relationships |
| --- | --- | --- |
| World positions | [`world_format`](../src/maths/world_coordinates.h) | Unsigned position words, eight within-cell bits and a separate movement fraction byte. Collision, flight, actor placement and save decoding all use these units. |
| Camera arithmetic | [`render_geometry`](../src/graphics/render_geometry.h) | Camera coordinates, retained projection fractions, accumulator width, signed wrapping, world-to-camera scale, near plane and projection overflow limits. |
| Scene traversal and fog | [`scene_limits`](../src/graphics/render_geometry.h) | Surface/tunnel traversal radii and distance-shade table limits. Scanner capacity is separate from representable projection distance. |
| Framebuffer dimensions | [`display_layout`, `source_sheet_layout`](../src/render/frame_layout.h) | Display bounds/strides and original resource-sheet dimensions are separate. Framebuffer templates carry both dimensions. |
| Cockpit viewports | [`cockpit_view_layout`](../src/graphics/screen_layout.h) | Caero and Skimma viewport heights, centres and Caero's top strip. Original artwork coordinates remain artwork coordinates. |
| Cities and tunnels | [`world_kind`, `world_profiles`](../src/resources/world_profile.h) | Geometry resource, map resource selection, damage-stage mask and beacon lighting. Configuration identifies world independently of craft. |
| Map dimensions and cell state | [`city_map_size`, `city_cell`](../src/game/city_map.h) | The native 128×128 allocation and target permission bit. Model variants/damage and beacon intensity share the state byte deliberately. |
| Craft and weapon definitions | [`object_catalogue`](../src/game/object_catalogue.h), [`object_definition`](../src/game/object_definition.h) | Shared definition/model slot layout; typed craft, player and projectile data; per-slot Skimma ammunition records. |
| Authored object parameters | [`original_object_definitions`](../src/game/object_definitions.h) | Generated native values, including speeds, steering, collision dimensions, resistance, weapon cost and callback selection. Update the generator as well as generated output when changing the representation. |
| Actor behaviours | [`object_update`](../src/game/object_update.h), [`scenario_actor`](../src/game/scenario_actor.h) | Named native callbacks and mutable actor state. Mission bytecode still supplies native slot numbers. |
| Flight tuning | [`flight_rules`](../src/game/flight_rules.h), [`caero_energy_state`](../src/game/caero_energy.h) | Skimma drive settings/assist limit/default bias, Caero startup/boost rules and energy capacities. Further coupling and rounding stay beside their flight equations. |
| Projectile storage and identities | [`projectile_limits`, `native_object_layout`](../src/game/native_object_layout.h) | Separate player/hostile capacities and DOS-style reference tokens. Compile-time checks retain the adjacent native token ranges. |
| Collision | [`collision_rules`](../src/game/collision_rules.h), [`collision_box`, `collision_category`](../src/game/collision_box.h), [`city_collision_boxes`](../src/game/city_collision.h) | Authored volumes and categories, model-state selection, expansion and native wrapping intersection arithmetic. |
| Simulation time | [`clock_tick`, `game_duration`, `campaign_clock`](../src/game/time.h) | Wrapping tick/deadline arithmetic is distinct from campaign elapsed time and host wall time. |
| Effects | [`effects`](../src/game/effects.h) | Shared trail/emitter capacities, typed packed animation and effect recipes. |

The world profile deliberately does not choose the cockpit or infer the palette
from the ship family. The final Skimma mission uses Delphi. Palette selection
also involves the original cockpit/presentation resources and underground
palette; see [cockpit rendering](cockpit_rendering.md).

## Increasing draw distance

Start with `scene_limits`, then follow `render_geometry` through `camera`,
`model_projection`, `near_clip`, `model_renderer` and `city_scene`.

A world cell contains 256 position units. Placement multiplies horizontal
positions by four and doubles the relative horizontal displacement again
before transformation. Signed word wrapping therefore matters well before the
scanner's maximum radius. Cached projection values retain a coordinate word
and fractional byte; their signed 24-bit wrap is an observable part of the
native renderer.

The transform-product shift is independent of coordinate storage width. The
native focal length happens to equal the projection fraction scale (256), so
direct projection divides its fixed-point numerator by whole depth without an
extra multiplication. An assertion makes that dependency explicit. Changing
FOV requires changing projection, including near-plane intersection projection,
not simply changing that constant.

A wider fork must also check matrix-product overflow, sorting-distance overflow,
scanner byte arithmetic, near-intersection saturation, culling, fog and tunnel
visibility. Resource words need not grow with runtime camera coordinates.
Native indexed-pixel comparisons should continue to exercise the unchanged
reference configuration.

## Changing internal resolution

`display_layout` describes the 320×240 output; `source_sheet_layout` describes
320×200 source sheets. Neither should be derived from the other. The current
blitters, original presentation layout and cockpit assets also assume matching
row widths in places. Named output bounds do not remove those asset assumptions.

A higher-resolution fork needs an explicit policy for placing/scaling original
artwork, fonts, buttons and masks, and for mapping input back to that artwork.
Keep those choices separate from polygon clipping bounds, projection centres,
FOV and host-window scaling. See [software polygons](software_polygons.md),
[model rendering](model_rendering.md) and [fonts and text](fonts_and_text.md).

## Adding craft, weapons or actors

`object_catalogue` records the shared layout used by both definitions and
special-model tables. Player definitions follow the five scenario
configurations; that is not a universal craft registry. Skimma weapon records
keep definition slots and both ammunition capacities together instead of in
parallel arrays.

A new craft requires corresponding state, flight/update behaviour, input,
weapons, cockpit and scenario setup. A new weapon requires its definition/model,
firing and target rules, damage behaviour and presentation. A new actor callback
must be handled by update dispatch and scenario decoding. Avoid adding a slot
and assuming the count alone supplies those behaviours. Existing switches make
those decisions visible without a speculative plugin architecture.

Projectile pool sizes are not independent of reference identities. The original
player and hostile pools occupy adjacent token ranges immediately before the
actor range. Enlarging either requires assigning non-overlapping identities and
reviewing token encoding/decoding and target repair. These tokens are not host
pointers; their numeric values also distinguish airborne from ground targets.

## Adding buildings or changing a world

Building type numbers belong to their geometry bank. The same number need not
mean the same object in Delphi and Halon. Keep special-case building rules
qualified by their world instead of inventing a universal building enum.

Geometry supplies collision volumes, view-dependent model bytecode, damage
variants and bounds. Collision categories describe impact handling independently
of target permission. Authored boxes may wrap in native coordinates; replacing
the collision algorithm with a conventional AABB test would change behaviour.
See [city collision](city_collision.md) and [world primitives](world_primitives.md).

A different map size affects more than allocation: cell addressing, coordinate
wrapping, beacon lattices, radar, mission placements, tunnel topology and the
original save layout all carry native assumptions. A world profile collects
resource selection and broad rules, while each subsystem retains responsibility
for its own algorithm. A larger or differently shaped map needs a format and
save-compatibility decision as well as a new size.

## Refactoring discipline

Keep rounding, truncation, signed interpretation, evaluation order and random
number consumption intact. Name an unexplained field only when evidence supports
its meaning; a precise native representation is preferable to a misleading
abstraction. Use the existing native reference fixtures for behaviour changes,
not new expected values derived from a rewritten implementation.

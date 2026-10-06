# Armchair Software Style Guide

This document defines Armchair Software's house style for C++ and related build/script files.
Use it as the default reference when writing or reviewing code in Armchair projects.
When existing code differs, follow this guide and normalise touched code toward it.

## 0. Baseline Example

```cpp
namespace example {

struct run_state {
  bool keep_running{true};
  unsigned int retry_limit{3};
};

auto process_item(item const &source, item &target)->bool {
  auto constexpr timeout{5s};
  if(source.age() > timeout) return false;
  if(!source.valid()) return false;

  target = item{
    .id{source.id},
    .name{source.name},
  };

  return true;
}

} // namespace example
```

Notes:

- Uses 2-space indentation.
- Uses attached braces.
- Uses no space before control parentheses.
- Uses reference style `type const &name`.
- Uses brace initialization and designated initializers.

## Table of Contents
{: .no_toc}

* TOC
{:toc}

## 1. Scope and Authority

### 1.1 Language scope

- House style targets modern C++.
- Baseline language expectation is C++23.
- Adopt C++26 features proactively when toolchain and project constraints permit.

### 1.2 In-scope file types (`.cpp`, `.h`)

- This guide applies to first-party C++ source and header files.

### 1.3 Out-of-scope code (third-party, vendored, generated)

- Third-party and vendored code (for example `include/`) is not style-authoritative.
- Generated files should migrate toward house style where practical, but generator output may temporarily diverge.

### 1.4 Rule priority when conflicts occur

- Explicit guide rules override historical inconsistencies.
- When examples disagree, follow the decision captured in this document.

### 1.5 Migration policy for existing inconsistent code

- Style fixes are proactive: when touching files, normalise code toward this guide.
- Historical patterns that conflict with this guide should be treated as debt to remove, not precedent.

## 2. Guide Authority

### 2.1 Source of truth

- This document is the authoritative source of house style.
- Historical code patterns that conflict with this document are debt, not precedent.

### 2.2 Ambiguity resolution

- When examples conflict, follow the explicit rule text in this guide.
- If a rule is unclear or contradictory, resolve it by updating this document, then normalise code to match.

## 3. File and Project Structure

### 3.1 Project root layout conventions

- Keep source grouped by subsystem/domain (`render`, `gui`, `world`, `net`, etc.).
- Keep shared internal libraries in clearly named module directories (`vectorstorm`, `logstorm`, `timestorm`, etc.).
- `*storm` libraries synchronised through `libvoxelstorm` must not be renamed or structurally reorganised locally.
- Improvements to synchronised libraries are encouraged, but should avoid breaking changes and remain suitable for upstream redistribution.
- Local edits in synchronised libraries are expected to be reviewed for upstream inclusion in `libvoxelstorm`.

### 3.2 Directory naming conventions

- Use lowercase snake_case directory names.
- Directory names should reflect subsystem function, not implementation detail.

### 3.3 Module boundaries and folder ownership

- Prefer internal includes through module boundaries rather than deep cross-folder coupling.
- Shared utilities belong in dedicated shared modules, not duplicated local helpers.

### 3.4 Header/source pairing and location

- Keep `.h` and `.cpp` paired in the same module directory when practical.
- A `.cpp` should generally correspond to a same-name header unless it is an entry point or intentionally translation-unit-local implementation.

### 3.5 Public vs private headers

- Headers intended for broad reuse should expose stable API surface and minimise transitive includes.
- Private implementation details should remain in `.cpp` files or tightly scoped internal headers.

### 3.6 Test/source placement conventions

- Test files should live in dedicated test paths.
- Test sources are style-authoritative and must follow the same style rules as production sources.

### 3.7 Asset and non-code file placement

- Non-code resources should be isolated under dedicated resource/asset directories and not mixed with first-party C++ source layout.

## 4. File Naming Conventions

### 4.1 `.cpp` and `.h` filename casing

- Use lowercase snake_case filenames.
- File names should reflect primary contained type or responsibility.

Example:

```text
world_manager.cpp
world_manager.h
```

### 4.2 Acronyms in names

- Acronyms should remain lowercase within snake_case names (`webgpu_renderer`, `api_client`).
- Avoid mixed-case acronym styling in filenames.

### 4.3 Prefixes/suffixes (`_manager`, `_renderer`, `_types`, `_forward`)

- Suffixes should encode role clearly when useful:
  - `_manager` for orchestration/stateful coordinators
  - `_renderer` for rendering subsystems
  - `_types` for alias/type bundles
  - `_forward` for forward-declaration headers

### 4.4 Special filenames (`main.cpp`, `version.h`, `protocol.h`)

- `main.cpp` is reserved for entry points.
- `version.h` and similar generated metadata files may be mechanically produced, but should be normalised toward guide style where feasible.
- Shared protocol/interface files should use stable, predictable naming (`protocol.h`) and live near owning module roots.

## 5. File Prologue and Includes

### 5.1 Header guard strategy (`#pragma once` vs include guards)

- Use `#pragma once` for all first-party headers.
- Convert legacy include guards to `#pragma once` proactively when files are touched.
- Do not use dual guard style (`#pragma once` + `#ifndef`) in new or cleaned-up headers.

### 5.2 Include block ordering

- In `.cpp` files, include the matching local header first when one exists.
- In unit test translation units, include the test framework header(s) first (for example Catch2, Boost.Test), above other include groups.
- Then apply this include group order:
  - standard language/library headers
  - operating system headers
  - Boost headers
  - other third-party headers (roughly descending significance/notability)
  - internal shared libraries/components (for example `whateverstorm` and similar)
  - internal project general utilities
  - specific functional/class includes

### 5.3 Include grouping and blank lines

- Do not insert blank lines between individual includes or between include groups.
- Avoid extra blank lines inside a single include group.
- Avoid interleaving groups unless required by platform/compiler constraints.
- Prefer alphabetical sorting within each include group.
- Enforce include sorting with tooling and review; unsorted includes are style violations.

### 5.4 Forward declaration preferences vs includes

- Prefer forward declarations in headers when only pointer/reference members are needed.
- Include full headers in `.cpp` where definitions are used.

### 5.5 Include path style (`"..."` vs `<...>`)

- Use `"..."` for project-local headers.
- Use `<...>` for standard library and external libraries.

## 6. Whitespace and Layout

### 6.1 Indentation width

- Use 2 spaces per indentation level.
- Do not use 4-space block indentation.
- Continuation lines may use either:
  - vertical alignment to a logical anchor, or
  - one extra 2-space step.

### 6.2 Tabs vs spaces

- Use spaces for indentation in first-party C++.
- Tabs are not idiomatic for first-party `.cpp/.h` files.
- Editor configuration should convert tabs to spaces automatically.

### 6.3 Trailing whitespace policy

- No trailing whitespace in first-party hand-authored code.
- Generated files may preserve generator output until generation is standardised.
- Editor configuration should strip trailing whitespace and end-of-file whitespace automatically.

### 6.4 Consecutive blank line policy

- Use a single blank line to separate logical blocks.
- Avoid multiple consecutive blank lines inside function bodies except when separating major phases.

### 6.5 Horizontal alignment policy

- Alignment is allowed for local readability when grouping related declarations.
- Alignment is encouraged when it clearly improves readability.
- In manually aligned blocks, keep opening delimiters attached to the preceding token (`name{`, `fn(`), not detached by alignment spaces.
- Place alignment padding inside the affected delimiters where needed, not between the identifier and its opening delimiter.

Example (avoid):

```cpp
bool show_log_window{true};
bool show_console   {true};
```

Example (preferred):

```cpp
bool show_log_window{true};
bool show_console{   true};
```

### 6.6 Line length policy and exceptions

- There is no hard line-length limit.
- Prioritise readability over numeric width targets.
- Avoid aggressively splitting readable long lines just to satisfy an arbitrary length.
- Comments do not have a hard wrapping requirement; keep them on one line when they remain legible in a typical modern editor viewport (around 200 visible columns).
- Allow longer lines for:
  - URLs and machine-oriented strings,
  - generated/string-table content,
  - dense expressions or comments where wrapping harms readability.

### 6.7 Long statement wrapping and continuation indentation

- Prefer operator-leading or operator-trailing wrapped chains with consistent continuation indentation.
- For stream chains and builder-like code, keep one operator per line after wrap.
- Keep wrapped initializer/designated-initializer blocks vertically readable.

Example:

```cpp
logger << "DEBUG: on_close, event_type " << event_type
       << ", websocket " << event->socket
       << " closed" << (event->wasClean ? " cleanly" : ", not cleanly")
       << ", status: " << closeevent_code_to_text(event->code)
       << ", reason: \"" << event->reason << "\"";
```

### 6.8 Editor behaviour and defaults

- Configure editors to:
  - use 2-space tab width
  - insert spaces instead of tab characters
  - strip trailing whitespace and EOF whitespace on save

## 7. Braces and Block Formatting

### 7.1 Function/class/struct brace placement

- Use attached braces for functions, classes, structs, and namespaces.

Example:

```cpp
class network {
public:
  void run();
};
```

### 7.2 `if/else` brace and `else` placement

- Opening brace stays on the same line as the condition.
- Place `else` as `} else {`.

Example:

```cpp
if(ready) {
  run_once();
} else {
  rebuild_state();
}
```

### 7.3 Loop/switch brace style

- Use attached braces for `for(...)`, `while(...)`, and `switch(...)`.
- Keep `case` labels at the same indentation level as `switch`.
- Indent statements inside each `case` by one level.
- Add braces inside a `case` only when needed to control scope (for example local variable lifetime).

Example:

```cpp
switch(mode) {
case mode_type::a:
  {
    int const value{1};
  }
  break;
case mode_type::b:
  execute();
  break;
}
```

### 7.4 Namespace brace style and closing comments

- Prefer a closing namespace comment for non-trivial scopes:
  - `} // namespace module::submodule`
- When closing an anonymous namespace, use an explicit comment such as:
  - `} // anonymous namespace`
  - `} // unnamed namespace`
- Small/local namespaces may omit this when still clear.

### 7.5 Lambda brace style

- Use attached braces for lambda bodies.
- Format lambda internals the same way as ordinary function bodies.

Example:

```cpp
auto predicate{[&](int value) {
  if(value < 0) return false;
  return true;
}};
```

### 7.6 Single-line statement policy

- Single-line guard clauses are acceptable for simple exits:
  - `if(!ready) return;`
- Single-line bodies are also acceptable for simple assignment/function-call bodies when readability stays high.
- This allowance applies to control statements only, not function definitions (see §13.9).
- Use braces once a branch has multiple statements.
- If the body appears on a new line, braces are mandatory (no newline-separated unbraced bodies).

Example:

```cpp
if(ready) run_once();                    // allowed

if(ready) {                              // required when body is on next line
  run_once();
}
```

### 7.7 What does not get indented

- Do not indent:
  - `public:`, `private:`, `protected:` labels
  - `case` labels under `switch`
  - primary content inside `namespace` scopes

### 7.8 Conditional indentation in special blocks

- Indentation may still be used inside preprocessor-controlled blocks and diagnostic blocks when it improves readability:
  - code inside `#ifdef` regions is often indented
  - code inside `#pragma GCC diagnostic push` regions may be indented when helpful

## 8. Spacing Rules

### 8.1 Control statement spacing (`if(` vs `if (` etc.)

- Use no space before `(` in control statements:
  - `if(...)`, `for(...)`, `while(...)`, `switch(...)`, `catch(...)`

### 8.2 Function declaration/definition spacing

- No space between function name and `(` in declarations and definitions.
- Trailing return type examples use `auto f()->T` without spaces around `->`.

Example:

```cpp
auto build_cache(config const &cfg)->cache;
void set_enabled(bool enabled);

void set_enabled(bool enabled) {
  enabled_state = enabled;
}
```

### 8.3 Operator spacing (arithmetic, assignment, comparison, logical)

- Use spaces around binary operators.
- Keep unary operators compact (`++i`, `!flag`).

### 8.4 Comma/semicolon/colon spacing

- One space after commas.
- No space before commas or semicolons.
- No space before `:` in labels (for example `case ready:`).
- Use one space on both sides of `:` in range-for clauses.

Example:

```cpp
for(auto const &[name, value] : my_map) {
  switch(value.state) {
  case state::ready:
    consume(name, value);
    break;
  case state::skipped:
    continue;
  }
}
```

### 8.5 Pointer/reference spacing

- Bind pointer/reference symbols to the type side:
  - `type *ptr`
  - `type &ref`
  - `type const &value`
- When there is no declarator name (for example in type-only contexts such as template arguments, function type signatures, or cast targets), no trailing space is added after the symbol:
  - `type const&`
  - `type*`

### 8.6 Template angle bracket spacing

- No extra spaces inside template angle brackets.

### 8.7 Cast spacing

- Prefer named casts with regular call spacing:
  - `static_cast<T>(value)`

### 8.8 Multi-line comma-separated list formatting

- For function declarations/definitions, keep the first parameter on the same line as the function name whenever practical.
- Do not force a line break immediately after `(` for short or medium-length signatures if it adds clutter.
- Break parameter lists only when length/complexity genuinely improves readability.
- When a parameter list is wrapped, align continuation parameters under the first parameter (or a consistent readable anchor on that line).
- Keep the closing `)` on the same line as the final parameter unless a specific readability constraint requires otherwise.
- For most other brace-based lists (especially aggregate/initializer content), prefer:
  - opening brace on the introducing line
  - one element per subsequent line
  - one indentation level for each element

Example (avoid):

```cpp
unsigned int get_named_node_index(
  std::vector<std::string> const &node_names,
  std::string_view const node_name,
  std::string_view const node_kind
) {
  return 0;
}
```

Example (preferred):

```cpp
unsigned int get_named_node_index(std::vector<std::string> const &node_names,
                                  std::string_view const node_name,
                                  std::string_view const node_kind) {
  return 0;
}
```

## 9. Naming and Case Conventions

### 9.1 Namespace names

- Use lowercase snake_case namespace names.
- Prefer nested namespace syntax where suitable (`namespace armchair::render {`).

### 9.2 Type names (class/struct/enum/alias)

- Use lowercase snake_case for project-defined type names.
- `enum class` names follow the same pattern (`exposure_type`, `timescale`).

### 9.3 Function and method names

- Use lowercase snake_case (`get_open_price`, `apply_level_update`, `read_output`).

### 9.4 Variable names

- Use lowercase snake_case for local variables and non-member state.
- Allow short loop counters (`i`, `x`, `y`, `z`) in narrow scopes.
- Otherwise, variable names must be meaningful and legible.
- Avoid terse acronyms and short opaque names except where representing true mathematical symbols.

### 9.5 Member names

- Use lowercase snake_case without `m_`/`_` prefixes.
- Keep member names identical to conceptual domain terms.

### 9.6 Constants and `constexpr` names

- Use lowercase snake_case for `constexpr` and `const` object names.
- Macro-style uppercase is reserved for preprocessor constants.

### 9.7 Macro names

- Use `UPPER_SNAKE_CASE` for preprocessor symbols (`DEBUG_WEBGPU`, `BOOST_NO_EXCEPTIONS`).

### 9.8 Enum value names

- Use lowercase snake_case enumerators (`none`, `up`, `down`, `diagonal`).

### 9.9 Template parameter names

- Use short UpperCamel or single-letter template parameter names (`T`, `FromT`, `Tcpp`, `Tc`).

### 9.10 Namespace usage rules

- Never use `using namespace ...` for non-literal namespaces.
- `using namespace std::chrono_literals;` and similar literal namespaces are allowed in source files when they improve readability.
- Do not place `using namespace ...literals` in headers, to avoid namespace pollution for includers.
- Namespace aliases are permitted only when they clearly improve legibility and do not introduce ambiguity.

### 9.11 Language and spelling conventions

- Prefer British English spellings in identifiers, comments, and diagnostics (`colour`, `synchronised`, `initialise`).
- Keep externally defined spellings unchanged when interfacing with third-party APIs, protocols, schemas, or file formats.

## 10. Declarations and Definitions

### 10.1 `auto` usage policy

- Use `auto` when type is obvious from initializer or when avoiding repetition improves clarity.
- Avoid `auto` when it obscures important concrete type semantics.

### 10.2 Trailing return type style (`auto f()->T`)

- Trailing return style is allowed but not preferred by default.
- It is encouraged for entry points and callback-heavy signatures.
- Prefer direct return type syntax when it is shorter and clearer.
- Do not add a trailing return type when the return type is likely to be inferred correctly and the annotation adds no real clarity.
- When a trailing return type is used, write it with no spaces around `->`:
  - `auto f()->T`
  - `[](int value)->float {`

### 10.3 Reference, const, and constexpr placement style

- Use east-const with left-bound reference/pointer symbols:
  - `type const &value`
  - `type *ptr`
- For object declarations, place `constexpr` alongside `const` after the type:
  - `unsigned int constexpr max_threads{128};`
  - `auto constexpr timeout{5s};`
- Do not write west-`constexpr` object declarations such as `constexpr unsigned int max_threads{128};`.
- For function declarations/definitions, `constexpr` remains a function specifier and stays before the return type in the usual position:
  - `constexpr auto has_flag(render_flags value, render_flags flag)->bool`
  - `inline constexpr bool empty() const`

### 10.4 Declaration ordering inside classes

- Keep access sections explicit (`public`, `private`, `protected`).
- Preferred class member order:
  - external references set at initialization (injected dependencies, handles)
  - nested class/struct/enum definitions (except those tightly associated with a specific member)
  - member objects and variables
  - constructors and destructor
  - other member functions
  - friend declarations
- Preserve pragmatism for data layout/packing when ordering member objects.

### 10.5 Initializer list formatting

- For multi-member constructor initialization, place each initializer on its own line under `:`.

### 10.6 Default member initialization

- Prefer in-class member initialization with braces for defaults.

### 10.7 `using` aliases and `typedef` policy

- Prefer `using` over `typedef`.
- Use alias names that preserve existing lowercase snake_case conventions.

### 10.8 `static`/type/`constexpr`/`const` ordering

- Prefer declaration ordering:
  - `static` first (if present)
  - type
  - `constexpr` or `const`
  - pointer/reference marker
  - variable name

Example:

```cpp
static std::chrono::seconds constexpr update_interval{5s};
std::string const &name_ref{source.name};
constexpr auto has_flag(render_flags value, render_flags flag)->bool;
```

### 10.9 Integer signedness defaults

- Prefer unsigned integer types for counters, sizes, indices, and quantities that cannot logically be negative.
- For general non-negative counters/state where no width contract is required, prefer `unsigned int` by default.
- Use `size_t` when interacting with container sizes/indices or when size-width semantics are required.
- Avoid narrow fixed-width integer types (`uint8_t`, `uint16_t`) unless data layout/memory packing requirements or benchmarks justify them.
- Use signed integers only when negative values are meaningful and expected by the domain.

Example:

```cpp
unsigned int retry_limit{3};
size_t const item_count{items.size()};
for(size_t i{0}; i != item_count; ++i) {
  process(items[i]);
}
```

### 10.10 Enum declarations and underlying types

- Prefer scoped enums (`enum class`) over unscoped enums.
- Use lowercase snake_case names for enum types and enumerators.
- If an enum is stored/transmitted/packed, specify an explicit underlying type sized to the value range (for example `uint8_t`).
- For purely internal enums without storage/ABI constraints, an explicit underlying type is optional.
- Convert enums explicitly when integer conversion is required (`static_cast`), rather than relying on implicit conversion.

Example:

```cpp
enum class colour : uint8_t {
  red,
  green,
  blue,
};
```

### 10.11 Bitmask enum style

- Use `enum class` for flags/bitmasks, with an explicit unsigned underlying type.
- Name bit values in lowercase snake_case, and make bit positions explicit.
- Keep bitwise operator helpers local to the owning namespace/type.
- Avoid mixing raw integers with flags at call sites; use typed helpers.

Example:

```cpp
enum class render_flags : uint32_t {
  none = 0u,
  shadows = 1u << 0,
  bloom = 1u << 1,
  vsync = 1u << 2,
};

constexpr auto operator|(render_flags lhs, render_flags rhs)->render_flags {
  return static_cast<render_flags>(
      static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
}

constexpr auto has_flag(render_flags value, render_flags flag)->bool {
  return (static_cast<uint32_t>(value) & static_cast<uint32_t>(flag)) != 0u;
}
```

## 11. Initialization and Expressions

### 11.1 Uniform initialization (house preference: universal braces)

- Use brace initialization as the default when a declaration is intentionally initialized, as opposed to `=` initialisation.
- This applies to scalars, class types, aggregates, containers, and temporaries where practical.
- Do not add empty braces when they are semantically redundant.
- If omission would leave a value uninitialized and a specific default value is intended, spell that value explicitly inside the braces rather than relying on empty braces; this does not endorse redundant empty-brace default construction where omission is equivalent.
- Existing declaration-site `=` initialization in legacy files is non-idiomatic and should be migrated when touched.

Example:

```cpp
int count{0};
std::vector<uint8_t> bytes;
std::string const name{"sample"};
auto constexpr timeout{5s};
```

Avoid:

```cpp
int count{};
std::vector<uint8_t> bytes{};
```

Prefer:

```cpp
int count{0};
std::vector<uint8_t> bytes;
```

### 11.2 Assignment vs initialization distinctions

- Use `=` primarily for assignment after declaration.
- At declaration site, prefer braces unless language constraints require another form.

### 11.3 Designated initializers

- Use designated initializers wherever possible for aggregate setup.
- Keep one designated field per line for multi-field initialization.

Example:

```cpp
result = item{
  .id{source.id},
  .name{source.name},
};
```

### 11.4 Literal formatting (`1.0f`, digit separators, chrono literals)

- Use explicit suffixes where type clarity matters (`1.0f`, `5s`).
- Use digit separators for large literals when it improves readability (`100'000'000.0f`).
- For bitmasks and protocol/register bit patterns, prefer binary literals with separators for legibility.
- Group binary digits on meaningful boundaries (field/byte); if no natural boundary exists, group in nibbles.
- Use unsigned suffixes on integer bitmask literals to avoid unintended signed behaviour (`0b...u`, `0x...u`).
- Prefer raw string literals (`R"(...)`) when they materially reduce escaping noise (for example regexes and embedded snippets).
- In source files, prefer string-view literals (`"..."sv`) for non-owning compile-time string references.
- For packed hex constants, use digit separators on byte boundaries (`0xFF'00'AA'55u`).

Example:

```cpp
auto constexpr packet_mask{0b1000'0000'1010'0101u};
auto constexpr flags_mask{0b0011'1100u};
auto constexpr high_nibble{0b1111'0000u};
auto constexpr rgba_mask{0xFF'00'AA'55u};
auto const pattern{R"(^[A-Z]{3}\d{2}$)"};
using namespace std::string_view_literals;
auto constexpr status_text{"ready"sv};
```

### 11.5 Cast policy (`static_cast`, C-style cast restrictions)

- Prefer named casts (`static_cast`, `reinterpret_cast`, etc.).
- C-style casts must be proactively removed from first-party C++.
- When encountered, convert to the appropriate named cast.

### 11.6 Ternary operator formatting

- Keep short ternaries inline.
- For long ternaries, wrap cleanly and keep branches simple enough to scan.

### 11.7 Boolean expression clarity

- Prefer explicit boolean conditions and guard clauses.
- Avoid deeply negated or dense chained expressions without intermediate naming.

Example (prefer):

```cpp
bool const timed_out{now >= deadline};
bool const has_capacity{queue_size < max_queue_size};
if(timed_out || !has_capacity) return false;
```

Example (avoid):

```cpp
if(!(now < deadline && (queue_size < max_queue_size || allow_overflow))) {
  return false;
}
```

### 11.8 Type deduction with brace initialization

- When the desired type is unambiguous from context, prefer implicit type deduction over repeating the type.
- In typed return contexts, prefer `return {...};` over `return explicit_type{...};`.
- Use explicit type spelling when deduction is not available at that declaration site (for example function parameters and non-static data members).

Example:

```cpp
auto constexpr timeout{5s};

auto build_config()->config {
  return {
    .retry_limit{3},
    .enabled{true},
  };
}
```

## 12. Control Flow Style

### 12.1 Early exit vs deep nesting

- Prefer early exits (`return`, `continue`) to reduce nested indentation.
- Use guard clauses for invalid/empty/error states.
- Keep scopes as small as practical; create inner scopes in long functions to limit object lifetimes.

### 12.2 Condition complexity thresholds

- Keep complex conditions readable; extract named booleans when expressions become dense.
- Prefer `if` init-statements when a temporary is only needed for the condition/body.

### 12.3 `switch` style and exhaustiveness handling

- Prefer `switch` for closed enum dispatch.
- Avoid silent fallback behaviour; enforce exhaustiveness where practical.
- When intentionally exhaustive, omit `default` and document intent.

### 12.4 Loop form preferences (`for`, range-for, `while`)

- Prefer range-for when iterating container elements directly.
- Use indexed `for` where indices are semantically meaningful.
- Use `while` for open-ended loops tied to runtime state flags.
- Prefer modern ranges/views/zip-style iteration helpers over manual iterator increment code when viable.

### 12.5 `continue`/`break` usage

- For unsigned index iteration, prefer `!= end` termination when iterating over known bounds.
- Use `continue` for explicit fast-path skips when it simplifies loop structure.
- Use `break` when explicit early termination improves clarity over condition mutation.

## 13. Functions and APIs

### 13.1 Function size and responsibility

- Function structure should optimise for performance and optimiser visibility first.
- When readability and performance trade off, performance takes priority.
- Large functions are acceptable when they represent a coherent flow and avoid needless call/forwarding overhead.
- If logic is called from only one place, keep it inline at that call site unless extraction provides clear structural value (for example shared class-member access, reusable behaviour, or isolation of unavoidable complexity).
- Prefer liberal documentary comments over artificial fragmentation into mini-functions.

Example:

```cpp
void process() {
  /// Validate input and fast-fail
  if(!ready) return;

  {                                      // setup phase
    setup_context const ctx{build_context()};
    state.setup(ctx);
  }

  for(size_t i{0}; i != items.size(); ++i) {
    state.update(items[i]);
  }
}
```

### 13.2 Parameter passing conventions

- Pass cheap scalar types by value.
- Pass heavy objects as `type const &` for read-only access.
- Pass by non-const reference only for intentional mutation.
- Use forwarding references (`T&&`) and perfect forwarding for generic pass-through APIs where preserving value category is required.
- For by-value parameters:
  - declarations should omit `const`
  - definitions should mark parameters `const` by default, omitting `const` only when deliberate local mutation is required
- Declare temporaries `const` by default unless mutation is intentional.
- Prefer `constexpr` for compile-time literal constants when the type supports it.

Example:

```cpp
void set_count(unsigned int count);       // declaration

void set_count(unsigned int const count) { // definition
  cached_count = count;
}

void set_name(std::string name) {
  current_name = std::move(name);
}

template<typename T>
void push(T&& value) {
  storage.emplace_back(std::forward<T>(value));
}
```

### 13.3 Output parameters and return values

- Prefer return values over output parameters when possible.
- Use aggregates/structs for multi-value returns.
- Use output parameters only when they materially improve API clarity or performance.

### 13.4 Overload design rules

- Keep overload sets small and unambiguous.
- Prefer explicit naming differences over confusing overload combinations.

### 13.5 `[[nodiscard]]` usage

- Use `[[nodiscard]]` only where discarding the return value is itself an error.
- Typical cases are pure/result-only functions with no meaningful side effects.
- Do not overuse `[[nodiscard]]`; some getter-like calls may legitimately be used for side effects (tests, benchmarks, cache refresh paths).

### 13.6 `noexcept` guidance

- Mark functions `noexcept` when non-throwing behaviour is guaranteed and useful to consumers/optimisers.
- Do not mark `noexcept` speculatively.

### 13.7 Standard vs C library symbol usage

- Prefer C++ standard-library function symbols with `std::` qualification (for example `std::abs` rather than `abs`).
- Do not force `std::` qualification for type names where house style prefers unqualified forms.
- Prefer `size_t`, `uint8_t`, `uint16_t`, `uint32_t`, `uint64_t`, and related fixed-width aliases without `std::` qualification.
- Write `size_t`, not `std::size_t`, and `uint8_t`, not `std::uint8_t`.

Example:

```cpp
size_t const count{values.size()};
uint8_t const flags{0b1010'0001};
double const magnitude{std::abs(delta)};
```

### 13.8 Unused names (parameters and structured bindings)

- In declarations, keep the ordinary parameter name:
  - `void do_something(int value);`
- In definitions, keep unused parameter names present but commented inline:
  - `void do_something(int /*value*/)`
- Do not suppress unused function parameters with `(void)value` inside the function body.
- For unused structured-binding elements, bind the unused part as `_` instead of using `(void)name` casts after binding.

Example:

```cpp
void write_metric(std::string const &name, int sample_count);

void write_metric(std::string const &name, int const /*sample_count*/) {
  metrics.emplace_back(name);
}

auto const &[key, unused_value]{entry}; // avoid
(void)unused_value;
use(key);

auto const &[key, _]{entry};            // preferred
use(key);
```

### 13.9 Function grouping and spacing

- Keep function declarations grouped by purpose or other sensible arrangement.
- In implementation files, keep related function definitions adjacent with no blank separator when they are part of the same tight group (for example overload sets, closely related getters/setters, or small helper clusters for one feature).
- Otherwise, separate function definitions with one blank line.
- Do not write function definitions in single-line inline-body form; place function bodies on following lines.

Example:

```cpp
auto get_width() const->unsigned int {
  return width;
}
auto get_height() const->unsigned int {
  return height;
}
void set_width(unsigned int const value) {
  width = value;
}
void set_height(unsigned int const value) {
  height = value;
}

void rebuild_layout() {
  // separate function group
}
```

### 13.10 Declaration/definition ordering

- In implementation files, keep function definitions in the same order as declarations in the corresponding header.
- Reordering is allowed only when there is a technical reason (for example conditional compilation constraints, explicit template instantiation ordering, or unavoidable dependency constraints).

## 14. Classes, Structs, and OOP

### 14.1 `struct` vs `class` usage

- Use `struct` for passive data aggregates and simple transport/state objects.
- Use `class` when enforcing invariants/encapsulation and behaviour-rich APIs.

### 14.2 Access section ordering

- Access labels are not indented inside class definitions.

Example:

```cpp
class manager {
public:
  manager();

private:
  state current_state{};
};
```

### 14.3 Constructor and destructor conventions

- Prefer in-class member defaults plus constructor initialization lists.
- Do not add `explicit` mechanically to every single-argument constructor.
- Use `explicit` when implicit construction creates a credible risk of an unwanted or misleading conversion, such as when the argument could plausibly be mistaken for the constructed value at a call site.
- Omit `explicit` when the argument and constructed types have clearly distinct roles and implicit conversion presents no realistic risk of confusion or incorrect overload selection.
- Keep constructors focused on establishing valid object state.

Example:

```cpp
class percentage {
public:
  explicit percentage(float value);      // prevents accidental numeric conversion
};

class renderer {
public:
  renderer(platform_context &context);    // distinct types; no credible confusion
};
```

### 14.4 Rule of 0/3/5 defaults

- Prefer Rule of 0 where possible.
- If custom ownership/resource behaviour exists, define the appropriate special member set explicitly and consistently.

### 14.5 Inheritance formatting

- Keep inheritance declarations compact and readable:
  - `class child : public base {};`
- Prefer composition over inheritance when inheritance does not clearly model an is-a relationship.

### 14.6 Virtual and override conventions

- Use `override` on overridden virtual functions.
- Use `final` where extension is intentionally blocked.
- Polymorphic base classes should provide a virtual destructor.

## 15. Templates and Generic Code

### 15.1 Template declaration formatting

- Use compact template declaration formatting:
  - `template<typename T>`
- Keep template parameter lists readable; wrap only when needed.

### 15.2 Constraints/concepts style

- Prefer modern constraints/concepts when they improve correctness and readability.
- Avoid verbose SFINAE patterns when a modern alternative is available.

### 15.3 Specialization formatting

- Keep specializations visually close to primary templates where practical.
- Preserve the same naming and formatting style as primary template definitions.

### 15.4 Generic naming conventions

- Use conventional short template parameter naming (`T`, `FromT`, `Tcpp`, etc.) unless a semantic name improves clarity.

### 15.5 Modern C++ feature preference

- Prefer the most modern C++ feature set available to the target toolchain (C++23 baseline, C++26 adoption when viable).
- Prefer modern constructs over legacy equivalents when readability and maintainability improve.

## 16. Lambdas and Callables

### 16.1 Capture style

- Capture only what is needed.
- Prefer explicit captures over broad `[=]`/`[&]` in complex contexts.

### 16.2 Parameter and return formatting

- Follow normal function formatting for lambda parameters and return type annotations.
- Omit empty `()` for parameterless lambdas (modern C++ style).
- Do not place a space before a lambda body opening brace (`[]{`, `[](int v){}`).
- If a lambda has no trailing return type, keep the closing `]` or `)` (if used) attached directly to the opening brace:
  - `[](int value){`
- If a lambda has a trailing return type, keep `)->type` compact with no spaces around `->`, and keep the single ordinary body-opening space before `{`:
  - `[](size_t count)->float {`
- Do not specify a lambda trailing return type unless inference is insufficient or the annotation materially improves clarity.

Example:

```cpp
auto tick{[]{
  return true;
}};

auto percentage{[this](size_t count)->float {
  return total == 0 ? 0.0f : static_cast<float>(count) / static_cast<float>(total);
}};
```

### 16.3 Multi-line lambda formatting

- Use attached braces and normal block indentation.
- For lambdas passed inline, wrap arguments cleanly and keep the lambda body readable.

### 16.4 Callback conventions

- Use trailing return types on callbacks only when they are needed or materially improve clarity (especially in complex signatures).
- Keep callback side effects explicit and small where possible.

## 17. Attributes, Macros, and Preprocessor

### 17.1 Standard attributes (`[[nodiscard]]`, `[[maybe_unused]]`, `[[likely]]`)

- Use standard attributes where they communicate API intent or branch likelihood.
- Place attributes immediately before the declaration they modify.

### 17.2 Compiler attributes (`[[gnu::pure]]`, etc.)

- Compiler attributes are encouraged when likely to be useful.
- Prefer GNU-compatible attributes because target toolchains are GCC and Clang.
- Use non-GNU attribute forms only when strictly necessary for the task.

### 17.3 `#define` naming and scope

- Use `UPPER_SNAKE_CASE` macro names.
- Keep debug feature toggles local to file scope when possible.
- Avoid object-like macros for values that can be `constexpr`.

### 17.4 Conditional compilation (`#if`, `#ifdef`, `#ifndef`) style

- Use `#ifdef`/`#ifndef` for feature flags and debug toggles.
- Keep branch bodies indented to surrounding block style.

### 17.5 Indentation and comments for preprocessor blocks

- `#if`/`#ifdef` directives may be indented to match surrounding code.
- Close conditionals with explicit comments:
  - `#endif // DEBUG_FEATURE`

Example:

```cpp
#ifdef DEBUG_WEBGPU
  logger << "DEBUG: Adapter info: " << adapter_info.description;
#endif // DEBUG_WEBGPU
```

### 17.6 Debug flag conventions

- Use `DEBUG_*` naming for file/module debug toggles.
- Keep toggles near file top for discoverability.

### 17.7 Warning suppression with diagnostics pragmas

- Suppress warnings narrowly with `#pragma GCC diagnostic push/pop` around the smallest possible code region.
- Add an inline comment that explains why suppression is correct for that specific case.
- Do not suppress warnings globally or file-wide when local suppression is possible.

Example:

```cpp
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
if(lhs == rhs) return true;              // byte-stable float compare is required by protocol
#pragma GCC diagnostic pop
```

## 18. Comments and Documentation

### 18.1 Line comments vs block comments

- Prefer `//` comments over block comments for most code documentation.
- Use block comments sparingly (large doc blocks or temporarily disabled regions during active refactor).

### 18.2 API doc comments (`///`)

- Use `///` for concise API intent and behavioural notes.
- Function bodies should typically begin with a `///` documentary comment describing intent/behaviour.
- Function declarations should usually not repeat obvious intent comments; documentary comments belong primarily on definitions.
- In classes, prefer inline comments on data members over documentary comments on member declarations.
- Full doxygen-style comments are allowed.
- Doxygen parameter/return annotations are optional, but if used they must be correct and doxygen-compatible.

Example:

```cpp
void world_gui::draw(game_state &state) {
  /// Render the components of the world GUI for the current frame
  draw_windows(state);
}

/// @brief Parse command line arguments for the launcher
/// @param argc Number of arguments
/// @param argv Argument values
/// @return true if launch should continue
auto parse_args(int argc, char const *argv[])->bool {
  return argc > 0 && argv != nullptr;
}
```

### 18.3 Inline rationale comments

- Prefer same-line comments where appropriate.
- Avoid splitting comments across multiple lines unless the split improves clarity.
- Align inline comments to start at column 80 exactly.
- If code already extends beyond column 80, place comment one space after existing code rather than forcing a wrap.
- When documenting scope intent, place the comment on the opening brace line, not the closing brace line.
- Exception: closing-namespace comments (`} // namespace ...`) stay visually coupled to the closing brace.
- Exception: preprocessor closing comments (`#endif // ...`) stay visually coupled to the directive.
- In function bodies and inline code comments, start comments lowercase unless grammar/acronyms require otherwise.
- For one-line comments, avoid trailing punctuation to reduce visual noise.
- `///` documentary comments should start with capitalized text.
- Markdown examples in this document may use compact inline-comment spacing for viewport readability.
- Use shared `scripts` repo comment-alignment tools when possible:
  - `comment_aligner.sh`
  - `comment_aligner_project.sh`
  - `comment_aligner_cmake.sh`

Example:

```cpp
{                                        // temporary loading scope
  loader_context const ctx{build_loader_context()};
  run_loader(ctx);
}
```

### 18.4 TODO/FIXME/HACK conventions

- Keep TODO-style comments specific and actionable.
- Prefer brief reason + intent over vague placeholders.

### 18.5 Commented-out code policy

- Do not remove existing comments unless they add no value.
- Commented code may be kept intentionally when it conveys useful context (for example illustrative alternatives, defaulted options that may be toggled later, or documented integration variants).
- When commenting out code, place `//` directly before the code with no intervening space; the usual space after `//` applies to prose comments, not disabled code.
- If commented code is intentionally retained, keep it accurate and legible, and prefer a brief nearby note when intent is not obvious.
- Remove stale commented code only when it is clearly obsolete or misleading.

Example:

```cpp
// enable this while diagnosing renderer setup
//renderer.enable_debug_output();
```

## 19. Error Handling and Diagnostics

### 19.1 Exception use and catch formatting

- Catch formatting uses attached control parentheses (`catch(...)`).
- Catch by `const &` for exception types unless mutation is required.
- Keep catch handlers focused: log context, clean up, and return/propagate.
- When exception handling applies to the whole function body, prefer function-level try/catch syntax over wrapping the entire body in an inner `try` block.

Example:

```cpp
auto run_job_inner_try()->bool {          // avoid
  try {
    run();
    return true;
  } catch(std::exception const &e) {
    logger << "ERROR: run failed: " << e.what() << std::endl;
    return false;
  }
}

auto run_job()->bool try {                // preferred
  run();
  return true;
} catch(std::exception const &e) {
  logger << "ERROR: run failed: " << e.what() << std::endl;
  return false;
}
```

### 19.2 Error logging style

- Log failures with actionable context (operation, key identifiers, error text).
- Prefer consistent severity prefixes (`ERROR`, `WARNING`, `DEBUG`) matching project conventions.
- For console and file stream output, prefer `std::endl` over `'\n'`/`"\n"` by default.
- Use `'\n'`/`"\n"` only when there is a specific reason (for example avoiding flush behaviour or optimising high-volume output paths).

Example:

```cpp
logger << "ERROR: upload failed for chunk " << chunk_id << std::endl;
bulk_log << line << '\n'; // allowed when non-flushing output is intentional
```

### 19.3 Assertions and defensive checks

- Use assertions for invariants and programmer errors.
- Use runtime checks for recoverable invalid input/state.
- Prefer unique, clear assertion messages via `&& "..."` to make failures searchable without a debugger.

Example:

```cpp
assert(buffer_size > 0 && "buffer_size must be positive in upload path");
```

### 19.4 Failure message quality and consistency

- Keep messages specific and concise.
- Avoid ambiguous error text that omits operation context.
- Make error messages unique where practical so origin can be searched directly in code.

### 19.5 Program exit status from `main`

- When returning from `main`, use `EXIT_SUCCESS` for normal success and `EXIT_FAILURE` for generic failure.
- Do not return bare numeric `0` for success or bare numeric `1` for generic failure.
- Additional named/specific exit codes may be introduced where the program has a documented need to signal distinct outcomes.

Example:

```cpp
auto main()->int {
  if(!initialise()) {
    return EXIT_FAILURE;
  }

  run();
  return EXIT_SUCCESS;
}
```

## 20. CMakeLists.txt Style (Brief)

### 20.1 Indentation and list formatting

- Use 2-space indentation inside command argument blocks.
- Keep long lists (`set(...)`, `add_executable(...)`, warning flags) one item per line.
- Use lowercase CMake command names consistently.
- Keep CMake keywords/specifiers uppercase (`VERSION`, `PRIVATE`, `CACHE`, `STRING`, `STATUS`, `FATAL_ERROR`, `STREQUAL`, `ON`, `OFF`).
- Standardise boolean literals to `ON`/`OFF` for project CMake code.
- Avoid mixed boolean spellings (`True`/`False`, `YES`/`NO`, `1`/`0`) unless an external interface explicitly requires them.
- Keep no space between command name and `(` (`target_link_libraries(...)`, not `target_link_libraries (...)`).
- For multi-line calls, place the closing `)` on its own line.

### 20.2 Condition and build-type block style

- Normalise build type with `string(TOLOWER ...)` and branch on explicit values.
- Keep one responsibility per conditional block (build mode, exception mode, feature toggles).
- Emit clear `message(STATUS ...)` lines for selected mode.
- Prefer uppercase for cache/global CMake variables and lowercase snake_case for local helper variables.

Example:

```cmake
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(EXCEPTION_HANDLING wasm CACHE STRING "Exception handling mode")
string(TOLOWER "${CMAKE_BUILD_TYPE}" build_type)

if(build_type STREQUAL "debug")
  message(STATUS "debug build")
else()
  message(FATAL_ERROR "invalid build type: ${CMAKE_BUILD_TYPE}")
endif()
```

### 20.3 Target declarations and grouping

- Group sources by role using comments:
  - project-specific
  - shared/internal libs
  - third-party
- Keep `target_link_libraries`, `target_compile_options`, and `target_link_options` in separate clear blocks.
- Do not use source globbing for build targets; list files explicitly.
- In multi-line `target_link_libraries(...)`, keep visibility and library on the same line (`PRIVATE libname`).
- Keep target-related blocks adjacent and separated by a single blank line for scanability.

Example:

```cmake
add_executable(example_tool
  main.cpp
  app.cpp
  app.h
)

target_link_libraries(example_tool
  PRIVATE vectorstorm
  PRIVATE logstorm
)
```

### 20.4 Compile/link options layout

- Keep compile/link options in named variables when reused.
- Keep warnings and optimisation flags grouped with comment headers for readability.

## 21. Bash Script Style (Brief)

### 21.1 Shebang and safety defaults

- Use `#!/bin/bash`.
- Prefer explicit failure checks (`|| exit 1`) for critical commands.
- For user-facing build scripts, preserve existing explicit flow over implicit shell options.
- Avoid `set -euo pipefail`; prefer explicit error handling and clear control flow.

### 21.2 Variable naming and quoting

- Use lowercase snake_case for script variables (`build_dir`, `compiled_resources_total`).
- Quote variable expansions unless intentional word-splitting is required.

### 21.3 Conditionals, loops, and spacing

- Use `[ ... ]` condition style consistently with spaces around brackets.
- Use straightforward `for` loops over arrays for file batches.
- Keep condition bodies compact and readable.

### 21.4 Command failure handling

- Use `|| exit 1` on build-critical commands (`cmake`, `make`, resource compilation).
- Check presence of optional tools before use (`which ccache`, `which naga`).

### 21.5 Logging output conventions

- Use short, explicit `echo` status messages.
- Preserve the current convention of redirecting stdout to stderr for script status logs where used (`exec 1>&2`).

## 22. Revision Control

### 22.1 Line endings

- Commit text files with Unix line endings (`LF`) only.
- Configure Git to avoid unintended CRLF churn:
  - on Unix-like systems, prefer `git config --global core.autocrlf input`
- Repository-level attributes should enforce LF where practical.

### 22.2 Committed file permissions

- Non-executable committed files should use mode `664`.
- Executable files (scripts/tools intended to run directly) should use mode `775`.
- Do not set executable bits on non-executable source/data files.

### 22.3 Commit structure

- Commits should be frequent and atomic (ideally one feature or one tightly scoped change per commit).
- Avoid bundling unrelated changes in a single commit.

### 22.4 Commit message style

- Keep subject lines clear, brief, and specific.
- Commit messages should describe what changed, not how or why.
- No commit-signing requirement is imposed by this style guide.

## 23. Enforcement

- Human review enforces full adherence to this style guide.
- Any unresolved divergence from guide rules is a valid reason to reject the change.
- Automated checks should enforce deterministic style constraints:
  - tabs/trailing-whitespace rejection
  - include-order validation
  - `#pragma once` enforcement in first-party headers
  - C-style cast detection
  - forbidden `using namespace` (except allowed literal namespaces in source files)

### 23.1 Tools and automation

- This style-guide repository provides configuration files that can be copied into project repositories to enforce core formatting and repository-hygiene rules automatically.
- These configs define baseline behavior shared across Armchair projects and reduce per-project setup drift.

### 23.2 `.editorconfig`

- Used by: editors and IDEs with EditorConfig support (native or plugin-based).
- Enforces: indentation style/width, UTF-8 encoding, final newline, trailing-whitespace handling, plus file-type-specific overrides.
- How to use:
  - copy `.editorconfig` to the project root
  - ensure contributors have EditorConfig enabled in their editor
  - allow more-specific project overrides only when intentionally required

### 23.3 `.gitattributes`

- Used by: Git itself (working tree/commit normalization behavior).
- Enforces: repository text normalization defaults (`* text=auto`) so text content is committed consistently while checkout style follows contributor Git settings.
- How to use:
  - copy `.gitattributes` to the project root
  - keep project-specific exceptions in the same file only when required by that project's asset/toolchain needs

### 23.4 C++ autoformatters

- No general-purpose C++ autoformatter is currently adopted as an enforcement tool for this style guide.
- Reason: tool behavior (notably whitespace normalization and preprocessor-block handling) conflicts with required Armchair layout conventions, including intentional manual alignment.
- Use targeted automation scripts for specific checks instead (for example include ordering, `#pragma once`, banned constructs), with human review enforcing remaining style rules.

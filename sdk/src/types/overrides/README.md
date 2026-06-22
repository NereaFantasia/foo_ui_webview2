# `@codegen-override` Registry

Hand-written type definitions consumed by `scripts/gen_sdk_types.mjs --all`
when the auto-generator under `sdk/src/types/generated/` cannot infer the
correct shape from the Phase A C++ schema alone.

> **Authority document**: `docs/execution/sdk-codegen/SDK_CODEGEN_PLAN.md`
> §7.3.2 (override mechanism) + §11.4 (when to override).

## When to add an override

Reach for an override only when the auto-generated type would be **wrong**
or **unusably loose** for downstream consumers, e.g.

| Symptom in `code-index.json`                                         | Why an override helps |
|----------------------------------------------------------------------|-----------------------|
| All fields land as `call:basic_json` (Phase A saw `.toJson()` calls) | Hand-written type can describe the actual JSON shape the helper produces. |
| The C++ side packs JSON-`object`/`array` placeholders with no items_schema | Override enumerates the real key set / element shape. |
| The C++ shape is generic (e.g. `state.value` may be any JSON)        | Override exposes a TS generic parameter (`SharedStateChange<T>`). |
| The runtime shape is a discriminated union (different keys per case) | Override emits a TS `|`-union; the auto-generator only handles flat structs. |

If none of those apply, **prefer fixing the auto-generator** (better
mapping in `scripts/graph/lib/cpp-to-ts-mapping.mjs` / Phase A extractor)
instead of pinning a hand-written type. Overrides are an escape hatch, not
the default path.

## Author guide

Every override block must satisfy three rules. The generator enforces all
three at run time and will fail with a non-zero exit code otherwise.

### 1. Pin the override target

Each `export interface` / `export type` declaration is preceded by a JSDoc
block with exactly one `@codegen-override` tag:

```ts
/**
 * @codegen-override <kind>:<name>
 */
export interface XyzPayload { /* ... */ }
```

`<kind>` must be one of:

- `params`   — handler parameter shape (key matches `cpp_api_handler.metadata.api_name`)
- `response` — handler response shape (same key as above)
- `event`    — emitted event payload (key matches `cpp_api_event.metadata.event_name`)

`<name>` uses the *exact* C++ surface key — dots for handler names
(`playback.setPosition`), colons for event names (`metadb:changed`).

### 2. Pin a signature snapshot

Same JSDoc block must also carry a `@codegen-snapshot` line whose payload
encodes the C++ schema's *shape class* signature at the time the override
was written. The generator recomputes the signature on every run; a
mismatch produces a hard error so we never silently keep an outdated
override around.

```text
@codegen-snapshot key1:class,key2:class,...
```

Where `class` is one of:

| Class       | Triggered by                                                |
|-------------|-------------------------------------------------------------|
| `primitive` | `bool` / `int` / `int64` / `uint32` / `float` / `double` / `string` / `null` / `enum<...>` |
| `array`     | `array` / `array<T>`                                        |
| `object`    | `object` / `map<...>`                                       |
| `callexpr`  | `call:*` (Phase A saw a function call but could not see the return type) |
| `unknown`   | Schema entry has no `type` field, or `type === "unknown"`   |
| Variant     | `variant<a|b|...>` collapses to the *widest* member class. |

Keys are sorted alphabetically and joined with commas. Whitespace is
ignored. The generator only cares about the shape class — adding optional
markers, swapping `array<int>` for `array<string>`, etc. does **not**
invalidate the snapshot. Adding or removing keys, or moving a key from
`primitive` to `array`, does.

To regenerate the snapshot after an intentional C++ schema change:

```bash
node scripts/gen_sdk_types.mjs --all --fix-override-stale
```

The flag rewrites the `@codegen-snapshot` tag in place, leaving the
interface body untouched. Always commit the snapshot update together with
the C++ change that motivated it.

### 3. Keep the override interface ergonomically named

The `interface` / `type` name immediately following the JSDoc block must
match the name the auto-generator would have produced — see
`makeApiInterfaceName` / `makeEventInterfaceName` in
`scripts/graph/lib/cpp-to-ts-mapping.mjs`. The override is then re-exported
verbatim from `sdk/src/types/generated/{params,responses,events}.ts`, so
the public surface is unchanged.

For example:

| `<kind>:<name>`           | Required interface name           |
|---------------------------|------------------------------------|
| `event:metadb:changed`    | `MetadbChangedPayload`             |
| `params:playback.setPosition` | `PlaybackSetPositionParams`     |
| `response:library.search` | `LibrarySearchResponse`            |

Mismatched names are accepted by the registry but break Phase D, which
expects the override symbols to satisfy the typed `bridge.invoke<M>` /
`fb.on<E>` overload set. The CI gate (`audit:all`) will surface the
mismatch as an unresolved import.

## File layout

Group overrides by C++ surface namespace, not by override kind. One file
per namespace keeps the generated re-export header tidy:

```
overrides/
├── events.ts     # event:metadb:changed, event:state:changed, ...
├── playback.ts   # params/response overrides for `playback.*` (if any)
├── library.ts    # ...
└── README.md     # this file
```

The generator scans every `.ts` file under this directory recursively;
filename is purely organisational.

## Local validation

```bash
# Verify all overrides match current C++ schemas (no write)
node scripts/gen_sdk_types.mjs --all --diff

# Refresh stale snapshots in place
node scripts/gen_sdk_types.mjs --all --fix-override-stale

# Full sanity check (write generated/ + tsc --noEmit)
node scripts/gen_sdk_types.mjs --all --validate
```

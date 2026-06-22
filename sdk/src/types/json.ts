/**
 * `JsonValue` — canonical recursive type for any JSON-serialisable value
 * exchanged with the C++ bridge (`nlohmann::json`).
 *
 * Use this type for API contracts that genuinely accept arbitrary
 * JSON-compatible payloads (e.g. SMP `window.GetProperty`,
 * `config.get(default)`, opaque user-defined properties). Prefer
 * concrete response interfaces everywhere else — `JsonValue` is only a
 * correct choice when the value shape is truly dynamic at runtime.
 */
export type JsonValue =
    | string
    | number
    | boolean
    | null
    | undefined
    | JsonValue[]
    | { [key: string]: JsonValue };

/**
 * Object form of `JsonValue` (shorthand for `{ [k: string]: JsonValue }`).
 *
 * `undefined` is included in `JsonValue` so TypeScript optional fields
 * (`event?: string` ≡ `string | undefined`) remain compatible with
 * `extends JsonObject`. `undefined` properties are dropped by
 * `JSON.stringify` at the bridge boundary, matching JSON semantics.
 */
export type JsonObject = { [key: string]: JsonValue };

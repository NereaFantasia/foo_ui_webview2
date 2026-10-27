/**
 * Type definitions for MCP tool declarations.
 */

/** Top-level input contract for a tool or an individual action. */
export interface InputSchema {
    type: "object";
    /** Map of argument names to their respective schemas. */
    properties: Record<string, SchemaProperty>;
    /** Names of required arguments; omitted when all arguments are optional. */
    required?: string[];
}

/**
 * Semantic hints describing tool behavior as MCP tool annotations.
 * `destructiveHint` and `idempotentHint` apply only when `readOnlyHint` is `false`.
 */
export interface ToolAnnotations {
    /** Indicates that the tool does not modify state. */
    readOnlyHint: boolean;
    /** Indicates that actions may delete or overwrite persisted user data. */
    destructiveHint?: boolean;
    /** Indicates that repeating a call with the same arguments produces no additional side effects. */
    idempotentHint?: boolean;
    /** Indicates that the tool interacts with resources outside the foobar2000 instance. */
    openWorldHint: boolean;
}

/** Single host method callable by a bridge tool. */
export interface ToolAction {
    /**
     * Exact input contract and defaults for the method. Arguments are
     * validated against this schema prior to invocation, regardless of what
     * the tool-level schema permits.
     */
    inputSchema: InputSchema;
    /**
     * Top-level result field containing an image as a `data:<mime>;base64,`
     * URL. When present, the image is extracted and returned as an MCP image
     * block, while remaining fields are returned as JSON text.
     */
    image?: string;
}

/**
 * Declarative definition of a bridge tool, detailing the host methods invoked
 * via the `action` argument and the schema exposed to MCP clients.
 */
export interface ToolDefinition {
    /** Unique tool name exposed to MCP clients (e.g. `fb2k_playback_read`). */
    name: string;
    /** Human-readable description presented to MCP clients, detailing available actions. */
    description: string;
    /** Semantic annotations describing the tool's behavior and side effects. */
    annotations: ToolAnnotations;
    /**
     * Aggregated input schema exposed to clients, combining the `action`
     * discriminator and parameter schemas across all actions.
     */
    inputSchema: InputSchema;
    /** Host methods keyed by `namespace.method`, in their declared order. */
    actions: Readonly<Record<string, ToolAction>>;
}

/** JSON value accepted by MCP tool defaults and bridge arguments. */
export type JsonValue =
    | null
    | boolean
    | number
    | string
    | JsonValue[]
    | { [key: string]: JsonValue };

interface SchemaPropertyBase {
    /** Human-readable description of the property. */
    description?: string;
    /** Default value applied when the argument is omitted. */
    default?: JsonValue;
}

/** A JSON-Schema string property. */
export interface StringSchemaProperty extends SchemaPropertyBase {
    type: "string";
    /** Allowed values when the property is an enumeration. */
    enum?: string[];
    /** Minimum character count accepted; ignored when {@link enum} is defined. */
    minLength?: number;
}

/** A JSON-Schema numeric property. */
export interface NumberSchemaProperty extends SchemaPropertyBase {
    type: "number" | "integer";
    /** Inclusive lower bound for numeric types. */
    minimum?: number;
    /** Inclusive upper bound for numeric types. */
    maximum?: number;
}

/** A JSON-Schema boolean property. */
export interface BooleanSchemaProperty extends SchemaPropertyBase {
    type: "boolean";
}

/** A union of JSON-Schema property alternatives. */
export interface UnionSchemaProperty extends SchemaPropertyBase {
    type: "union";
    /** Alternative property schemas (at least two). */
    anyOf: SchemaProperty[];
}

/** A JSON-Schema array property. */
export interface ArraySchemaProperty extends SchemaPropertyBase {
    type: "array";
    /** Element schema when {@link SchemaProperty.type} is `"array"`. */
    items: SchemaProperty;
    /** Minimum element count accepted. */
    minItems?: number;
}

/** Arbitrary JSON value passed directly to the bridge without transformation. */
export interface JsonSchemaProperty extends SchemaPropertyBase {
    type: "json";
}

/** A JSON-Schema object property. */
export interface ObjectSchemaProperty extends SchemaPropertyBase {
    type: "object";
    /** Nested properties when {@link SchemaProperty.type} is `"object"`. */
    properties?: Record<string, SchemaProperty>;
    /** Required nested properties for object schemas. */
    required?: string[];
    /** Validation policy or value schema for undeclared object properties. */
    additionalProperties?: boolean | SchemaProperty;
}

/** Recursive JSON-Schema subset used by MCP tool declarations. */
export type SchemaProperty =
    | StringSchemaProperty
    | NumberSchemaProperty
    | BooleanSchemaProperty
    | UnionSchemaProperty
    | ArraySchemaProperty
    | ObjectSchemaProperty
    | JsonSchemaProperty;

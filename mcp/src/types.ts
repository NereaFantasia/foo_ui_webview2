/**
 * Type definitions for MCP tool declarations.
 */

/** The top-level input contract of a tool or of one of its actions. */
export interface InputSchema {
    type: "object";
    /** Map of argument name to its schema. */
    properties: Record<string, SchemaProperty>;
    /** Names of required arguments; omitted when all are optional. */
    required?: string[];
}

/**
 * Hints about what a tool does, as MCP tool annotations. `destructiveHint` and
 * `idempotentHint` are meaningful only when `readOnlyHint` is `false`.
 */
export interface ToolAnnotations {
    /** The tool changes nothing. */
    readOnlyHint: boolean;
    /** Some action may remove or overwrite data the user keeps. */
    destructiveHint?: boolean;
    /** Repeating a call with the same arguments has no further effect. */
    idempotentHint?: boolean;
    /** The tool reaches beyond the foobar2000 instance. */
    openWorldHint: boolean;
}

/** One host method a bridge tool can call. */
export interface ToolAction {
    /**
     * The method's exact input contract, with its defaults. Arguments are
     * checked against it before the call, whatever the tool-level schema lets
     * through.
     */
    inputSchema: InputSchema;
    /**
     * The top-level result field that holds a picture as a
     * `data:<mime>;base64,` URL. The picture is sent as an MCP image and the
     * other fields as JSON text.
     */
    image?: string;
}

/**
 * Declarative definition of a bridge tool: the host methods it calls, picked
 * by the `action` argument, and the schema clients see.
 */
export interface ToolDefinition {
    /** Unique tool name exposed to MCP clients (e.g. `fb2k_playback_read`). */
    name: string;
    /** Human-readable summary shown to MCP clients, listing every action. */
    description: string;
    /** What the tool's actions do. */
    annotations: ToolAnnotations;
    /**
     * The schema clients see: `action` plus every action's parameters, with
     * each parameter accepting what any action using it accepts.
     */
    inputSchema: InputSchema;
    /** The host methods, keyed by `namespace.method`, in the order the tool lists them. */
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
    /** Default applied when the argument is omitted. */
    default?: JsonValue;
}

/** A JSON-Schema string property. */
export interface StringSchemaProperty extends SchemaPropertyBase {
    type: "string";
    /** Allowed values when the property is an enumeration. */
    enum?: string[];
    /** Fewest characters accepted; ignored when {@link enum} is set. */
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
    /** At least two alternative property schemas. */
    anyOf: SchemaProperty[];
}

/** A JSON-Schema array property. */
export interface ArraySchemaProperty extends SchemaPropertyBase {
    type: "array";
    /** Element schema when {@link SchemaProperty.type} is `"array"`. */
    items: SchemaProperty;
    /** Fewest elements accepted. */
    minItems?: number;
}

/** Any JSON value, passed to the bridge unchanged. */
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
    /** Policy or value schema for undeclared object keys. */
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

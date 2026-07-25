/**
 * Type definitions for MCP tool declarations.
 */

/**
 * Declarative definition of a single MCP tool: its name, human-readable
 * description, and JSON-Schema input contract.
 */
export interface ToolDefinition {
    /** Unique tool name exposed to MCP clients (e.g. `fb2k_playback_play`). */
    name: string;
    /** Human-readable summary shown to MCP clients. */
    description: string;
    /** JSON-Schema describing the tool's input arguments. */
    inputSchema: {
        type: "object";
        /** Map of argument name to its schema. */
        properties: Record<string, SchemaProperty>;
        /** Names of required arguments; omitted when all are optional. */
        required?: string[];
    };
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
    | ObjectSchemaProperty;

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

/**
 * A single JSON-Schema property used within a tool's input schema.
 */
export interface SchemaProperty {
    /** JSON-Schema type of this property. */
    type: "string" | "number" | "boolean" | "integer" | "array" | "object";
    /** Human-readable description of the property. */
    description?: string;
    /** Allowed values when the property is an enumeration. */
    enum?: string[];
    /** Default applied when the argument is omitted. */
    default?: unknown;
    /** Inclusive lower bound for numeric types. */
    minimum?: number;
    /** Inclusive upper bound for numeric types. */
    maximum?: number;
    /** Element schema when {@link SchemaProperty.type} is `"array"`. */
    items?: SchemaProperty;
}

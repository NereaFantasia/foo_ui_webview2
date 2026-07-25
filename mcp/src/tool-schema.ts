import { z } from "zod";

import type {
    NumberSchemaProperty,
    ObjectSchemaProperty,
    SchemaProperty,
    StringSchemaProperty,
    ToolDefinition,
    UnionSchemaProperty,
} from "./types.js";

/**
 * Build the Zod property shape registered for an MCP bridge tool.
 *
 * Defaults are validated and injected for optional properties. Malformed
 * declarations fail before the MCP server starts.
 *
 * @param inputSchema - Declarative input contract for one MCP tool.
 * @returns A Zod raw shape suitable for `McpServer.registerTool()`.
 * @throws When required names, keyword/type combinations, defaults, bounds,
 * enums, arrays, or nested schemas are invalid.
 */
export function buildToolInputShape(
    inputSchema: ToolDefinition["inputSchema"]
): Record<string, z.ZodTypeAny> {
    validateInputSchemaDeclaration(inputSchema);
    return buildObjectShape(
        inputSchema.properties,
        inputSchema.required,
        "root",
        new WeakMap<object, string>()
    );
}

/**
 * Build the complete top-level schema registered for an MCP bridge tool.
 *
 * Undeclared top-level arguments are preserved because the bridge runtime is
 * the authority for optional and forward-compatible parameters.
 *
 * @param inputSchema - Declarative input contract for one MCP tool.
 * @returns A loose Zod object that validates declared arguments.
 * @throws When the input declaration is malformed.
 */
export function buildToolInputSchema(
    inputSchema: ToolDefinition["inputSchema"]
): z.ZodObject<Record<string, z.ZodTypeAny>> {
    return addRuntimeObjectSafetyCheck(
        z.looseObject(buildToolInputShape(inputSchema)),
        "root"
    );
}

function buildObjectShape(
    properties: Record<string, SchemaProperty>,
    requiredNames?: string[],
    path = "object",
    activePaths = new WeakMap<object, string>()
): Record<string, z.ZodTypeAny> {
    const required = new Set(requiredNames ?? []);
    if (required.size !== (requiredNames ?? []).length) {
        throw new Error(`${path}.required must not contain duplicate names`);
    }
    for (const name of required) {
        if (!Object.hasOwn(properties, name)) {
            throw new Error(`${path}.required references undeclared property '${name}'`);
        }
    }

    const shape = Object.create(null) as Record<string, z.ZodTypeAny>;
    for (const [name, property] of Object.entries(properties)) {
        const propertyPath = `${path}.${name}`;
        const baseSchema = buildPropertySchema(property, propertyPath, activePaths);
        shape[name] = applyPropertyDefault(
            property,
            baseSchema,
            required.has(name),
            propertyPath
        );
    }

    return shape;
}

function applyPropertyDefault(
    property: SchemaProperty,
    baseSchema: z.ZodTypeAny,
    required: boolean,
    path: string
): z.ZodTypeAny {
    const descriptor = Object.getOwnPropertyDescriptor(property, "default");
    if (!descriptor) {
        return required ? baseSchema : baseSchema.optional();
    }
    if (required) {
        throw new Error(`${path} cannot be required and declare a default`);
    }
    if (!("value" in descriptor) || !descriptor.enumerable) {
        throw new Error(`default for ${path} must be a plain data property`);
    }
    validateJsonValue(descriptor.value, `default for ${path}`);
    const parsedDefault = baseSchema.safeParse(descriptor.value);
    if (!parsedDefault.success) {
        throw new Error(`default for ${path} does not satisfy its schema`);
    }
    return baseSchema.default(parsedDefault.data);
}

function buildPropertySchema(
    property: SchemaProperty,
    path: string,
    activePaths: WeakMap<object, string>
): z.ZodTypeAny {
    const existingPath = activePaths.get(property);
    if (existingPath !== undefined) {
        throw new Error(`${path} contains a circular schema reference to ${existingPath}`);
    }
    activePaths.set(property, path);
    try {
        return buildPropertySchemaUnchecked(property, path, activePaths);
    } finally {
        activePaths.delete(property);
    }
}

function buildPropertySchemaUnchecked(
    property: SchemaProperty,
    path: string,
    activePaths: WeakMap<object, string>
): z.ZodTypeAny {
    if (!isKnownPropertyType((property as { type?: unknown }).type)) {
        throw new Error(`${path}.type is unsupported: ${formatDeclarationValue((property as { type?: unknown }).type)}`);
    }
    validatePropertyKeywords(property, path);
    let schema: z.ZodTypeAny;
    switch (property.type) {
        case "string":
            schema = buildStringSchema(property, path);
            break;
        case "number":
            schema = buildNumberSchema(property, false, path);
            break;
        case "integer":
            schema = buildNumberSchema(property, true, path);
            break;
        case "boolean":
            schema = z.boolean();
            break;
        case "union":
            schema = buildUnionSchema(property, path, activePaths);
            break;
        case "array":
            if (!property.items) {
                throw new Error(`${path} array must declare items`);
            }
            schema = z.array(buildPropertySchema(property.items, `${path}[]`, activePaths));
            break;
        case "object":
            schema = buildObjectSchema(property, path, activePaths);
            break;
    }

    return property.description ? schema.describe(property.description) : schema;
}

function buildUnionSchema(
    property: UnionSchemaProperty,
    path: string,
    activePaths: WeakMap<object, string>
): z.ZodTypeAny {
    if (property.anyOf.length < 2) {
        throw new Error(`${path}.anyOf must contain at least two alternatives`);
    }
    return z.union(
        property.anyOf.map((alternative, index) =>
            buildPropertySchema(alternative, `${path}.anyOf[${index}]`, activePaths)
        ) as [z.ZodTypeAny, z.ZodTypeAny, ...z.ZodTypeAny[]]
    );
}

function buildStringSchema(
    property: StringSchemaProperty,
    path: string
): z.ZodTypeAny {
    if (property.enum === undefined) {
        return z.string();
    }
    const values = [...new Set(property.enum)];
    if (values.length === 0) {
        throw new Error(`${path}.enum must contain at least one value`);
    }
    if (values.length !== property.enum.length) {
        throw new Error(`${path}.enum values must be unique`);
    }
    return z.enum(values as [string, ...string[]]);
}

function buildNumberSchema(
    property: NumberSchemaProperty,
    integer: boolean,
    path: string
): z.ZodNumber {
    validateNumericBounds(property.minimum, property.maximum, path);
    let schema = integer ? z.number().int() : z.number();
    if (property.minimum !== undefined) {
        schema = schema.min(property.minimum);
    }
    if (property.maximum !== undefined) {
        schema = schema.max(property.maximum);
    }
    return schema;
}

function buildObjectSchema(
    property: ObjectSchemaProperty,
    path: string,
    activePaths: WeakMap<object, string>
): z.ZodTypeAny {
    const shape = buildObjectShape(
        property.properties ?? {},
        property.required,
        path,
        activePaths
    );
    if (property.additionalProperties === false) {
        return addRawObjectSafetyCheck(z.strictObject(shape), path);
    }
    if (
        property.additionalProperties &&
        typeof property.additionalProperties === "object"
    ) {
        return addRawObjectSafetyCheck(
            z.object(shape).catchall(
                buildPropertySchema(property.additionalProperties, `${path}.*`, activePaths)
            ),
            path
        );
    }
    return addRawObjectSafetyCheck(z.looseObject(shape), path);
}

function addRawObjectSafetyCheck(
    schema: z.ZodTypeAny,
    path: string
): z.ZodTypeAny {
    return z.preprocess((value, context) => {
        const issue = findRuntimeValueIssue(value, path);
        if (issue) {
            context.addIssue({ code: "custom", message: issue });
            return z.NEVER;
        }
        return value;
    }, schema);
}

function addRuntimeObjectSafetyCheck<T extends z.ZodObject<Record<string, z.ZodTypeAny>>>(
    schema: T,
    path: string
): T {
    return schema.superRefine((value, context) => {
        const issue = findRuntimeValueIssue(value, path);
        if (issue) {
            context.addIssue({ code: "custom", message: issue });
        }
    });
}

function validateNumericBounds(
    minimum: number | undefined,
    maximum: number | undefined,
    path: string
): void {
    if (minimum !== undefined && !Number.isFinite(minimum)) {
        throw new Error(`${path}.minimum must be finite`);
    }
    if (maximum !== undefined && !Number.isFinite(maximum)) {
        throw new Error(`${path}.maximum must be finite`);
    }
    if (
        minimum !== undefined &&
        maximum !== undefined &&
        minimum > maximum
    ) {
        throw new Error(`${path}.minimum cannot exceed maximum`);
    }
}

function validateInputSchemaDeclaration(inputSchema: unknown): asserts inputSchema is ToolDefinition["inputSchema"] {
    if (!isRecord(inputSchema)) {
        throw new Error("root must be an object schema");
    }
    const allowed = new Set(["type", "properties", "required"]);
    for (const key of Object.keys(inputSchema)) {
        if (!allowed.has(key)) {
            throw new Error(`root.${key} is not valid for an input object schema`);
        }
    }
    if (inputSchema.type !== "object") {
        throw new Error(`root.type must be 'object', received ${formatDeclarationValue(inputSchema.type)}`);
    }
    validateProperties(inputSchema.properties, "root.properties");
    validateRequired(inputSchema.required, "root.required");
}

function isKnownPropertyType(type: unknown): type is SchemaProperty["type"] {
    return type === "string"
        || type === "number"
        || type === "integer"
        || type === "boolean"
    || type === "union"
        || type === "array"
        || type === "object";
}

function validatePropertyKeywords(property: SchemaProperty, path: string): void {
    const record = property as unknown as Record<string, unknown>;
    const allowed = new Set([
        "type",
        "description",
        "default",
        ...(property.type === "union" ? ["anyOf"] : []),
        ...(property.type === "string" ? ["enum"] : []),
        ...(property.type === "number" || property.type === "integer"
            ? ["minimum", "maximum"]
            : []),
        ...(property.type === "array" ? ["items"] : []),
        ...(property.type === "object"
            ? ["properties", "required", "additionalProperties"]
            : []),
    ]);
    for (const key of Object.keys(record)) {
        if (!allowed.has(key)) {
            throw new Error(`${path}.${key} is not valid for type '${property.type}'`);
        }
    }
    if (record.description !== undefined && typeof record.description !== "string") {
        throw new Error(`${path}.description must be a string`);
    }
    if (property.type === "string" && property.enum !== undefined) {
        if (!Array.isArray(property.enum) || Array.from(property.enum).some((value) => typeof value !== "string")) {
            throw new Error(`${path}.enum must be an array of strings`);
        }
    }
    if (property.type === "union") {
        if (!Array.isArray(property.anyOf) || property.anyOf.length < 2) {
            throw new Error(`${path}.anyOf must contain at least two property schemas`);
        }
        for (const [index, alternative] of property.anyOf.entries()) {
            if (!isRecord(alternative)) {
                throw new Error(`${path}.anyOf[${index}] must be a property schema`);
            }
        }
    }
    if (property.type === "number" || property.type === "integer") {
        if (property.minimum !== undefined && typeof property.minimum !== "number") {
            throw new Error(`${path}.minimum must be a number`);
        }
        if (property.maximum !== undefined && typeof property.maximum !== "number") {
            throw new Error(`${path}.maximum must be a number`);
        }
    }
    if (property.type === "array") {
        if (!isRecord(property.items)) {
            throw new Error(`${path}.items must be a property schema`);
        }
    }
    if (property.type === "object") {
        if (property.properties !== undefined) {
            validateProperties(property.properties, `${path}.properties`);
        }
        validateRequired(property.required, `${path}.required`);
        if (
            property.additionalProperties !== undefined
            && typeof property.additionalProperties !== "boolean"
            && !isRecord(property.additionalProperties)
        ) {
            throw new Error(`${path}.additionalProperties must be a boolean or property schema`);
        }
    }
}

function validateProperties(value: unknown, path: string): asserts value is Record<string, SchemaProperty> {
    if (!isRecord(value)) {
        throw new Error(`${path} must be an object`);
    }
    for (const [name, property] of Object.entries(value)) {
        if (isPrototypeSensitiveName(name)) {
            throw new Error(`${path}.${name} uses a prototype-sensitive property name`);
        }
        if (!isRecord(property)) {
            throw new Error(`${path}.${name} must be a property schema`);
        }
    }
}

function validateRequired(value: unknown, path: string): asserts value is string[] | undefined {
    if (value === undefined) {
        return;
    }
    if (!Array.isArray(value) || Array.from(value).some((name) => typeof name !== "string")) {
        throw new Error(`${path} must be an array of strings`);
    }
}

function isRecord(value: unknown): value is Record<string, unknown> {
    if (value === null || typeof value !== "object" || Array.isArray(value)) {
        return false;
    }
    const prototype = Object.getPrototypeOf(value);
    return prototype === Object.prototype || prototype === null;
}

/**
 * Find an object key or cycle that cannot safely cross the JSON bridge.
 *
 * @param value - Runtime value to inspect without cloning it.
 * @param path - Human-readable root path used in validation errors.
 * @returns A path-bearing issue, or undefined when the value is safe.
 */
export function findRuntimeValueIssue(
    value: unknown,
    path = "root"
): string | undefined {
    return findRuntimeValueIssueRecursive(value, path, new WeakMap<object, string>());
}

/**
 * Find prototype-sensitive keys in a pre-serialization SDK message object.
 *
 * Undefined internal fields are allowed here because stdio JSON serialization
 * omits them before the strict raw-message validation boundary.
 */
export function findUnsafeObjectStructureIssue(
    value: unknown,
    path = "root"
): string | undefined {
    return findUnsafeObjectStructureIssueRecursive(
        value,
        path,
        new WeakMap<object, string>()
    );
}

function findUnsafeObjectStructureIssueRecursive(
    value: unknown,
    path: string,
    activePaths: WeakMap<object, string>
): string | undefined {
    if (value === null || typeof value !== "object") {
        return undefined;
    }
    const existingPath = activePaths.get(value);
    if (existingPath !== undefined) {
        return `${path} contains a circular value reference to ${existingPath}`;
    }
    activePaths.set(value, path);
    try {
        const array = Array.isArray(value);
        const prototype = Object.getPrototypeOf(value);
        if (array ? prototype !== Array.prototype : prototype !== Object.prototype && prototype !== null) {
            return `${path} must be a plain JSON ${array ? "array" : "object"}`;
        }
        for (const key of Reflect.ownKeys(value)) {
            if (array && key === "length") {
                continue;
            }
            if (typeof key !== "string") {
                return `${path} must not contain symbol properties`;
            }
            const keyPath = array ? `${path}[${key}]` : `${path}.${key}`;
            if (isPrototypeSensitiveName(key)) {
                return `${keyPath} uses a prototype-sensitive property name`;
            }
            const descriptor = Object.getOwnPropertyDescriptor(value, key);
            if (!descriptor || !("value" in descriptor)) {
                return `${keyPath} must be a plain data property`;
            }
            const issue = findUnsafeObjectStructureIssueRecursive(
                descriptor.value,
                keyPath,
                activePaths
            );
            if (issue) {
                return issue;
            }
        }
        return undefined;
    } finally {
        activePaths.delete(value);
    }
}

function findRuntimeValueIssueRecursive(
    value: unknown,
    path: string,
    activePaths: WeakMap<object, string>
): string | undefined {
    if (value === null || typeof value === "string" || typeof value === "boolean") {
        return undefined;
    }
    if (typeof value === "number") {
        return Number.isFinite(value)
            ? undefined
            : `${path} must contain only finite JSON numbers`;
    }
    if (typeof value !== "object") {
        return `${path} must be a JSON value`;
    }
    const existingPath = activePaths.get(value);
    if (existingPath !== undefined) {
        return `${path} contains a circular value reference to ${existingPath}`;
    }
    activePaths.set(value, path);
    try {
        return Array.isArray(value)
            ? findJsonArrayIssue(value, path, activePaths)
            : findJsonObjectIssue(value, path, activePaths);
    } finally {
        activePaths.delete(value);
    }
}

function findJsonArrayIssue(
    value: unknown[],
    path: string,
    activePaths: WeakMap<object, string>
): string | undefined {
    if (Object.getPrototypeOf(value) !== Array.prototype) {
        return `${path} must be a plain JSON array`;
    }
    const allowedKeys = new Set(["length"]);
    for (let index = 0; index < value.length; index += 1) {
        allowedKeys.add(String(index));
        if (!Object.hasOwn(value, index)) {
            return `${path}[${index}] must not be a sparse array item`;
        }
    }
    if (Reflect.ownKeys(value).some((key) => !allowedKeys.has(String(key)))) {
        return `${path} must not contain non-JSON array properties`;
    }
    return findJsonPropertyIssue(value, path, activePaths, true);
}

function findJsonObjectIssue(
    value: object,
    path: string,
    activePaths: WeakMap<object, string>
): string | undefined {
    const prototype = Object.getPrototypeOf(value);
    if (prototype !== Object.prototype && prototype !== null) {
        return `${path} must be a plain JSON object`;
    }
    return findJsonPropertyIssue(value, path, activePaths, false);
}

function findJsonPropertyIssue(
    value: object,
    path: string,
    activePaths: WeakMap<object, string>,
    array: boolean
): string | undefined {
    for (const key of Reflect.ownKeys(value)) {
        if (array && key === "length") {
            continue;
        }
        if (typeof key !== "string") {
            return `${path} must not contain symbol properties`;
        }
        const keyPath = array ? `${path}[${key}]` : `${path}.${key}`;
        const issue = findJsonDataPropertyIssue(value, key, keyPath, activePaths);
        if (issue) {
            return issue;
        }
    }
    return undefined;
}

function findJsonDataPropertyIssue(
    value: object,
    key: string,
    path: string,
    activePaths: WeakMap<object, string>
): string | undefined {
    if (isPrototypeSensitiveName(key)) {
        return `${path} uses a prototype-sensitive property name`;
    }
    const descriptor = Object.getOwnPropertyDescriptor(value, key);
    if (!descriptor || !("value" in descriptor) || !descriptor.enumerable) {
        return `${path} must be a plain JSON data property`;
    }
    return findRuntimeValueIssueRecursive(descriptor.value, path, activePaths);
}

function validateJsonValue(
    value: unknown,
    path: string
): void {
    const issue = findRuntimeValueIssue(value, path);
    if (issue) {
        throw new TypeError(issue);
    }
}

function isPrototypeSensitiveName(name: string): boolean {
    return name === "__proto__" || name === "prototype" || name === "constructor";
}

function formatDeclarationValue(value: unknown): string {
    if (typeof value === "string") {
        return value;
    }
    try {
        const serialized = JSON.stringify(value);
        return serialized ?? String(value);
    } catch {
        return Object.prototype.toString.call(value);
    }
}

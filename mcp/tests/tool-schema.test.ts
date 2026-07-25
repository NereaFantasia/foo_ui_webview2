import fs from "node:fs";

import { Client } from "@modelcontextprotocol/sdk/client/index.js";
import { InMemoryTransport } from "@modelcontextprotocol/sdk/inMemory.js";
import { McpServer } from "@modelcontextprotocol/sdk/server/mcp.js";
import { describe, expect, it, vi } from "vitest";

import { guardBridgeTransport, registerBridgeTools } from "../src/bridge-tools.js";
import { buildToolInputSchema, buildToolInputShape } from "../src/tool-schema.js";
import { artworkTools } from "../src/tools/artwork.js";
import { libraryTools } from "../src/tools/library.js";
import { metadataMethodMap, metadataTools } from "../src/tools/metadata.js";
import { playbackMethodMap, playbackTools } from "../src/tools/playback.js";
import { playbackExtMethodMap, playbackExtTools } from "../src/tools/playback-ext.js";
import { playlistTools } from "../src/tools/playlist.js";
import { playlistExtTools } from "../src/tools/playlist-ext.js";
import { queueTools } from "../src/tools/queue.js";
import type { ToolDefinition } from "../src/types.js";

function parseInput(inputSchema: ToolDefinition["inputSchema"], value: unknown) {
    return buildToolInputSchema(inputSchema).safeParse(value);
}

function malformedProperty(value: unknown): ToolDefinition["inputSchema"] {
    return {
        type: "object",
        properties: {
            value: value as never,
        },
    };
}

describe("buildToolInputShape", () => {
    it("enforces inclusive number minimum and maximum", () => {
        const inputSchema: ToolDefinition["inputSchema"] = {
            type: "object",
            properties: {
                volume: { type: "number", minimum: 0, maximum: 100 },
            },
            required: ["volume"],
        };

        expect(parseInput(inputSchema, { volume: 0 }).success).toBe(true);
        expect(parseInput(inputSchema, { volume: 100 }).success).toBe(true);
        expect(parseInput(inputSchema, { volume: -1 }).success).toBe(false);
        expect(parseInput(inputSchema, { volume: 101 }).success).toBe(false);
    });

    it("enforces integer array item type and minimum", () => {
        const inputSchema: ToolDefinition["inputSchema"] = {
            type: "object",
            properties: {
                indices: {
                    type: "array",
                    items: { type: "integer", minimum: 0 },
                },
            },
            required: ["indices"],
        };

        expect(parseInput(inputSchema, { indices: [0, 2] }).success).toBe(true);
        expect(parseInput(inputSchema, { indices: [-1] }).success).toBe(false);
        expect(parseInput(inputSchema, { indices: [1.5] }).success).toBe(false);
        expect(parseInput(inputSchema, { indices: ["1"] }).success).toBe(false);
    });

    it("enforces string array item type and required fields", () => {
        const inputSchema: ToolDefinition["inputSchema"] = {
            type: "object",
            properties: {
                paths: { type: "array", items: { type: "string" } },
            },
            required: ["paths"],
        };

        expect(parseInput(inputSchema, { paths: ["a.flac"] }).success).toBe(true);
        expect(parseInput(inputSchema, { paths: [42] }).success).toBe(false);
        expect(parseInput(inputSchema, {}).success).toBe(false);
    });

    it("enforces union alternatives and their nested constraints", () => {
        const inputSchema: ToolDefinition["inputSchema"] = {
            type: "object",
            properties: {
                order: {
                    type: "union",
                    anyOf: [
                        { type: "integer", minimum: 0, maximum: 6 },
                        { type: "string", enum: ["default", "random"] },
                    ],
                },
            },
            required: ["order"],
        };

        expect(parseInput(inputSchema, { order: 0 }).success).toBe(true);
        expect(parseInput(inputSchema, { order: 6 }).success).toBe(true);
        expect(parseInput(inputSchema, { order: "random" }).success).toBe(true);
        expect(parseInput(inputSchema, { order: -1 }).success).toBe(false);
        expect(parseInput(inputSchema, { order: 7 }).success).toBe(false);
        expect(parseInput(inputSchema, { order: "shuffle" }).success).toBe(false);
        expect(parseInput(inputSchema, { order: 1.5 }).success).toBe(false);
    });

    it("preserves dynamic keys in an open object", () => {
        const inputSchema: ToolDefinition["inputSchema"] = {
            type: "object",
            properties: {
                tags: { type: "object" },
            },
            required: ["tags"],
        };

        const result = buildToolInputSchema(inputSchema).parse({
            tags: { TITLE: "Song", RATING: 5 },
        });

        expect(result).toEqual({ tags: { TITLE: "Song", RATING: 5 } });
        expect(parseInput(inputSchema, { tags: 42 }).success).toBe(false);
        expect(parseInput(inputSchema, { tags: null }).success).toBe(false);
    });

    it("enforces nested object properties and required keys", () => {
        const inputSchema: ToolDefinition["inputSchema"] = {
            type: "object",
            properties: {
                items: {
                    type: "array",
                    items: {
                        type: "object",
                        properties: {
                            path: { type: "string" },
                            tags: { type: "object" },
                        },
                        required: ["path", "tags"],
                    },
                },
            },
            required: ["items"],
        };

        expect(parseInput(inputSchema, {
            items: [{ path: "track.flac", tags: { TITLE: "Song" } }],
        }).success).toBe(true);
        expect(parseInput(inputSchema, {
            items: [{ path: "track.flac" }],
        }).success).toBe(false);
        expect(parseInput(inputSchema, { items: [null] }).success).toBe(false);
    });

    it("supports strict and typed nested additional properties", () => {
        const strictInput: ToolDefinition["inputSchema"] = {
            type: "object",
            properties: {
                value: {
                    type: "object",
                    properties: { known: { type: "string" } },
                    additionalProperties: false,
                },
            },
            required: ["value"],
        };
        const typedInput: ToolDefinition["inputSchema"] = {
            type: "object",
            properties: {
                value: {
                    type: "object",
                    properties: { known: { type: "string" } },
                    additionalProperties: { type: "integer", minimum: 0 },
                },
            },
            required: ["value"],
        };

        expect(parseInput(strictInput, { value: { known: "ok" } }).success).toBe(true);
        expect(parseInput(strictInput, { value: { known: "ok", extra: 1 } }).success)
            .toBe(false);
        expect(parseInput(typedInput, { value: { known: "ok", extra: 1 } }).success)
            .toBe(true);
        expect(parseInput(typedInput, { value: { known: "ok", extra: -1 } }).success)
            .toBe(false);
        expect(parseInput(typedInput, { value: { known: "ok", extra: "bad" } }).success)
            .toBe(false);
    });

    it("applies enum and falsy defaults without overriding explicit values", () => {
        const inputSchema: ToolDefinition["inputSchema"] = {
            type: "object",
            properties: {
                target: { type: "string", enum: ["embedded", "file"], default: "embedded" },
                enabled: { type: "boolean", default: false },
                index: { type: "integer", minimum: 0, default: 0 },
            },
        };
        const schema = buildToolInputSchema(inputSchema);

        expect(schema.parse({})).toEqual({
            target: "embedded",
            enabled: false,
            index: 0,
        });
        expect(schema.parse({ target: "file", enabled: true, index: 2 })).toEqual({
            target: "file",
            enabled: true,
            index: 2,
        });
        expect(schema.safeParse({ target: "sidecar" }).success).toBe(false);
    });

    it("rejects malformed declarations while building", () => {
        expect(() => buildToolInputShape({
            type: "object",
            properties: {
                value: { type: "number", minimum: 10, maximum: 5 },
            },
        })).toThrow(/minimum.*maximum/i);

        expect(() => buildToolInputShape(malformedProperty({
            type: "array",
        }))).toThrow(/root\.value.*items/i);

        expect(() => buildToolInputShape({
            type: "object",
            properties: {},
            required: ["missing"],
        })).toThrow(/required.*missing/i);

        expect(() => buildToolInputShape(malformedProperty({
            type: "boolean",
            minimum: 0,
        }))).toThrow(/root\.value\.minimum.*boolean/i);

        expect(() => buildToolInputShape(malformedProperty({
            type: "number",
            enum: ["1"],
        }))).toThrow(/root\.value\.enum.*number/i);

        expect(() => buildToolInputShape(malformedProperty({
            type: "mystery",
        }))).toThrow(/root\.value\.type.*mystery/i);

        expect(() => buildToolInputShape(malformedProperty({
            type: "number",
            minimum: Number.NaN,
        }))).toThrow(/root\.value\.minimum.*finite/i);

        expect(() => buildToolInputShape({
            type: "object",
            properties: { value: { type: "string" } },
            required: ["value", "value"],
        })).toThrow(/required.*duplicate/i);

        expect(() => buildToolInputShape({
            type: "object",
            properties: {
                value: { type: "integer", minimum: 0, default: -1 },
            },
        })).toThrow(/default.*root\.value/i);

        expect(() => buildToolInputShape({
            type: "array",
            properties: {},
        } as never)).toThrow(/root\.type.*object/i);

        expect(() => buildToolInputShape({
            type: "object",
            properties: {},
            unknownKeyword: true,
        } as never)).toThrow(/root\.unknownKeyword/i);

        expect(() => buildToolInputShape(malformedProperty({
            type: "string",
            enum: "embedded",
        }))).toThrow(/root\.value\.enum.*array of strings/i);

        expect(() => buildToolInputShape(malformedProperty({
            type: "object",
            properties: null,
        }))).toThrow(/root\.value\.properties.*object/i);

        expect(() => buildToolInputShape(malformedProperty({
            type: "object",
            required: "path",
        }))).toThrow(/root\.value\.required.*array of strings/i);

        expect(() => buildToolInputShape(malformedProperty({
            type: "object",
            additionalProperties: "open",
        }))).toThrow(/root\.value\.additionalProperties.*boolean or property schema/i);

        expect(() => buildToolInputShape(malformedProperty({
            type: "union",
            anyOf: [{ type: "string" }],
        }))).toThrow(/root\.value\.anyOf.*two/i);
    });

    it("rejects defaults that are not safe JSON values", () => {
        const sparse = new Array(1);
        const customPrototypeArray: unknown[] = [];
        Object.setPrototypeOf(customPrototypeArray, Object.create(Array.prototype));
        const circular: Record<string, unknown> = {};
        circular.self = circular;
        const prototypeSensitive = JSON.parse(
            '{"__proto__":{"polluted":true}}'
        ) as Record<string, unknown>;
        const accessor = Object.defineProperty({}, "value", {
            enumerable: true,
            get: () => "unsafe",
        });
        const invalidDefaults = [
            undefined,
            1n,
            () => "unsafe",
            Symbol("unsafe"),
            Number.NaN,
            sparse,
            customPrototypeArray,
            circular,
            prototypeSensitive,
            accessor,
            new Date(0),
        ];

        for (const defaultValue of invalidDefaults) {
            expect(() => buildToolInputShape(malformedProperty({
                type: "object",
                default: defaultValue,
            }))).toThrow(/default for root\.value/i);
        }
    });

    it("rejects an accessor-backed default without executing its getter", () => {
        const getter = vi.fn(() => ({ safe: true }));
        const property = { type: "object" } as Record<string, unknown>;
        Object.defineProperty(property, "default", {
            enumerable: true,
            get: getter,
        });

        expect(() => buildToolInputShape(malformedProperty(property)))
            .toThrow(/default for root\.value.*plain data property/i);
        expect(getter).not.toHaveBeenCalled();
    });

    it("rejects prototype-sensitive property names before registration", () => {
        const properties = Object.create(null) as Record<string, never>;
        properties.__proto__ = { type: "integer", minimum: 0 } as never;
        const inputSchema = {
            type: "object",
            properties,
            required: ["__proto__"],
        } as ToolDefinition["inputSchema"];

        expect(() => buildToolInputSchema(inputSchema))
            .toThrow(/root\.properties\.__proto__.*prototype-sensitive/i);
    });

    it("rejects circular declarations with the property path", () => {
        const objectCycle = { type: "object", properties: {} } as never;
        (objectCycle as { properties: Record<string, unknown> }).properties.self = objectCycle;

        const arrayCycle = { type: "array" } as never;
        (arrayCycle as { items: unknown }).items = arrayCycle;

        const catchallCycle = { type: "object" } as never;
        (catchallCycle as { additionalProperties: unknown }).additionalProperties = catchallCycle;

        expect(() => buildToolInputShape(malformedProperty(objectCycle)))
            .toThrow(/root\.value\.self.*circular.*root\.value/i);
        expect(() => buildToolInputShape(malformedProperty(arrayCycle)))
            .toThrow(/root\.value\[\].*circular.*root\.value/i);
        expect(() => buildToolInputShape(malformedProperty(catchallCycle)))
            .toThrow(/root\.value\.\*.*circular.*root\.value/i);

        const unionCycle = { type: "union", anyOf: [] } as never;
        (unionCycle as { anyOf: unknown[] }).anyOf.push(
            { type: "string" },
            unionCycle
        );
        expect(() => buildToolInputShape(malformedProperty(unionCycle)))
            .toThrow(/root\.value\.anyOf\[1\].*circular.*root\.value/i);
    });
});

describe("production MCP tool schemas", () => {
    const allTools = [
        ...playbackTools,
        ...playbackExtTools,
        ...playlistTools,
        ...playlistExtTools,
        ...libraryTools,
        ...artworkTools,
        ...queueTools,
        ...metadataTools,
    ];

    it("builds all 98 bridge tool schemas", () => {
        expect(allTools).toHaveLength(98);
        for (const tool of allTools) {
            expect(() => buildToolInputShape(tool.inputSchema), tool.name).not.toThrow();
        }
    });

    it("pins the MCP SDK range to the first Zod 4 object-schema compatible release", () => {
        const packageJson = JSON.parse(
            fs.readFileSync(new URL("../package.json", import.meta.url), "utf8")
        ) as { dependencies?: Record<string, string> };

        expect(packageJson.dependencies?.["@modelcontextprotocol/sdk"])
            .toBe("^1.23.0");
    });

    it("enforces declared production constraints", () => {
        const byName = new Map(allTools.map((tool) => [tool.name, tool]));
        const schemaFor = (name: string) => {
            const tool = byName.get(name);
            if (!tool) throw new Error(`missing production tool ${name}`);
            return buildToolInputSchema(tool.inputSchema);
        };

        expect(schemaFor("fb2k_playback_set_volume").safeParse({ volume: 101 }).success)
            .toBe(false);
        expect(schemaFor("fb2k_playlist_remove_tracks").safeParse({ items: [-1] }).success)
            .toBe(false);

        expect(schemaFor("fb2k_metadata_write").parse({
            path: "track.flac",
            tags: { TITLE: "Song", RATING: 5 },
        })).toEqual({
            path: "track.flac",
            tags: { TITLE: "Song", RATING: 5 },
        });

        expect(schemaFor("fb2k_metadata_write_batch").parse({
            items: [{ path: "track.flac" }],
        })).toEqual({ items: [{ path: "track.flac" }] });

        const playbackOrder = schemaFor("fb2k_playback_set_playback_order");
        expect(playbackOrder.parse({ order: 0 })).toEqual({ order: 0 });
        expect(playbackOrder.parse({ order: "shuffle-albums" })).toEqual({
            order: "shuffle-albums",
        });
        expect(playbackOrder.safeParse({ order: 7 }).success).toBe(false);
        expect(playbackOrder.safeParse({ order: "shuffle" }).success).toBe(false);
    });
});

describe("registerBridgeTools integration", () => {
    async function createHarness() {
        const server = new McpServer({ name: "schema-test-server", version: "1.0.0" });
        const client = new Client(
            { name: "schema-test-client", version: "1.0.0" },
            { capabilities: {} }
        );
        const call = vi.fn().mockResolvedValue({ success: true, data: { ok: true } });
        const tools = [
            playbackTools.find((tool) => tool.name === "fb2k_playback_set_volume")!,
            playbackExtTools.find((tool) => tool.name === "fb2k_playback_set_playback_order")!,
            metadataTools.find((tool) => tool.name === "fb2k_metadata_write")!,
            metadataTools.find((tool) => tool.name === "fb2k_metadata_write_batch")!,
        ];
        registerBridgeTools(server, { call }, tools, {
            ...playbackMethodMap,
            ...playbackExtMethodMap,
            ...metadataMethodMap,
        });
        const [clientTransport, serverTransport] = InMemoryTransport.createLinkedPair();
        await server.connect(guardBridgeTransport(serverTransport));
        await client.connect(clientTransport);
        return { server, client, call };
    }

    it("lists and applies a safe object default through the production path", async () => {
        const server = new McpServer({ name: "schema-test-server", version: "1.0.0" });
        const client = new Client(
            { name: "schema-test-client", version: "1.0.0" },
            { capabilities: {} }
        );
        const call = vi.fn().mockResolvedValue({ success: true, data: { ok: true } });
        const tool: ToolDefinition = {
            name: "fb2k_test_default",
            description: "Test a JSON object default",
            inputSchema: {
                type: "object",
                properties: {
                    options: {
                        type: "object",
                        properties: {
                            enabled: { type: "boolean" },
                        },
                        required: ["enabled"],
                        default: { enabled: false },
                    },
                },
            },
        };
        registerBridgeTools(server, { call }, [tool], {
            fb2k_test_default: "test.default",
        });
        const [clientTransport, serverTransport] = InMemoryTransport.createLinkedPair();
        await server.connect(guardBridgeTransport(serverTransport));
        await client.connect(clientTransport);
        try {
            const listed = await client.listTools();
            const listedTool = listed.tools.find((entry) => entry.name === tool.name);
            const result = await client.callTool({ name: tool.name, arguments: {} });

            expect(listedTool?.inputSchema.properties?.options).toMatchObject({
                type: "object",
            });
            expect(result.isError).not.toBe(true);
            expect(call).toHaveBeenCalledWith("test.default", {
                options: { enabled: false },
            });
        } finally {
            await client.close();
            await server.close();
        }
    });

    it("rejects invalid declared production arguments before invoking the bridge", async () => {
        const { server, client, call } = await createHarness();
        try {
            const volumeResult = await client.callTool({
                name: "fb2k_playback_set_volume",
                arguments: { volume: 101 },
            });

            expect(volumeResult.isError).toBe(true);
            expect(call).not.toHaveBeenCalled();
        } finally {
            await client.close();
            await server.close();
        }
    });

    it("preserves valid playback order union inputs and rejects invalid ones", async () => {
        const { server, client, call } = await createHarness();
        try {
            const listed = await client.listTools();
            const tool = listed.tools.find(
                (entry) => entry.name === "fb2k_playback_set_playback_order"
            );
            expect(tool?.inputSchema.properties?.order).toMatchObject({
                anyOf: [
                    { type: "integer", minimum: 0, maximum: 6 },
                    { type: "string", enum: ["default", "repeat-playlist", "repeat-track", "random", "shuffle-tracks", "shuffle-albums", "shuffle-folders"] },
                ],
            });

            const numericResult = await client.callTool({
                name: "fb2k_playback_set_playback_order",
                arguments: { order: 3 },
            });
            const stringResult = await client.callTool({
                name: "fb2k_playback_set_playback_order",
                arguments: { order: "random" },
            });
            const invalidResult = await client.callTool({
                name: "fb2k_playback_set_playback_order",
                arguments: { order: 7 },
            });
            const invalidNameResult = await client.callTool({
                name: "fb2k_playback_set_playback_order",
                arguments: { order: "shuffle" },
            });

            expect(numericResult.isError).not.toBe(true);
            expect(stringResult.isError).not.toBe(true);
            expect(invalidResult.isError).toBe(true);
            expect(invalidNameResult.isError).toBe(true);
            expect(call).toHaveBeenNthCalledWith(1, "playback.setPlaybackOrder", { order: 3 });
            expect(call).toHaveBeenNthCalledWith(2, "playback.setPlaybackOrder", { order: "random" });
            expect(call).toHaveBeenCalledTimes(2);
        } finally {
            await client.close();
            await server.close();
        }
    });

    it("preserves dynamic metadata tags through the production registration path", async () => {
        const { server, client, call } = await createHarness();
        try {
            const tags = { TITLE: "Song", RATING: 5 };
            const result = await client.callTool({
                name: "fb2k_metadata_write",
                arguments: { path: "track.flac", tags },
            });

            expect(result.isError).not.toBe(true);
            expect(call).toHaveBeenCalledWith("metadata.write", {
                path: "track.flac",
                tags,
            });
        } finally {
            await client.close();
            await server.close();
        }
    });

    it("rejects nested prototype-sensitive keys before invoking the bridge", async () => {
        const { server, client, call } = await createHarness();
        try {
            const tags = JSON.parse(
                '{"__proto__":{"polluted":true},"TITLE":"Song"}'
            ) as Record<string, unknown>;
            await expect(client.callTool({
                name: "fb2k_metadata_write",
                arguments: { path: "track.flac", tags },
            })).rejects.toThrow(/prototype-sensitive/i);
            expect(call).not.toHaveBeenCalled();
        } finally {
            await client.close();
            await server.close();
        }
    });

    it("rejects top-level prototype-sensitive keys before SDK normalization", async () => {
        const { server, client, call } = await createHarness();
        try {
            const argumentsWithPrototypeKey = JSON.parse(
                '{"path":"track.flac","tags":{"TITLE":"Song"},"__proto__":{"polluted":true}}'
            ) as Record<string, unknown>;

            await expect(client.callTool({
                name: "fb2k_metadata_write",
                arguments: argumentsWithPrototypeKey,
            })).rejects.toThrow(/prototype-sensitive/i);
            expect(call).not.toHaveBeenCalled();
        } finally {
            await client.close();
            await server.close();
        }
    });

    it("preserves undeclared top-level arguments through the production path", async () => {
        const { server, client, call } = await createHarness();
        try {
            const listed = await client.listTools();
            const volumeTool = listed.tools.find(
                (tool) => tool.name === "fb2k_playback_set_volume"
            );
            const result = await client.callTool({
                name: "fb2k_playback_set_volume",
                arguments: { volume: 50, transitionMs: 250 },
            });

            expect(volumeTool?.inputSchema.additionalProperties).toEqual({});
            expect(result.isError).not.toBe(true);
            expect(call).toHaveBeenCalledWith("playback.setVolume", {
                volume: 50,
                transitionMs: 250,
            });
        } finally {
            await client.close();
            await server.close();
        }
    });

    it("preserves writeBatch item-level error handling for the bridge runtime", async () => {
        const { server, client, call } = await createHarness();
        try {
            const items = [
                { path: "valid.flac", tags: { TITLE: "Song" } },
                { path: "missing-tags.flac" },
            ];
            const result = await client.callTool({
                name: "fb2k_metadata_write_batch",
                arguments: { items },
            });

            expect(result.isError).not.toBe(true);
            expect(call).toHaveBeenCalledWith("metadata.writeBatch", { items });
        } finally {
            await client.close();
            await server.close();
        }
    });

    it("rejects a tool definition without a bridge method mapping", () => {
        const server = new McpServer({ name: "schema-test-server", version: "1.0.0" });
        const call = vi.fn();

        expect(() => registerBridgeTools(
            server,
            { call },
            [playbackTools[0]],
            {}
        )).toThrow(/missing bridge method mapping/i);
    });
});
import fs from "node:fs";

import { Client } from "@modelcontextprotocol/sdk/client/index.js";
import { InMemoryTransport } from "@modelcontextprotocol/sdk/inMemory.js";
import { McpServer } from "@modelcontextprotocol/sdk/server/mcp.js";
import { describe, expect, it, vi } from "vitest";

import {
    guardBridgeTransport,
    registerBridgeTools,
    type BridgeToolOptions,
} from "../src/bridge-tools.js";
import { CallRateLimiter } from "../src/rate-limit.js";
import { buildToolInputSchema, buildToolInputShape } from "../src/tool-schema.js";
import { bridgeTools } from "../src/generated/bridge-tools.js";
import type { InputSchema, ToolDefinition } from "../src/types.js";

function bridgeTool(name: string): ToolDefinition {
    const tool = bridgeTools.find((entry) => entry.name === name);
    if (!tool) throw new Error(`missing generated bridge tool ${name}`);
    return tool;
}

/** The exact schema of one action, found in whichever tool holds it. */
function actionSchema(api: string) {
    const tool = bridgeTools.find((entry) => api in entry.actions);
    if (!tool) throw new Error(`no generated tool has the action ${api}`);
    return buildToolInputSchema(tool.actions[api].inputSchema);
}

/** The tool-table entry of one action. */
function tableEntry(api: string): { bounds?: unknown } {
    const table = JSON.parse(
        fs.readFileSync(new URL("../tool-table.json", import.meta.url), "utf8")
    ) as { tools: Record<string, { actions: Record<string, { bounds?: unknown }> }> };
    for (const tool of Object.values(table.tools)) {
        if (tool.actions[api]) return tool.actions[api];
    }
    throw new Error(`the tool table has no action ${api}`);
}

function parseInput(inputSchema: InputSchema, value: unknown) {
    return buildToolInputSchema(inputSchema).safeParse(value);
}

function malformedProperty(value: unknown): InputSchema {
    return {
        type: "object",
        properties: {
            value: value as never,
        },
    };
}

describe("buildToolInputShape", () => {
    it("enforces inclusive number minimum and maximum", () => {
        const inputSchema: InputSchema = {
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
        const inputSchema: InputSchema = {
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
        const inputSchema: InputSchema = {
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
        const inputSchema: InputSchema = {
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
        const inputSchema: InputSchema = {
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
        const inputSchema: InputSchema = {
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
        const strictInput: InputSchema = {
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
        const typedInput: InputSchema = {
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
        const inputSchema: InputSchema = {
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
        } as InputSchema;

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
    it("builds every generated tool schema and every action schema", () => {
        expect(bridgeTools.length).toBeGreaterThan(0);
        for (const tool of bridgeTools) {
            expect(() => buildToolInputShape(tool.inputSchema), tool.name).not.toThrow();
            for (const [api, action] of Object.entries(tool.actions)) {
                expect(() => buildToolInputShape(action.inputSchema), api).not.toThrow();
            }
        }
    });

    it("pins the MCP SDK range to the first Zod 4 object-schema compatible release", () => {
        const packageJson = JSON.parse(
            fs.readFileSync(new URL("../package.json", import.meta.url), "utf8")
        ) as { dependencies?: Record<string, string> };

        expect(packageJson.dependencies?.["@modelcontextprotocol/sdk"])
            .toBe("^1.23.0");
    });

    it("enforces declared production constraints on each action", () => {
        expect(actionSchema("playback.setVolume").safeParse({ volume: 101 }).success)
            .toBe(false);
        // Row and queue-position arrays: the declaration bounds every item at 0 and the tool
        // table adds no bounds of its own, so the refusal comes from the declaration.
        const rowArrays: [string, Record<string, unknown>, Record<string, unknown>][] = [
            ["playlist.removeTracks", { items: [0, -1] }, { items: [0, 1] }],
            ["playlist.moveTracks", { items: [-1], delta: 1 }, { items: [0], delta: -1 }],
            ["playlist.setSelection", { indices: [-1] }, { indices: [0] }],
            ["playlist.reorder", { newOrder: [1, -1] }, { newOrder: [1, 0] }],
            ["playlist.reorderPlaylists", { newOrder: [-1] }, { newOrder: [0] }],
            ["queue.add", { tracks: [-1] }, { tracks: [0] }],
            ["queue.remove", { indices: [2, -1] }, { indices: [0] }],
        ];
        for (const [api, negative, valid] of rowArrays) {
            expect(tableEntry(api).bounds, api).toBeUndefined();
            expect(actionSchema(api).safeParse(negative).success, api).toBe(false);
            expect(actionSchema(api).safeParse(valid).success, api).toBe(true);
        }
        expect(actionSchema("playlist.addPaths").safeParse({ paths: [] }).success)
            .toBe(false);
        expect(actionSchema("playback.playPath").safeParse({ path: "" }).success)
            .toBe(false);

        expect(actionSchema("metadata.write").parse({
            path: "track.flac",
            tags: { TITLE: "Song", RATING: 5 },
        })).toEqual({
            path: "track.flac",
            tags: { TITLE: "Song", RATING: 5 },
            cueIndex: -1,
        });

        // The declared default of each item member is filled in, as it is for top-level ones.
        expect(actionSchema("metadata.writeBatch").parse({
            items: [{ path: "track.flac" }],
        })).toEqual({ items: [{ path: "track.flac", cueIndex: -1 }] });
        expect(actionSchema("metadata.writeBatch").safeParse({
            items: [{ path: "track.flac", tag: {} }],
        }).success).toBe(false);

        const playbackOrder = actionSchema("playback.setPlaybackOrder");
        expect(playbackOrder.parse({ order: 0 })).toEqual({ order: 0 });
        expect(playbackOrder.parse({ name: "shuffle-albums" })).toEqual({
            name: "shuffle-albums",
        });
        expect(playbackOrder.safeParse({ order: 7 }).success).toBe(false);
        expect(playbackOrder.safeParse({ name: "shuffle" }).success).toBe(false);
    });

    it("the merged schema carries no defaults and accepts what any of its actions accepts", () => {
        for (const tool of bridgeTools) {
            // A `default` keyword, not the playback order named "default".
            expect(JSON.stringify(tool.inputSchema), tool.name).not.toMatch(/"default":/);
        }
        const library = buildToolInputSchema(bridgeTool("fb2k_library_read").inputSchema);
        expect(library.safeParse({ action: "library.getArtists", limit: 800 }).success).toBe(true);
        expect(actionSchema("library.search").safeParse({ query: "x", limit: 800 }).success).toBe(false);
        // queue.setContents and queue.insertNext take differently shaped items.
        const queue = buildToolInputSchema(bridgeTool("fb2k_queue_edit").inputSchema);
        expect(queue.safeParse({ action: "queue.setContents", items: [{ queueIndex: 0 }] }).success).toBe(true);
        expect(queue.safeParse({ action: "queue.insertNext", items: [{ playlist: 0, item: 1 }] }).success).toBe(true);
    });
});

describe("registerBridgeTools integration", () => {
    async function createHarness(
        options: BridgeToolOptions = {},
        data: unknown = { ok: true }
    ) {
        const server = new McpServer({ name: "schema-test-server", version: "1.0.0" });
        const client = new Client(
            { name: "schema-test-client", version: "1.0.0" },
            { capabilities: {} }
        );
        const call = vi.fn().mockResolvedValue({ success: true, data });
        const registered = registerBridgeTools(server, { call }, bridgeTools, options);
        const [clientTransport, serverTransport] = InMemoryTransport.createLinkedPair();
        await server.connect(guardBridgeTransport(serverTransport));
        await client.connect(clientTransport);
        return {
            client,
            call,
            registered,
            async close() {
                await client.close();
                await server.close();
            },
        };
    }

    function text(result: Awaited<ReturnType<Client["callTool"]>>): string {
        const content = result.content as Array<{ type: string; text?: string }>;
        return content.map((c) => c.text ?? "").join("\n");
    }

    it("lists every tool with its annotations, a strict top level and the action enum", async () => {
        const h = await createHarness();
        try {
            const { tools } = await h.client.listTools();
            expect(tools.map((t) => t.name)).toEqual(bridgeTools.map((t) => t.name));
            for (const listed of tools) {
                const tool = bridgeTool(listed.name);
                expect(listed.annotations, listed.name).toEqual(tool.annotations);
                expect(listed.inputSchema.additionalProperties, listed.name).toBe(false);
                expect(listed.inputSchema.required, listed.name).toEqual(["action"]);
                expect(listed.inputSchema.properties?.action, listed.name).toMatchObject({
                    type: "string",
                    enum: Object.keys(tool.actions),
                });
            }
        } finally {
            await h.close();
        }
    });

    it("lists and applies a safe object default through the production path", async () => {
        const server = new McpServer({ name: "schema-test-server", version: "1.0.0" });
        const client = new Client(
            { name: "schema-test-client", version: "1.0.0" },
            { capabilities: {} }
        );
        const call = vi.fn().mockResolvedValue({ success: true, data: { ok: true } });
        const options = {
            type: "object" as const,
            properties: { enabled: { type: "boolean" as const } },
            required: ["enabled"],
        };
        const tool: ToolDefinition = {
            name: "fb2k_test_default",
            description: "Test a JSON object default",
            annotations: { readOnlyHint: true, openWorldHint: false },
            inputSchema: {
                type: "object",
                properties: {
                    action: { type: "string", enum: ["test.default"] },
                    options,
                },
                required: ["action"],
            },
            actions: {
                "test.default": {
                    inputSchema: {
                        type: "object",
                        properties: { options: { ...options, default: { enabled: false } } },
                    },
                },
            },
        };
        registerBridgeTools(server, { call }, [tool]);
        const [clientTransport, serverTransport] = InMemoryTransport.createLinkedPair();
        await server.connect(guardBridgeTransport(serverTransport));
        await client.connect(clientTransport);
        try {
            const listed = await client.listTools();
            const listedTool = listed.tools.find((entry) => entry.name === tool.name);
            const result = await client.callTool({ name: tool.name, arguments: { action: "test.default" } });

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
        const h = await createHarness();
        try {
            const volumeResult = await h.client.callTool({
                name: "fb2k_playback_control",
                arguments: { action: "playback.setVolume", volume: 101 },
            });

            expect(volumeResult.isError).toBe(true);
            expect(h.call).not.toHaveBeenCalled();
        } finally {
            await h.close();
        }
    });

    it("accepts the playback order by index or by name and rejects invalid ones", async () => {
        const h = await createHarness();
        try {
            const listed = await h.client.listTools();
            const tool = listed.tools.find((entry) => entry.name === "fb2k_playback_control");
            expect(tool?.inputSchema.properties?.order).toMatchObject({
                type: "integer",
                minimum: 0,
                maximum: 6,
            });
            expect(tool?.inputSchema.properties?.name).toMatchObject({
                type: "string",
                enum: ["default", "repeat-playlist", "repeat-track", "random", "shuffle-tracks", "shuffle-albums", "shuffle-folders"],
            });

            const order = (args: Record<string, unknown>) => h.client.callTool({
                name: "fb2k_playback_control",
                arguments: { action: "playback.setPlaybackOrder", ...args },
            });
            expect((await order({ order: 3 })).isError).not.toBe(true);
            expect((await order({ name: "random" })).isError).not.toBe(true);
            expect((await order({ order: 7 })).isError).toBe(true);
            expect((await order({ name: "shuffle" })).isError).toBe(true);
            expect(h.call).toHaveBeenNthCalledWith(1, "playback.setPlaybackOrder", { order: 3 });
            expect(h.call).toHaveBeenNthCalledWith(2, "playback.setPlaybackOrder", { name: "random" });
            expect(h.call).toHaveBeenCalledTimes(2);
        } finally {
            await h.close();
        }
    });

    it("preserves dynamic metadata tags and fills the declared default", async () => {
        const h = await createHarness();
        try {
            const tags = { TITLE: "Song", RATING: 5, ARTIST: ["甲", "乙", "甲"], COMMENT: [] };
            const result = await h.client.callTool({
                name: "fb2k_track_write",
                arguments: { action: "metadata.write", path: "track.flac", tags },
            });

            expect(result.isError).not.toBe(true);
            expect(h.call).toHaveBeenCalledWith("metadata.write", {
                path: "track.flac",
                tags,
                cueIndex: -1,
            });
        } finally {
            await h.close();
        }
    });

    it("rejects nested prototype-sensitive keys before invoking the bridge", async () => {
        const h = await createHarness();
        try {
            const tags = JSON.parse(
                '{"__proto__":{"polluted":true},"TITLE":"Song"}'
            ) as Record<string, unknown>;
            await expect(h.client.callTool({
                name: "fb2k_track_write",
                arguments: { action: "metadata.write", path: "track.flac", tags },
            })).rejects.toThrow(/prototype-sensitive/i);
            expect(h.call).not.toHaveBeenCalled();
        } finally {
            await h.close();
        }
    });

    it("rejects top-level prototype-sensitive keys before SDK normalization", async () => {
        const h = await createHarness();
        try {
            const argumentsWithPrototypeKey = JSON.parse(
                '{"action":"metadata.write","path":"track.flac","tags":{"TITLE":"Song"},"__proto__":{"polluted":true}}'
            ) as Record<string, unknown>;

            await expect(h.client.callTool({
                name: "fb2k_track_write",
                arguments: argumentsWithPrototypeKey,
            })).rejects.toThrow(/prototype-sensitive/i);
            expect(h.call).not.toHaveBeenCalled();
        } finally {
            await h.close();
        }
    });

    it("refuses an argument no action declares", async () => {
        const h = await createHarness();
        try {
            const result = await h.client.callTool({
                name: "fb2k_playback_control",
                arguments: { action: "playback.setVolume", volume: 50, transitionMs: 250 },
            });

            expect(result.isError).toBe(true);
            expect(text(result)).toMatch(/transitionMs/);
            expect(h.call).not.toHaveBeenCalled();
        } finally {
            await h.close();
        }
    });

    it("refuses another action's parameter and says what the chosen one takes", async () => {
        const h = await createHarness();
        try {
            const result = await h.client.callTool({
                name: "fb2k_playlist_read",
                arguments: { action: "playlist.getAll", start: 1 },
            });

            expect(result.isError).toBe(true);
            expect(text(result)).toMatch(/Invalid arguments for playlist\.getAll: Unrecognized key: "start"\. It takes no parameters\./);
            expect(h.call).not.toHaveBeenCalled();
        } finally {
            await h.close();
        }
    });

    it("names a missing required parameter and lists the action's parameters", async () => {
        const h = await createHarness();
        try {
            const result = await h.client.callTool({
                name: "fb2k_playlist_manage",
                arguments: { action: "playlist.rename", playlist: 0 },
            });

            expect(result.isError).toBe(true);
            expect(text(result)).toBe(
                "Invalid arguments for playlist.rename: `name` is required. It takes `playlist`, `playlistGuid`, `name` (required)."
            );
            expect(h.call).not.toHaveBeenCalled();
        } finally {
            await h.close();
        }
    });

    it("passes writeBatch items through for the host to judge each one", async () => {
        const h = await createHarness();
        try {
            const items = [
                { path: "valid.flac", tags: { TITLE: "Song", ARTIST: ["甲", "乙"], COMMENT: [] } },
                { path: "invalid-array.flac", tags: { ARTIST: ["valid", 1] } },
                { path: "missing-tags.flac" },
            ];
            const result = await h.client.callTool({
                name: "fb2k_track_write",
                arguments: { action: "metadata.writeBatch", items },
            });

            expect(result.isError).not.toBe(true);
            expect(h.call).toHaveBeenCalledWith("metadata.writeBatch", {
                items: items.map((item) => ({ ...item, cueIndex: -1 })),
            });
        } finally {
            await h.close();
        }
    });

    it("passes the embed artwork target array to the host as declared", async () => {
        const h = await createHarness();
        try {
            const embed = (args: Record<string, unknown>) => h.client.callTool({
                name: "fb2k_track_write",
                arguments: { action: "metadata.embedArtwork", path: "track.flac", imageData: "AAAA", ...args },
            });
            expect((await embed({})).isError).not.toBe(true);
            expect((await embed({ target: ["file"] })).isError).not.toBe(true);
            expect((await embed({ target: "file" })).isError).toBe(true);
            // An omitted target stays omitted: the host writes `embedded` then.
            expect(h.call).toHaveBeenNthCalledWith(1, "metadata.embedArtwork", {
                path: "track.flac",
                imageData: "AAAA",
                type: "front",
            });
            expect(h.call).toHaveBeenNthCalledWith(2, "metadata.embedArtwork", {
                path: "track.flac",
                imageData: "AAAA",
                type: "front",
                target: ["file"],
            });
            expect(h.call).toHaveBeenCalledTimes(2);
        } finally {
            await h.close();
        }
    });

    it("registers only the read-only tools in read-only mode", async () => {
        const h = await createHarness({ readOnly: true });
        try {
            const { tools } = await h.client.listTools();
            const readOnly = bridgeTools.filter((t) => t.annotations.readOnlyHint).map((t) => t.name);
            expect(h.registered).toEqual(readOnly);
            expect(tools.map((t) => t.name)).toEqual(readOnly);
            expect(readOnly).toEqual([
                "fb2k_playback_read",
                "fb2k_playlist_read",
                "fb2k_library_read",
                "fb2k_track_read",
            ]);
        } finally {
            await h.close();
        }
    });

    it("refuses calls over the rate limit without reaching the bridge", async () => {
        let now = 0;
        const limiter = new CallRateLimiter({ burst: 2, perSecond: 1, now: () => now });
        const h = await createHarness({ limiter });
        try {
            const getState = () => h.client.callTool({
                name: "fb2k_playback_read",
                arguments: { action: "playback.getState" },
            });
            expect((await getState()).isError).not.toBe(true);
            expect((await getState()).isError).not.toBe(true);
            const refused = await getState();
            expect(refused.isError).toBe(true);
            expect(text(refused)).toMatch(/Rate limit: .* Retry in 1000 ms\./);
            now = 1000;
            expect((await getState()).isError).not.toBe(true);
            expect(h.call).toHaveBeenCalledTimes(3);
        } finally {
            await h.close();
        }
    });

    it("sends a cover as an image block the client accepts, and honours the byte limit", async () => {
        const png = "iVBORw0KGgo=";
        const cover = { available: true, type: "front", mimeType: "image/png", size: 8, dataUrl: `data:image/png;base64,${png}` };
        const getCurrent = { name: "fb2k_track_read", arguments: { action: "artwork.getCurrent" } };
        const h = await createHarness({}, cover);
        try {
            const result = await h.client.callTool(getCurrent);

            expect(result.isError).not.toBe(true);
            expect(result.content).toEqual([
                { type: "text", text: '{"available":true,"type":"front","mimeType":"image/png","size":8}' },
                { type: "image", data: png, mimeType: "image/png" },
            ]);
            expect(h.call).toHaveBeenCalledWith("artwork.getCurrent", { type: "front" });
        } finally {
            await h.close();
        }

        const small = await createHarness({ maxImageBytes: 4 }, cover);
        try {
            const result = await small.client.callTool(getCurrent);
            expect(result.content).toHaveLength(1);
            expect(text(result)).toMatch(/\[Picture not attached: it has 8 bytes, more than the 4 this server attaches/);
        } finally {
            await small.close();
        }
    });

    it("returns compact JSON and cuts a result longer than the limit", async () => {
        const h = await createHarness({ maxResponseChars: 40 }, { rows: "x".repeat(100) });
        try {
            const result = await h.client.callTool({
                name: "fb2k_playback_read",
                arguments: { action: "playback.getState" },
            });
            const body = text(result);

            expect(result.isError).not.toBe(true);
            expect(body.startsWith('{"rows":"xxxx')).toBe(true);
            expect(body).toMatch(/\n\[Truncated: the result has 111 characters and this server returns at most 40\./);
        } finally {
            await h.close();
        }
    });
});

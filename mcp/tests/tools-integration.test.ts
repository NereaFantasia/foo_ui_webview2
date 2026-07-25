/**
 * MCP 工具集成测试 — 错误路径与边界情况
 *
 * 使用 Mock CDP 验证 MCP Server 的完整工具调用链路：
 * - 工具定义完整性（每个工具有 name/description/schema）
 * - 工具名 → bridge 方法映射完整性
 * - 参数 schema 结构正确性
 * - 方法映射一致性
 */

import { describe, it, expect } from "vitest";

import { playbackTools, playbackMethodMap } from "../src/tools/playback.js";
import { playbackExtTools, playbackExtMethodMap } from "../src/tools/playback-ext.js";
import { playlistTools, playlistMethodMap } from "../src/tools/playlist.js";
import { playlistExtTools, playlistExtMethodMap } from "../src/tools/playlist-ext.js";
import { libraryTools, libraryMethodMap } from "../src/tools/library.js";
import { artworkTools, artworkMethodMap } from "../src/tools/artwork.js";
import { queueTools, queueMethodMap } from "../src/tools/queue.js";
import { metadataTools, metadataMethodMap } from "../src/tools/metadata.js";
import { buildToolInputSchema } from "../src/tool-schema.js";
import type { ToolDefinition } from "../src/types.js";

const allTools: ToolDefinition[] = [
    ...playbackTools,
    ...playbackExtTools,
    ...playlistTools,
    ...playlistExtTools,
    ...libraryTools,
    ...artworkTools,
    ...queueTools,
    ...metadataTools,
];

const allMethodMaps: Record<string, string> = {
    ...playbackMethodMap,
    ...playbackExtMethodMap,
    ...playlistMethodMap,
    ...playlistExtMethodMap,
    ...libraryMethodMap,
    ...artworkMethodMap,
    ...queueMethodMap,
    ...metadataMethodMap,
};

// ── 工具定义完整性 ──────────────────────────

describe("工具定义完整性", () => {
    for (const tool of allTools) {
        describe(tool.name, () => {
            it("有非空 description", () => {
                expect(tool.description).toBeTruthy();
                expect(tool.description.length).toBeGreaterThan(0);
            });

            it("inputSchema.type 是 'object'", () => {
                expect(tool.inputSchema.type).toBe("object");
            });

            it("inputSchema.properties 是对象", () => {
                expect(tool.inputSchema.properties).toBeDefined();
                expect(typeof tool.inputSchema.properties).toBe("object");
            });

            it("有对应的 bridge 方法映射", () => {
                expect(allMethodMaps[tool.name]).toBeDefined();
            });

            // 检查 required 字段中的 key 都在 properties 中
            if (tool.inputSchema.required) {
                it("required 字段都在 properties 中", () => {
                    for (const key of tool.inputSchema.required!) {
                        expect(
                            tool.inputSchema.properties[key],
                            `required key '${key}' not in properties`
                        ).toBeDefined();
                    }
                });
            }

            // 检查每个 property 的 type 有效
            for (const [key, prop] of Object.entries(tool.inputSchema.properties)) {
                it(`属性 '${key}' 有有效的 type`, () => {
                    expect([
                        "string",
                        "number",
                        "integer",
                        "boolean",
                        "union",
                        "array",
                        "object",
                    ]).toContain(prop.type);
                });

                it(`属性 '${key}' 有 description`, () => {
                    expect(prop.description).toBeTruthy();
                });
            }
        });
    }
});

// ── 方法映射一致性 ──────────────────────────

describe("方法映射一致性", () => {
    it("每个工具都有且只有一个方法映射", () => {
        const toolNames = allTools.map((t) => t.name);
        const mapNames = Object.keys(allMethodMaps);

        // 工具定义和映射表的 key 集合应该一致
        expect(new Set(toolNames)).toEqual(new Set(mapNames));
    });

    it("方法映射格式正确（namespace.method）", () => {
        for (const [, method] of Object.entries(allMethodMaps)) {
            expect(method).toMatch(
                /^[a-z]+\.[a-z][a-zA-Z]*$/,
                `方法名 '${method}' 不符合 namespace.method 格式`
            );
        }
    });

    it("工具名格式正确（fb2k_namespace_method）", () => {
        for (const tool of allTools) {
            expect(tool.name).toMatch(
                /^fb2k_[a-z]+_[a-z_]+$/,
                `工具名 '${tool.name}' 不符合 fb2k_namespace_method 格式`
            );
        }
    });
});

// ── 命名空间分组 ────────────────────────────

describe("命名空间分组", () => {
    it("playback 有 12 个工具", () => {
        expect(playbackTools).toHaveLength(12);
    });

    it("playback-ext 有 13 个工具", () => {
        expect(playbackExtTools).toHaveLength(13);
    });

    it("playlist 有 7 个工具", () => {
        expect(playlistTools).toHaveLength(7);
    });

    it("playlist-ext 有 40 个工具", () => {
        expect(playlistExtTools).toHaveLength(40);
    });

    it("library 有 4 个工具", () => {
        expect(libraryTools).toHaveLength(4);
    });

    it("artwork 有 2 个工具", () => {
        expect(artworkTools).toHaveLength(2);
    });

    it("queue 有 8 个工具", () => {
        expect(queueTools).toHaveLength(8);
    });

    it("metadata 有 12 个工具", () => {
        expect(metadataTools).toHaveLength(12);
    });

    it("总计 98 个 bridge 工具", () => {
        expect(allTools).toHaveLength(98);
    });
});

// ── 参数校验边界 ────────────────────────────

describe("参数校验边界", () => {
    it("playback.setPosition 的 seconds 有 minimum 约束", () => {
        const tool = playbackTools.find(
            (t) => t.name === "fb2k_playback_set_position"
        )!;
        expect(tool.inputSchema.properties.seconds.minimum).toBe(0);
        expect(tool.inputSchema.required).toContain("seconds");
    });

    it("playback.setVolume 的 volume 有 min/max 约束", () => {
        const tool = playbackTools.find(
            (t) => t.name === "fb2k_playback_set_volume"
        )!;
        expect(tool.inputSchema.properties.volume.minimum).toBe(0);
        expect(tool.inputSchema.properties.volume.maximum).toBe(100);
        expect(tool.inputSchema.required).toContain("volume");
    });

    it("playlist.getTracks 的 count 有 max 约束", () => {
        const tool = playlistTools.find(
            (t) => t.name === "fb2k_playlist_get_tracks"
        )!;
        expect(tool.inputSchema.properties.count.maximum).toBe(500);
    });

    it("library.search 的 query 是必填", () => {
        const tool = libraryTools.find(
            (t) => t.name === "fb2k_library_search"
        )!;
        expect(tool.inputSchema.required).toContain("query");
    });

    it("library.search 与 Bridge 默认值 100 对齐且不注入 MCP default", () => {
        const tool = libraryTools.find(
            (t) => t.name === "fb2k_library_search"
        )!;
        const limit = tool.inputSchema.properties.limit;

        expect(limit.description).toContain("default 100");
        expect(limit.default).toBeUndefined();
        expect(buildToolInputSchema(tool.inputSchema).parse({ query: "artist IS Mili" }))
            .toEqual({ query: "artist IS Mili" });
    });

    it("artwork.getForTrack 的 path 是必填", () => {
        const tool = artworkTools.find(
            (t) => t.name === "fb2k_artwork_get_for_track"
        )!;
        expect(tool.inputSchema.required).toContain("path");
    });

    it("artwork 封面类型枚举完整", () => {
        const tool = artworkTools.find(
            (t) => t.name === "fb2k_artwork_get_current"
        )!;
        expect(tool.inputSchema.properties.type.enum).toEqual([
            "front",
            "back",
            "disc",
            "icon",
            "artist",
        ]);
    });

    it("playlist.create 的 name 是必填 string", () => {
        const tool = playlistTools.find(
            (t) => t.name === "fb2k_playlist_create"
        )!;
        expect(tool.inputSchema.required).toContain("name");
        expect(tool.inputSchema.properties.name.type).toBe("string");
    });

    it("playlist.playTrack 的 index 是必填，playlist 是可选", () => {
        const tool = playlistTools.find(
            (t) => t.name === "fb2k_playlist_play_track"
        )!;
        expect(tool.inputSchema.required).toContain("index");
        expect(tool.inputSchema.required).not.toContain("playlist");
    });
});

// ── 无参数工具 ──────────────────────────────

describe("无参数工具", () => {
    const noParamTools = allTools.filter(
        (t) => Object.keys(t.inputSchema.properties).length === 0
    );

    it("无参数工具的 properties 是空对象", () => {
        for (const tool of noParamTools) {
            expect(Object.keys(tool.inputSchema.properties)).toHaveLength(0);
        }
    });

    it("无参数工具没有 required 字段或为空", () => {
        for (const tool of noParamTools) {
            if (tool.inputSchema.required) {
                expect(tool.inputSchema.required).toHaveLength(0);
            }
        }
    });

    // 预期的无参工具列表
    const expectedNoParam = [
        // playback P0
        "fb2k_playback_play",
        "fb2k_playback_pause",
        "fb2k_playback_stop",
        "fb2k_playback_next",
        "fb2k_playback_previous",
        "fb2k_playback_play_pause",
        "fb2k_playback_get_state",
        "fb2k_playback_get_current_track",
        "fb2k_playback_get_position",
        "fb2k_playback_get_volume",
        // playback-ext
        "fb2k_playback_toggle_mute",
        "fb2k_playback_volume_up",
        "fb2k_playback_volume_down",
        "fb2k_playback_get_playback_order",
        "fb2k_playback_get_stop_after_current",
        "fb2k_playback_toggle_stop_after_current",
        "fb2k_playback_get_playing_playlist",
        "fb2k_playback_random",
        // playlist P0
        "fb2k_playlist_get_all",
        "fb2k_playlist_get_active",
        // playlist-ext
        "fb2k_playlist_get_count",
        "fb2k_playlist_get_playing",
        "fb2k_playlist_get_available_columns",
        // library
        "fb2k_library_get_albums",
        "fb2k_library_get_artists",
        "fb2k_library_get_stats",
        // queue
        "fb2k_queue_get",
        "fb2k_queue_clear",
        "fb2k_queue_get_count",
        "fb2k_queue_flush",
    ];

    it(`有 ${expectedNoParam.length} 个无参数工具`, () => {
        const names = noParamTools.map((t) => t.name);
        expect(new Set(names)).toEqual(new Set(expectedNoParam));
    });
});

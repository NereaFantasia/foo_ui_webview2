/**
 * 生成的 Bridge 工具集成测试
 *
 * 工具定义由 scripts/api-schema/generate.mjs 从宿主方法声明与 mcp/tool-table.json 生成，
 * 生成器负责核对方法存在、参数对得上、注解由方法的 @effect 算出。这里核对生成物本身：
 * - 工具面：哪些工具、各自的 action、注解，以及不再暴露的方法
 * - 合并 schema 与逐 action schema 的关系
 * - 覆盖表里比声明更严的限制（必填、数值范围）确实落在对应 action 上
 */

import { describe, it, expect } from "vitest";

import { bridgeTools } from "../src/generated/bridge-tools.js";
import { buildToolInputSchema } from "../src/tool-schema.js";
import type { InputSchema, SchemaProperty, ToolDefinition } from "../src/types.js";

function tool(name: string): ToolDefinition {
    const found = bridgeTools.find((t) => t.name === name);
    if (!found) throw new Error(`missing generated bridge tool ${name}`);
    return found;
}

function action(api: string): InputSchema {
    const owner = bridgeTools.find((t) => api in t.actions);
    if (!owner) throw new Error(`no generated tool has the action ${api}`);
    return owner.actions[api].inputSchema;
}

function property(api: string, key: string): SchemaProperty {
    const found = action(api).properties[key];
    if (!found) throw new Error(`${api} has no parameter ${key}`);
    return found;
}

function bounds(prop: SchemaProperty): { minimum?: number; maximum?: number } {
    if (prop.type !== "integer" && prop.type !== "number") {
        throw new Error(`expected a numeric parameter, got ${prop.type}`);
    }
    return { minimum: prop.minimum, maximum: prop.maximum };
}

const keys = (api: string) => Object.keys(action(api).properties);

// ── 工具面 ──────────────────────────────────

describe("工具面", () => {
    it("10 个工具，按命名空间与读写性质分组", () => {
        expect(bridgeTools.map((t) => t.name)).toEqual([
            "fb2k_playback_read",
            "fb2k_playback_control",
            "fb2k_playlist_read",
            "fb2k_playlist_manage",
            "fb2k_playlist_edit",
            "fb2k_playlist_select",
            "fb2k_library_read",
            "fb2k_queue_edit",
            "fb2k_track_read",
            "fb2k_track_write",
        ]);
    });

    it("共 90 个 action，每个宿主方法只属于一个工具", () => {
        const all = bridgeTools.flatMap((t) => Object.keys(t.actions));
        expect(all).toHaveLength(90);
        expect(new Set(all).size).toBe(all.length);
    });

    it("废弃、别名与被覆盖的方法不再暴露", () => {
        const all = new Set(bridgeTools.flatMap((t) => Object.keys(t.actions)));
        for (const api of [
            "playlist.focusTrack",
            "playlist.getFocusTrack",
            "queue.flush",
            "metadata.removeField",
            "playlist.getCount",
            "playlist.getTrackCount",
            "playlist.isLocked",
            "playlist.isAutoplaylist",
            "playlist.getAutoplaylistQuery",
            "queue.getCount",
            "metadata.readByPath",
            "metadata.removeTag",
            "playlist.addPathsAsync",
        ]) {
            expect(all.has(api), api).toBe(false);
        }
    });

    it("注解由方法的 @effect 算出：读工具只读，删除或覆盖数据的工具标破坏性", () => {
        const annotations = Object.fromEntries(bridgeTools.map((t) => [t.name, t.annotations]));
        const read = { readOnlyHint: true, openWorldHint: false };
        expect(annotations).toEqual({
            fb2k_playback_read: read,
            fb2k_playlist_read: read,
            fb2k_library_read: read,
            fb2k_track_read: read,
            fb2k_playback_control: { readOnlyHint: false, destructiveHint: false, idempotentHint: false, openWorldHint: false },
            fb2k_playlist_select: { readOnlyHint: false, destructiveHint: false, idempotentHint: true, openWorldHint: false },
            fb2k_playlist_manage: { readOnlyHint: false, destructiveHint: true, idempotentHint: false, openWorldHint: false },
            fb2k_playlist_edit: { readOnlyHint: false, destructiveHint: true, idempotentHint: false, openWorldHint: false },
            fb2k_queue_edit: { readOnlyHint: false, destructiveHint: true, idempotentHint: false, openWorldHint: false },
            fb2k_track_write: { readOnlyHint: false, destructiveHint: true, idempotentHint: true, openWorldHint: false },
        });
    });
});

// ── 定义完整性 ──────────────────────────────

describe("定义完整性", () => {
    for (const t of bridgeTools) {
        describe(t.name, () => {
            it("描述列出每个 action", () => {
                for (const api of Object.keys(t.actions)) {
                    expect(t.description).toContain(`- ${api}`);
                }
            });

            it("合并 schema 能建出，action 必填且枚举正好是全部 action", () => {
                expect(() => buildToolInputSchema(t.inputSchema)).not.toThrow();
                expect(t.inputSchema.required).toEqual(["action"]);
                const actionProp = t.inputSchema.properties.action;
                expect(actionProp.type === "string" ? actionProp.enum : undefined).toEqual(Object.keys(t.actions));
            });

            it("合并 schema 的参数正好是各 action 参数的并集，且都有说明", () => {
                const union = new Set(Object.values(t.actions).flatMap((a) => Object.keys(a.inputSchema.properties)));
                expect(new Set(Object.keys(t.inputSchema.properties))).toEqual(new Set(["action", ...union]));
                for (const [key, prop] of Object.entries(t.inputSchema.properties)) {
                    expect(prop.description, key).toBeTruthy();
                }
            });

            it("每个 action 的 schema 能建出，required 都在 properties 里", () => {
                for (const [api, a] of Object.entries(t.actions)) {
                    expect(() => buildToolInputSchema(a.inputSchema), api).not.toThrow();
                    for (const key of a.inputSchema.required ?? []) {
                        expect(a.inputSchema.properties[key], `${api}.${key}`).toBeDefined();
                    }
                }
            });
        });
    }
});

// ── 覆盖表里比声明更严的限制 ────────────────

describe("覆盖表的限制", () => {
    it("playback.setPosition 的 position 必填且不能为负", () => {
        expect(bounds(property("playback.setPosition", "position")).minimum).toBe(0);
        expect(action("playback.setPosition").required).toContain("position");
    });

    it("playback.setVolume 的 volume 是 0 到 100 的百分比", () => {
        expect(bounds(property("playback.setVolume", "volume"))).toEqual({ minimum: 0, maximum: 100 });
    });

    it("playlist.getTracks、library.search 与 library.getAlbums 一页最多 500 行", () => {
        expect(bounds(property("playlist.getTracks", "count")).maximum).toBe(500);
        expect(bounds(property("library.search", "limit")).maximum).toBe(500);
        expect(bounds(property("library.getAlbums", "limit")).maximum).toBe(500);
    });

    it("library.getAlbums 的 limit 仍可为 0，只取 total", () => {
        const albums = buildToolInputSchema(action("library.getAlbums"));
        expect(albums.parse({ limit: 0 })).toMatchObject({ limit: 0 });
        expect(() => albums.parse({ limit: 501 })).toThrow();
    });

    it("library.getArtists 的 limit 上限取声明的默认值 1000，省略时照旧是 1000", () => {
        expect(bounds(property("library.getArtists", "limit")).maximum).toBe(1000);
        const artists = buildToolInputSchema(action("library.getArtists"));
        expect(artists.parse({})).toMatchObject({ limit: 1000 });
        expect(() => artists.parse({ limit: 1001 })).toThrow();
    });

    it("library.search 必须给出 query，limit 的默认值取自声明", () => {
        expect(action("library.search").required).toContain("query");
        expect(buildToolInputSchema(action("library.search")).parse({ query: "artist IS Mili" }))
            .toEqual({ query: "artist IS Mili", offset: 0, limit: 100 });
    });

    it("声明里的可选参数照常暴露，只有会内联封面的两个与 playlist.remove 的 playlistGuid 不暴露", () => {
        expect(keys("library.getAlbums")).toEqual([
            "sort",
            "query",
            "offset",
            "limit",
            "includeTracks",
            "useCache",
        ]);
        expect(keys("library.getArtists")).toEqual(["sort", "limit", "includeAlbums"]);
        expect(keys("library.search")).toContain("fields");
        expect(keys("playlist.getTracks")).toEqual([
            "playlist",
            "playlistGuid",
            "start",
            "count",
            "formats",
            "fields",
        ]);
        expect(keys("playlist.playTrack")).toEqual([
            "playlist",
            "playlistGuid",
            "index",
            "deferred",
            "muted",
        ]);
        // playlist 必填时再给 playlistGuid 就是两者都给，宿主以 INVALID_PARAMS 拒绝
        expect(keys("playlist.remove")).toEqual(["playlist"]);
        expect(keys("playlist.create")).toEqual(["name", "position"]);
        expect(keys("metadata.write")).toEqual(["path", "tags", "cueIndex"]);
    });

    it("合并 schema 不暴露会内联封面的参数", () => {
        const merged = Object.keys(tool("fb2k_library_read").inputSchema.properties);
        expect(merged).not.toContain("includeCover");
        expect(merged).not.toContain("coverMaxSize");
    });

    it("必须点名目标的 action", () => {
        expect(action("playlist.playTrack").required).toEqual(["index"]);
        expect(action("playlist.create").required).toEqual(["name"]);
        expect(action("playlist.remove").required).toEqual(["playlist"]);
        expect(action("playlist.setFocusedTrack").required).toEqual(["index"]);
    });

    it("只有返回 data URL 封面的两个 action 把 dataUrl 当图片返回，说明里也写明", () => {
        const withImage = bridgeTools.flatMap((t) =>
            Object.entries(t.actions).filter(([, a]) => a.image).map(([api, a]) => [api, a.image]));
        expect(withImage).toEqual([
            ["artwork.getForTrack", "dataUrl"],
            ["artwork.getCurrent", "dataUrl"],
        ]);
        const description = tool("fb2k_track_read").description;
        expect(description).toContain("- artwork.getCurrent(type?): Get the cover art of the currently playing track; the picture comes back as an image");
        expect(description).not.toMatch(/base64/);
    });

    it("封面类型枚举取自声明", () => {
        const type = property("artwork.getCurrent", "type");
        expect(type.type === "string" ? type.enum : undefined).toEqual([
            "front",
            "cover_front",
            "back",
            "cover_back",
            "disc",
            "icon",
            "artist",
        ]);
    });
});

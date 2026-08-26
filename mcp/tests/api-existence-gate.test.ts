/**
 * API 存在性硬门禁 — 防幻觉拦截层
 *
 * 此测试文件是 MCP 工具的一道防线：确保每一个 MCP 工具映射的 C++ API
 * 端点**确实存在**于 src/api/*.cpp 的 RegisterApi 注册中。
 *
 * 背景：AI Agent 在新增 MCP 工具时极易"虚构"不存在的 C++ RegisterApi 端点。
 *
 * 单一真相源：直接实时解析 src/api/*.cpp 的 RegisterApi（与 api-alignment.test.ts
 * 同源）。不再维护手写白名单——白名单会过期、需人工同步，且与实时解析重复。
 *
 * 校验层次：
 * 1. [本文件] MCP 方法映射 → C++ RegisterApi（实时源码校验，拦截虚构端点）
 * 2. [本文件] MCP 工具名唯一性
 * 3. [api-alignment] MCP 方法映射 → C++ RegisterApi + 参数 key 对齐（更深一层）
 *
 * 注：src/api/ 不在发布 npm 包内；当目录缺失时本门禁自动跳过，仅在仓库上下文强制。
 */

import { describe, it, expect } from "vitest";
import { readFileSync, readdirSync, existsSync } from "fs";
import { resolve, join } from "path";

import { playbackMethodMap } from "../src/tools/playback.js";
import { playbackExtMethodMap } from "../src/tools/playback-ext.js";
import { playlistMethodMap } from "../src/tools/playlist.js";
import { playlistExtMethodMap } from "../src/tools/playlist-ext.js";
import { libraryMethodMap } from "../src/tools/library.js";
import { artworkMethodMap } from "../src/tools/artwork.js";
import { queueMethodMap } from "../src/tools/queue.js";
import { metadataMethodMap } from "../src/tools/metadata.js";

// ── 工具方法映射全集 ─────────────────────────
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

// ── C++ 源码解析（单一真相源）─────────────────
const PROJECT_ROOT = resolve(__dirname, "../..");
const API_DIR = join(PROJECT_ROOT, "src", "api");

/**
 * 实时提取 src/api/*.cpp 中所有 RegisterApi("method.name", ...) 注册的方法名。
 *
 * 延迟响应变体 RegisterApiDeferred("method.name", ...) 计入同一个注册面：BridgeCore
 * 把两类 handler 分表存放，但对外只暴露一个注册面（src/api/BridgeCore.cpp:117 HasApi
 * 对两张表取或，GetRegisteredApiNames 取并），所以把一个 API 迁到 deferred 不该让它
 * 从存在性集合里掉出去、被误判成幻觉端点。与 Graph 侧权威扫描
 * scripts/graph/lib/cpp-runtime-contract-scan.mjs 的 REGISTER_API_RE 同口径。
 */
function extractRegisteredApisFromCpp(): Set<string> {
    if (!existsSync(API_DIR)) return new Set();

    const apis = new Set<string>();
    const pattern = /RegisterApi(?:Deferred)?\s*\(\s*"([^"]+)"/g;

    const files = readdirSync(API_DIR).filter((f) => f.endsWith(".cpp"));
    for (const file of files) {
        const content = readFileSync(join(API_DIR, file), "utf-8");
        let match: RegExpExecArray | null;
        while ((match = pattern.exec(content)) !== null) {
            apis.add(match[1]);
        }
    }
    return apis;
}

// ═══════════════════════════════════════════════════

describe("API 存在性硬门禁（实时校验 C++ RegisterApi）", () => {
    const cppApis = extractRegisteredApisFromCpp();

    describe("层 1: MCP 工具映射 → C++ RegisterApi（实时源码校验，防幻觉）", () => {
        if (cppApis.size === 0) {
            it.skip("src/api/ 目录不存在（npm 包上下文），跳过存在性门禁", () => {});
        } else {
            for (const [toolName, method] of Object.entries(allMethodMaps)) {
                it(`${toolName} → ${method} 在 C++ src/api 中已注册`, () => {
                    expect(
                        cppApis.has(method),
                        `\n` +
                        `╔══════════════════════════════════════════════════╗\n` +
                        `║ 疑似幻觉 API 端点！                                ║\n` +
                        `║                                                  ║\n` +
                        `║ MCP 工具 "${toolName}"\n` +
                        `║ 映射到 bridge 方法 "${method}"\n` +
                        `║ 但 src/api/*.cpp 中没有对应的 RegisterApi 注册。   ║\n` +
                        `║                                                  ║\n` +
                        `║ 请执行：                                          ║\n` +
                        `║ 1. grep -E 'RegisterApi(Deferred)?\\("${method}"' src/api/*.cpp\n` +
                        `║ 2. 若存在 → 检查方法名拼写是否一致                 ║\n` +
                        `║ 3. 若不存在 → 删除该 MCP 工具映射或修正方法名      ║\n` +
                        `╚══════════════════════════════════════════════════╝`
                    ).toBe(true);
                });
            }
        }
    });

    describe("层 2: 完整性", () => {
        it("所有 MCP 工具名唯一", () => {
            const names = Object.keys(allMethodMaps);
            expect(new Set(names).size).toBe(names.length);
        });
    });
});

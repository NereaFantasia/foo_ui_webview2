/**
 * API 参数对齐门禁测试
 *
 * 自动解析 C++ 源码中的 RegisterApi 调用和 handler 参数 key，
 * 与 MCP 工具定义进行交叉验证，防止参数不匹配。
 *
 * 验证三点：
 * 1. MCP 工具映射的 bridge 方法必须在 C++ 端有 RegisterApi 注册
 * 2. MCP 工具定义的参数 key 必须在 C++ handler 中有对应的 params 读取
 * 3. C++ handler 中 required 的参数 key 必须在 MCP 工具定义中出现
 */

import { describe, it, expect } from "vitest";
import { readFileSync, readdirSync } from "fs";
import { resolve, join } from "path";

import { playbackMethodMap } from "../src/tools/playback.js";
import { playlistMethodMap } from "../src/tools/playlist.js";
import { libraryMethodMap } from "../src/tools/library.js";
import { artworkMethodMap } from "../src/tools/artwork.js";
import { playbackExtMethodMap } from "../src/tools/playback-ext.js";
import { playlistExtMethodMap } from "../src/tools/playlist-ext.js";
import { queueMethodMap } from "../src/tools/queue.js";
import { metadataMethodMap } from "../src/tools/metadata.js";
import { playbackTools } from "../src/tools/playback.js";
import { playlistTools } from "../src/tools/playlist.js";
import { libraryTools } from "../src/tools/library.js";
import { artworkTools } from "../src/tools/artwork.js";
import { playbackExtTools } from "../src/tools/playback-ext.js";
import { playlistExtTools } from "../src/tools/playlist-ext.js";
import { queueTools } from "../src/tools/queue.js";
import { metadataTools } from "../src/tools/metadata.js";
import type { ToolDefinition } from "../src/types.js";

// ── 路径配置 ─────────────────────────────────
const PROJECT_ROOT = resolve(__dirname, "../..");
const API_DIR = join(PROJECT_ROOT, "src", "api");

// ── 工具索引 ─────────────────────────────────
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

// ── C++ 源码解析工具 ─────────────────────────

/** 读取 src/api/ 下所有 .cpp 文件的内容 */
function loadApiSources(): Map<string, string> {
    const sources = new Map<string, string>();
    const files = readdirSync(API_DIR).filter((f) => f.endsWith(".cpp"));
    for (const file of files) {
        const content = readFileSync(join(API_DIR, file), "utf-8");
        sources.set(file, content);
    }
    return sources;
}

/**
 * 从所有源码中提取 RegisterApi("method.name", ...) 注册记录。
 *
 * 延迟响应变体 RegisterApiDeferred 计入同一个注册面：BridgeCore 分表存放两类
 * handler，但对外只有一个注册面（src/api/BridgeCore.cpp:117 HasApi 对两张表取或），
 * 迁到 deferred 的 API 不该从注册记录里掉出去。与 Graph 侧权威扫描
 * scripts/graph/lib/cpp-runtime-contract-scan.mjs 的 REGISTER_API_RE 同口径。
 */
function extractRegisteredApis(
    sources: Map<string, string>
): Map<string, { file: string; line: number }> {
    const apis = new Map<string, { file: string; line: number }>();
    const pattern = /RegisterApi(?:Deferred)?\s*\(\s*"([^"]+)"/g;

    for (const [file, content] of sources) {
        const lines = content.split("\n");
        for (let i = 0; i < lines.length; i++) {
            let match: RegExpExecArray | null;
            pattern.lastIndex = 0;
            while ((match = pattern.exec(lines[i])) !== null) {
                apis.set(match[1], { file, line: i + 1 });
            }
        }
    }
    return apis;
}

/**
 * 从 C++ 源码中提取 handler 函数读取的参数 key。
 * 
 * 搜索模式：
 * - params.value("key", ...)
 * - params["key"]
 * - params.at("key")
 * - params.contains("key")
 * 
 * 策略：找到 RegisterApi 绑定的 handler 函数名，然后在对应源文件中搜索
 * 该函数体内所有 params 访问。
 */
function extractHandlerParamKeys(
    apiName: string,
    sources: Map<string, string>
): string[] {
    const keys = new Set<string>();

    // 1. 找到 RegisterApi("apiName", HandlerFunc) 中的 HandlerFunc 名。
    //    RegisterApiDeferred 的第二参同样是 handler 符号（LibraryApi.cpp:2619 的
    //    LibrarySearchDeferred），捕获组语义不变；其定义形如
    //    `void XxxDeferred(const json& params, const DeferredResponder&)`，
    //    返回类型 void 已在下方 funcPattern 的备选里，函数体仍能定位。
    const registerPattern = new RegExp(
        `RegisterApi(?:Deferred)?\\s*\\(\\s*"${escapeRegex(apiName)}"\\s*,\\s*(\\w+)`
    );
    let handlerName: string | undefined;
    let sourceFile: string | undefined;

    for (const [file, content] of sources) {
        const match = registerPattern.exec(content);
        if (match) {
            handlerName = match[1];
            sourceFile = file;
            break;
        }
    }

    if (!handlerName || !sourceFile) return [];

    const content = sources.get(sourceFile)!;

    // 2. 找到 handler 函数体（从函数定义开始到下一个同层 } ）
    const funcPattern = new RegExp(
        `(?:^|\\n)\\s*(?:static\\s+)?(?:json|nlohmann::json|void|auto)\\s+${escapeRegex(handlerName)}\\s*\\(`
    );
    const funcMatch = funcPattern.exec(content);

    if (!funcMatch) {
        // 可能是 lambda 或其他形式，回退到全文搜索该 handler 名附近的 params 访问
        extractParamKeysFromText(content, keys);
        return [...keys];
    }

    // 从函数开始位置提取函数体（简化：取函数开始后 200 行）
    const startIdx = funcMatch.index;
    const funcBody = content.substring(startIdx, startIdx + 15000);

    extractParamKeysFromText(funcBody, keys);

    // 3. 检查是否调用了 helper 函数（如 GetPlaylistIndexFromParams）
    const helperPattern = /(\w+)\s*\(\s*params/g;
    let helperMatch: RegExpExecArray | null;
    while ((helperMatch = helperPattern.exec(funcBody)) !== null) {
        const helperName = helperMatch[1];
        if (helperName === "value" || helperName === "contains" || helperName === "at") continue;
        // 搜索 helper 函数的实现
        const helperFuncPattern = new RegExp(
            `(?:^|\\n)\\s*(?:static\\s+)?(?:\\w+)\\s+${escapeRegex(helperName)}\\s*\\(`,
            "m"
        );
        for (const [, helperContent] of sources) {
            const hMatch = helperFuncPattern.exec(helperContent);
            if (hMatch) {
                const helperBody = helperContent.substring(
                    hMatch.index,
                    hMatch.index + 5000
                );
                extractParamKeysFromText(helperBody, keys);
            }
        }
    }

    return [...keys];
}

function extractParamKeysFromText(text: string, keys: Set<string>): void {
    // params.value("key", ...)
    const valuePattern = /params\.value\(\s*"([^"]+)"/g;
    let m: RegExpExecArray | null;
    while ((m = valuePattern.exec(text)) !== null) keys.add(m[1]);

    // params["key"]
    const bracketPattern = /params\["([^"]+)"\]/g;
    while ((m = bracketPattern.exec(text)) !== null) keys.add(m[1]);

    // params.at("key")
    const atPattern = /params\.at\(\s*"([^"]+)"/g;
    while ((m = atPattern.exec(text)) !== null) keys.add(m[1]);

    // params.contains("key")
    const containsPattern = /params\.contains\(\s*"([^"]+)"/g;
    while ((m = containsPattern.exec(text)) !== null) keys.add(m[1]);
}

function escapeRegex(str: string): string {
    return str.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
}

// ── 测试 ─────────────────────────────────────

describe("API 参数对齐门禁", () => {
    const sources = loadApiSources();
    const registeredApis = extractRegisteredApis(sources);

    describe("验证点 1: API 注册存在性", () => {
        for (const [toolName, method] of Object.entries(allMethodMaps)) {
            it(`${toolName} → ${method} 应在 C++ 端已注册`, () => {
                const registration = registeredApis.get(method);
                expect(
                    registration,
                    `RegisterApi("${method}") / RegisterApiDeferred("${method}") 均未在 src/api/*.cpp 中找到`
                ).toBeDefined();
            });
        }
    });

    describe("验证点 2: MCP 参数 key → C++ handler 有读取", () => {
        for (const tool of allTools) {
            const method = allMethodMaps[tool.name];
            if (!method) continue;

            const mcpKeys = Object.keys(tool.inputSchema.properties);
            if (mcpKeys.length === 0) continue;

            it(`${tool.name}: MCP 参数 {${mcpKeys.join(", ")}} 应在 C++ handler 中被读取`, () => {
                const cppKeys = extractHandlerParamKeys(method, sources);

                for (const key of mcpKeys) {
                    expect(
                        cppKeys,
                        `MCP 工具 ${tool.name} 的参数 "${key}" 在 C++ handler 中未找到对应的 params 读取。\n` +
                        `C++ handler 读取的 key: [${cppKeys.join(", ")}]`
                    ).toContain(key);
                }
            });
        }
    });

    describe("验证点 3: C++ required 参数在 MCP 定义中存在", () => {
        // 此处列举 C++ 端明确必填的参数（无 default 值、或检查后报错的 key）
        // 从三点验证的手动审计结果中提取
        const cppRequiredParams: Record<string, string[]> = {
            "artwork.getForTrack": ["path"],
            "library.search": ["query"],
            "playlist.setActive": ["playlist"],
            "playlist.remove": ["playlist"],
            "playlist.playTrack": ["index"],
            "playlist.create": ["name"],
            "playback.setPosition": ["seconds"],  // C++ 也接受 "position" 作为首选
            "playback.setVolume": ["volume"],
        };

        for (const [method, requiredKeys] of Object.entries(cppRequiredParams)) {
            const tool = allTools.find(
                (t) => allMethodMaps[t.name] === method
            );
            if (!tool) continue;

            it(`${tool.name}: C++ 必填参数 {${requiredKeys.join(", ")}} 应在 MCP 定义中存在`, () => {
                const mcpKeys = Object.keys(tool.inputSchema.properties);
                for (const key of requiredKeys) {
                    expect(
                        mcpKeys,
                        `C++ handler 的必填参数 "${key}" 在 MCP 工具 ${tool.name} 的 inputSchema 中缺失`
                    ).toContain(key);
                }
            });
        }
    });

    describe("完整性: 所有工具都有方法映射", () => {
        for (const tool of allTools) {
            it(`${tool.name} 应有对应的 bridge 方法映射`, () => {
                expect(
                    allMethodMaps[tool.name],
                    `工具 ${tool.name} 在 methodMap 中无对应条目`
                ).toBeDefined();
            });
        }
    });

    describe("唯一性: 无重复工具名", () => {
        it("所有工具名应唯一", () => {
            const names = allTools.map((t) => t.name);
            const uniqueNames = new Set(names);
            expect(names.length).toBe(uniqueNames.size);
        });
    });
});

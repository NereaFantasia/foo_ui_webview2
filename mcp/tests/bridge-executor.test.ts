/**
 * BridgeExecutor 单元测试
 *
 * 使用 Mock CdpClient 验证：
 * - call() 正常调用和结果标准化
 * - call() 错误处理
 * - screenshot() 委托
 * - evaluate() 委托
 * - getConsoleMessages() 委托
 */

import { describe, it, expect, vi, beforeEach } from "vitest";
import {
    BridgeExecutor,
    createBridgeToolHandler,
    formatBridgeFailure,
    type BridgeResultOptions,
} from "../src/bridge-executor.js";
import type { CdpClient } from "../src/cdp-client.js";

// ── Mock CdpClient ──────────────────────────

function createMockCdp(): CdpClient {
    return {
        invoke: vi.fn(),
        evaluate: vi.fn(),
        screenshot: vi.fn(),
        getConsoleMessages: vi.fn(),
        connect: vi.fn(),
        disconnect: vi.fn(),
        ensureConnected: vi.fn(),
        connected: true,
    } as unknown as CdpClient;
}

describe("BridgeExecutor", () => {
    let cdp: ReturnType<typeof createMockCdp>;
    let bridge: BridgeExecutor;

    beforeEach(() => {
        cdp = createMockCdp();
        bridge = new BridgeExecutor(cdp);
    });

    // ── call() ──────────────────────────────

    describe("call()", () => {
        it("正常调用返回 success: true 和 data", async () => {
            const mockResult = { state: "playing", canPause: true };
            vi.mocked(cdp.invoke).mockResolvedValue(mockResult);

            const result = await bridge.call("playback.getState");

            expect(cdp.invoke).toHaveBeenCalledWith("playback.getState", undefined);
            expect(result).toEqual({
                success: true,
                data: mockResult,
            });
        });

        it("带参数调用正确传递 params", async () => {
            vi.mocked(cdp.invoke).mockResolvedValue({ success: true });

            await bridge.call("playback.setPosition", { position: 42 });

            expect(cdp.invoke).toHaveBeenCalledWith("playback.setPosition", {
                position: 42,
            });
        });

        it("API 返回 null 时 data 为 null", async () => {
            vi.mocked(cdp.invoke).mockResolvedValue(null);

            const result = await bridge.call("playback.stop");

            expect(result).toEqual({
                success: true,
                data: null,
            });
        });

        it("API 返回数组时 data 为数组", async () => {
            const tracks = [
                { title: "Track A", artist: "Artist 1" },
                { title: "Track B", artist: "Artist 2" },
            ];
            vi.mocked(cdp.invoke).mockResolvedValue(tracks);

            const result = await bridge.call("playlist.getTracks", {
                playlist: 0,
                start: 0,
                count: 2,
            });

            expect(result.success).toBe(true);
            expect(result.data).toEqual(tracks);
        });

        it("handler success:false 返回结构化 MCP failure", async () => {
            vi.mocked(cdp.invoke).mockResolvedValue({
                success: false,
                error: "Invalid playlist index",
                code: "INVALID_INDEX",
                details: { playlist: 999 },
            });

            const result = await bridge.call("playlist.remove", { playlist: 999 });

            expect(result).toEqual({
                success: false,
                error: "Invalid playlist index",
                code: "INVALID_INDEX",
                details: { playlist: 999 },
            });
        });

        it("handler success:false 缺少 error 时 fail-closed", async () => {
            vi.mocked(cdp.invoke).mockResolvedValue({
                success: false,
                code: "INVALID_REQUEST",
            });

            const result = await bridge.call("playlist.remove", { playlist: -1 });

            expect(result).toEqual({
                success: false,
                error: "Bridge method 'playlist.remove' returned success:false",
                code: "INVALID_REQUEST",
            });
        });

        it("嵌套对象中的 success:false 不会被误判为 handler failure", async () => {
            const mockResult = {
                success: true,
                item: { success: false, reason: "unavailable" },
            };
            vi.mocked(cdp.invoke).mockResolvedValue(mockResult);

            const result = await bridge.call("library.getStatus");

            expect(result).toEqual({
                success: true,
                data: mockResult,
            });
        });

        it("CDP invoke 抛出 Error 时返回 success: false", async () => {
            vi.mocked(cdp.invoke).mockRejectedValue(
                new Error("fb2k.invoke('playback.play') failed: not connected")
            );

            const result = await bridge.call("playback.play");

            expect(result).toEqual({
                success: false,
                error: "fb2k.invoke('playback.play') failed: not connected",
            });
        });

        it("CDP invoke 抛出非 Error 时也能处理", async () => {
            vi.mocked(cdp.invoke).mockRejectedValue("string error");

            const result = await bridge.call("playback.play");

            expect(result).toEqual({
                success: false,
                error: "string error",
            });
        });

        it("CDP invoke 超时时返回错误", async () => {
            vi.mocked(cdp.invoke).mockRejectedValue(
                new Error("invoke timed out after 30000ms")
            );

            const result = await bridge.call("library.search", { query: "test" });

            expect(result.success).toBe(false);
            expect(result.error).toContain("timed out");
        });
    });

    // ── screenshot() ────────────────────────

    describe("screenshot()", () => {
        it("委托给 cdp.screenshot 并返回 base64", async () => {
            const base64Png = "iVBORw0KGgo...";
            vi.mocked(cdp.screenshot).mockResolvedValue(base64Png);

            const result = await bridge.screenshot({ fullPage: true });

            expect(cdp.screenshot).toHaveBeenCalledWith({ fullPage: true });
            expect(result).toBe(base64Png);
        });

        it("无参数时传递 undefined", async () => {
            vi.mocked(cdp.screenshot).mockResolvedValue("base64data");

            await bridge.screenshot();

            expect(cdp.screenshot).toHaveBeenCalledWith(undefined);
        });

        it("截图失败时抛出错误", async () => {
            vi.mocked(cdp.screenshot).mockRejectedValue(
                new Error("Page.captureScreenshot failed")
            );

            await expect(bridge.screenshot()).rejects.toThrow(
                "Page.captureScreenshot failed"
            );
        });
    });

    // ── evaluate() ──────────────────────────

    describe("evaluate()", () => {
        it("委托给 cdp.evaluate 并返回结果", async () => {
            vi.mocked(cdp.evaluate).mockResolvedValue(42);

            const result = await bridge.evaluate("1 + 41");

            expect(cdp.evaluate).toHaveBeenCalledWith("1 + 41");
            expect(result).toBe(42);
        });

        it("可以返回复杂对象", async () => {
            const obj = { a: 1, b: [2, 3] };
            vi.mocked(cdp.evaluate).mockResolvedValue(obj);

            const result = await bridge.evaluate("({ a: 1, b: [2, 3] })");
            expect(result).toEqual(obj);
        });

        it("JS 执行错误时抛出", async () => {
            vi.mocked(cdp.evaluate).mockRejectedValue(
                new Error("JS evaluation error: ReferenceError: x is not defined")
            );

            await expect(bridge.evaluate("x")).rejects.toThrow(
                "JS evaluation error"
            );
        });
    });

    // ── getConsoleMessages() ────────────────

    describe("getConsoleMessages()", () => {
        it("返回控制台消息数组", async () => {
            const messages = [
                { level: "log", text: "Hello" },
                { level: "error", text: "Something went wrong" },
            ];
            vi.mocked(cdp.getConsoleMessages).mockResolvedValue(messages);

            const result = await bridge.getConsoleMessages();

            expect(result).toEqual(messages);
        });

        it("无消息时返回空数组", async () => {
            vi.mocked(cdp.getConsoleMessages).mockResolvedValue([]);

            const result = await bridge.getConsoleMessages();

            expect(result).toEqual([]);
        });
    });
});

describe("formatBridgeFailure", () => {
    it("保留 handler error 的 code 和 details", () => {
        expect(formatBridgeFailure({
            success: false,
            error: "Invalid playlist index",
            code: "INVALID_INDEX",
            details: { playlist: 999 },
        })).toBe(
            "Error: Invalid playlist index\n" +
            "Code: INVALID_INDEX\n" +
            'Details: {"playlist":999}'
        );
    });

    it("transport error 保持既有 Error 前缀", () => {
        expect(formatBridgeFailure({
            success: false,
            error: "CDP connection closed",
        })).toBe("Error: CDP connection closed");
    });
});

describe("createBridgeToolHandler", () => {
    it("将结构化 handler failure 映射为最终 MCP tool error", async () => {
        const call = vi.fn().mockResolvedValue({
            success: false,
            error: "Invalid playlist index",
            code: "INVALID_INDEX",
            details: { playlist: 999 },
        });
        const handler = createBridgeToolHandler({ call }, "playlist.remove");

        const result = await handler({ playlist: 999 });

        expect(call).toHaveBeenCalledWith("playlist.remove", { playlist: 999 });
        expect(result).toEqual({
            content: [{
                type: "text",
                text:
                    "Error: Invalid playlist index\n" +
                    "Code: INVALID_INDEX\n" +
                    'Details: {"playlist":999}',
            }],
            isError: true,
        });
    });

    it("将成功结果映射为紧凑 JSON 文本且不设置 isError", async () => {
        const call = vi.fn().mockResolvedValue({
            success: true,
            data: { playing: true },
        });
        const handler = createBridgeToolHandler({ call }, "playback.getState");

        const result = await handler({});

        expect(result).toEqual({
            content: [{
                type: "text",
                text: '{"playing":true}',
            }],
        });
    });

    it("没有返回值时给出 null 而不是空文本", async () => {
        const call = vi.fn().mockResolvedValue({ success: true, data: undefined });
        const handler = createBridgeToolHandler({ call }, "playback.stop");

        expect(await handler({})).toEqual({ content: [{ type: "text", text: "null" }] });
    });

    it("超过上限的结果被截断并附说明", async () => {
        const call = vi.fn().mockResolvedValue({ success: true, data: "x".repeat(50) });
        const handler = createBridgeToolHandler({ call }, "playback.getState", { maxResponseChars: 20 });

        const result = await handler({});
        const text = (result.content[0] as { text: string }).text;

        expect(text.startsWith('"xxxxxxxxxxxxxxxxxxx')).toBe(true);
        expect(text).toContain("[Truncated: the result has 52 characters and this server returns at most 20.");
    });

    describe("image 字段", () => {
        // 8 字节的 PNG 签名，base64 为 12 个字符
        const png = "iVBORw0KGgo=";
        const picture = (data: unknown, options: Omit<BridgeResultOptions, "image"> = {}) =>
            createBridgeToolHandler(
                { call: vi.fn().mockResolvedValue({ success: true, data }) },
                "artwork.getCurrent",
                { image: "dataUrl", ...options }
            )({});

        it("data URL 拆成图片块，其余字段留在紧凑 JSON 里", async () => {
            const result = await picture({ available: true, mimeType: "image/png", size: 8, dataUrl: `data:image/png;base64,${png}` });

            expect(result).toEqual({
                content: [
                    { type: "text", text: '{"available":true,"mimeType":"image/png","size":8}' },
                    { type: "image", data: png, mimeType: "image/png" },
                ],
            });
        });

        it("没有图片时照常只返回 JSON 文本", async () => {
            const result = await picture({ available: false, type: "front", reason: "not_found" });

            expect(result).toEqual({
                content: [{ type: "text", text: '{"available":false,"type":"front","reason":"not_found"}' }],
            });
        });

        it("客户端不显示的类型不附图片，也不把 base64 放进文本", async () => {
            const result = await picture({ available: true, dataUrl: "data:image/bmp;base64,Qk0AAA==" });

            expect(result.content).toHaveLength(1);
            const text = (result.content[0] as { text: string }).text;
            expect(text).toBe(
                '{"available":true}\n[Picture not attached: it is image/bmp, and this server attaches only PNG, JPEG, GIF and WebP pictures.]'
            );
        });

        it("超过字节上限的图片不附上，文本写明大小与上限", async () => {
            const result = await picture({ available: true, dataUrl: `data:image/png;base64,${png}` }, { maxImageBytes: 7 });

            expect(result.content).toEqual([{
                type: "text",
                text: '{"available":true}\n[Picture not attached: it has 8 bytes, more than the 7 this server attaches (FB2K_MAX_IMAGE_BYTES).]',
            }]);
            expect((await picture({ available: true, dataUrl: `data:image/png;base64,${png}` }, { maxImageBytes: 8 })).content)
                .toHaveLength(2);
        });

        it("字段不是合法的 base64 data URL 时原样作为 JSON 返回", async () => {
            for (const dataUrl of ["fb2k://artwork/?path=a.flac", "data:image/png;base64,not base64", "data:image/png;base64,abc"]) {
                const result = await picture({ available: true, dataUrl });
                expect(result.content, dataUrl).toEqual([{ type: "text", text: JSON.stringify({ available: true, dataUrl }) }]);
            }
        });

        it("文本部分仍受字符上限约束，图片不受", async () => {
            const result = await picture(
                { note: "x".repeat(50), dataUrl: `data:image/png;base64,${png}` },
                { maxResponseChars: 20 }
            );

            expect((result.content[0] as { text: string }).text).toContain("[Truncated:");
            expect(result.content[1]).toEqual({ type: "image", data: png, mimeType: "image/png" });
        });
    });
});

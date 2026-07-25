/**
 * CdpClient 单元测试
 *
 * 使用 vi.mock 模拟 chrome-remote-interface，验证：
 * - connect / disconnect 生命周期
 * - target 发现逻辑（过滤 devtools:// URL）
 * - 自动重连逻辑（ensureConnected）
 * - invoke() 参数构造与错误传播
 * - evaluate() 正常与异常路径
 * - screenshot() 委托
 * - 超时机制
 */

import { describe, it, expect, vi, beforeEach, afterEach } from "vitest";

// ── Mock chrome-remote-interface (hoisted) ──

const {
    mockRuntimeEnable,
    mockRuntimeEvaluate,
    mockPageEnable,
    mockPageCaptureScreenshot,
    mockPageGetLayoutMetrics,
    mockEmulationSetDeviceMetricsOverride,
    mockEmulationClearDeviceMetricsOverride,
    mockClose,
    mockOn,
    mockCDP,
    mockList,
} = vi.hoisted(() => {
    const mockRuntimeEnable = vi.fn().mockResolvedValue(undefined);
    const mockRuntimeEvaluate = vi.fn();
    const mockPageEnable = vi.fn().mockResolvedValue(undefined);
    const mockPageCaptureScreenshot = vi.fn().mockResolvedValue({ data: "" });
    const mockPageGetLayoutMetrics = vi.fn();
    const mockEmulationSetDeviceMetricsOverride = vi.fn();
    const mockEmulationClearDeviceMetricsOverride = vi.fn();
    const mockClose = vi.fn().mockResolvedValue(undefined);
    const mockOn = vi.fn();

    const mockClient = {
        Runtime: {
            enable: mockRuntimeEnable,
            evaluate: mockRuntimeEvaluate,
        },
        Page: {
            enable: mockPageEnable,
            captureScreenshot: mockPageCaptureScreenshot,
            getLayoutMetrics: mockPageGetLayoutMetrics,
        },
        Emulation: {
            setDeviceMetricsOverride: mockEmulationSetDeviceMetricsOverride,
            clearDeviceMetricsOverride: mockEmulationClearDeviceMetricsOverride,
        },
        close: mockClose,
        on: mockOn,
    };

    const mockCDP = vi.fn().mockResolvedValue(mockClient) as any;
    const mockList = vi.fn();
    mockCDP.List = mockList;

    return {
        mockRuntimeEnable,
        mockRuntimeEvaluate,
        mockPageCaptureScreenshot,
        mockPageGetLayoutMetrics,
        mockEmulationSetDeviceMetricsOverride,
        mockEmulationClearDeviceMetricsOverride,
        mockClose,
        mockOn,
        mockCDP,
        mockList,
    };
});

vi.mock("chrome-remote-interface", () => {
    return {
        default: mockCDP,
    };
});

import { CdpClient } from "../src/cdp-client.js";

describe("CdpClient", () => {
    let client: CdpClient;

    beforeEach(() => {
        vi.clearAllMocks();
        client = new CdpClient({ host: "localhost", port: 9222 });

        // 默认 target 列表：1 个 page + 1 个 devtools
        mockList.mockResolvedValue([
            {
                type: "page",
                url: "http://localhost:8080/index.html",
                id: "target-1",
            },
            {
                type: "page",
                url: "devtools://devtools/inspector.html",
                id: "target-2",
            },
        ]);
    });

    afterEach(async () => {
        // 确保清理
        try {
            await client.disconnect();
        } catch {
            // ignore
        }
    });

    // ── connect() ───────────────────────────

    describe("connect()", () => {
        it("成功连接到第一个 page target（过滤 devtools://）", async () => {
            await client.connect();

            expect(mockList).toHaveBeenCalledWith({
                host: "localhost",
                port: 9222,
            });
            expect(mockCDP).toHaveBeenCalledWith(
                expect.objectContaining({
                    host: "localhost",
                    port: 9222,
                    target: expect.objectContaining({
                        type: "page",
                        url: "http://localhost:8080/index.html",
                    }),
                })
            );
            expect(mockRuntimeEnable).toHaveBeenCalled();
            expect(client.connected).toBe(true);
        });

        it("重复调用 connect() 时不会重复连接", async () => {
            await client.connect();
            await client.connect();

            expect(mockCDP).toHaveBeenCalledTimes(1);
        });

        it("没有 page target 时抛出错误", async () => {
            mockList.mockResolvedValue([
                {
                    type: "page",
                    url: "devtools://devtools/inspector.html",
                    id: "target-1",
                },
            ]);

            await expect(client.connect()).rejects.toThrow("No page target found");
        });

        it("targets 列表为空时抛出错误", async () => {
            mockList.mockResolvedValue([]);

            await expect(client.connect()).rejects.toThrow("No page target found");
        });

        it("CDP 连接失败时抛出明确错误", async () => {
            mockList.mockRejectedValue(new Error("ECONNREFUSED"));

            await expect(client.connect()).rejects.toThrow(
                /Failed to connect to WebView2 CDP/
            );
        });

        it("连接后注册 disconnect 事件监听", async () => {
            await client.connect();

            expect(mockOn).toHaveBeenCalledWith("disconnect", expect.any(Function));
        });
    });

    // ── disconnect() ────────────────────────

    describe("disconnect()", () => {
        it("断开已有连接", async () => {
            await client.connect();
            await client.disconnect();

            expect(mockClose).toHaveBeenCalled();
            expect(client.connected).toBe(false);
        });

        it("未连接时 disconnect 不报错", async () => {
            await client.disconnect(); // should not throw
            expect(mockClose).not.toHaveBeenCalled();
        });
    });

    // ── ensureConnected() ───────────────────

    describe("ensureConnected()", () => {
        it("已连接时直接返回", async () => {
            await client.connect();
            vi.clearAllMocks();

            await client.ensureConnected();

            expect(mockCDP).not.toHaveBeenCalled();
        });

        it("未连接时自动连接", async () => {
            await client.ensureConnected();

            expect(mockCDP).toHaveBeenCalledTimes(1);
            expect(client.connected).toBe(true);
        });

        it("首次连接失败后重试", async () => {
            let callCount = 0;
            mockList.mockImplementation(async () => {
                callCount++;
                if (callCount === 1) {
                    throw new Error("ECONNREFUSED");
                }
                return [
                    {
                        type: "page",
                        url: "http://localhost:8080/index.html",
                        id: "target-1",
                    },
                ];
            });

            // 使用短间隔加速测试
            const fastClient = new CdpClient({
                host: "localhost",
                port: 9222,
                maxReconnect: 3,
                reconnectBaseMs: 10,
            });

            await fastClient.ensureConnected();

            expect(callCount).toBe(2);
        });

        it("超过最大重试次数后抛出最后的错误", async () => {
            mockList.mockRejectedValue(new Error("ECONNREFUSED"));

            const fastClient = new CdpClient({
                host: "localhost",
                port: 9222,
                maxReconnect: 2,
                reconnectBaseMs: 10,
            });

            await expect(fastClient.ensureConnected()).rejects.toThrow(
                /Failed to connect/
            );
        });
    });

    // ── invoke() ────────────────────────────

    describe("invoke()", () => {
        beforeEach(async () => {
            await client.connect();
        });

        it("无参数调用构造正确的表达式", async () => {
            mockRuntimeEvaluate.mockResolvedValue({
                result: { value: { state: "playing" } },
            });

            const result = await client.invoke("playback.getState");

            expect(mockRuntimeEvaluate).toHaveBeenCalledWith(
                expect.objectContaining({
                    expression: expect.stringContaining(
                        "window.fb2k.invoke('playback.getState', undefined)"
                    ),
                    awaitPromise: true,
                    returnByValue: true,
                })
            );
            expect(result).toEqual({ state: "playing" });
        });

        it("带参数调用构造 JSON 字符串", async () => {
            mockRuntimeEvaluate.mockResolvedValue({
                result: { value: { success: true } },
            });

            await client.invoke("playback.setVolume", { volume: 80 });

            expect(mockRuntimeEvaluate).toHaveBeenCalledWith(
                expect.objectContaining({
                    expression: expect.stringContaining(
                        'window.fb2k.invoke(\'playback.setVolume\', {"volume":80})'
                    ),
                })
            );
        });

        it("invoke 返回 __error 时抛出 Error", async () => {
            mockRuntimeEvaluate.mockResolvedValue({
                result: {
                    value: { __error: "unknown method" },
                },
            });

            await expect(
                client.invoke("nonexistent.method")
            ).rejects.toThrow(/unknown method/);
        });

        it("JS 执行异常时抛出错误", async () => {
            mockRuntimeEvaluate.mockResolvedValue({
                result: { value: undefined },
                exceptionDetails: {
                    text: "ReferenceError: fb2k is not defined",
                },
            });

            await expect(client.invoke("playback.play")).rejects.toThrow(
                "JS evaluation error"
            );
        });
    });

    // ── evaluate() ──────────────────────────

    describe("evaluate()", () => {
        beforeEach(async () => {
            await client.connect();
        });

        it("执行表达式并返回结果", async () => {
            mockRuntimeEvaluate.mockResolvedValue({
                result: { value: 42 },
            });

            const result = await client.evaluate("21 + 21");

            expect(mockRuntimeEvaluate).toHaveBeenCalledWith(
                expect.objectContaining({
                    expression: "21 + 21",
                    awaitPromise: true,
                    returnByValue: true,
                })
            );
            expect(result).toBe(42);
        });

        it("表达式异常时抛出", async () => {
            mockRuntimeEvaluate.mockResolvedValue({
                result: { value: undefined },
                exceptionDetails: {
                    text: "SyntaxError",
                    exception: { description: "SyntaxError: Unexpected end of input" },
                },
            });

            await expect(client.evaluate("function(")).rejects.toThrow(
                /JS evaluation error/
            );
        });
    });

    // ── screenshot() ────────────────────────

    describe("screenshot()", () => {
        beforeEach(async () => {
            await client.connect();
        });

        it("正常截图返回 base64 数据", async () => {
            mockPageCaptureScreenshot.mockResolvedValue({
                data: "iVBORw0KGgoAAAANSUhEUg==",
            });

            const data = await client.screenshot();

            expect(mockPageCaptureScreenshot).toHaveBeenCalledWith({
                format: "png",
            });
            expect(data).toBe("iVBORw0KGgoAAAANSUhEUg==");
        });

        it("全页截图设置 DeviceMetrics", async () => {
            mockPageGetLayoutMetrics.mockResolvedValue({
                contentSize: { width: 1920, height: 5000 },
            });
            mockPageCaptureScreenshot.mockResolvedValue({
                data: "base64data",
            });

            await client.screenshot({ fullPage: true });

            expect(mockEmulationSetDeviceMetricsOverride).toHaveBeenCalledWith({
                width: 1920,
                height: 5000,
                deviceScaleFactor: 1,
                mobile: false,
            });
            expect(mockEmulationClearDeviceMetricsOverride).toHaveBeenCalled();
        });
    });

    // ── getConsoleMessages() ────────────────

    describe("getConsoleMessages()", () => {
        beforeEach(async () => {
            await client.connect();
        });

        it("返回控制台消息数组", async () => {
            const messages = [{ level: "log", text: "debug info" }];
            mockRuntimeEvaluate.mockResolvedValue({
                result: { value: messages },
            });

            const result = await client.getConsoleMessages();

            expect(result).toEqual(messages);
        });

        it("__fb2kMcpConsoleLogs 不存在时返回空数组", async () => {
            mockRuntimeEvaluate.mockResolvedValue({
                result: { value: null },
            });

            const result = await client.getConsoleMessages();

            expect(result).toEqual([]);
        });
    });

    // ── 构造参数 ────────────────────────────

    describe("构造参数", () => {
        it("使用自定义端口", () => {
            const customClient = new CdpClient({ port: 9333 });
            expect(customClient.connected).toBe(false);
        });

        it("默认参数", () => {
            const defaultClient = new CdpClient();
            expect(defaultClient.connected).toBe(false);
        });
    });

    // ── connect 超时 ─────────────────────────

    describe("connect 超时", () => {
        it("List 永不返回时按 connectTimeoutMs 超时并附带挂起提示", async () => {
            mockList.mockImplementation(() => new Promise(() => {}));

            const fastClient = new CdpClient({
                host: "localhost",
                port: 9222,
                connectTimeoutMs: 50,
            });

            await expect(fastClient.connect()).rejects.toThrow(
                /connect timed out after 50ms.*suspended/s
            );
        });

        it("warmup 截图挂死不阻塞 connect（受 warmupTimeoutMs 约束）", async () => {
            mockPageCaptureScreenshot.mockImplementationOnce(
                () => new Promise(() => {})
            );

            const fastClient = new CdpClient({
                host: "localhost",
                port: 9222,
                connectTimeoutMs: 500,
                warmupTimeoutMs: 20,
            });

            await fastClient.connect();
            expect(fastClient.connected).toBe(true);
        });
    });

    // ── 惰性心跳 ─────────────────────────────

    describe("惰性心跳", () => {
        it("空闲超阈值后 ping 失败则丢弃旧连接并重连", async () => {
            const pingClient = new CdpClient({
                host: "localhost",
                port: 9222,
                pingIdleMs: 0, // 每次 ensureConnected 都触发健康检查
                pingTimeoutMs: 50,
                reconnectBaseMs: 1,
                warmupTimeoutMs: 20,
            });
            await pingClient.connect();
            expect(mockCDP).toHaveBeenCalledTimes(1);

            mockRuntimeEvaluate.mockRejectedValueOnce(new Error("socket hang up"));

            await pingClient.ensureConnected();

            expect(mockClose).toHaveBeenCalled();
            expect(mockCDP).toHaveBeenCalledTimes(2);
            expect(pingClient.connected).toBe(true);
        });

        it("空闲超阈值后 ping 成功则复用缓存连接", async () => {
            const pingClient = new CdpClient({
                host: "localhost",
                port: 9222,
                pingIdleMs: 0,
                pingTimeoutMs: 50,
            });
            await pingClient.connect();
            expect(mockCDP).toHaveBeenCalledTimes(1);

            mockRuntimeEvaluate.mockResolvedValueOnce({ result: { value: 1 } });

            await pingClient.ensureConnected();

            expect(mockClose).not.toHaveBeenCalled();
            expect(mockCDP).toHaveBeenCalledTimes(1);
        });

        it("空闲未超阈值时不 ping 直接复用", async () => {
            await client.connect();
            vi.clearAllMocks();

            await client.ensureConnected();

            expect(mockRuntimeEvaluate).not.toHaveBeenCalled();
            expect(mockCDP).not.toHaveBeenCalled();
        });
    });

    // ── target 选择 ──────────────────────────

    describe("target 选择", () => {
        it("优先选择非 about:blank 的 page target", async () => {
            mockList.mockResolvedValue([
                { type: "page", url: "about:blank", id: "overlay" },
                {
                    type: "page",
                    url: "https://foo-ui-webview2.local/index.html",
                    id: "main",
                },
            ]);

            await client.connect();

            expect(mockCDP).toHaveBeenCalledWith(
                expect.objectContaining({
                    target: expect.objectContaining({ id: "main" }),
                })
            );
        });

        it("候选全为 about:blank 时回退选取第一个（测试页兜底场景）", async () => {
            mockList.mockResolvedValue([
                { type: "page", url: "about:blank", id: "testpage-main" },
            ]);

            await client.connect();

            expect(mockCDP).toHaveBeenCalledWith(
                expect.objectContaining({
                    target: expect.objectContaining({ id: "testpage-main" }),
                })
            );
            expect(client.connected).toBe(true);
        });

        it("targetUrlFilter 命中时选中匹配 target", async () => {
            mockList.mockResolvedValue([
                { type: "page", url: "about:blank", id: "overlay" },
                {
                    type: "page",
                    url: "https://foo-ui-webview2.local/index.html",
                    id: "main",
                },
                { type: "page", url: "https://example.com/popup.html", id: "popup" },
            ]);

            const pinned = new CdpClient({
                host: "localhost",
                port: 9222,
                targetUrlFilter: "foo-ui-webview2.local",
            });
            await pinned.connect();

            expect(mockCDP).toHaveBeenCalledWith(
                expect.objectContaining({
                    target: expect.objectContaining({ id: "main" }),
                })
            );
        });

        it("targetUrlFilter 未命中时报错并列出候选", async () => {
            mockList.mockResolvedValue([
                { type: "page", url: "about:blank", id: "overlay" },
                { type: "page", url: "https://example.com/popup.html", id: "popup" },
            ]);

            const pinned = new CdpClient({
                host: "localhost",
                port: 9222,
                targetUrlFilter: "foo-ui-webview2.local",
            });

            await expect(pinned.connect()).rejects.toThrow(
                /No page target matches FB2K_CDP_TARGET_URL="foo-ui-webview2\.local".*about:blank.*example\.com/s
            );
        });

        it("targetUrlFilter 可显式选中 about:blank", async () => {
            mockList.mockResolvedValue([
                {
                    type: "page",
                    url: "https://example.com/popup.html",
                    id: "popup",
                },
                { type: "page", url: "about:blank", id: "overlay" },
            ]);

            const pinned = new CdpClient({
                host: "localhost",
                port: 9222,
                targetUrlFilter: "about:blank",
            });
            await pinned.connect();

            expect(mockCDP).toHaveBeenCalledWith(
                expect.objectContaining({
                    target: expect.objectContaining({ id: "overlay" }),
                })
            );
        });
    });

    // ── 调用超时提示 ─────────────────────────

    describe("调用超时提示", () => {
        it("invoke 超时错误包含挂起诊断提示", async () => {
            const slow = new CdpClient({
                host: "localhost",
                port: 9222,
                invokeTimeoutMs: 50,
                warmupTimeoutMs: 20,
            });
            await slow.connect();
            mockRuntimeEvaluate.mockImplementationOnce(() => new Promise(() => {}));

            await expect(slow.invoke("playback.getState")).rejects.toThrow(
                /timed out after 50ms.*keep-alive/s
            );
        });

        it("screenshot 超时错误包含挂起诊断提示", async () => {
            const slow = new CdpClient({
                host: "localhost",
                port: 9222,
                screenshotTimeoutMs: 50,
                warmupTimeoutMs: 20,
            });
            await slow.connect();
            mockPageCaptureScreenshot.mockImplementationOnce(
                () => new Promise(() => {})
            );

            await expect(slow.screenshot()).rejects.toThrow(
                /screenshot timed out after 50ms.*suspended/s
            );
        });
    });
});

/**
 * 页面工具测试
 *
 * 用 InMemoryTransport 起真 McpServer，页面访问用替身，核对：
 * - fb2k_page_inspect 的三个 action 与它们的参数归属
 * - fb2k_page_evaluate 只在开启 eval 且不在只读模式时注册
 * - 注解与失败时的错误结果
 */

import { Client } from "@modelcontextprotocol/sdk/client/index.js";
import { InMemoryTransport } from "@modelcontextprotocol/sdk/inMemory.js";
import { McpServer } from "@modelcontextprotocol/sdk/server/mcp.js";
import { describe, expect, it, vi } from "vitest";

import { registerPageTools, type PageAccess, type PageToolOptions } from "../src/page-tools.js";

async function harness(options: PageToolOptions = {}, overrides: Partial<PageAccess> = {}) {
    const page = {
        screenshot: vi.fn().mockResolvedValue("iVBORw0KGgo="),
        evaluate: vi.fn().mockResolvedValue("html\n  body\n"),
        getConsoleMessages: vi.fn().mockResolvedValue([
            { level: "log", text: "loaded" },
            { level: "exception", text: "TypeError: boom" },
        ]),
        ...overrides,
    };
    const server = new McpServer({ name: "page-test-server", version: "1.0.0" });
    const registered = registerPageTools(server, page, options);
    const client = new Client({ name: "page-test-client", version: "1.0.0" }, { capabilities: {} });
    const [clientTransport, serverTransport] = InMemoryTransport.createLinkedPair();
    await server.connect(serverTransport);
    await client.connect(clientTransport);
    return {
        client,
        page,
        registered,
        call: (name: string, args: Record<string, unknown>) => client.callTool({ name, arguments: args }),
        async close() {
            await client.close();
            await server.close();
        },
    };
}

function text(result: Awaited<ReturnType<Client["callTool"]>>): string {
    return (result.content as Array<{ text?: string }>).map((c) => c.text ?? "").join("\n");
}

describe("fb2k_page_inspect", () => {
    it("是只读工具，默认只注册它", async () => {
        const h = await harness();
        try {
            const { tools } = await h.client.listTools();
            expect(h.registered).toEqual(["fb2k_page_inspect"]);
            expect(tools.map((t) => t.name)).toEqual(["fb2k_page_inspect"]);
            expect(tools[0].annotations).toEqual({ readOnlyHint: true, openWorldHint: false });
            expect(tools[0].inputSchema.additionalProperties).toBe(false);
        } finally {
            await h.close();
        }
    });

    it("screenshot 返回 PNG 图片内容，fullPage 原样传给页面", async () => {
        const h = await harness();
        try {
            const result = await h.call("fb2k_page_inspect", { action: "screenshot", fullPage: true });
            expect(result.content).toEqual([{ type: "image", data: "iVBORw0KGgo=", mimeType: "image/png" }]);
            expect(h.page.screenshot).toHaveBeenCalledWith({ fullPage: true });
        } finally {
            await h.close();
        }
    });

    it("domSnapshot 返回页面脚本给出的文本", async () => {
        const h = await harness();
        try {
            const result = await h.call("fb2k_page_inspect", { action: "domSnapshot" });
            expect(text(result)).toBe("html\n  body\n");
            expect(h.page.evaluate).toHaveBeenCalledWith(expect.stringContaining("document.documentElement"));
        } finally {
            await h.close();
        }
    });

    it("consoleMessages 每行一条，limit 传给记录", async () => {
        const h = await harness();
        try {
            const result = await h.call("fb2k_page_inspect", { action: "consoleMessages", limit: 5 });
            expect(text(result)).toBe("[log] loaded\n[exception] TypeError: boom");
            expect(h.page.getConsoleMessages).toHaveBeenCalledWith(5);
        } finally {
            await h.close();
        }
    });

    it("没有记录时如实说明", async () => {
        const h = await harness({}, { getConsoleMessages: vi.fn().mockResolvedValue([]) });
        try {
            const result = await h.call("fb2k_page_inspect", { action: "consoleMessages" });
            expect(text(result)).toBe("(no console messages)");
        } finally {
            await h.close();
        }
    });

    it("参数用在不属于它的 action 上时报错，不访问页面", async () => {
        const h = await harness();
        try {
            const full = await h.call("fb2k_page_inspect", { action: "domSnapshot", fullPage: true });
            const limit = await h.call("fb2k_page_inspect", { action: "screenshot", limit: 3 });
            expect(full.isError).toBe(true);
            expect(text(full)).toBe("`fullPage` applies to the screenshot action only, not domSnapshot.");
            expect(limit.isError).toBe(true);
            expect(h.page.evaluate).not.toHaveBeenCalled();
            expect(h.page.screenshot).not.toHaveBeenCalled();
        } finally {
            await h.close();
        }
    });

    it("页面访问失败时返回工具执行错误", async () => {
        const h = await harness({}, { screenshot: vi.fn().mockRejectedValue(new Error("Target closed")) });
        try {
            const result = await h.call("fb2k_page_inspect", { action: "screenshot" });
            expect(result.isError).toBe(true);
            expect(text(result)).toBe("screenshot failed: Target closed");
        } finally {
            await h.close();
        }
    });
});

describe("fb2k_page_evaluate", () => {
    it("开启 eval 时注册，标为破坏性且触达外部", async () => {
        const h = await harness({ enableEval: true }, { evaluate: vi.fn().mockResolvedValue({ a: 1 }) });
        try {
            const { tools } = await h.client.listTools();
            const evaluate = tools.find((t) => t.name === "fb2k_page_evaluate");
            expect(h.registered).toEqual(["fb2k_page_inspect", "fb2k_page_evaluate"]);
            expect(evaluate?.annotations).toEqual({
                readOnlyHint: false,
                destructiveHint: true,
                idempotentHint: false,
                openWorldHint: true,
            });
            const result = await h.call("fb2k_page_evaluate", { expression: "({ a: 1 })" });
            expect(text(result)).toBe('{"a":1}');
        } finally {
            await h.close();
        }
    });

    it("只读模式下即使开启 eval 也不注册", async () => {
        const h = await harness({ enableEval: true, readOnly: true });
        try {
            expect(h.registered).toEqual(["fb2k_page_inspect"]);
        } finally {
            await h.close();
        }
    });

    it("求值失败时返回工具执行错误", async () => {
        const h = await harness({ enableEval: true }, { evaluate: vi.fn().mockRejectedValue(new Error("SyntaxError")) });
        try {
            const result = await h.call("fb2k_page_evaluate", { expression: "(" });
            expect(result.isError).toBe(true);
            expect(text(result)).toBe("Evaluation failed: SyntaxError");
        } finally {
            await h.close();
        }
    });
});

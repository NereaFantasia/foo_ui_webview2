/**
 * Page tools: inspect, and optionally script, the WebView2 page through the
 * Chrome DevTools Protocol rather than through a bridge method.
 */

import type { McpServer } from "@modelcontextprotocol/sdk/server/mcp.js";
import type { CallToolResult } from "@modelcontextprotocol/sdk/types.js";
import { z } from "zod";

import type { BridgeExecutor } from "./bridge-executor.js";
import { CONSOLE_CAPACITY, DEFAULT_CONSOLE_LIMIT } from "./cdp-client.js";
import { withRateLimit, type CallRateLimiter } from "./rate-limit.js";
import { DEFAULT_MAX_RESPONSE_CHARS, errorMessage, errorResult, jsonResult, textResult } from "./tool-results.js";
import type { ToolAnnotations } from "./types.js";

/** How the page tools are registered. */
export interface PageToolOptions {
    /** Leave out the tools that can change anything. */
    readOnly?: boolean;
    /** Register `fb2k_page_evaluate`, which runs arbitrary JavaScript in the page. */
    enableEval?: boolean;
    /** Longest text result, in characters; a longer one is cut. */
    maxResponseChars?: number;
    /** Budget every call takes from; omitted, calls are not limited. */
    limiter?: CallRateLimiter;
}

/** What the page tools need from the bridge executor. */
export type PageAccess = Pick<BridgeExecutor, "screenshot" | "evaluate" | "getConsoleMessages">;

/** Name of the read-only page tool: screenshot, DOM snapshot and console messages. */
export const PAGE_INSPECT = "fb2k_page_inspect";
/** Name of the tool that runs JavaScript in the page; registered only with `enableEval` and outside read-only mode. */
export const PAGE_EVALUATE = "fb2k_page_evaluate";

const INSPECT_ANNOTATIONS: ToolAnnotations = { readOnlyHint: true, openWorldHint: false };
// The page can call every bridge method, so an expression can change anything and reach anywhere.
const EVALUATE_ANNOTATIONS: ToolAnnotations = {
    readOnlyHint: false,
    destructiveHint: true,
    idempotentHint: false,
    openWorldHint: true,
};

const INSPECT_DESCRIPTION = [
    "Inspect the WebView2 page that shows the foobar2000 UI: take a screenshot, get a simplified DOM tree, or read the page's recent console messages. Changes nothing.",
    "",
    "Pass one of these as `action`; `?` marks an optional parameter.",
    "- screenshot(fullPage?): a PNG of the visible viewport, or of the whole page with `fullPage` true",
    "- domSnapshot: one line per element, `tag#id.class \"text\"`, indented by depth",
    "- consoleMessages(limit?): the page's recent console messages and uncaught exceptions, oldest first",
].join("\n");

// Walks the document and prints one line per element; text only for an element whose one
// child is a text node, cut to 80 characters.
const DOM_SNAPSHOT_SCRIPT = `
(function() {
    function walk(el, depth) {
        const indent = '  '.repeat(depth);
        const tag = el.tagName?.toLowerCase() || '#text';
        const id = el.id ? '#' + el.id : '';
        const cls = el.className && typeof el.className === 'string'
            ? '.' + el.className.trim().split(/\\s+/).join('.')
            : '';
        const text = el.childNodes.length === 1 && el.childNodes[0].nodeType === 3
            ? ' "' + el.childNodes[0].textContent.trim().substring(0, 80) + '"'
            : '';
        let result = indent + tag + id + cls + text + '\\n';
        for (const child of el.children || []) {
            result += walk(child, depth + 1);
        }
        return result;
    }
    return walk(document.documentElement, 0);
})()
`;

const inspectSchema = z.strictObject({
    action: z.enum(["screenshot", "domSnapshot", "consoleMessages"]).describe("What to inspect."),
    fullPage: z.boolean().optional().describe("For `screenshot`: capture the whole page instead of the visible viewport."),
    limit: z
        .number()
        .int()
        .min(1)
        .max(CONSOLE_CAPACITY)
        .optional()
        .describe(`For \`consoleMessages\`: most messages to return, the newest ones; default ${DEFAULT_CONSOLE_LIMIT}.`),
});

const evaluateSchema = z.strictObject({
    expression: z.string().min(1).describe("JavaScript expression to evaluate in the page; a promise is awaited."),
});

/**
 * Register the page tools.
 *
 * @returns The names of the tools registered.
 */
export function registerPageTools(server: McpServer, page: PageAccess, options: PageToolOptions = {}): string[] {
    const maxChars = options.maxResponseChars ?? DEFAULT_MAX_RESPONSE_CHARS;
    server.registerTool(PAGE_INSPECT, {
        description: INSPECT_DESCRIPTION,
        inputSchema: inspectSchema,
        annotations: INSPECT_ANNOTATIONS,
    }, withRateLimit(options.limiter, (args: z.infer<typeof inspectSchema>) => inspect(page, args, maxChars)));
    const registered = [PAGE_INSPECT];
    if (options.enableEval && !options.readOnly) {
        server.registerTool(PAGE_EVALUATE, {
            description:
                "Evaluate a JavaScript expression in the WebView2 page and return its value as JSON. " +
                "The page can call every Bridge method, so an expression can change anything. " +
                "Registered only when FB2K_ENABLE_EVAL is set, for development and debugging.",
            inputSchema: evaluateSchema,
            annotations: EVALUATE_ANNOTATIONS,
        }, withRateLimit(options.limiter, async ({ expression }: z.infer<typeof evaluateSchema>) => {
            try {
                return jsonResult(await page.evaluate(expression), maxChars);
            } catch (err) {
                return errorResult(`Evaluation failed: ${errorMessage(err)}`);
            }
        }));
        registered.push(PAGE_EVALUATE);
    }
    return registered;
}

async function inspect(
    page: PageAccess,
    args: z.infer<typeof inspectSchema>,
    maxChars: number
): Promise<CallToolResult> {
    const { action, fullPage, limit } = args;
    if (fullPage !== undefined && action !== "screenshot") {
        return errorResult(`\`fullPage\` applies to the screenshot action only, not ${action}.`);
    }
    if (limit !== undefined && action !== "consoleMessages") {
        return errorResult(`\`limit\` applies to the consoleMessages action only, not ${action}.`);
    }
    try {
        switch (action) {
            case "screenshot":
                return {
                    content: [{ type: "image", data: await page.screenshot({ fullPage }), mimeType: "image/png" }],
                };
            case "domSnapshot":
                return textResult(String(await page.evaluate(DOM_SNAPSHOT_SCRIPT)), maxChars);
            case "consoleMessages": {
                const messages = await page.getConsoleMessages(limit);
                return textResult(
                    messages.length
                        ? messages.map((m) => `[${m.level}] ${m.text}`).join("\n")
                        : "(no console messages)",
                    maxChars
                );
            }
        }
    } catch (err) {
        return errorResult(`${action} failed: ${errorMessage(err)}`);
    }
}

#!/usr/bin/env node
/**
 * foo-ui-webview2-mcp — MCP Server entry point.
 *
 * Connects to the fb2k WebView2 over CDP and exposes the bridge API as
 * MCP tools. Transport: stdio.
 */

import { McpServer } from "@modelcontextprotocol/sdk/server/mcp.js";
import { z } from "zod";

import { CdpClient } from "./cdp-client.js";
import { BridgeExecutor } from "./bridge-executor.js";
import { registerBridgeTools } from "./bridge-tools.js";
import { GuardedStdioServerTransport } from "./guarded-stdio-transport.js";
import { logger } from "./logger.js";
import { playbackTools, playbackMethodMap } from "./tools/playback.js";
import { playbackExtTools, playbackExtMethodMap } from "./tools/playback-ext.js";
import { playlistTools, playlistMethodMap } from "./tools/playlist.js";
import { playlistExtTools, playlistExtMethodMap } from "./tools/playlist-ext.js";
import { libraryTools, libraryMethodMap } from "./tools/library.js";
import { artworkTools, artworkMethodMap } from "./tools/artwork.js";
import { queueTools, queueMethodMap } from "./tools/queue.js";
import { metadataTools, metadataMethodMap } from "./tools/metadata.js";
import type { ToolDefinition } from "./types.js";

// ── CDP connection parameters ──
const CDP_PORT = parseInt(process.env.FB2K_CDP_PORT || "9222", 10);
const CDP_HOST = process.env.FB2K_CDP_HOST || "localhost";
// Optional URL substring to pin the CDP page target when several WebViews
// (popups, panels, overlays) share the debugging port.
const CDP_TARGET_URL = process.env.FB2K_CDP_TARGET_URL || "";

// ── Initialization ──
const cdp = new CdpClient({
    host: CDP_HOST,
    port: CDP_PORT,
    ...(CDP_TARGET_URL ? { targetUrlFilter: CDP_TARGET_URL } : {}),
});
const bridge = new BridgeExecutor(cdp);

// Merge all bridge method maps.
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

// Merge all tool definitions.
const allBridgeTools: ToolDefinition[] = [
    ...playbackTools,
    ...playbackExtTools,
    ...playlistTools,
    ...playlistExtTools,
    ...libraryTools,
    ...artworkTools,
    ...queueTools,
    ...metadataTools,
];

// ── MCP Server ──

const server = new McpServer({
    name: "foo-ui-webview2-mcp",
    version: "0.1.0",
});

// Register bridge API tools through the shared production registration path.
registerBridgeTools(server, bridge, allBridgeTools, allMethodMaps);

// ── UI testing tools (special handlers) ─────────────────

server.registerTool(
    "fb2k_screenshot",
    {
        description: "Take a screenshot of the WebView2 page (returns base64 PNG)",
        inputSchema: { fullPage: z.boolean().optional() },
    },
    async ({ fullPage }) => {
        try {
            const data = await bridge.screenshot({ fullPage });
            return {
                content: [{ type: "image" as const, data, mimeType: "image/png" }],
            };
        } catch (err) {
            return {
                content: [
                    {
                        type: "text" as const,
                        text: `Screenshot failed: ${err instanceof Error ? err.message : err}`,
                    },
                ],
                isError: true,
            };
        }
    }
);

server.registerTool(
    "fb2k_dom_snapshot",
    {
        description: "Get a DOM snapshot of the current page (simplified accessibility tree)",
    },
    async () => {
        try {
            const snapshot = await cdp.evaluate(`
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
            `);
            return {
                content: [{ type: "text" as const, text: snapshot as string }],
            };
        } catch (err) {
            return {
                content: [
                    {
                        type: "text" as const,
                        text: `DOM snapshot failed: ${err instanceof Error ? err.message : err}`,
                    },
                ],
                isError: true,
            };
        }
    }
);

// ── Evaluate tool (opt-in) ──

const ENABLE_EVAL = process.env.FB2K_ENABLE_EVAL === "1" || process.env.FB2K_ENABLE_EVAL === "true";

if (ENABLE_EVAL) {
    server.registerTool(
        "fb2k_evaluate",
        {
            description: "Execute a JavaScript expression in the WebView2 page (dev/debug only, requires FB2K_ENABLE_EVAL=1)",
            inputSchema: { expression: z.string() },
        },
        async ({ expression }) => {
            try {
                const result = await cdp.evaluate(expression);
                return {
                    content: [
                        {
                            type: "text" as const,
                            text: JSON.stringify(result, null, 2),
                        },
                    ],
                };
            } catch (err) {
                return {
                    content: [
                        {
                            type: "text" as const,
                            text: `Eval failed: ${err instanceof Error ? err.message : err}`,
                        },
                    ],
                    isError: true,
                };
            }
        }
    );
}

server.registerTool(
    "fb2k_console_messages",
    {
        description: "Get recent console log messages from the WebView2 page",
    },
    async () => {
        try {
            const messages = await bridge.getConsoleMessages();
            return {
                content: [
                    {
                        type: "text" as const,
                        text: messages.length
                            ? messages
                                  .map((m) => `[${m.level}] ${m.text}`)
                                  .join("\n")
                            : "(no console messages captured)",
                    },
                ],
            };
        } catch (err) {
            return {
                content: [
                    {
                        type: "text" as const,
                        text: `Console fetch failed: ${err instanceof Error ? err.message : err}`,
                    },
                ],
                isError: true,
            };
        }
    }
);

// ── Startup ──

async function main() {
    const transport = new GuardedStdioServerTransport();
    await server.connect(transport);
    logger.info("Server started", { cdpHost: CDP_HOST, cdpPort: CDP_PORT });

    // Pre-connect CDP to remove cold-start latency on the first tool call.
    cdp.connect().then(() => {
        logger.info("CDP pre-connected", { host: CDP_HOST, port: CDP_PORT });
    }).catch(() => {
        logger.warn("CDP not available yet, will connect on first tool call");
    });
}

main().catch((err) => {
    logger.error("Fatal error", { error: err instanceof Error ? err.message : String(err) });
    process.exit(1);
});

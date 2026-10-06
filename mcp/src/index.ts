#!/usr/bin/env node
/**
 * foo-ui-webview2-mcp — MCP Server entry point.
 *
 * Connects to the fb2k WebView2 over CDP and exposes the bridge API as
 * MCP tools. Transport: stdio.
 */

import { createRequire } from "node:module";

import { McpServer } from "@modelcontextprotocol/sdk/server/mcp.js";

import { CdpClient } from "./cdp-client.js";
import { BridgeExecutor } from "./bridge-executor.js";
import { registerBridgeTools } from "./bridge-tools.js";
import { GuardedStdioServerTransport } from "./guarded-stdio-transport.js";
import { logger } from "./logger.js";
import { registerPageTools } from "./page-tools.js";
import { CallRateLimiter } from "./rate-limit.js";
import { DEFAULT_MAX_IMAGE_BYTES, DEFAULT_MAX_RESPONSE_CHARS } from "./tool-results.js";
import { bridgeTools } from "./generated/bridge-tools.js";

const flag = (name: string) => process.env[name] === "1" || process.env[name] === "true";

/** A positive integer environment variable, or the fallback when it is unset or not one. */
function positiveInt(name: string, fallback: number): number {
    const raw = process.env[name];
    if (raw === undefined || raw === "") return fallback;
    const value = Number(raw);
    if (Number.isInteger(value) && value > 0) return value;
    logger.warn(`ignoring ${name}: expected a positive integer`, { value: raw });
    return fallback;
}

// ── CDP connection parameters ──
const CDP_PORT = parseInt(process.env.FB2K_CDP_PORT || "9222", 10);
const CDP_HOST = process.env.FB2K_CDP_HOST || "localhost";
// Optional URL substring to pin the CDP page target when several WebViews
// (popups, panels, overlays) share the debugging port.
const CDP_TARGET_URL = process.env.FB2K_CDP_TARGET_URL || "";

// ── Tool options ──
const READ_ONLY = flag("FB2K_READ_ONLY");
const ENABLE_EVAL = flag("FB2K_ENABLE_EVAL");
const MAX_RESPONSE_CHARS = positiveInt("FB2K_MAX_RESPONSE_CHARS", DEFAULT_MAX_RESPONSE_CHARS);
const MAX_IMAGE_BYTES = positiveInt("FB2K_MAX_IMAGE_BYTES", DEFAULT_MAX_IMAGE_BYTES);

// The server reports the version of the package it ships in.
const { version } = createRequire(import.meta.url)("../package.json") as { version: string };

// ── Initialization ──
const cdp = new CdpClient({
    host: CDP_HOST,
    port: CDP_PORT,
    ...(CDP_TARGET_URL ? { targetUrlFilter: CDP_TARGET_URL } : {}),
});
const bridge = new BridgeExecutor(cdp);
const limiter = new CallRateLimiter();

const server = new McpServer({ name: "foo-ui-webview2-mcp", version });

const tools = [
    ...registerBridgeTools(server, bridge, bridgeTools, {
        readOnly: READ_ONLY,
        maxResponseChars: MAX_RESPONSE_CHARS,
        maxImageBytes: MAX_IMAGE_BYTES,
        limiter,
    }),
    ...registerPageTools(server, bridge, {
        readOnly: READ_ONLY,
        enableEval: ENABLE_EVAL,
        maxResponseChars: MAX_RESPONSE_CHARS,
        limiter,
    }),
];

// ── Startup ──

async function main() {
    const transport = new GuardedStdioServerTransport();
    await server.connect(transport);
    logger.info("Server started", {
        version,
        cdpHost: CDP_HOST,
        cdpPort: CDP_PORT,
        readOnly: READ_ONLY,
        tools: tools.length,
    });

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

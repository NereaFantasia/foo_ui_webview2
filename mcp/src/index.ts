#!/usr/bin/env node
/**
 * foo-ui-webview2-mcp — MCP server entry point.
 *
 * Connects to foobar2000's WebView2 instance over CDP and exposes bridge
 * APIs and page inspection tools via stdio transport.
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

/** Parses a positive integer from an environment variable, returning `fallback` if unset or invalid. */
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
// Optional URL substring filter to target a specific WebView2 instance
// when multiple panels, popups, or overlays share the remote debugging port.
const CDP_TARGET_URL = process.env.FB2K_CDP_TARGET_URL || "";

// ── Tool options ──
const READ_ONLY = flag("FB2K_READ_ONLY");
const ENABLE_EVAL = flag("FB2K_ENABLE_EVAL");
const MAX_RESPONSE_CHARS = positiveInt("FB2K_MAX_RESPONSE_CHARS", DEFAULT_MAX_RESPONSE_CHARS);
const MAX_IMAGE_BYTES = positiveInt("FB2K_MAX_IMAGE_BYTES", DEFAULT_MAX_IMAGE_BYTES);

// Reports server version from package metadata.
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

    // Eagerly pre-connect to CDP in the background to eliminate cold-start latency on the initial tool call.
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

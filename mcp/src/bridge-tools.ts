import type { McpServer } from "@modelcontextprotocol/sdk/server/mcp.js";
import type { Transport } from "@modelcontextprotocol/sdk/shared/transport.js";
import type { CallToolResult } from "@modelcontextprotocol/sdk/types.js";
import type { z } from "zod";

import {
    createBridgeToolHandler,
    type BridgeExecutor,
} from "./bridge-executor.js";
import { getValidRequestId } from "./guarded-stdio-transport.js";
import { withRateLimit, type CallRateLimiter } from "./rate-limit.js";
import { errorResult, type ResultLimits } from "./tool-results.js";
import {
    buildToolInputSchema,
    findUnsafeObjectStructureIssue,
} from "./tool-schema.js";
import type { InputSchema, ToolDefinition } from "./types.js";

/** How the bridge tools are registered, with the size limits of their results. */
export interface BridgeToolOptions extends ResultLimits {
    /** Register only the tools whose actions change nothing. */
    readOnly?: boolean;
    /** Budget every call takes from; omitted, calls are not limited. */
    limiter?: CallRateLimiter;
}

/**
 * Register the bridge tools with their merged schemas and annotations.
 *
 * @param server - MCP server that will expose the tools.
 * @param bridge - Bridge executor used by every registered handler.
 * @param tools - Declarative tool definitions to register.
 * @returns The names of the tools registered.
 * @throws When a tool or action schema is malformed.
 */
export function registerBridgeTools(
    server: McpServer,
    bridge: Pick<BridgeExecutor, "call">,
    tools: readonly ToolDefinition[],
    options: BridgeToolOptions = {}
): string[] {
    const registered: string[] = [];
    for (const tool of tools) {
        if (options.readOnly && !tool.annotations.readOnlyHint) continue;
        server.registerTool(tool.name, {
            description: tool.description,
            inputSchema: buildToolInputSchema(tool.inputSchema),
            annotations: tool.annotations,
        }, withRateLimit(options.limiter, createActionToolHandler(bridge, tool, options)));
        registered.push(tool.name);
    }
    return registered;
}

/**
 * Create the handler of a bridge tool: it picks the action's host method,
 * checks the other arguments against that method's exact schema, and calls
 * it. Arguments the method would refuse come back as a tool execution error
 * that names the problem and lists what the method takes.
 *
 * @throws When an action schema is malformed.
 */
export function createActionToolHandler(
    bridge: Pick<BridgeExecutor, "call">,
    tool: ToolDefinition,
    limits: ResultLimits = {}
): (args: Record<string, unknown>) => Promise<CallToolResult> {
    const actions = new Map(Object.entries(tool.actions).map(([method, action]) => [method, {
        schema: buildToolInputSchema(action.inputSchema),
        call: createBridgeToolHandler(bridge, method, {
            maxResponseChars: limits.maxResponseChars,
            maxImageBytes: limits.maxImageBytes,
            image: action.image,
        }),
        takes: describeParams(action.inputSchema),
    }]));
    return async (args) => {
        const { action, ...params } = args;
        const entry = typeof action === "string" ? actions.get(action) : undefined;
        if (!entry) {
            return errorResult(
                `${tool.name} has no action ${JSON.stringify(action)}. Pass one of: ${[...actions.keys()].join(", ")}.`
            );
        }
        const parsed = entry.schema.safeParse(params);
        if (!parsed.success) {
            return errorResult(`Invalid arguments for ${action as string}: ${formatIssues(parsed.error)}. It takes ${entry.takes}.`);
        }
        return entry.call(parsed.data);
    };
}

/** The parameters of an action for an error message, required ones marked. */
function describeParams(schema: InputSchema): string {
    const required = new Set(schema.required ?? []);
    const keys = Object.keys(schema.properties);
    if (!keys.length) return "no parameters";
    return keys.map((k) => (required.has(k) ? `\`${k}\` (required)` : `\`${k}\``)).join(", ");
}

/** Validation issues as one line each: where, then what is wrong. */
function formatIssues(error: z.ZodError): string {
    return error.issues
        .map((issue) => {
            const where = issue.path.map(String).join(".");
            // A missing required argument reads better said plainly than as a type mismatch.
            if (where && issue.code === "invalid_type" && /received undefined$/.test(issue.message)) {
                return `\`${where}\` is required`;
            }
            return where ? `${where}: ${issue.message}` : issue.message;
        })
        .join("; ");
}

/**
 * Guard inbound MCP messages before SDK record parsing can normalize unsafe keys.
 *
 * @param transport - Server-side transport owned by the MCP server.
 * @returns A transport proxy that rejects unsafe JSON-RPC values.
 */
export function guardBridgeTransport(transport: Transport): Transport {
    return new BridgeTransportGuard(transport);
}

class BridgeTransportGuard implements Transport {
    constructor(private readonly inner: Transport) {}

    get sessionId(): string | undefined {
        return this.inner.sessionId;
    }

    get onclose(): Transport["onclose"] {
        return this.inner.onclose;
    }

    set onclose(handler: Transport["onclose"]) {
        this.inner.onclose = handler;
    }

    get onerror(): Transport["onerror"] {
        return this.inner.onerror;
    }

    set onerror(handler: Transport["onerror"]) {
        this.inner.onerror = handler;
    }

    get onmessage(): Transport["onmessage"] {
        return this.inner.onmessage;
    }

    set onmessage(handler: Transport["onmessage"]) {
        this.inner.onmessage = handler
            ? (message, extra) => {
                const issue = findUnsafeObjectStructureIssue(message, "message");
                if (!issue) {
                    handler(message, extra);
                    return;
                }
                const requestId = getValidRequestId(message);
                if (requestId !== undefined) {
                    void Promise.resolve().then(() => this.inner.send({
                            jsonrpc: "2.0",
                            id: requestId,
                            error: {
                                code: -32602,
                                message: `Invalid JSON-RPC value: ${issue}`,
                            },
                        })).catch((error: unknown) => {
                        this.inner.onerror?.(
                            error instanceof Error ? error : new Error(String(error))
                        );
                    });
                    return;
                }
                this.inner.onerror?.(new Error(`Invalid JSON-RPC value: ${issue}`));
            }
            : undefined;
    }

    start(): Promise<void> {
        return this.inner.start();
    }

    send(...args: Parameters<Transport["send"]>): ReturnType<Transport["send"]> {
        return this.inner.send(...args);
    }

    close(): Promise<void> {
        return this.inner.close();
    }

    setProtocolVersion(version: string): void {
        this.inner.setProtocolVersion?.(version);
    }
}

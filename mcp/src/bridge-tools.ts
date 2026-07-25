import type { McpServer } from "@modelcontextprotocol/sdk/server/mcp.js";
import type { Transport } from "@modelcontextprotocol/sdk/shared/transport.js";

import {
    createBridgeToolHandler,
    type BridgeExecutor,
} from "./bridge-executor.js";
import { getValidRequestId } from "./guarded-stdio-transport.js";
import {
    buildToolInputSchema,
    findUnsafeObjectStructureIssue,
} from "./tool-schema.js";
import type { ToolDefinition } from "./types.js";

/**
 * Register bridge-backed MCP tools with their production validation schemas.
 *
 * @param server - MCP server that will expose the tools.
 * @param bridge - Bridge executor used by every registered handler.
 * @param tools - Declarative tool definitions to register.
 * @param methodMap - Tool-name to `namespace.method` mapping.
 * @throws When a tool has no bridge method mapping or its schema is malformed.
 */
export function registerBridgeTools(
    server: McpServer,
    bridge: Pick<BridgeExecutor, "call">,
    tools: ToolDefinition[],
    methodMap: Record<string, string>
): void {
    for (const tool of tools) {
        const method = methodMap[tool.name];
        if (!method) {
            throw new Error(`Missing bridge method mapping for tool '${tool.name}'`);
        }

        server.registerTool(tool.name, {
            description: tool.description,
            inputSchema: buildToolInputSchema(tool.inputSchema),
        }, createBridgeToolHandler(bridge, method));
    }
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

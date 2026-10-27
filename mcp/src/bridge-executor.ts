/**
 * Bridge executor module.
 *
 * Invokes host API methods via `fb2k.invoke()` over CDP and formats their
 * results into MCP tool responses. Arguments are pre-validated against the
 * method's schema prior to invocation (see `bridge-tools.ts`).
 */

import type { CallToolResult } from "@modelcontextprotocol/sdk/types.js";

import { CdpClient } from "./cdp-client.js";
import { errorResult, jsonResult, pictureResult, type ResultLimits } from "./tool-results.js";

/**
 * Executes bridge API calls over CDP and formats their results into
 * the response structures expected by MCP tools.
 */
export class BridgeExecutor {
    constructor(private readonly cdp: CdpClient) {}

    /**
     * Invokes a bridge API method and returns a normalized result envelope.
     *
     * Never throws. Transport and host errors are captured in the returned
     * {@link BridgeResult} with `success: false`.
     *
     * @param method - Bridge method identifier, e.g. `"playback.play"`.
     * @param params - Optional method arguments.
     * @returns A {@link BridgeResult} containing either response data or error details.
     */
    async call(
        method: string,
        params?: Record<string, unknown>
    ): Promise<BridgeResult> {
        try {
            const result = await this.cdp.invoke(method, params);
            if (isHandlerFailure(result)) {
                return {
                    success: false,
                    error: typeof result.error === "string" && result.error.length > 0
                        ? result.error
                        : `Bridge method '${method}' returned success:false`,
                    ...(typeof result.code === "string" ? { code: result.code } : {}),
                    ...(Object.prototype.hasOwnProperty.call(result, "details")
                        ? { details: result.details }
                        : {}),
                };
            }
            return {
                success: true,
                data: result,
            };
        } catch (err) {
            return {
                success: false,
                error: err instanceof Error ? err.message : String(err),
            };
        }
    }

    /**
     * Captures a screenshot and returns it as a base64-encoded image string.
     *
     * @param options - When `fullPage` is true, captures the entire page layout
     *   instead of only the current viewport.
     */
    async screenshot(options?: { fullPage?: boolean }): Promise<string> {
        return this.cdp.screenshot(options);
    }

    /**
     * Evaluates an arbitrary JavaScript expression in the page context and
     * returns its evaluated result.
     */
    async evaluate(expression: string): Promise<unknown> {
        return this.cdp.evaluate(expression);
    }

    /**
     * Retrieves recent page console messages in chronological order (oldest first).
     *
     * @param limit - Maximum message count to return; defaults to the client limit.
     */
    async getConsoleMessages(limit?: number): Promise<Array<{ level: string; text: string }>> {
        return this.cdp.getConsoleMessages(limit);
    }
}

function isHandlerFailure(value: unknown): value is Record<string, unknown> & {
    success: false;
} {
    return value !== null
        && typeof value === "object"
        && !Array.isArray(value)
        && (value as Record<string, unknown>).success === false;
}

/**
 * Formats a failed bridge call into a readable error string for MCP text content,
 * preserving stable error codes and structured details.
 */
export function formatBridgeFailure(result: BridgeResult): string {
    const lines = [`Error: ${result.error ?? "Unknown bridge error"}`];
    if (result.code) {
        lines.push(`Code: ${result.code}`);
    }
    if (result.details !== undefined) {
        lines.push(`Details: ${JSON.stringify(result.details)}`);
    }
    return lines.join("\n");
}

/** Configuration options for shaping successful bridge tool results. */
export interface BridgeResultOptions extends ResultLimits {
    /**
     * Result field holding a `data:<mime>;base64,` picture URL. When set,
     * the picture is extracted and returned as an image block instead of raw JSON text.
     */
    image?: string;
}

/**
 * Creates a tool handler for a single bridge method with pre-validated arguments.
 *
 * Returns successful results as compact JSON text (or extracts `options.image` as an
 * image block when specified). Returns host and transport failures as tool execution errors.
 */
export function createBridgeToolHandler(
    bridge: Pick<BridgeExecutor, "call">,
    method: string,
    options: BridgeResultOptions = {}
): (params: Record<string, unknown>) => Promise<CallToolResult> {
    return async (params) => {
        const result = await bridge.call(method, params);
        if (!result.success) return errorResult(formatBridgeFailure(result));
        return options.image
            ? pictureResult(result.data, options.image, options)
            : jsonResult(result.data, options.maxResponseChars);
    };
}

/**
 * Normalized result of a bridge call, returned by {@link BridgeExecutor.call}.
 */
export interface BridgeResult {
    /** Whether the bridge call completed successfully. */
    success: boolean;
    /** Payload returned by the bridge method; present when `success` is true. */
    data?: unknown;
    /** Error message; present when `success` is false. */
    error?: string;
    /** Stable host error code when provided by the handler. */
    code?: string;
    /** Structured host error context when provided by the handler. */
    details?: unknown;
}

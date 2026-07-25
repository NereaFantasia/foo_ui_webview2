/**
 * Bridge executor.
 *
 * Translates MCP tool calls into `fb2k.invoke()` CDP calls, mapping
 * arguments and normalizing return values.
 */

import { CdpClient } from "./cdp-client.js";

/**
 * Executes bridge API calls over CDP and normalizes their results into
 * the shape expected by MCP tools.
 */
export class BridgeExecutor {
    constructor(private readonly cdp: CdpClient) {}

    /**
     * Invoke a bridge API method and return a normalized result.
     *
     * Never throws: transport or host errors are captured into the
     * returned {@link BridgeResult} with `success: false`.
     *
     * @param method - Bridge method id, e.g. `"playback.play"`.
     * @param params - Optional method arguments.
     * @returns A {@link BridgeResult} wrapping the data or error message.
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
     * Capture a screenshot and return it as a base64-encoded image.
     *
     * @param options - When `fullPage` is true, captures the full page
     *   instead of just the current viewport.
     */
    async screenshot(options?: { fullPage?: boolean }): Promise<string> {
        return this.cdp.screenshot(options);
    }

    /**
     * Evaluate an arbitrary JavaScript expression in the page and return
     * its result.
     */
    async evaluate(expression: string): Promise<unknown> {
        return this.cdp.evaluate(expression);
    }

    /**
     * Retrieve buffered console messages from the page.
     */
    async getConsoleMessages(): Promise<Array<{ level: string; text: string }>> {
        return this.cdp.getConsoleMessages();
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
 * Format a failed bridge call for MCP text content without discarding the
 * stable host error code or structured details.
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

/**
 * Create the generic MCP handler used by bridge-backed tools.
 */
export function createBridgeToolHandler(
    bridge: Pick<BridgeExecutor, "call">,
    method: string
) {
    return async (params: Record<string, unknown>) => {
        const result = await bridge.call(method, params);
        if (!result.success) {
            return {
                content: [
                    { type: "text" as const, text: formatBridgeFailure(result) },
                ],
                isError: true,
            };
        }
        return {
            content: [
                {
                    type: "text" as const,
                    text: JSON.stringify(result.data, null, 2),
                },
            ],
        };
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
    /** Stable host error code when the handler returned one. */
    code?: string;
    /** Structured host error context when the handler returned it. */
    details?: unknown;
}

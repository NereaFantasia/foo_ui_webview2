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
}

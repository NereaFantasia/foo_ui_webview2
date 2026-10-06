/**
 * A token-bucket limit on tool calls, shared by every tool of the server.
 */

import type { CallToolResult } from "@modelcontextprotocol/sdk/types.js";

import { errorResult } from "./tool-results.js";

/** How many calls the server takes. */
export interface RateLimitOptions {
    /** Calls allowed back to back before the rate applies. */
    burst: number;
    /** Calls allowed per second once the burst is spent. */
    perSecond: number;
    /** Clock in milliseconds; `Date.now` by default. */
    now?: () => number;
}

/** The server's default budget: 40 calls in a burst, then 20 a second. */
export const DEFAULT_RATE_LIMIT: Readonly<RateLimitOptions> = { burst: 40, perSecond: 20 };

/** Counts tool calls against a budget that refills at a steady rate. */
export class CallRateLimiter {
    private readonly burst: number;
    private readonly perSecond: number;
    private readonly now: () => number;
    private tokens: number;
    private last: number;

    constructor(options: RateLimitOptions = DEFAULT_RATE_LIMIT) {
        if (!(options.burst >= 1) || !(options.perSecond > 0)) {
            throw new Error("a rate limit needs a burst of at least 1 and a positive rate");
        }
        this.burst = options.burst;
        this.perSecond = options.perSecond;
        this.now = options.now ?? Date.now;
        this.tokens = this.burst;
        this.last = this.now();
    }

    /**
     * Take one call from the budget.
     *
     * @returns `0` when the call may go ahead, otherwise the milliseconds to
     *   wait before one will.
     */
    take(): number {
        const now = this.now();
        this.tokens = Math.min(this.burst, this.tokens + ((now - this.last) * this.perSecond) / 1000);
        this.last = now;
        if (this.tokens >= 1) {
            this.tokens -= 1;
            return 0;
        }
        return Math.ceil(((1 - this.tokens) * 1000) / this.perSecond);
    }

    /** The refusal a call over the budget gets. */
    refusal(waitMs: number): CallToolResult {
        return errorResult(
            `Rate limit: this server takes ${this.burst} calls in a burst and ${this.perSecond} a second after that. ` +
            `Retry in ${waitMs} ms.`
        );
    }
}

/** A tool handler that first takes its call from `limiter`, and is refused when none is left. */
export function withRateLimit<A extends unknown[]>(
    limiter: CallRateLimiter | undefined,
    handler: (...args: A) => Promise<CallToolResult>
): (...args: A) => Promise<CallToolResult> {
    if (!limiter) return handler;
    return async (...args: A) => {
        const wait = limiter.take();
        return wait === 0 ? handler(...args) : limiter.refusal(wait);
    };
}

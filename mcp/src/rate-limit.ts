/**
 * Token-bucket rate limiter for tool execution across the server.
 */

import type { CallToolResult } from "@modelcontextprotocol/sdk/types.js";

import { errorResult } from "./tool-results.js";

/** Configuration options for the token-bucket rate limiter. */
export interface RateLimitOptions {
    /** Maximum number of burst calls allowed before rate limiting applies. */
    burst: number;
    /** Sustained call rate allowed per second once burst capacity is exhausted. */
    perSecond: number;
    /** Timestamp provider returning epoch milliseconds; defaults to `Date.now`. */
    now?: () => number;
}

/** Default rate limit: burst capacity of 40 calls, refilling at 20 calls per second. */
export const DEFAULT_RATE_LIMIT: Readonly<RateLimitOptions> = { burst: 40, perSecond: 20 };

/** Tracks tool invocation rate against a token bucket that refills steadily over time. */
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
     * Consumes one token from the budget.
     *
     * @returns `0` if the call is permitted immediately, or the number of milliseconds
     *   to wait before a token becomes available.
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

    /** Formats an error result when a call exceeds the rate limit. */
    refusal(waitMs: number): CallToolResult {
        return errorResult(
            `Rate limit: this server takes ${this.burst} calls in a burst and ${this.perSecond} a second after that. ` +
            `Retry in ${waitMs} ms.`
        );
    }
}

/** Wraps a tool handler with rate limiting, returning a refusal error if the rate limit is exceeded. */
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

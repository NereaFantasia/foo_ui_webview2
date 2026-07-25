/**
 * CDP connection manager.
 *
 * Establishes and maintains a connection to the WebView2 Chrome DevTools
 * Protocol endpoint. Defaults to `localhost:9222` (the WebView2
 * remote-debugging-port).
 */

import CDP from "chrome-remote-interface";
import { logger } from "./logger.js";

/**
 * Options for {@link CdpClient}. All fields are optional and fall back to
 * the documented defaults.
 */
export interface CdpClientOptions {
    /** CDP host to connect to. Defaults to `"localhost"`. */
    host?: string;
    /** CDP port to connect to. Defaults to `9222`. */
    port?: number;
    /** Maximum number of automatic reconnect attempts. Defaults to `3`. */
    maxReconnect?: number;
    /** Base reconnect delay in milliseconds; doubles on each attempt. Defaults to `1000`. */
    reconnectBaseMs?: number;
    /** Timeout for a single invoke or evaluate call, in milliseconds. Defaults to `30000`. */
    invokeTimeoutMs?: number;
    /** Timeout for establishing a connection, in milliseconds. Defaults to `10000`. */
    connectTimeoutMs?: number;
    /** Timeout for a screenshot capture, in milliseconds. Defaults to `15000`. */
    screenshotTimeoutMs?: number;
    /**
     * Idle time after which a cached connection is health-checked before
     * reuse, in milliseconds. Defaults to `30000`.
     */
    pingIdleMs?: number;
    /** Timeout for the health-check ping, in milliseconds. Defaults to `2000`. */
    pingTimeoutMs?: number;
    /**
     * Budget for the warmup screenshot issued right after connecting, in
     * milliseconds. Warmup failures and timeouts are ignored. Defaults to `3000`.
     */
    warmupTimeoutMs?: number;
    /**
     * URL substring used to pin the CDP page target. When set and no target
     * matches, connecting fails (listing the candidates) instead of silently
     * binding to an arbitrary page. Usually supplied through the
     * `FB2K_CDP_TARGET_URL` environment variable.
     */
    targetUrlFilter?: string;
}

type ResolvedOptions = Required<Omit<CdpClientOptions, "targetUrlFilter">> &
    Pick<CdpClientOptions, "targetUrlFilter">;

const DEFAULT_OPTIONS: Required<Omit<CdpClientOptions, "targetUrlFilter">> = {
    host: "localhost",
    port: 9222,
    maxReconnect: 3,
    reconnectBaseMs: 1000,
    invokeTimeoutMs: 30_000,
    connectTimeoutMs: 10_000,
    screenshotTimeoutMs: 15_000,
    pingIdleMs: 30_000,
    pingTimeoutMs: 2_000,
    warmupTimeoutMs: 3_000,
};

/**
 * Hint appended to timeout errors. A hidden WebView2 page stops producing
 * frames and gets its timers throttled, which is the most common reason a
 * previously working CDP session suddenly stalls.
 */
const SUSPENDED_PAGE_HINT =
    "The target page may be suspended (minimized / tray / locked screen). " +
    "Restore the foobar2000 window, or enable CDP keep-alive under " +
    "Preferences > Advanced > Tools > WebView2 UI.";

/** Element type of `CDP.List()` results, derived from the library types. */
type CdpTarget = Awaited<ReturnType<typeof CDP.List>>[number];

/**
 * Manages a single CDP connection to a WebView2 page target and exposes
 * helpers to invoke bridge methods, evaluate JS, capture screenshots, and
 * read console output. Reconnects automatically with exponential backoff,
 * health-checks idle connections before reuse, and bounds every CDP round
 * trip with a timeout.
 */
export class CdpClient {
    private client: CDP.Client | null = null;
    private readonly options: ResolvedOptions;
    private reconnectCount = 0;
    private lastActivityMs = 0;

    constructor(options?: CdpClientOptions) {
        this.options = { ...DEFAULT_OPTIONS, ...options };
    }

    /** Whether a CDP connection is currently established. */
    get connected(): boolean {
        return this.client !== null;
    }

    /**
     * Connect to the WebView2 CDP endpoint, picking a page target (see
     * remarks on target selection in the class description).
     *
     * No-op if already connected.
     *
     * @throws When no suitable page target is found, the connection fails,
     *   or the connection attempt exceeds `connectTimeoutMs`.
     */
    async connect(): Promise<void> {
        if (this.client) return;
        await this.withTimeout(
            this.connectInternal(),
            this.options.connectTimeoutMs,
            "connect"
        );
    }

    private async connectInternal(): Promise<void> {
        try {
            const targets = await CDP.List({
                host: this.options.host,
                port: this.options.port,
            });

            const pageTarget = this.selectPageTarget(targets);

            this.client = await CDP({
                host: this.options.host,
                port: this.options.port,
                target: pageTarget,
            });

            // Enable the Runtime and Page domains (Page is needed for
            // screenshots and layout operations).
            await Promise.all([
                this.client.Runtime.enable(),
                this.client.Page.enable(),
            ]);

            // 1x1 warmup screenshot to initialize the WebView2 render
            // pipeline, avoiding the ~8s latency on the first real
            // screenshot. Bounded so a suspended page cannot stall connect().
            await this.withTimeout(
                this.client.Page.captureScreenshot({
                    format: "png",
                    clip: { x: 0, y: 0, width: 1, height: 1, scale: 1 },
                }),
                this.options.warmupTimeoutMs,
                "warmup screenshot"
            ).catch(() => undefined);

            this.reconnectCount = 0;
            this.touch();

            // Drop the cached client when the connection is lost.
            this.client.on("disconnect", () => {
                this.client = null;
            });
        } catch (err) {
            this.client = null;
            throw new Error(
                `Failed to connect to WebView2 CDP at ${this.options.host}:${this.options.port}. ` +
                `Ensure fb2k is running and DevTools is enabled. Original error: ${err}`
            );
        }
    }

    /**
     * Pick the page target to attach to.
     *
     * Auxiliary WebViews (such as the tray menu overlay) are rendered via
     * `NavigateToString` and always report `about:blank`; however, a main
     * window without a deployed template shows the built-in test page and
     * reports `about:blank` too. Targets with a concrete URL are therefore
     * preferred and `about:blank` is used only as a fallback, never excluded.
     */
    private selectPageTarget(targets: CdpTarget[]): CdpTarget {
        const candidates = targets.filter(
            (t) => t.type === "page" && !t.url?.startsWith("devtools://")
        );

        if (candidates.length === 0) {
            throw new Error(
                `No page target found at ${this.options.host}:${this.options.port}. ` +
                "Is WebView2 running with DevTools enabled?"
            );
        }

        const filter = this.options.targetUrlFilter;
        if (filter) {
            const matched = candidates.filter((t) =>
                (t.url ?? "").includes(filter)
            );
            if (matched.length === 0) {
                const urls = candidates.map((t) => t.url ?? "<unknown>").join(", ");
                throw new Error(
                    `No page target matches FB2K_CDP_TARGET_URL="${filter}". ` +
                    `Candidates: ${urls}`
                );
            }
            return matched[0];
        }

        const concrete = candidates.filter((t) => (t.url ?? "") !== "about:blank");
        const pool = concrete.length > 0 ? concrete : candidates;
        if (concrete.length === 0) {
            logger.warn(
                "all CDP page targets report about:blank; using the first one. " +
                "Deploy a template or set FB2K_CDP_TARGET_URL to pin a target",
                { candidates: candidates.map((t) => t.url) }
            );
        } else if (pool.length > 1) {
            logger.warn(
                "multiple CDP page targets found; using the first one. " +
                "Set FB2K_CDP_TARGET_URL to pin a target",
                { candidates: pool.map((t) => t.url) }
            );
        }
        return pool[0];
    }

    /**
     * Close the CDP connection. No-op if not connected.
     */
    async disconnect(): Promise<void> {
        if (this.client) {
            const client = this.client;
            this.client = null;
            await client.close();
        }
    }

    /**
     * Ensure a healthy connection is available.
     *
     * A cached connection that has been idle for longer than `pingIdleMs`
     * is health-checked first; if the ping fails, the stale connection is
     * dropped and a new one is established with exponential backoff (up to
     * `maxReconnect` attempts).
     *
     * @throws The last connection error if all attempts fail.
     */
    async ensureConnected(): Promise<void> {
        if (this.client) {
            if (Date.now() - this.lastActivityMs < this.options.pingIdleMs) {
                return;
            }
            if (await this.pingCachedClient()) {
                return;
            }
            logger.warn("cached CDP connection failed its health check; reconnecting");
            await this.disconnect().catch(() => undefined);
        }

        let lastError: unknown;
        for (let i = 0; i <= this.options.maxReconnect; i++) {
            try {
                if (i > 0) {
                    const delay = this.options.reconnectBaseMs * Math.pow(2, i - 1);
                    await new Promise((r) => setTimeout(r, delay));
                }
                await this.connect();
                return;
            } catch (err) {
                lastError = err;
            }
        }
        throw lastError;
    }

    /**
     * Execute `fb2k.invoke()` inside the WebView2 page over CDP.
     *
     * @param method - Bridge API method id, e.g. `"playback.play"`.
     * @param params - Optional argument object.
     * @returns The value returned by the bridge method.
     * @throws When the method name is malformed or the bridge call fails.
     */
    async invoke(method: string, params?: Record<string, unknown>): Promise<unknown> {
        await this.ensureConnected();

        // Validate the method format to prevent JS injection.
        if (!/^[a-zA-Z][a-zA-Z0-9]*\.[a-zA-Z][a-zA-Z0-9]*$/.test(method)) {
            throw new Error(`Invalid method name: "${method}". Expected format: namespace.method`);
        }

        const paramsJson = params ? JSON.stringify(params) : "undefined";
        const expression = `window.fb2k.invoke('${method}', ${paramsJson})`;

        const result = await this.withTimeout(
            this.evaluateRaw(
                `(async () => { try { return await ${expression}; } catch(e) { return { __error: e.message }; } })()`
            ),
            this.options.invokeTimeoutMs,
            `fb2k.invoke('${method}')`
        );

        if (
            result &&
            typeof result === "object" &&
            "__error" in (result as Record<string, unknown>)
        ) {
            throw new Error(
                `fb2k.invoke('${method}') failed: ${(result as Record<string, unknown>).__error}`
            );
        }

        return result;
    }

    /**
     * Evaluate an arbitrary JavaScript expression in the WebView2 page.
     */
    async evaluate(expression: string): Promise<unknown> {
        await this.ensureConnected();
        return this.withTimeout(
            this.evaluateRaw(expression),
            this.options.invokeTimeoutMs,
            "evaluate"
        );
    }

    /**
     * Capture a screenshot of the WebView2 page.
     *
     * @param options - When `fullPage` is true, the viewport is resized to
     *   the full content size before capturing.
     * @returns Base64-encoded PNG image data.
     */
    async screenshot(options?: { fullPage?: boolean }): Promise<string> {
        await this.ensureConnected();
        return this.withTimeout(
            this.screenshotInternal(options),
            this.options.screenshotTimeoutMs,
            "screenshot"
        );
    }

    private async screenshotInternal(options?: { fullPage?: boolean }): Promise<string> {
        if (options?.fullPage) {
            // Measure the full page size.
            const metrics = await this.client!.Page.getLayoutMetrics();
            const { width, height } = metrics.contentSize;
            await this.client!.Emulation.setDeviceMetricsOverride({
                width: Math.ceil(width),
                height: Math.ceil(height),
                deviceScaleFactor: 1,
                mobile: false,
            });
        }

        const { data } = await this.client!.Page.captureScreenshot({
            format: "png",
        });

        if (options?.fullPage) {
            await this.client!.Emulation.clearDeviceMetricsOverride();
        }

        this.touch();
        return data;
    }

    /**
     * Retrieve buffered console messages from the page (last 100).
     */
    async getConsoleMessages(): Promise<Array<{ level: string; text: string }>> {
        await this.ensureConnected();

        const result = await this.withTimeout(
            this.evaluateRaw(`
            (window.__fb2kMcpConsoleLogs || []).slice(-100)
        `),
            this.options.invokeTimeoutMs,
            "console message fetch"
        );

        return (result as Array<{ level: string; text: string }>) || [];
    }

    // ── Internal helpers ──

    private async evaluateRaw(expression: string): Promise<unknown> {
        const { result, exceptionDetails } =
            await this.client!.Runtime.evaluate({
                expression,
                awaitPromise: true,
                returnByValue: true,
            });

        if (exceptionDetails) {
            throw new Error(
                `JS evaluation error: ${exceptionDetails.text || exceptionDetails.exception?.description}`
            );
        }

        this.touch();
        return result.value;
    }

    /** Cheap liveness probe for a cached connection; never throws. */
    private async pingCachedClient(): Promise<boolean> {
        if (!this.client) return false;
        try {
            await this.withTimeout(
                this.client.Runtime.evaluate({ expression: "1", returnByValue: true }),
                this.options.pingTimeoutMs,
                "connection health check"
            );
            this.touch();
            return true;
        } catch {
            return false;
        }
    }

    private touch(): void {
        this.lastActivityMs = Date.now();
    }

    private async withTimeout<T>(
        work: Promise<T>,
        timeoutMs: number,
        label: string
    ): Promise<T> {
        let timer: ReturnType<typeof setTimeout> | undefined;
        const timeoutPromise = new Promise<never>((_, reject) => {
            timer = setTimeout(
                () =>
                    reject(
                        new Error(
                            `${label} timed out after ${timeoutMs}ms. ${SUSPENDED_PAGE_HINT}`
                        )
                    ),
                timeoutMs
            );
        });
        try {
            return await Promise.race([work, timeoutPromise]);
        } finally {
            clearTimeout(timer);
        }
    }
}

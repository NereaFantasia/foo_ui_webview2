/**
 * CDP connection manager.
 *
 * Establishes and maintains a connection to the WebView2 Chrome DevTools
 * Protocol endpoint, defaulting to `localhost:9222` (the WebView2 remote-debugging-port).
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

/** Maximum number of recent console messages retained in memory. */
export const CONSOLE_CAPACITY = 200;
/** Default number of console messages returned when no limit is specified. */
export const DEFAULT_CONSOLE_LIMIT = 100;

/** Log entry captured from a page console call or uncaught exception. */
export interface ConsoleMessage {
    /** Log severity or source (`log`, `warning`, `error`, or `exception` for uncaught errors). */
    level: string;
    /** Formatted message text with arguments delimited by spaces. */
    text: string;
}

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
    private readonly consoleLog: ConsoleMessage[] = [];

    constructor(options?: CdpClientOptions) {
        this.options = { ...DEFAULT_OPTIONS, ...options };
    }

    /** Whether a CDP connection is currently established. */
    get connected(): boolean {
        return this.client !== null;
    }

    /**
     * Connects to the WebView2 CDP endpoint and attaches to the target page.
     *
     * No-op if already connected.
     *
     * @throws When no matching page target is found, connection establishment fails,
     *   or the attempt exceeds `connectTimeoutMs`.
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

            // Listen for console calls and unhandled exceptions. Enabling the Runtime domain
            // also flushes buffered messages logged prior to connection establishment.
            this.client.on("Runtime.consoleAPICalled", (event: unknown) => {
                this.recordConsole(consoleCallMessage(event));
            });
            this.client.on("Runtime.exceptionThrown", (event: unknown) => {
                this.recordConsole(exceptionMessage(event));
            });

            // Enable Runtime and Page domains (Page is required for screenshots and layout metrics).
            await Promise.all([
                this.client.Runtime.enable(),
                this.client.Page.enable(),
            ]);

            // Issue a 1x1 warmup screenshot to prime the WebView2 rendering pipeline,
            // preventing an ~8s latency spike on the initial real screenshot request.
            // Bounded with a timeout so a suspended page does not block connection.
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

            // Clear client reference when the WebSocket connection closes.
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
     * Selects the target page for CDP attachment.
     *
     * Auxiliary WebViews (such as the tray menu overlay) use `NavigateToString` and
     * report `about:blank`. However, a main window without a deployed template shows
     * the built-in test page and also reports `about:blank`. Targets with concrete URLs
     * are therefore prioritized, while `about:blank` serves as a fallback.
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
     * Ensures an active, healthy CDP connection is available.
     *
     * If the cached connection has been idle longer than `pingIdleMs`, it is validated
     * with a health check. If the check fails, the connection is closed and re-established
     * with exponential backoff (up to `maxReconnect` attempts).
     *
     * @throws The final connection error if all reconnection attempts fail.
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
     * Invokes `window.fb2k.invoke()` within the WebView2 page over CDP.
     *
     * @param method - Bridge API method identifier, e.g. `"playback.play"`.
     * @param params - Optional method arguments object.
     * @returns The value returned by the bridge method.
     * @throws When the method identifier is malformed or the bridge invocation fails.
     */
    async invoke(method: string, params?: Record<string, unknown>): Promise<unknown> {
        await this.ensureConnected();

        // Validate method format to guard against JavaScript injection.
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
     * Evaluates an arbitrary JavaScript expression in the WebView2 page context.
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
     * Captures a screenshot of the WebView2 page.
     *
     * @param options - When `fullPage` is true, the viewport is temporarily resized
     *   to match the full scrollable content size before capture.
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
            // Measure full scrollable content size and apply layout override.
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
     * Retrieves recent page console messages and uncaught exceptions in chronological order (oldest first).
     *
     * Captures messages starting from initial connection (including pre-connection messages
     * buffered by the browser). Retains up to {@link CONSOLE_CAPACITY} entries across reconnections.
     *
     * @param limit - Maximum number of recent messages to return; defaults to {@link DEFAULT_CONSOLE_LIMIT}.
     */
    async getConsoleMessages(limit = DEFAULT_CONSOLE_LIMIT): Promise<ConsoleMessage[]> {
        await this.ensureConnected();
        return limit > 0 ? this.consoleLog.slice(-limit) : [];
    }

    private recordConsole(message: ConsoleMessage | null): void {
        if (!message) return;
        this.consoleLog.push(message);
        if (this.consoleLog.length > CONSOLE_CAPACITY) {
            this.consoleLog.splice(0, this.consoleLog.length - CONSOLE_CAPACITY);
        }
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

    /** Lightweight liveness probe for cached connections; evaluates a trivial expression and never throws. */
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

// ── Console events ──

const isRecord = (value: unknown): value is Record<string, unknown> =>
    value !== null && typeof value === "object" && !Array.isArray(value);

/**
 * Formats a console argument into readable text. Primitives are stringified by value,
 * while object remote references are represented by their description or type.
 */
function remoteObjectText(arg: unknown): string {
    if (!isRecord(arg)) return String(arg);
    if ("value" in arg) {
        return typeof arg.value === "string" ? arg.value : JSON.stringify(arg.value) ?? String(arg.value);
    }
    if (typeof arg.unserializableValue === "string") return arg.unserializableValue;
    if (typeof arg.description === "string") return arg.description;
    return typeof arg.type === "string" ? arg.type : "?";
}

/** Converts a `Runtime.consoleAPICalled` event into a {@link ConsoleMessage}, or `null` if malformed. */
function consoleCallMessage(event: unknown): ConsoleMessage | null {
    if (!isRecord(event) || typeof event.type !== "string") return null;
    const args = Array.isArray(event.args) ? event.args : [];
    return { level: event.type, text: args.map(remoteObjectText).join(" ") };
}

/** Converts a `Runtime.exceptionThrown` event into a {@link ConsoleMessage}, or `null` if malformed. */
function exceptionMessage(event: unknown): ConsoleMessage | null {
    if (!isRecord(event) || !isRecord(event.exceptionDetails)) return null;
    const details = event.exceptionDetails;
    const described = isRecord(details.exception) ? details.exception.description : undefined;
    const text = typeof described === "string" ? described : typeof details.text === "string" ? details.text : "";
    return { level: "exception", text };
}

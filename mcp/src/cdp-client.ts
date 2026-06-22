/**
 * CDP connection manager.
 *
 * Establishes and maintains a connection to the WebView2 Chrome DevTools
 * Protocol endpoint. Defaults to `localhost:9222` (the WebView2
 * remote-debugging-port).
 */

import CDP from "chrome-remote-interface";

/**
 * Options for {@link CdpClient}. All fields are optional and fall back to
 * {@link DEFAULT_OPTIONS}.
 */
export interface CdpClientOptions {
    /** CDP host to connect to. Defaults to `"localhost"`. */
    host?: string;
    /** CDP port to connect to. Defaults to `9222`. */
    port?: number;
    /** Maximum number of automatic reconnect attempts. */
    maxReconnect?: number;
    /** Base reconnect delay in milliseconds; doubles on each attempt. */
    reconnectBaseMs?: number;
    /** Timeout for a single invoke call, in milliseconds. */
    invokeTimeoutMs?: number;
}

const DEFAULT_OPTIONS: Required<CdpClientOptions> = {
    host: "localhost",
    port: 9222,
    maxReconnect: 3,
    reconnectBaseMs: 1000,
    invokeTimeoutMs: 30_000,
};

/**
 * Manages a single CDP connection to a WebView2 page target and exposes
 * helpers to invoke bridge methods, evaluate JS, capture screenshots, and
 * read console output. Reconnects automatically with exponential backoff.
 */
export class CdpClient {
    private client: CDP.Client | null = null;
    private readonly options: Required<CdpClientOptions>;
    private reconnectCount = 0;

    constructor(options?: CdpClientOptions) {
        this.options = { ...DEFAULT_OPTIONS, ...options };
    }

    /** Whether a CDP connection is currently established. */
    get connected(): boolean {
        return this.client !== null;
    }

    /**
     * Connect to the WebView2 CDP endpoint, auto-discovering the first
     * page target.
     *
     * No-op if already connected.
     *
     * @throws When no page target is found or the connection fails.
     */
    async connect(): Promise<void> {
        if (this.client) return;

        try {
            // List all targets and pick the first page.
            const targets = await CDP.List({
                host: this.options.host,
                port: this.options.port,
            });

            const pageTarget = targets.find(
                (t: { type: string; url?: string }) =>
                    t.type === "page" && !t.url?.startsWith("devtools://")
            );
            if (!pageTarget) {
                throw new Error(
                    `No page target found at ${this.options.host}:${this.options.port}. ` +
                    "Is WebView2 running with DevTools enabled?"
                );
            }

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
            // pipeline, avoiding the ~8s latency on the first real screenshot.
            await this.client.Page.captureScreenshot({
                format: "png",
                clip: { x: 0, y: 0, width: 1, height: 1, scale: 1 },
            }).catch(() => { /* ignore warmup failure */ });

            this.reconnectCount = 0;

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
     * Close the CDP connection. No-op if not connected.
     */
    async disconnect(): Promise<void> {
        if (this.client) {
            await this.client.close();
            this.client = null;
        }
    }

    /**
     * Ensure the connection is available, reconnecting with exponential
     * backoff (up to {@link CdpClientOptions.maxReconnect} attempts).
     *
     * @throws The last connection error if all attempts fail.
     */
    async ensureConnected(): Promise<void> {
        if (this.client) return;

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

        const result = await this.evaluateWithTimeout(
            `(async () => { try { return await ${expression}; } catch(e) { return { __error: e.message }; } })()`,
            this.options.invokeTimeoutMs
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
        return this.evaluateRaw(expression);
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

        return data;
    }

    /**
     * Retrieve buffered console messages from the page (last 100).
     */
    async getConsoleMessages(): Promise<Array<{ level: string; text: string }>> {
        await this.ensureConnected();

        const result = await this.evaluateRaw(`
            (window.__fb2kMcpConsoleLogs || []).slice(-100)
        `);

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

        return result.value;
    }

    private async evaluateWithTimeout(
        expression: string,
        timeoutMs: number
    ): Promise<unknown> {
        let timer: ReturnType<typeof setTimeout>;
        const timeoutPromise = new Promise<never>((_, reject) => {
            timer = setTimeout(
                () => reject(new Error(`invoke timed out after ${timeoutMs}ms`)),
                timeoutMs
            );
        });
        try {
            return await Promise.race([this.evaluateRaw(expression), timeoutPromise]);
        } finally {
            clearTimeout(timer!);
        }
    }
}

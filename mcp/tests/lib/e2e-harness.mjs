/**
 * Shared CDP harness for the real-instance e2e scripts in mcp/tests.
 *
 * Those scripts drive a running foobar2000 through the WebView2 DevTools port,
 * so they all need the same four pieces: locate the page that exposes
 * window.fb2k, invoke bridge methods under a bounded timeout, record
 * machine-checkable cases, and emit one JSON summary plus a meaningful exit
 * code.
 *
 * Exit codes: 0 every case passed, 1 a case failed or the run threw,
 * 2 blocked before any case could run (no bridge page, bridge unresponsive).
 *
 * Environment: FB2K_CDP_PORT (default 9222), FB2K_E2E_TIMEOUT_MS (default
 * 5000), FB2K_CDP_TARGET_URL (substring that pins which page to attach to),
 * FB2K_E2E_RESULTS_DIR (when set, report() also writes the summary there).
 * A script may override the timeout to keep its own historical variable.
 */

import CDP from "chrome-remote-interface";
import { mkdirSync, writeFileSync } from "node:fs";
import { basename, join } from "node:path";

export const EXIT_PASSED = 0;
export const EXIT_FAILED = 1;
export const EXIT_BLOCKED = 2;

/** Reads a positive integer env var, falling back when unset or unparseable. */
export function envInt(name, fallback) {
    const raw = process.env[name];
    if (raw === undefined || raw === "") return fallback;
    const parsed = Number.parseInt(raw, 10);
    return Number.isInteger(parsed) && parsed > 0 ? parsed : fallback;
}

export function resolvePort() {
    return envInt("FB2K_CDP_PORT", 9222);
}

/** Tags an error as blocked so report() maps it to exit code 2, not 1. */
export function blockedError(message, detail) {
    const error = new Error(message);
    error.blocked = true;
    if (detail !== undefined) error.detail = detail;
    return error;
}

/**
 * Ranks a candidate page: an explicit FB2K_CDP_TARGET_URL match wins, then real
 * documents, then data: URLs.
 *
 * A running instance usually exposes the bridge on more than one page - the main
 * UI plus auxiliary windows such as the menu overlay, which is a data: document.
 * Attaching to whichever page the port happens to list first makes results
 * depend on window creation order: an overlay has no drag-drop registration and
 * an untrusted origin, so path-side contracts silently become unobservable.
 */
function rankPage(page, preferUrl) {
    const url = String(page.url || "");
    if (preferUrl && url.includes(preferUrl)) return 0;
    return url.startsWith("data:") ? 2 : 1;
}

/**
 * Attaches to the highest-ranked page that exposes the bridge. Every candidate
 * is probed so the caller can report what else was available; all clients except
 * the chosen one are closed before returning.
 */
export async function connectBridgePage({
    port = resolvePort(),
    preferUrl = process.env.FB2K_CDP_TARGET_URL || "",
} = {}) {
    const targets = await CDP.List({ port });
    const pages = targets
        .filter((target) => target.type === "page" && !target.url.startsWith("devtools://"))
        .map((page, order) => ({ page, order, rank: rankPage(page, preferUrl) }))
        .sort((a, b) => a.rank - b.rank || a.order - b.order);

    const candidates = [];
    let chosen;

    for (const entry of pages) {
        const client = await CDP({ port, target: entry.page });
        const probe = await client.Runtime.evaluate({
            expression:
                "typeof window.fb2k === 'object' && typeof window.fb2k.invoke === 'function'",
            returnByValue: true,
        });
        const hasBridge = probe.result.value === true;
        candidates.push({
            title: String(entry.page.title || "").slice(0, 80),
            url: String(entry.page.url || "").slice(0, 120),
            rank: entry.rank,
            hasBridge,
            chosen: hasBridge && !chosen,
        });
        if (hasBridge && !chosen) {
            chosen = { client, page: entry.page };
        } else {
            await client.close();
        }
    }

    if (!chosen) {
        throw blockedError("No WebView2 page exposes window.fb2k.invoke", {
            port,
            candidates,
        });
    }

    return { ...chosen, port, candidates };
}

export async function closeClient(client, { timeoutMs = 1000 } = {}) {
    if (!client) return;
    await Promise.race([
        client.close().catch(() => undefined),
        new Promise((resolve) => setTimeout(resolve, timeoutMs)),
    ]);
}

/**
 * Wraps a CDP Runtime domain into bridge callers.
 *
 * invokeRaw never throws on bridge-level outcomes: it returns a probe envelope
 * whose kind is result, error, timeout (the page-side race lost) or cdp-timeout
 * (Runtime.evaluate itself never came back). invoke is the strict variant that
 * throws on anything but result, for setup steps whose failure aborts the run.
 */
export function createBridge(
    Runtime,
    { invokeTimeoutMs = envInt("FB2K_E2E_TIMEOUT_MS", 5000) } = {},
) {
    function invokeExpression(method, params, timeoutMs) {
        const methodJson = JSON.stringify(method);
        const paramsJson = params === undefined ? "undefined" : JSON.stringify(params);
        return `Promise.race([
        window.fb2k.invoke(${methodJson}, ${paramsJson}).then(
            value => ({ kind: 'result', value }),
            error => ({ kind: 'error', error: String(error && error.message ? error.message : error) })
        ),
        new Promise(resolve => setTimeout(
            () => resolve({ kind: 'timeout', timeoutMs: ${timeoutMs} }),
            ${timeoutMs}
        ))
    ])`;
    }

    async function evaluateValue(
        expression,
        awaitPromise = true,
        timeoutMs = invokeTimeoutMs + 1000,
    ) {
        let timerId;
        const evaluatePromise = Runtime.evaluate({
            expression,
            awaitPromise,
            returnByValue: true,
        });
        const timeoutPromise = new Promise((_, reject) => {
            timerId = setTimeout(() => {
                const error = new Error(
                    `CDP Runtime.evaluate timed out after ${timeoutMs} ms`,
                );
                error.code = "CDP_EVALUATE_TIMEOUT";
                reject(error);
            }, timeoutMs);
        });

        let response;
        try {
            response = await Promise.race([evaluatePromise, timeoutPromise]);
        } finally {
            clearTimeout(timerId);
        }
        if (response.exceptionDetails) {
            throw new Error(
                response.exceptionDetails.exception?.description ||
                    response.exceptionDetails.text ||
                    "Runtime.evaluate failed",
            );
        }
        return response.result.value;
    }

    async function invokeRaw(method, params, timeoutMs = invokeTimeoutMs) {
        try {
            return await evaluateValue(
                invokeExpression(method, params, timeoutMs),
                true,
                timeoutMs + 1000,
            );
        } catch (error) {
            if (error?.code === "CDP_EVALUATE_TIMEOUT") {
                return { kind: "cdp-timeout", timeoutMs: timeoutMs + 1000 };
            }
            throw error;
        }
    }

    async function invoke(method, params) {
        const outcome = await invokeRaw(method, params);
        if (outcome?.kind === "timeout") {
            throw new Error(`${method} timed out after ${outcome.timeoutMs} ms`);
        }
        if (outcome?.kind === "error") {
            throw new Error(`${method} rejected: ${outcome.error}`);
        }
        if (!outcome || outcome.kind !== "result") {
            throw new Error(`${method} returned an invalid probe envelope`);
        }
        return outcome.value;
    }

    return { invokeTimeoutMs, evaluateValue, invokeRaw, invoke };
}

/**
 * Separates "the call never left the page" from "the host never answered":
 * launches a read-only file.exists without awaiting it, then polls the page for
 * the settled state. stage is postMessage or response when not responsive.
 */
export async function probeBridgeResponsiveness(bridge, { probeId } = {}) {
    const probeKey = `__fb2kE2eProbe_${probeId ?? Date.now()}`;
    const launchExpression = `(() => {
        const key = ${JSON.stringify(probeKey)};
        window[key] = { status: 'pending', startedAt: Date.now() };
        window.fb2k.invoke('file.exists', { path: '%TEMP%' }).then(
            value => { window[key] = { status: 'resolved', value, settledAt: Date.now() }; },
            error => { window[key] = { status: 'rejected', error: String(error && error.message ? error.message : error), settledAt: Date.now() }; }
        );
        return {
            launched: true,
            callId: window.fb2k._callId,
            callbacks: window.fb2k._callbacks?.size ?? null
        };
    })()`;

    let launch;
    try {
        launch = await bridge.evaluateValue(
            launchExpression,
            false,
            bridge.invokeTimeoutMs + 1000,
        );
    } catch (error) {
        return {
            responsive: false,
            stage: "postMessage",
            error: String(error?.message || error),
        };
    }

    const deadline = Date.now() + bridge.invokeTimeoutMs;
    let state;
    do {
        await new Promise((resolve) => setTimeout(resolve, 100));
        state = await bridge.evaluateValue(
            `(() => ({
                probe: window[${JSON.stringify(probeKey)}] || null,
                callId: window.fb2k?._callId ?? null,
                callbacks: window.fb2k?._callbacks?.size ?? null
            }))()`,
            false,
            1000,
        );
        if (state?.probe?.status !== "pending") break;
    } while (Date.now() < deadline);

    await bridge
        .evaluateValue(`delete window[${JSON.stringify(probeKey)}]; true`, false, 1000)
        .catch(() => undefined);

    if (state?.probe?.status === "resolved") {
        return { responsive: true, launch, state };
    }
    return { responsive: false, stage: "response", launch, state };
}

/** Throws a blocked error when the bridge cannot answer a read-only probe. */
export async function requireResponsiveBridge(bridge, options) {
    const probe = await probeBridgeResponsiveness(bridge, options);
    if (!probe.responsive) {
        throw blockedError(
            `file.exists preflight blocked at ${probe.stage}: ${JSON.stringify(probe)}`,
            probe,
        );
    }
    return probe;
}

/**
 * Buffers host events in the page so a script can trigger an action and then
 * assert what arrived.
 *
 * Events reach the page asynchronously, after the invoke that caused them has
 * already resolved, so subscribing has to happen before the trigger and reading
 * has to be able to wait. waitFor polls the page buffer and accumulates
 * everything seen so far; received keeps the full ordered history, which is what
 * makes event sequence assertions possible.
 */
export async function createEventCollector(bridge, eventNames, { collectorId } = {}) {
    const key = `__fb2kE2eEvents_${collectorId ?? Date.now()}`;
    const keyJson = JSON.stringify(key);

    await bridge.evaluateValue(
        `(() => {
            const store = { events: [], handlers: [] };
            for (const name of ${JSON.stringify(eventNames)}) {
                const handler = (payload) => {
                    store.events.push({ name, at: Date.now(), payload });
                };
                window.fb2k.on(name, handler);
                store.handlers.push([name, handler]);
            }
            window[${keyJson}] = store;
            return store.handlers.length;
        })()`,
        false,
        3000,
    );

    const received = [];

    async function drain() {
        const batch = await bridge.evaluateValue(
            `(() => {
                const store = window[${keyJson}];
                if (!store) return [];
                const events = store.events;
                store.events = [];
                return events;
            })()`,
            false,
            3000,
        );
        if (Array.isArray(batch) && batch.length > 0) {
            received.push(...batch);
        }
        return received;
    }

    async function waitFor(until, { timeoutMs = 2000, pollMs = 50 } = {}) {
        const deadline = Date.now() + timeoutMs;
        for (;;) {
            await drain();
            if (!until || until(received)) return received;
            if (Date.now() >= deadline) return received;
            await new Promise((resolve) => setTimeout(resolve, pollMs));
        }
    }

    async function stop() {
        await bridge
            .evaluateValue(
                `(() => {
                    const store = window[${keyJson}];
                    if (!store) return false;
                    for (const [name, handler] of store.handlers) {
                        window.fb2k.off(name, handler);
                    }
                    delete window[${keyJson}];
                    return true;
                })()`,
                false,
                3000,
            )
            .catch(() => undefined);
    }

    return { key, received, drain, waitFor, stop };
}

export function createRecorder({ log = console.log } = {}) {
    const cases = [];

    function addCase(name, passed, observed, expected) {
        cases.push({ name, passed, observed, expected });
        log(`[${passed ? "PASS" : "FAIL"}] ${name}`);
        if (!passed) {
            log(`  expected: ${JSON.stringify(expected)}`);
            log(`  observed: ${JSON.stringify(observed)}`);
        }
    }

    function assertCase(name, condition, observed, expected) {
        addCase(name, Boolean(condition), observed, expected);
    }

    return { cases, addCase, assertCase };
}

export function summarize({ recorder, blocked = false, fatalError, extra = {} }) {
    const cases = recorder?.cases ?? [];
    const failed = cases.filter((item) => !item.passed);
    return {
        ...extra,
        status: blocked
            ? "blocked"
            : fatalError
              ? "error"
              : failed.length
                ? "failed"
                : "passed",
        passed: cases.length - failed.length,
        failed: failed.length,
        cases,
        fatalError: fatalError ? String(fatalError.message || fatalError) : undefined,
    };
}

export function exitCodeFor(summary) {
    if (summary.status === "blocked") return EXIT_BLOCKED;
    return summary.status === "passed" ? EXIT_PASSED : EXIT_FAILED;
}

/**
 * Prints the JSON summary and returns the exit code the script should use.
 *
 * When FB2K_E2E_RESULTS_DIR is set (run-e2e.mjs sets it for every suite it
 * spawns), the same summary is also written to <dir>/<suite>.json, so a run's
 * outcome survives the terminal it was printed in. A suite started by hand
 * without the variable only prints. Failing to write the file is reported on
 * stderr and does not change the exit code: persistence must never turn a
 * green run red.
 */
export function report(options) {
    const summary = summarize(options);
    console.log(JSON.stringify(summary, null, 2));
    const dir = process.env.FB2K_E2E_RESULTS_DIR;
    if (dir) {
        try {
            mkdirSync(dir, { recursive: true });
            const suite = basename(process.argv[1] ?? "suite", ".mjs");
            writeFileSync(join(dir, `${suite}.json`), JSON.stringify(summary, null, 2) + "\n");
        } catch (error) {
            console.error(`note: could not persist the summary: ${error?.message ?? error}`);
        }
    }
    return exitCodeFor(summary);
}

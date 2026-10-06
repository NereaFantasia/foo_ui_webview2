/**
 * Checks the two page-refresh endpoints of the window namespace and, above all,
 * that they are not the same operation.
 *
 * refreshWebView nudges the WebView2 controller bounds by one pixel and commits
 * the composition again; it never navigates, so page JavaScript state survives.
 * reload is ICoreWebView2::Reload: the document is replaced in place, page state
 * is gone, and window.fb2k comes back only once the document-created script has
 * run again. Both answer {success:true} before any of that has happened, so the
 * only oracle that tells them apart is a sentinel written into the page before
 * each call: it must survive refreshWebView and vanish after reload.
 *
 * reload destroys every listener and buffer the other suites leave in the page,
 * which is why this script must be the last entry in run-e2e.mjs.
 *
 * It skips reload when the instance is streaming through the JIT queue: the
 * reload can drop a pending hand-off and cause an audible gap even if the queue
 * state machine recovers afterwards.
 *
 * Usage: node mcp/tests/e2e-window-reload.mjs
 */

import CDP from "chrome-remote-interface";

import {
    closeClient,
    connectBridgePage,
    createBridge,
    createRecorder,
    envInt,
    report,
    requireResponsiveBridge,
    resolvePort,
} from "./lib/e2e-harness.mjs";

const invokeTimeoutMs = envInt("FB2K_E2E_TIMEOUT_MS", 8000);
const reloadBudgetMs = envInt("FB2K_E2E_RELOAD_BUDGET_MS", 15000);
const runId = new Date().toISOString().replace(/[-:.TZ]/g, "");
const sentinelValue = `ws-${runId}`;

const SENTINEL_READ = "(() => window.__wsSentinel === undefined ? null : String(window.__wsSentinel))()";
const BRIDGE_READY =
    "typeof window.fb2k === 'object' && typeof window.fb2k.invoke === 'function'";

async function listPageTargets(port) {
    const targets = await CDP.List({ port });
    return targets.filter((target) => target.type === "page");
}

function findTarget(targets, url) {
    return targets.find((target) => target.url === url) ?? null;
}

/**
 * Evaluates an expression while the page may be mid-navigation. The execution
 * context is torn down and rebuilt during reload, so a single evaluate can fail
 * with "context destroyed"; that is reported as null rather than thrown so the
 * caller can keep polling.
 */
async function evaluateOrNull(bridge, expression) {
    try {
        return await bridge.evaluateValue(expression, false, 2000);
    } catch {
        return null;
    }
}

/**
 * Polls until the sentinel is gone and the bridge is callable again, returning
 * how long each took. On timeout it returns what it last saw so the case fails
 * with the observed state rather than a bare timeout.
 */
async function waitForReloadedPage(bridge, { budgetMs, pollMs = 50 }) {
    const startedAt = Date.now();
    let sentinelGoneAt = null;
    let bridgeReadyAt = null;
    let lastSentinel;
    let lastReady;
    while (Date.now() - startedAt < budgetMs) {
        lastSentinel = await evaluateOrNull(bridge, SENTINEL_READ);
        if (lastSentinel === null && sentinelGoneAt === null) {
            sentinelGoneAt = Date.now() - startedAt;
        }
        lastReady = await evaluateOrNull(bridge, BRIDGE_READY);
        if (sentinelGoneAt !== null && lastReady === true) {
            bridgeReadyAt = Date.now() - startedAt;
            break;
        }
        await new Promise((resolve) => setTimeout(resolve, pollMs));
    }
    return {
        sentinelGoneMs: sentinelGoneAt,
        bridgeReadyMs: bridgeReadyAt,
        lastSentinel,
        lastReady,
        budgetMs,
    };
}

const recorder = createRecorder();
let client;
let bridge;
let blocked = false;
let fatalError;
let targets;
let pageUrl;
let observedTiming;

try {
    const connection = await connectBridgePage();
    client = connection.client;
    targets = connection.candidates;
    pageUrl = connection.page.url;
    console.log(`Target: ${connection.page.title} ${connection.page.url}`);
    bridge = createBridge(client.Runtime, { invokeTimeoutMs });
    await requireResponsiveBridge(bridge, { probeId: runId });

    const { invoke, invokeRaw, evaluateValue } = bridge;
    const port = resolvePort();

    const [playbackBefore, jitBefore] = await Promise.all([
        invoke("playback.getState", {}),
        invoke("jitQueue.getState", {}),
    ]);
    const targetBefore = findTarget(await listPageTargets(port), pageUrl);

    // refreshWebView: the sentinel must survive, and the target must not change.
    await evaluateValue(`window.__wsSentinel = ${JSON.stringify(sentinelValue)}; true`, false, 2000);
    const refreshed = await invokeRaw("window.refreshWebView", {});
    const sentinelAfterRefresh = await evaluateValue(SENTINEL_READ, false, 2000);
    const targetAfterRefresh = findTarget(await listPageTargets(port), pageUrl);
    recorder.assertCase(
        "WR-01 refreshWebView answers success and leaves page state and target untouched",
        refreshed?.kind === "result" &&
            refreshed.value?.success === true &&
            Object.keys(refreshed.value).length === 1 &&
            sentinelAfterRefresh === sentinelValue &&
            targetAfterRefresh?.id === targetBefore?.id,
        {
            envelope: refreshed,
            sentinelAfterRefresh,
            targetIdBefore: targetBefore?.id,
            targetIdAfter: targetAfterRefresh?.id,
        },
        {
            envelope: { success: true },
            sentinelAfterRefresh: sentinelValue,
            sameTargetId: true,
        },
    );

    // reload: the sentinel must vanish and window.fb2k must come back. Skipped
    // when the JIT queue is mid-stream, because the hand-off in flight is lost.
    const jitStreaming = jitBefore?.isActive === true;
    if (jitStreaming) {
        recorder.assertCase(
            "WR-02 reload is skipped while the JIT queue is streaming for the listener",
            true,
            { jitState: jitBefore, playbackState: playbackBefore?.state },
            { skipped: true, reason: "reload would drop the JIT hand-off in flight" },
        );
    } else {
        const reloaded = await invokeRaw("window.reload", {});
        observedTiming = await waitForReloadedPage(bridge, { budgetMs: reloadBudgetMs });
        const targetAfterReload = findTarget(await listPageTargets(port), pageUrl);
        recorder.assertCase(
            "WR-02 reload answers success, replaces page state and re-exposes window.fb2k",
            reloaded?.kind === "result" &&
                reloaded.value?.success === true &&
                Object.keys(reloaded.value).length === 1 &&
                observedTiming.sentinelGoneMs !== null &&
                observedTiming.bridgeReadyMs !== null,
            {
                envelope: reloaded,
                timing: observedTiming,
                targetIdBefore: targetBefore?.id,
                targetIdAfter: targetAfterReload?.id,
                jitState: jitBefore?.state,
                playbackState: playbackBefore?.state,
            },
            {
                envelope: { success: true },
                sentinelGone: true,
                bridgeReadyWithinMs: reloadBudgetMs,
            },
        );

        // The target is expected to survive the reload; either way the reading
        // is recorded because the retry budget of the other suites depends on it.
        recorder.assertCase(
            "WR-03 reload keeps the same DevTools target",
            targetAfterReload?.id === targetBefore?.id,
            { targetIdBefore: targetBefore?.id, targetIdAfter: targetAfterReload?.id },
            { sameTargetId: true },
        );

        const afterReloadId = await invokeRaw("window.getCurrentWindowId", {});
        const playbackAfter = await invokeRaw("playback.getState", {});
        recorder.assertCase(
            "WR-04 the bridge answers again after reload and playback state is unchanged",
            afterReloadId?.kind === "result" &&
                afterReloadId.value?.windowId === "main" &&
                playbackAfter?.kind === "result" &&
                playbackAfter.value?.state === playbackBefore?.state,
            {
                windowId: afterReloadId,
                playbackBefore: playbackBefore?.state,
                playbackAfter: playbackAfter?.value?.state,
            },
            { windowId: "main", playbackUnchanged: true },
        );
    }
} catch (error) {
    fatalError = error;
    blocked = Boolean(error?.blocked);
    console.error(`${blocked ? "BLOCKED" : "ERROR"}: ${error?.message || error}`);
} finally {
    if (bridge) {
        await bridge
            .evaluateValue("delete window.__wsSentinel; true", false, 1000)
            .catch(() => undefined);
    }
    await closeClient(client);
}

process.exit(
    report({
        recorder,
        blocked,
        fatalError,
        extra: {
            runId,
            targetPort: resolvePort(),
            invokeTimeoutMs,
            reloadBudgetMs,
            reloadTiming: observedTiming,
            targets,
        },
    }),
);

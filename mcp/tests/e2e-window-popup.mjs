/**
 * Covers the popup half of the window namespace: creating and closing popups,
 * their behaviour policy, click-through and its exclude regions, the
 * beforeClose handshake, and the two cross-window message endpoints.
 *
 * Five groups, in an order that is not negotiable:
 *
 * 1. Zero-disturbance cases first. Everything here either touches only the
 *    main window's own page (sendMessage to itself, broadcast on a single
 *    window) or exercises a refusal branch that returns before any window is
 *    touched. closeAllPopups is only ever called here, while the popup count
 *    is still zero: with popups present it would close windows the user opened
 *    (mini player, desktop lyrics) and nothing can bring them back.
 * 2. One popup's lifecycle, created by this suite and closed before the group
 *    ends. click-through leaves a one-way latch (overlayIntent_) that
 *    setClickThrough(false) cannot clear, so the only clean reset is closing
 *    the window - which is why the suite never borrows a popup it did not make.
 * 3. The beforeClose handshake, which needs a second CDP session attached to
 *    the popup's own page: cancelClose and confirmClose resolve the caller by
 *    HWND and, from the main window, always answer "Window not found".
 * 4. The development server fallback, because it rewrites a persisted
 *    setting: the server address points at a dead loopback port while one
 *    popup opens, and the original setting is written back afterwards.
 * 5. The origin gate. A local server on 127.0.0.2, which nothing in the host
 *    trusts, stands in for a remote site: a popup opened there loads but gets
 *    ORIGIN_DENIED and no events, a URL that puts the popup's own origin in
 *    front of an "@" gets the same, and an absolute URL the opener trusts
 *    keeps the bridge.
 *
 * Groups 2 to 5 run only when getAllWindows lists no popup at all; the bridge
 * cannot tell a user's popup from a stale one, so a non-zero count skips the
 * group and records the reading.
 *
 * Everything observable is pinned as it is, including the gaps: success from
 * confirmClose/cancelClose with nothing pending is a no-op; the truncation
 * warning on exclude regions is
 * computed from the raw array length while truncation happens after invalid
 * entries are dropped; a wrong-typed behaviour flag is stored verbatim and
 * displaces the boolean override it replaces.
 *
 * Usage: node mcp/tests/e2e-window-popup.mjs
 * Environment: FB2K_CDP_PORT, FB2K_E2E_TIMEOUT_MS, FB2K_EVENT_TIMEOUT_MS,
 * FB2K_POPUP_READY_TIMEOUT_MS (how long a new popup may take to expose a
 * usable bridge on the DevTools port, default 20000).
 */

import { createServer as createHttpServer } from "node:http";
import { createServer } from "node:net";

import CDP from "chrome-remote-interface";

import {
    blockedError,
    closeClient,
    connectBridgePage,
    createBridge,
    createEventCollector,
    createRecorder,
    envInt,
    report,
    requireResponsiveBridge,
    resolvePort,
} from "./lib/e2e-harness.mjs";

const invokeTimeoutMs = envInt("FB2K_E2E_TIMEOUT_MS", 8000);
const eventTimeoutMs = envInt("FB2K_EVENT_TIMEOUT_MS", 3000);
const popupReadyTimeoutMs = envInt("FB2K_POPUP_READY_TIMEOUT_MS", 20000);
const runId = new Date().toISOString().replace(/[-:.TZ]/g, "");

const POPUP_WIDTH = 240;
const POPUP_HEIGHT = 180;
const POPUP_TITLE = `foo_ui_webview2 e2e popup ${runId}`;
// The popup loads the user's active front-end template, which this suite
// cannot audit; pinning the size from both sides keeps a template that
// restores its own bounds on start-up from blowing the test window up.
const POPUP_GEOMETRY = {
    width: POPUP_WIDTH,
    height: POPUP_HEIGHT,
    maxWidth: POPUP_WIDTH,
    maxHeight: POPUP_HEIGHT,
    resizable: false,
};
const MISSING_POPUP_ID = "popup_999999";
const POPUP_ID_PATTERN = /^popup_\d+$/;
const EXCLUDE_REGION_LIMIT = 32;
// PopupWindow::CLOSE_TIMEOUT_MS: a pending beforeClose is force-closed after
// this many milliseconds, so the cancel handshake has to land inside it.
const BEFORE_CLOSE_BUDGET_MS = 3000;
const NEGATIVE_SILENCE_MS = 300;
const POPUP_GONE_TIMEOUT_MS = 6000;

const MAIN_EVENT_NAMES = [
    "window:message",
    "window:popupOpened",
    "window:popupClosed",
    "window:behaviorChanged",
    "window:hoverStateChanged",
];
const POPUP_EVENT_NAMES = ["window:message", "window:beforeClose"];

const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms));

function named(events, name) {
    return events.filter((entry) => entry.name === name);
}

function forWindow(events, name, windowId) {
    return named(events, name).filter((entry) => entry.payload?.windowId === windowId);
}

function popupItems(allWindows) {
    const items = Array.isArray(allWindows?.items) ? allWindows.items : [];
    return items.filter((item) => item?.isMain !== true);
}

function findItem(allWindows, windowId) {
    const items = Array.isArray(allWindows?.items) ? allWindows.items : [];
    return items.find((item) => item?.windowId === windowId);
}

function sameJson(a, b) {
    return JSON.stringify(a) === JSON.stringify(b);
}

function isNotFound(outcome) {
    return (
        outcome?.value?.success === false &&
        outcome.value.error === "Window not found" &&
        outcome.value.code === "NOT_FOUND"
    );
}

function isInvalidParams(outcome, error) {
    return (
        outcome?.value?.success === false &&
        outcome.value.code === "INVALID_PARAMS" &&
        (error === undefined || outcome.value.error === error)
    );
}

/**
 * Reads until the predicate holds or the budget runs out, returning the last
 * reading either way so a failed case shows what it actually saw.
 */
async function pollUntil(read, predicate, { timeoutMs, intervalMs = 50 }) {
    const startedAt = Date.now();
    let attempts = 0;
    let value;
    for (;;) {
        attempts += 1;
        value = await read();
        const satisfied = Boolean(predicate(value));
        const elapsedMs = Date.now() - startedAt;
        if (satisfied || elapsedMs >= timeoutMs) {
            return { value, attempts, elapsedMs, satisfied };
        }
        await sleep(intervalMs);
    }
}

async function waitForPopupGone(bridge, windowId, timeoutMs = POPUP_GONE_TIMEOUT_MS) {
    return pollUntil(
        () => bridge.invoke("window.getAllWindows", {}),
        (all) => !findItem(all, windowId),
        { timeoutMs, intervalMs: 100 },
    );
}

async function readPendingDestroy(bridge, windowId) {
    const all = await bridge.invoke("window.getAllWindows", {});
    const item = findItem(all, windowId);
    return { listed: Boolean(item), pendingDestroy: item?.shell?.lifecycle?.pendingDestroy };
}

/**
 * Picks a screen corner for the test popup from the page's own screen object,
 * since the bridge has no monitor endpoint. Physical pixels, bottom-right,
 * inset a little so the frame stays on screen. Falls back to letting the
 * window manager centre it when the reading looks unusable.
 */
async function cornerPlacement(bridge) {
    let info = null;
    try {
        info = await bridge.evaluateValue(
            "({ left: screen.availLeft, top: screen.availTop, width: screen.availWidth, height: screen.availHeight, dpr: window.devicePixelRatio })",
            false,
            3000,
        );
    } catch {
        return { placement: {}, screen: null };
    }
    const dpr = Number(info?.dpr) || 1;
    const right = Math.round((Number(info?.left) + Number(info?.width)) * dpr);
    const bottom = Math.round((Number(info?.top) + Number(info?.height)) * dpr);
    if (
        !Number.isFinite(right) ||
        !Number.isFinite(bottom) ||
        right < POPUP_WIDTH + 16 ||
        bottom < POPUP_HEIGHT + 16
    ) {
        return { placement: {}, screen: info };
    }
    return {
        placement: { x: right - POPUP_WIDTH - 8, y: bottom - POPUP_HEIGHT - 8 },
        screen: info,
    };
}

/**
 * Attaches a second CDP session to a popup's own page.
 *
 * Popups share the main window's WebView2 environment and DevTools port, so
 * the new page shows up in CDP.List; the only thing that identifies it is the
 * windowId query parameter that BuildNavigationUrl appends. The bridge script
 * is injected on document creation, but the page is listed before it has
 * navigated, so both the listing and the bridge have to be polled. The last
 * step - getCurrentWindowId answering with the popup's id - is what proves the
 * session is really talking to that popup and the host resolves its caller.
 */
async function connectPopupPage(port, popupId, timeoutMs) {
    const startedAt = Date.now();
    const deadline = startedAt + timeoutMs;
    const urlPattern = new RegExp(`[?&]windowId=${popupId}(?:[&#]|$)`);
    let page;
    let seen = [];
    while (!page && Date.now() < deadline) {
        const targets = await CDP.List({ port });
        seen = targets
            .filter((target) => target.type === "page")
            .map((target) => String(target.url || "").slice(0, 120));
        page = targets.find(
            (target) => target.type === "page" && urlPattern.test(String(target.url || "")),
        );
        if (!page) await sleep(100);
    }
    if (!page) {
        return { stage: "target", seen, waitedMs: Date.now() - startedAt };
    }

    const client = await CDP({ port, target: page });
    const bridge = createBridge(client.Runtime, { invokeTimeoutMs });

    let hasBridge = false;
    while (!hasBridge && Date.now() < deadline) {
        try {
            hasBridge =
                (await bridge.evaluateValue(
                    "typeof window.fb2k === 'object' && typeof window.fb2k.invoke === 'function'",
                    false,
                    2000,
                )) === true;
        } catch {
            hasBridge = false;
        }
        if (!hasBridge) await sleep(100);
    }
    if (!hasBridge) {
        await closeClient(client);
        return { stage: "bridge", page, seen, waitedMs: Date.now() - startedAt };
    }

    // The page may still be navigating when the bridge first shows up, and an
    // evaluate that lands in a torn-down context throws; that is a retry here.
    let identity;
    while (Date.now() < deadline) {
        try {
            identity = await bridge.invokeRaw("window.getCurrentWindowId", {});
        } catch (error) {
            identity = { kind: "error", error: String(error?.message || error) };
        }
        if (identity?.value?.windowId === popupId) break;
        await sleep(100);
    }
    return {
        stage: identity?.value?.windowId === popupId ? "ready" : "identity",
        client,
        bridge,
        page,
        identity,
        seen,
        waitedMs: Date.now() - startedAt,
    };
}

async function runZeroDisturbanceCases(bridge, recorder, events, selfId, entryPopups) {
    const { invoke, invokeRaw } = bridge;

    // closeAllPopups discards how many windows it closed and cannot restore
    // them, so the only branch this suite may exercise is the one where there
    // is nothing to close. The count is read right here, not at suite entry.
    if (entryPopups.length === 0) {
        const closedNone = await invoke("window.closeAllPopups", {});
        recorder.assertCase(
            "WP-01 closeAllPopups with no popup open answers a bare success, which is all its envelope ever says",
            closedNone?.success === true && Object.keys(closedNone).length === 1,
            { closedNone, popupCount: 0 },
            { success: true, otherFields: "none" },
        );
    } else {
        recorder.addCase(
            "WP-01 closeAllPopups with no popup open answers a bare success, which is all its envelope ever says",
            true,
            {
                skipped: "popups are open that this suite did not create; closeAllPopups would close them for good",
                popupCount: entryPopups.length,
                popupIds: entryPopups.map((item) => item?.windowId),
            },
            { skipped: "needs zero popups" },
        );
    }

    await events.drain();
    const toSelf = await invoke("window.sendMessage", {
        targetWindowId: selfId,
        message: { tag: runId, kind: "self" },
    });
    await events.waitFor(
        (all) => named(all, "window:message").some((e) => e.payload?.message?.tag === runId),
        { timeoutMs: eventTimeoutMs },
    );
    const selfMessage = named(events.received, "window:message").find(
        (entry) => entry.payload?.message?.tag === runId,
    );
    recorder.assertCase(
        "WP-02 sendMessage can target the caller's own window, and arrives as window:message wrapped in { sourceWindowId, message }",
        toSelf?.success === true &&
            Object.keys(toSelf).length === 1 &&
            selfMessage?.payload?.sourceWindowId === selfId &&
            selfMessage.payload.message?.kind === "self" &&
            sameJson(Object.keys(selfMessage.payload).sort(), ["message", "sourceWindowId"]),
        { toSelf, delivered: selfMessage?.payload },
        {
            toSelf: { success: true },
            delivered: { sourceWindowId: selfId, message: { tag: runId, kind: "self" } },
        },
    );

    const noParams = await invokeRaw("window.sendMessage", {});
    const noTarget = await invokeRaw("window.sendMessage", { message: { tag: runId } });
    const noMessage = await invokeRaw("window.sendMessage", { targetWindowId: selfId });
    const unknownTarget = await invokeRaw("window.sendMessage", {
        targetWindowId: MISSING_POPUP_ID,
        message: { tag: runId },
    });
    // The parameter reader checks targetWindowId before message, so a call
    // missing both is told only about the target.
    recorder.assertCase(
        "WP-03 sendMessage checks the target before the message, refuses a missing one with INVALID_PARAMS, and an unknown target with NOT_FOUND",
        isInvalidParams(noParams, "targetWindowId is required") &&
            isInvalidParams(noTarget, "targetWindowId is required") &&
            isInvalidParams(noMessage, "message is required") &&
            unknownTarget?.value?.success === false &&
            unknownTarget.value.error === "Target window not found" &&
            unknownTarget.value.code === "NOT_FOUND",
        {
            noParams: noParams?.value,
            noTarget: noTarget?.value,
            noMessage: noMessage?.value,
            unknownTarget: unknownTarget?.value,
        },
        {
            noParams: { success: false, error: "targetWindowId is required", code: "INVALID_PARAMS" },
            noTarget: { error: "targetWindowId is required", code: "INVALID_PARAMS" },
            noMessage: { error: "message is required", code: "INVALID_PARAMS" },
            unknownTarget: { error: "Target window not found", code: "NOT_FOUND" },
        },
    );

    // broadcast excludes the sender, so on a single-window instance the only
    // observable is that nothing comes back; the wait is what makes that a
    // real negative. Unlike port.postMessage (BI-12) it reports no recipients.
    await events.drain();
    const broadcastTag = `${runId}:broadcast-self`;
    const broadcast = await invoke("window.broadcast", { message: { tag: broadcastTag } });
    await sleep(NEGATIVE_SILENCE_MS);
    await events.drain();
    const echoed = named(events.received, "window:message").filter(
        (entry) => entry.payload?.message?.tag === broadcastTag,
    );
    const broadcastNoMessage = await invokeRaw("window.broadcast", {});
    recorder.assertCase(
        "WP-04 broadcast answers a bare success with no recipients count, does not deliver to the sender, and refuses a missing message",
        broadcast?.success === true &&
            !("recipients" in broadcast) &&
            Object.keys(broadcast).length === 1 &&
            echoed.length === 0 &&
            isInvalidParams(broadcastNoMessage, "message is required"),
        {
            broadcast,
            recipientsFieldPresent: "recipients" in (broadcast ?? {}),
            deliveredToSender: echoed.length,
            broadcastNoMessage: broadcastNoMessage?.value,
        },
        {
            broadcast: { success: true, recipientsField: "absent" },
            deliveredToSender: 0,
            broadcastNoMessage: { error: "message is required", code: "INVALID_PARAMS" },
        },
    );

    // The popup registry never contains "main", so a main-window caller with
    // no windowId falls through to "Window not found"; an explicit "main" is
    // NOT_SUPPORTED with its own wording, pinned by shape only (it mentions the
    // main window).
    const getSelf = await invokeRaw("window.getPopupBehavior", {});
    const getMain = await invokeRaw("window.getPopupBehavior", { windowId: "main" });
    const getUnknown = await invokeRaw("window.getPopupBehavior", { windowId: MISSING_POPUP_ID });
    recorder.assertCase(
        "WP-05 getPopupBehavior from the main window is refused: no id and an unknown id are 'Window not found', an explicit main gets a dedicated message",
        isNotFound(getSelf) &&
            isNotFound(getUnknown) &&
            getMain?.value?.success === false &&
            getMain.value.code === "NOT_SUPPORTED" &&
            typeof getMain.value.error === "string" &&
            /main/i.test(getMain.value.error) &&
            !("resolvedBehavior" in getMain.value),
        { getSelf: getSelf?.value, getMain: getMain?.value, getUnknown: getUnknown?.value },
        {
            getSelf: { success: false, error: "Window not found" },
            getMain: { success: false, error: "mentions main", code: "NOT_SUPPORTED" },
            getUnknown: { success: false, error: "Window not found" },
        },
    );

    const setSelf = await invokeRaw("window.setPopupBehavior", { behavior: { noActivate: true } });
    const setMain = await invokeRaw("window.setPopupBehavior", {
        windowId: "main",
        behavior: { noActivate: true },
    });
    const setUnknown = await invokeRaw("window.setPopupBehavior", {
        windowId: MISSING_POPUP_ID,
        behavior: { noActivate: true },
    });
    recorder.assertCase(
        "WP-06 setPopupBehavior from the main window is refused the same three ways, and touches nothing",
        isNotFound(setSelf) &&
            isNotFound(setUnknown) &&
            setMain?.value?.success === false &&
            setMain.value.code === "NOT_SUPPORTED" &&
            typeof setMain.value.error === "string" &&
            /main/i.test(setMain.value.error) &&
            !("resolvedBehavior" in setMain.value),
        { setSelf: setSelf?.value, setMain: setMain?.value, setUnknown: setUnknown?.value },
        {
            setSelf: { success: false, error: "Window not found" },
            setMain: { success: false, error: "mentions main", code: "NOT_SUPPORTED" },
            setUnknown: { success: false, error: "Window not found" },
        },
    );

    const clickThroughRefusals = {
        setClickThroughSelf: await invokeRaw("window.setClickThrough", { enabled: true }),
        setClickThroughMain: await invokeRaw("window.setClickThrough", {
            windowId: "main",
            enabled: true,
        }),
        isClickThroughSelf: await invokeRaw("window.isClickThrough", {}),
        isClickThroughMain: await invokeRaw("window.isClickThrough", { windowId: "main" }),
        setRegionsSelf: await invokeRaw("window.setClickThroughExcludeRegions", {
            regions: [{ x: 0, y: 0, width: 10, height: 10 }],
        }),
        setRegionsMain: await invokeRaw("window.setClickThroughExcludeRegions", {
            windowId: "main",
            regions: [{ x: 0, y: 0, width: 10, height: 10 }],
        }),
        clearRegionsSelf: await invokeRaw("window.clearClickThroughExcludeRegions", {}),
        clearRegionsMain: await invokeRaw("window.clearClickThroughExcludeRegions", {
            windowId: "main",
        }),
    };
    const refusalValues = Object.fromEntries(
        Object.entries(clickThroughRefusals).map(([key, outcome]) => [key, outcome?.value]),
    );
    recorder.assertCase(
        "WP-07 the four click-through endpoints only know popups: from the main window, with or without windowId 'main', all answer 'Window not found' and none of their success fields",
        Object.values(clickThroughRefusals).every(
            (outcome) =>
                isNotFound(outcome) &&
                !("clickThrough" in outcome.value) &&
                !("count" in outcome.value) &&
                !("dpiScale" in outcome.value),
        ),
        refusalValues,
        { each: { success: false, error: "Window not found", successFields: "absent" } },
    );

    // Both walk the popup table matching the caller's HWND and skip "main";
    // the main window has no beforeClose flow at all, so neither can close it.
    const confirmFromMain = await invokeRaw("window.confirmClose", {});
    const cancelFromMain = await invokeRaw("window.cancelClose", {});
    recorder.assertCase(
        "WP-08 confirmClose and cancelClose from the main window answer 'Window not found', and the main window stays",
        isNotFound(confirmFromMain) && isNotFound(cancelFromMain),
        { confirmFromMain: confirmFromMain?.value, cancelFromMain: cancelFromMain?.value },
        { each: { success: false, error: "Window not found" } },
    );

    const closeNoId = await invokeRaw("window.closePopup", {});
    const closeMain = await invokeRaw("window.closePopup", { windowId: "main" });
    const closeUnknown = await invokeRaw("window.closePopup", { windowId: MISSING_POPUP_ID });
    recorder.assertCase(
        "WP-09 closePopup refuses a missing id and 'main' with INVALID_PARAMS, and reports an unknown id as not found",
        isInvalidParams(closeNoId, "windowId is required") &&
            isInvalidParams(closeMain, "Cannot close main window via closePopup") &&
            isNotFound(closeUnknown),
        { closeNoId: closeNoId?.value, closeMain: closeMain?.value, closeUnknown: closeUnknown?.value },
        {
            closeNoId: { error: "windowId is required", code: "INVALID_PARAMS" },
            closeMain: { error: "Cannot close main window via closePopup", code: "INVALID_PARAMS" },
            closeUnknown: { error: "Window not found" },
        },
    );

    // The parameter reader refuses both before the handler runs, so nothing is
    // created; the window count is read back to prove it.
    const badBehavior = await invokeRaw("window.createPopup", { behavior: "not-an-object" });
    const badBackdrop = await invokeRaw("window.createPopup", { backdropPolicy: 1 });
    const afterRefusals = await invoke("window.getAllWindows", {});
    recorder.assertCase(
        "WP-10 createPopup refuses a non-object behavior or backdropPolicy with INVALID_PARAMS before creating a window",
        isInvalidParams(badBehavior, "behavior must be an object") &&
            !("windowId" in badBehavior.value) &&
            isInvalidParams(badBackdrop, "backdropPolicy must be an object") &&
            popupItems(afterRefusals).length === entryPopups.length,
        {
            badBehavior: badBehavior?.value,
            badBackdrop: badBackdrop?.value,
            popupCountAfter: popupItems(afterRefusals).length,
            popupCountBefore: entryPopups.length,
        },
        {
            badBehavior: { error: "behavior must be an object", code: "INVALID_PARAMS" },
            badBackdrop: { error: "backdropPolicy must be an object", code: "INVALID_PARAMS" },
            popupCountUnchanged: true,
        },
    );
}

// Used only to name the cases in the skipped branches; the live assertions
// carry the same names inline so the coverage ledger can attribute them.
const LIFECYCLE_CASE_NAMES = [
    "WP-11 createPopup answers a popup_N id that getAllWindows lists at once with popup capabilities and the given title, and window:popupOpened follows",
    "WP-12 sendMessage to a popup that has just been created may race its WebView registration, and succeeds once polled",
    "WP-13 getPopupBehavior reports the legacy profile, the raw behavior override, and all six resolved fields",
    "WP-14 setPopupBehavior merges the patch, keeps the raw override, converges showInTaskbar to false when showInAltTab is false, and announces window:behaviorChanged",
    "WP-15 setPopupBehavior converges allowMinimize to false when keepVisibleOnShowDesktop is true, and a null value deletes an override",
    "WP-16 a wrong-typed behavior flag is accepted, stored verbatim, and displaces the boolean override so the resolved value falls back to its default",
    "WP-17 setPopupBehavior refuses an unknown profile (shape only) and a non-object behavior, without touching the policy",
    "WP-18 setClickThrough true is reflected by isClickThrough, which reports the requested state only",
    "WP-19 setClickThroughExcludeRegions counts the valid regions, drops zero-sized ones silently, reports the DPI scale, and warns about nothing",
    "WP-20 the truncation warning is computed from the raw array length while truncation happens after invalid regions are dropped, so 40 regions with 30 valid warn yet keep all 30",
    "WP-21 clearClickThroughExcludeRegions answers with only success and the window id",
    "WP-22 setClickThrough false is reflected by isClickThrough and announces window:hoverStateChanged with hovering false",
    "WP-23 closePopup success only means WM_CLOSE was posted: the popup disappears on polling with window:popupClosed, and a second close is refused",
];

async function runLifecycleCases(bridge, recorder, events, createdPopups, placement) {
    const { invoke, invokeRaw } = bridge;

    await events.drain();
    const created = await invoke("window.createPopup", {
        ...placement,
        ...POPUP_GEOMETRY,
        title: POPUP_TITLE,
        behavior: { noActivate: true },
    });
    const popupId = created?.windowId;
    if (typeof popupId === "string") createdPopups.push(popupId);

    // The very first sendMessage goes out before anything else so the
    // registration race has a chance to be observed; its outcome is recorded,
    // not required, because the WebView may or may not be up yet.
    const firstSend = await invokeRaw("window.sendMessage", {
        targetWindowId: popupId,
        message: { tag: runId, kind: "first" },
    });
    const listedAtOnce = await invoke("window.getAllWindows", {});
    const listedItem = findItem(listedAtOnce, popupId);

    if (typeof popupId === "string") {
        await events.waitFor((all) => forWindow(all, "window:popupOpened", popupId).length > 0, {
            timeoutMs: popupReadyTimeoutMs,
        });
    }
    const opened = forWindow(events.received, "window:popupOpened", popupId).at(-1);
    recorder.assertCase(
        "WP-11 createPopup answers a popup_N id that getAllWindows lists at once with popup capabilities and the given title, and window:popupOpened follows",
        created?.success === true &&
            typeof popupId === "string" &&
            POPUP_ID_PATTERN.test(popupId) &&
            listedItem?.isMain === false &&
            listedItem.title === POPUP_TITLE &&
            listedItem.capabilities?.supportsPopupBehavior === true &&
            listedItem.capabilities?.supportsBeforeClose === true &&
            listedItem.shell?.lifecycle?.pendingDestroy === false &&
            opened?.payload?.windowId === popupId &&
            opened.payload.title === POPUP_TITLE &&
            opened.payload.url === "",
        {
            created,
            listedAtOnce: listedItem
                ? {
                      isMain: listedItem.isMain,
                      title: listedItem.title,
                      capabilities: listedItem.capabilities,
                      lifecycle: listedItem.shell?.lifecycle,
                  }
                : null,
            opened: opened?.payload,
            placement,
        },
        {
            created: { success: true, windowId: "popup_<digits>" },
            listedAtOnce: {
                isMain: false,
                title: POPUP_TITLE,
                capabilities: { supportsPopupBehavior: true, supportsBeforeClose: true },
                lifecycle: { pendingDestroy: false },
            },
            opened: { windowId: "the new id", title: POPUP_TITLE, url: "" },
        },
    );

    if (typeof popupId !== "string") {
        for (const name of LIFECYCLE_CASE_NAMES.slice(1)) {
            recorder.addCase(
                name,
                false,
                { skipped: "createPopup did not return a windowId; see WP-11", created },
                { skipped: "WP-11 did not pass" },
            );
        }
        return;
    }

    const eventualSend = await pollUntil(
        () =>
            invokeRaw("window.sendMessage", {
                targetWindowId: popupId,
                message: { tag: runId, kind: "polled" },
            }),
        (outcome) => outcome?.value?.success === true,
        { timeoutMs: popupReadyTimeoutMs, intervalMs: 100 },
    );
    recorder.assertCase(
        "WP-12 sendMessage to a popup that has just been created may race its WebView registration, and succeeds once polled",
        (firstSend?.value?.success === true ||
            firstSend?.value?.error === "Target window not found") &&
            eventualSend.satisfied,
        {
            firstSend: firstSend?.value,
            eventual: eventualSend.value?.value,
            attempts: eventualSend.attempts,
            elapsedMs: eventualSend.elapsedMs,
        },
        {
            firstSend: "success, or 'Target window not found' while the WebView is still coming up",
            eventual: { success: true },
        },
    );

    const behaviorRead = await invoke("window.getPopupBehavior", { windowId: popupId });
    const expectedResolved = {
        showInTaskbar: false,
        showInAltTab: false,
        keepVisibleOnShowDesktop: false,
        allowMinimize: false,
        owner: "none",
        noActivate: true,
    };
    recorder.assertCase(
        "WP-13 getPopupBehavior reports the legacy profile, the raw behavior override, and all six resolved fields",
        behaviorRead?.success === true &&
            behaviorRead.windowId === popupId &&
            behaviorRead.profile === "legacy" &&
            sameJson(behaviorRead.behavior, { noActivate: true }) &&
            sameJson(Object.keys(behaviorRead.resolvedBehavior ?? {}).sort(), Object.keys(expectedResolved).sort()) &&
            Object.entries(expectedResolved).every(
                ([key, value]) => behaviorRead.resolvedBehavior[key] === value,
            ),
        { behaviorRead },
        {
            success: true,
            windowId: popupId,
            profile: "legacy",
            behavior: { noActivate: true },
            resolvedBehavior: expectedResolved,
        },
    );

    await events.drain();
    const patchTaskbar = await invoke("window.setPopupBehavior", {
        windowId: popupId,
        behavior: { showInTaskbar: true, showInAltTab: false },
    });
    await events.waitFor((all) => forWindow(all, "window:behaviorChanged", popupId).length > 0, {
        timeoutMs: eventTimeoutMs,
    });
    const behaviorChanged = forWindow(events.received, "window:behaviorChanged", popupId).at(-1);
    recorder.assertCase(
        "WP-14 setPopupBehavior merges the patch, keeps the raw override, converges showInTaskbar to false when showInAltTab is false, and announces window:behaviorChanged",
        patchTaskbar?.success === true &&
            patchTaskbar.behavior?.showInTaskbar === true &&
            patchTaskbar.behavior?.showInAltTab === false &&
            patchTaskbar.behavior?.noActivate === true &&
            patchTaskbar.resolvedBehavior?.showInTaskbar === false &&
            patchTaskbar.resolvedBehavior?.showInAltTab === false &&
            patchTaskbar.resolvedBehavior?.noActivate === true &&
            behaviorChanged?.payload?.profile === "legacy" &&
            behaviorChanged.payload.behavior?.showInTaskbar === true &&
            behaviorChanged.payload.resolvedBehavior?.showInTaskbar === false,
        {
            patch: { behavior: patchTaskbar?.behavior, resolvedBehavior: patchTaskbar?.resolvedBehavior },
            behaviorChanged: behaviorChanged?.payload,
        },
        {
            behavior: { showInTaskbar: true, showInAltTab: false, noActivate: true },
            resolvedBehavior: { showInTaskbar: false, showInAltTab: false, noActivate: true },
            behaviorChanged: { profile: "legacy", behavior: "raw override", resolvedBehavior: "converged" },
        },
    );

    const patchDesktop = await invoke("window.setPopupBehavior", {
        windowId: popupId,
        behavior: { keepVisibleOnShowDesktop: true, allowMinimize: true, showInTaskbar: null },
    });
    recorder.assertCase(
        "WP-15 setPopupBehavior converges allowMinimize to false when keepVisibleOnShowDesktop is true, and a null value deletes an override",
        patchDesktop?.success === true &&
            patchDesktop.behavior?.keepVisibleOnShowDesktop === true &&
            patchDesktop.behavior?.allowMinimize === true &&
            !("showInTaskbar" in (patchDesktop.behavior ?? {})) &&
            patchDesktop.resolvedBehavior?.keepVisibleOnShowDesktop === true &&
            patchDesktop.resolvedBehavior?.allowMinimize === false &&
            patchDesktop.resolvedBehavior?.showInTaskbar === false,
        {
            behavior: patchDesktop?.behavior,
            resolvedBehavior: patchDesktop?.resolvedBehavior,
        },
        {
            behavior: { keepVisibleOnShowDesktop: true, allowMinimize: true, showInTaskbar: "deleted" },
            resolvedBehavior: { keepVisibleOnShowDesktop: true, allowMinimize: false },
        },
    );

    // UpdatePopupBehavior writes the patch value into the override map as-is
    // and re-resolves from scratch; TryGetBool ignores a non-boolean, so the
    // "yes" both survives in behavior and knocks out the true it replaced.
    const wrongType = await invoke("window.setPopupBehavior", {
        windowId: popupId,
        behavior: { noActivate: "yes" },
    });
    const restored = await invoke("window.setPopupBehavior", {
        windowId: popupId,
        behavior: { noActivate: true },
    });
    recorder.assertCase(
        "WP-16 a wrong-typed behavior flag is accepted, stored verbatim, and displaces the boolean override so the resolved value falls back to its default",
        wrongType?.success === true &&
            wrongType.behavior?.noActivate === "yes" &&
            wrongType.resolvedBehavior?.noActivate === false &&
            restored?.success === true &&
            restored.behavior?.noActivate === true &&
            restored.resolvedBehavior?.noActivate === true,
        {
            wrongType: { behavior: wrongType?.behavior, resolvedBehavior: wrongType?.resolvedBehavior },
            restored: { behavior: restored?.behavior, resolvedBehavior: restored?.resolvedBehavior },
        },
        {
            wrongType: { success: true, behavior: { noActivate: "yes" }, resolvedBehavior: { noActivate: false } },
            restored: { resolvedBehavior: { noActivate: true } },
        },
    );

    const badProfile = await invokeRaw("window.setPopupBehavior", {
        windowId: popupId,
        profile: `bogus-${runId}`,
    });
    const badBehaviorPatch = await invokeRaw("window.setPopupBehavior", {
        windowId: popupId,
        behavior: 5,
    });
    const afterRefusals = await invoke("window.getPopupBehavior", { windowId: popupId });
    recorder.assertCase(
        "WP-17 setPopupBehavior refuses an unknown profile (shape only) and a non-object behavior with INVALID_PARAMS, without touching the policy",
        isInvalidParams(badProfile) &&
            typeof badProfile.value.error === "string" &&
            badProfile.value.error.length > 0 &&
            isInvalidParams(badBehaviorPatch, "behavior must be an object") &&
            afterRefusals?.profile === "legacy" &&
            sameJson(afterRefusals.resolvedBehavior, restored?.resolvedBehavior),
        {
            badProfile: badProfile?.value,
            badBehaviorPatch: badBehaviorPatch?.value,
            afterRefusals: { profile: afterRefusals?.profile, resolvedBehavior: afterRefusals?.resolvedBehavior },
        },
        {
            badProfile: { success: false, error: "a non-empty string (wording not pinned)", code: "INVALID_PARAMS" },
            badBehaviorPatch: { error: "behavior must be an object", code: "INVALID_PARAMS" },
            afterRefusals: { profile: "legacy", resolvedBehavior: "unchanged" },
        },
    );

    const clickOn = await invoke("window.setClickThrough", { windowId: popupId, enabled: true });
    const isOn = await invoke("window.isClickThrough", { windowId: popupId });
    recorder.assertCase(
        "WP-18 setClickThrough true is reflected by isClickThrough, which reports the requested state only",
        clickOn?.success === true &&
            clickOn.clickThrough === true &&
            isOn?.success === true &&
            isOn.clickThrough === true,
        {
            clickOn,
            isOn,
            note: "clickThrough is the requested state; the effective native style diverges inside an exclude region and has no read-back",
        },
        { clickOn: { success: true, clickThrough: true }, isOn: { success: true, clickThrough: true } },
    );

    const validRegions = [
        { x: 0, y: 0, width: 50, height: 20 },
        { x: 60, y: 0, width: 50, height: 20 },
        { x: 0, y: 30, width: 50, height: 20 },
    ];
    const setRegions = await invoke("window.setClickThroughExcludeRegions", {
        windowId: popupId,
        regions: [...validRegions, { x: 0, y: 0, width: 0, height: 20 }],
    });
    recorder.assertCase(
        "WP-19 setClickThroughExcludeRegions counts the valid regions, drops zero-sized ones silently, reports the DPI scale, and warns about nothing",
        setRegions?.success === true &&
            setRegions.windowId === popupId &&
            setRegions.count === validRegions.length &&
            typeof setRegions.dpiScale === "number" &&
            setRegions.dpiScale > 0 &&
            !("warning" in setRegions),
        { setRegions, warningPresent: "warning" in (setRegions ?? {}) },
        { success: true, windowId: popupId, count: validRegions.length, dpiScale: "> 0", warning: "absent" },
    );

    const thirtyValid = Array.from({ length: 30 }, (_, index) => ({
        x: index * 10,
        y: 0,
        width: 5,
        height: 5,
    }));
    const tenEmpty = Array.from({ length: 10 }, () => ({ x: 0, y: 0, width: 0, height: 0 }));
    const fortyMixed = await invoke("window.setClickThroughExcludeRegions", {
        windowId: popupId,
        regions: [...thirtyValid, ...tenEmpty],
    });
    const fortyValid = await invoke("window.setClickThroughExcludeRegions", {
        windowId: popupId,
        regions: Array.from({ length: 40 }, (_, index) => ({ x: index * 10, y: 0, width: 5, height: 5 })),
    });
    recorder.assertCase(
        "WP-20 the truncation warning is computed from the raw array length while truncation happens after invalid regions are dropped, so 40 regions with 30 valid warn yet keep all 30",
        fortyMixed?.success === true &&
            fortyMixed.count === 30 &&
            typeof fortyMixed.warning === "string" &&
            /truncated/i.test(fortyMixed.warning) &&
            fortyValid?.success === true &&
            fortyValid.count === EXCLUDE_REGION_LIMIT &&
            typeof fortyValid.warning === "string",
        {
            fortyMixed: { count: fortyMixed?.count, warning: fortyMixed?.warning },
            fortyValid: { count: fortyValid?.count, warning: fortyValid?.warning },
        },
        {
            fortyMixed: { count: 30, warning: "present although nothing was truncated" },
            fortyValid: { count: EXCLUDE_REGION_LIMIT, warning: "present" },
        },
    );

    const cleared = await invoke("window.clearClickThroughExcludeRegions", { windowId: popupId });
    recorder.assertCase(
        "WP-21 clearClickThroughExcludeRegions answers with only success and the window id",
        cleared?.success === true &&
            cleared.windowId === popupId &&
            sameJson(Object.keys(cleared).sort(), ["success", "windowId"]),
        { cleared },
        { success: true, windowId: popupId, otherFields: "none" },
    );

    await events.drain();
    const clickOff = await invoke("window.setClickThrough", { windowId: popupId, enabled: false });
    const isOff = await invoke("window.isClickThrough", { windowId: popupId });
    await events.waitFor(
        (all) =>
            forWindow(all, "window:hoverStateChanged", popupId).some(
                (entry) => entry.payload?.hovering === false,
            ),
        { timeoutMs: eventTimeoutMs },
    );
    const hoverEnded = forWindow(events.received, "window:hoverStateChanged", popupId).find(
        (entry) => entry.payload?.hovering === false,
    );
    recorder.assertCase(
        "WP-22 setClickThrough false is reflected by isClickThrough and announces window:hoverStateChanged with hovering false",
        clickOff?.success === true &&
            clickOff.clickThrough === false &&
            isOff?.success === true &&
            isOff.clickThrough === false &&
            hoverEnded?.payload?.windowId === popupId &&
            hoverEnded.payload.hovering === false,
        {
            clickOff,
            isOff,
            hoverEnded: hoverEnded?.payload,
            note: "overlayIntent_ stays latched after this; only closing the popup resets it",
        },
        {
            clickOff: { success: true, clickThrough: false },
            isOff: { clickThrough: false },
            hoverEnded: { windowId: popupId, hovering: false },
        },
    );

    await events.drain();
    const closed = await invoke("window.closePopup", { windowId: popupId });
    const gone = await waitForPopupGone(bridge, popupId);
    await events.waitFor((all) => forWindow(all, "window:popupClosed", popupId).length > 0, {
        timeoutMs: eventTimeoutMs,
    });
    const closedEvent = forWindow(events.received, "window:popupClosed", popupId).at(-1);
    const closedAgain = await invokeRaw("window.closePopup", { windowId: popupId });
    recorder.assertCase(
        "WP-23 closePopup success only means WM_CLOSE was posted: the popup disappears on polling with window:popupClosed, and a second close is refused",
        closed?.success === true &&
            Object.keys(closed).length === 1 &&
            gone.satisfied &&
            closedEvent?.payload?.windowId === popupId &&
            isNotFound(closedAgain),
        {
            closed,
            goneAfterMs: gone.elapsedMs,
            goneAttempts: gone.attempts,
            stillListed: !gone.satisfied,
            closedEvent: closedEvent?.payload,
            closedAgain: closedAgain?.value,
        },
        {
            closed: { success: true },
            gone: true,
            closedEvent: { windowId: popupId },
            closedAgain: { success: false, error: "Window not found" },
        },
    );
}

const BEFORE_CLOSE_CASE_NAMES = [
    "WP-24 a beforeClose popup exposes its own page on the DevTools port, identified by the windowId in its URL, and getCurrentWindowId from that page reports the popup",
    "WP-25 sendMessage from the main window reaches the popup page as window:message with sourceWindowId main",
    "WP-26 broadcast reaches every other window but never the sender, in both directions",
    "WP-27 cancelClose and confirmClose with no close pending are no-ops that still answer success, and the popup stays",
    "WP-28 closePopup on a beforeClose popup emits window:beforeClose and sets pendingDestroy; cancelClose from the popup clears it and the window stays",
    "WP-29 closePopup again followed by confirmClose from the popup destroys it: its response is lost with the page, the main window sees it disappear with window:popupClosed",
];

async function runBeforeCloseCases(bridge, recorder, events, createdPopups, placement, port, session) {
    const { invoke, invokeRaw } = bridge;

    await events.drain();
    const created = await invoke("window.createPopup", {
        ...placement,
        ...POPUP_GEOMETRY,
        title: `${POPUP_TITLE} beforeClose`,
        beforeClose: true,
        behavior: { noActivate: true },
    });
    const popupId = created?.windowId;
    if (typeof popupId === "string") createdPopups.push(popupId);

    const attach =
        typeof popupId === "string"
            ? await connectPopupPage(port, popupId, popupReadyTimeoutMs)
            : { stage: "create", waitedMs: 0 };
    session.client = attach.client;
    session.waitedMs = attach.waitedMs;
    session.stage = attach.stage;
    const listed = typeof popupId === "string" ? await invoke("window.getAllWindows", {}) : null;
    const listedItem = listed ? findItem(listed, popupId) : null;
    recorder.assertCase(
        "WP-24 a beforeClose popup exposes its own page on the DevTools port, identified by the windowId in its URL, and getCurrentWindowId from that page reports the popup",
        created?.success === true &&
            POPUP_ID_PATTERN.test(String(popupId)) &&
            attach.stage === "ready" &&
            attach.identity?.value?.windowId === popupId &&
            listedItem?.capabilities?.supportsBeforeClose === true,
        {
            created,
            attachStage: attach.stage,
            attachWaitedMs: attach.waitedMs,
            popupUrl: String(attach.page?.url || "").slice(0, 120),
            identity: attach.identity?.value,
            pagesSeen: attach.seen,
            supportsBeforeClose: listedItem?.capabilities?.supportsBeforeClose,
        },
        {
            created: { success: true, windowId: "popup_<digits>" },
            attachStage: "ready",
            identity: { windowId: "the popup id" },
            supportsBeforeClose: true,
        },
    );

    if (attach.stage !== "ready") {
        for (const name of BEFORE_CLOSE_CASE_NAMES.slice(1)) {
            recorder.addCase(
                name,
                false,
                { skipped: "no usable CDP session on the popup page; see WP-24", attachStage: attach.stage },
                { skipped: "WP-24 did not pass" },
            );
        }
        return;
    }

    const popupBridge = attach.bridge;
    const popupEvents = await createEventCollector(popupBridge, POPUP_EVENT_NAMES, {
        collectorId: `${runId}-popup`,
    });
    session.events = popupEvents;

    const directTag = `${runId}:direct`;
    const direct = await pollUntil(
        () =>
            invokeRaw("window.sendMessage", {
                targetWindowId: popupId,
                message: { tag: directTag },
            }),
        (outcome) => outcome?.value?.success === true,
        { timeoutMs: popupReadyTimeoutMs, intervalMs: 100 },
    );
    await popupEvents.waitFor(
        (all) => named(all, "window:message").some((e) => e.payload?.message?.tag === directTag),
        { timeoutMs: eventTimeoutMs },
    );
    const directDelivered = named(popupEvents.received, "window:message").find(
        (entry) => entry.payload?.message?.tag === directTag,
    );
    recorder.assertCase(
        "WP-25 sendMessage from the main window reaches the popup page as window:message with sourceWindowId main",
        direct.satisfied &&
            directDelivered?.payload?.sourceWindowId === "main" &&
            directDelivered.payload.message?.tag === directTag,
        {
            send: direct.value?.value,
            attempts: direct.attempts,
            delivered: directDelivered?.payload,
        },
        { send: { success: true }, delivered: { sourceWindowId: "main", message: { tag: directTag } } },
    );

    await events.drain();
    await popupEvents.drain();
    const fromMainTag = `${runId}:from-main`;
    const fromPopupTag = `${runId}:from-popup`;
    const fromMain = await invoke("window.broadcast", { message: { tag: fromMainTag } });
    const fromPopup = await popupBridge.invoke("window.broadcast", { message: { tag: fromPopupTag } });
    await popupEvents.waitFor(
        (all) => named(all, "window:message").some((e) => e.payload?.message?.tag === fromMainTag),
        { timeoutMs: eventTimeoutMs },
    );
    await events.waitFor(
        (all) => named(all, "window:message").some((e) => e.payload?.message?.tag === fromPopupTag),
        { timeoutMs: eventTimeoutMs },
    );
    await sleep(NEGATIVE_SILENCE_MS);
    await events.drain();
    await popupEvents.drain();
    const popupGotMain = named(popupEvents.received, "window:message").find(
        (entry) => entry.payload?.message?.tag === fromMainTag,
    );
    const mainGotPopup = named(events.received, "window:message").find(
        (entry) => entry.payload?.message?.tag === fromPopupTag,
    );
    const mainGotOwn = named(events.received, "window:message").filter(
        (entry) => entry.payload?.message?.tag === fromMainTag,
    );
    const popupGotOwn = named(popupEvents.received, "window:message").filter(
        (entry) => entry.payload?.message?.tag === fromPopupTag,
    );
    recorder.assertCase(
        "WP-26 broadcast reaches every other window but never the sender, in both directions",
        fromMain?.success === true &&
            fromPopup?.success === true &&
            popupGotMain?.payload?.sourceWindowId === "main" &&
            mainGotPopup?.payload?.sourceWindowId === popupId &&
            mainGotOwn.length === 0 &&
            popupGotOwn.length === 0,
        {
            fromMain,
            fromPopup,
            popupGotMain: popupGotMain?.payload,
            mainGotPopup: mainGotPopup?.payload,
            senderEchoes: { main: mainGotOwn.length, popup: popupGotOwn.length },
        },
        {
            popupGotMain: { sourceWindowId: "main" },
            mainGotPopup: { sourceWindowId: popupId },
            senderEchoes: { main: 0, popup: 0 },
        },
    );

    // With pendingClose_ false both handlers return before doing anything, and
    // the envelope does not say so - success here carries no information.
    const idleCancel = await popupBridge.invokeRaw("window.cancelClose", {});
    const idleConfirm = await popupBridge.invokeRaw("window.confirmClose", {});
    await sleep(NEGATIVE_SILENCE_MS);
    const stillThere = await readPendingDestroy(bridge, popupId);
    recorder.assertCase(
        "WP-27 cancelClose and confirmClose with no close pending are no-ops that still answer success, and the popup stays",
        idleCancel?.value?.success === true &&
            idleConfirm?.value?.success === true &&
            stillThere.listed === true &&
            stillThere.pendingDestroy === false,
        { idleCancel: idleCancel?.value, idleConfirm: idleConfirm?.value, stillThere },
        {
            idleCancel: { success: true },
            idleConfirm: { success: true },
            stillThere: { listed: true, pendingDestroy: false },
        },
    );

    // Everything between closePopup and cancelClose has to fit inside the
    // 3-second force-close timer, so the pending read is a short poll and the
    // whole handshake is timed.
    await popupEvents.drain();
    const handshakeStartedAt = Date.now();
    const closeFirst = await invoke("window.closePopup", { windowId: popupId });
    const pendingOn = await pollUntil(
        () => readPendingDestroy(bridge, popupId),
        (state) => state.pendingDestroy === true,
        { timeoutMs: 1500, intervalMs: 25 },
    );
    const cancelled = await popupBridge.invokeRaw("window.cancelClose", {});
    const pendingOff = await pollUntil(
        () => readPendingDestroy(bridge, popupId),
        (state) => state.listed && state.pendingDestroy === false,
        { timeoutMs: 1500, intervalMs: 25 },
    );
    const handshakeMs = Date.now() - handshakeStartedAt;
    await popupEvents.waitFor((all) => forWindow(all, "window:beforeClose", popupId).length > 0, {
        timeoutMs: eventTimeoutMs,
    });
    const beforeCloseEvents = forWindow(popupEvents.received, "window:beforeClose", popupId);
    await sleep(NEGATIVE_SILENCE_MS);
    const survived = await readPendingDestroy(bridge, popupId);
    recorder.assertCase(
        "WP-28 closePopup on a beforeClose popup emits window:beforeClose and sets pendingDestroy; cancelClose from the popup clears it and the window stays",
        closeFirst?.success === true &&
            pendingOn.satisfied &&
            cancelled?.value?.success === true &&
            pendingOff.satisfied &&
            beforeCloseEvents.length === 1 &&
            survived.listed === true &&
            survived.pendingDestroy === false &&
            handshakeMs < BEFORE_CLOSE_BUDGET_MS,
        {
            closeFirst,
            pendingOn: { ...pendingOn.value, elapsedMs: pendingOn.elapsedMs },
            cancelled: cancelled?.value,
            pendingOff: { ...pendingOff.value, elapsedMs: pendingOff.elapsedMs },
            beforeCloseEvents: beforeCloseEvents.map((entry) => entry.payload),
            survived,
            handshakeMs,
            budgetMs: BEFORE_CLOSE_BUDGET_MS,
        },
        {
            closeFirst: { success: true },
            pendingOn: { pendingDestroy: true },
            cancelled: { success: true },
            pendingOff: { listed: true, pendingDestroy: false },
            beforeCloseEvents: [{ windowId: popupId }],
            survived: { listed: true, pendingDestroy: false },
            handshakeMs: `< ${BEFORE_CLOSE_BUDGET_MS}`,
        },
    );

    // ConfirmClose calls DestroyWindow inside the handler, so the popup's
    // WebView is gone before the response is posted back. The outcome of that
    // invoke is recorded whatever it is; the oracle is the main window's view.
    await events.drain();
    const closeSecond = await invoke("window.closePopup", { windowId: popupId });
    const pendingAgain = await pollUntil(
        () => readPendingDestroy(bridge, popupId),
        (state) => state.pendingDestroy === true,
        { timeoutMs: 1500, intervalMs: 25 },
    );
    let confirmOutcome;
    try {
        confirmOutcome = await popupBridge.invokeRaw("window.confirmClose", {});
    } catch (error) {
        confirmOutcome = { kind: "session-lost", error: String(error?.message || error) };
    }
    const gone = await waitForPopupGone(bridge, popupId);
    await events.waitFor((all) => forWindow(all, "window:popupClosed", popupId).length > 0, {
        timeoutMs: eventTimeoutMs,
    });
    const closedEvent = forWindow(events.received, "window:popupClosed", popupId).at(-1);
    recorder.assertCase(
        "WP-29 closePopup again followed by confirmClose from the popup destroys it: its response is lost with the page, the main window sees it disappear with window:popupClosed",
        closeSecond?.success === true &&
            pendingAgain.satisfied &&
            gone.satisfied &&
            closedEvent?.payload?.windowId === popupId,
        {
            closeSecond,
            pendingAgain: { ...pendingAgain.value, elapsedMs: pendingAgain.elapsedMs },
            confirmOutcome,
            goneAfterMs: gone.elapsedMs,
            stillListed: !gone.satisfied,
            closedEvent: closedEvent?.payload,
        },
        {
            closeSecond: { success: true },
            pendingAgain: { pendingDestroy: true },
            confirmOutcome: "recorded only; the page dies inside the handler",
            gone: true,
            closedEvent: { windowId: popupId },
        },
    );
}

const DEV_SERVER_FALLBACK_CASE_NAMES = [
    "WP-30 with the development server switched on but unreachable, a new popup falls back to the local template like the main window: its page comes from the virtual host with its windowId, and webview.getSource reports a folder source",
];
const LOCAL_FOLDER_SOURCES = ["panelTemplate", "activeTemplate", "componentDirectory", "defaultTemplate"];

/** A loopback port nothing listens on: taken from the OS, then released before it is used. */
async function unusedLoopbackPort() {
    const server = createServer();
    await new Promise((resolve, reject) => {
        server.once("error", reject);
        server.listen(0, "127.0.0.1", resolve);
    });
    const { port } = server.address();
    await new Promise((resolve) => server.close(resolve));
    return port;
}

/**
 * Points the development server at a dead port only for the time it takes to open one popup;
 * setDevServerConfig reloads nothing, so the main window keeps its page, and the original
 * setting is written back even when a step throws.
 */
async function runDevServerFallbackCases(bridge, recorder, createdPopups, placement, port) {
    const { invoke } = bridge;
    const original = await invoke("window.getDevServerConfig", {});
    const deadUrl = `http://127.0.0.1:${await unusedLoopbackPort()}/`;
    let attach = { stage: "create", waitedMs: 0 };
    let created;
    let source;
    let finalUrl;
    try {
        await invoke("window.setDevServerConfig", { useDevServer: true, devServerUrl: deadUrl });
        created = await invoke("window.createPopup", {
            ...placement,
            ...POPUP_GEOMETRY,
            title: `${POPUP_TITLE} dev server fallback`,
            behavior: { noActivate: true },
        });
        const popupId = created?.windowId;
        if (typeof popupId === "string") {
            createdPopups.push(popupId);
            attach = await connectPopupPage(port, popupId, popupReadyTimeoutMs);
        }
        if (attach.stage === "ready") {
            source = await attach.bridge.invokeRaw("webview.getSource", {});
            const targets = await CDP.List({ port });
            finalUrl = String(targets.find((target) => target.id === attach.page.id)?.url || "");
        }
    } finally {
        await closeClient(attach.client);
        await invoke("window.setDevServerConfig", {
            useDevServer: original?.useDevServer === true,
            devServerUrl: String(original?.devServerUrl ?? ""),
        });
    }

    const popupId = created?.windowId;
    // The next group needs zero popups, so this one leaves none behind.
    if (typeof popupId === "string") {
        await bridge.invokeRaw("window.closePopup", { windowId: popupId });
        await waitForPopupGone(bridge, popupId).catch(() => undefined);
    }
    const fromVirtualHost =
        finalUrl.startsWith("https://foo-ui-webview2.local/") &&
        new RegExp(`[?&]windowId=${popupId}(?:[&#]|$)`).test(finalUrl);
    const restored = await invoke("window.getDevServerConfig", {});
    recorder.assertCase(
        DEV_SERVER_FALLBACK_CASE_NAMES[0],
        created?.success === true &&
            attach.stage === "ready" &&
            fromVirtualHost &&
            source?.value?.success === true &&
            LOCAL_FOLDER_SOURCES.includes(source.value.source) &&
            typeof source.value.directory === "string" &&
            restored?.useDevServer === original?.useDevServer &&
            restored?.devServerUrl === original?.devServerUrl,
        {
            deadUrl,
            created,
            attachStage: attach.stage,
            attachWaitedMs: attach.waitedMs,
            finalUrl: finalUrl.slice(0, 160),
            source: source?.value,
            restored,
            original,
        },
        {
            finalUrl: "https://foo-ui-webview2.local/...?windowId=<the popup id>",
            source: { source: LOCAL_FOLDER_SOURCES.join(" | "), directory: "the mapped folder" },
            restored: "equal to original",
        },
    );
}

const ORIGIN_GATE_CASE_NAMES = [
    "WP-31 a popup opened at a remote http URL its opener does not trust loads, but its calls fail at once with ORIGIN_DENIED and no event reaches it",
    "WP-32 a URL that puts a trusted origin in front of an '@' does not borrow that origin's trust",
    "WP-33 a popup opened at an absolute URL its opener trusts keeps the bridge",
];

/** A page on 127.0.0.2, a loopback address nothing in the host trusts. */
async function startUntrustedServer() {
    const server = createHttpServer((req, res) => {
        res.writeHead(200, { "content-type": "text/html; charset=utf-8" });
        res.end("<!doctype html><title>untrusted</title><p>untrusted page</p>");
    });
    await new Promise((resolve, reject) => {
        server.once("error", reject);
        server.listen(0, "127.0.0.2", resolve);
    });
    return { server, base: `http://127.0.0.2:${server.address().port}` };
}

/** Attaches to the page whose URL matches, waiting only for the injected bridge object. */
async function connectPageByUrl(port, urlPattern, timeoutMs) {
    const deadline = Date.now() + timeoutMs;
    let page;
    while (!page && Date.now() < deadline) {
        const targets = await CDP.List({ port });
        page = targets.find((target) => target.type === "page" && urlPattern.test(String(target.url || "")));
        if (!page) await sleep(100);
    }
    if (!page) return { stage: "target" };
    const client = await CDP({ port, target: page });
    while (Date.now() < deadline) {
        const ready = await client.Runtime.evaluate({
            expression: "typeof window.fb2k === 'object' && typeof window.fb2k.invoke === 'function' && document.readyState === 'complete'",
            returnByValue: true,
        }).catch(() => undefined);
        if (ready?.result?.value === true) return { stage: "ready", client, page };
        await sleep(100);
    }
    await closeClient(client);
    return { stage: "bridge", page };
}

/** One call from inside the page, timed, with its rejection code if any. */
async function invokeFromPage(client, method, budgetMs = 5000) {
    const evaluated = await client.Runtime.evaluate({
        awaitPromise: true,
        returnByValue: true,
        expression: `(async () => {
            const started = performance.now();
            const outcome = await Promise.race([
                window.fb2k.invoke(${JSON.stringify(method)}, {}).then(
                    (value) => ({ answered: value }),
                    (error) => ({ rejected: String(error && error.message), code: error && error.code })),
                new Promise((resolve) => setTimeout(() => resolve({ timedOut: true }), ${budgetMs})),
            ]);
            return { ...outcome, elapsedMs: Math.round(performance.now() - started), href: location.href };
        })()`,
    });
    return evaluated?.result?.value;
}

async function navigatePage(client, url) {
    await client.Page.enable();
    const loaded = client.Page.loadEventFired();
    const navigation = await client.Page.navigate({ url });
    await Promise.race([loaded, sleep(5000)]);
    return navigation;
}

async function runOriginGateCases(bridge, recorder, createdPopups, placement, port) {
    const { invoke } = bridge;
    const { server, base } = await startUntrustedServer();
    const sessions = [];
    try {
        // WP-31: a remote popup the opener does not vouch for.
        const remote = await invoke("window.createPopup", {
            ...placement,
            ...POPUP_GEOMETRY,
            url: `${base}/remote.html`,
            title: `${POPUP_TITLE} remote`,
            behavior: { noActivate: true },
        });
        const remoteId = remote?.windowId;
        if (typeof remoteId === "string") createdPopups.push(remoteId);
        const remoteAttach = await connectPageByUrl(
            port,
            new RegExp(`^${base.replace(/\./g, "\\.")}/remote\\.html\\?windowId=${remoteId}$`),
            popupReadyTimeoutMs,
        );
        let call;
        let received = [];
        let sent;
        if (remoteAttach.stage === "ready") {
            sessions.push(remoteAttach.client);
            await remoteAttach.client.Runtime.evaluate({
                expression: "window.__e2eSeen = []; window.fb2k.on('window:message', (d) => window.__e2eSeen.push(d));",
            });
            call = await invokeFromPage(remoteAttach.client, "window.getCurrentWindowId");
            sent = await bridge.invokeRaw("window.sendMessage", { targetWindowId: remoteId, message: { probe: runId } });
            await sleep(NEGATIVE_SILENCE_MS);
            received = (await remoteAttach.client.Runtime.evaluate({ expression: "window.__e2eSeen", returnByValue: true }))
                ?.result?.value ?? [];
        }
        recorder.assertCase(
            ORIGIN_GATE_CASE_NAMES[0],
            remote?.success === true &&
                remoteAttach.stage === "ready" &&
                call?.code === "ORIGIN_DENIED" &&
                call.elapsedMs < 2000 &&
                Array.isArray(received) &&
                received.length === 0,
            { created: remote, attachStage: remoteAttach.stage, call, sendMessage: sent?.value, received },
            { call: { code: "ORIGIN_DENIED", elapsedMs: "< 2000" }, received: [] },
        );

        // WP-32: borrow a trusted origin through user info. The popup's own origin is the
        // one to borrow; an https one cannot be served here, so only an http origin is tried.
        const trusted = await invoke("window.createPopup", {
            ...placement,
            ...POPUP_GEOMETRY,
            title: `${POPUP_TITLE} borrow`,
            behavior: { noActivate: true },
        });
        const trustedId = trusted?.windowId;
        if (typeof trustedId === "string") createdPopups.push(trustedId);
        const trustedAttach =
            typeof trustedId === "string"
                ? await connectPopupPage(port, trustedId, popupReadyTimeoutMs)
                : { stage: "create" };
        if (trustedAttach.client) sessions.push(trustedAttach.client);
        const source = trustedAttach.stage === "ready"
            ? (await trustedAttach.bridge.invokeRaw("webview.getSource", {}))?.value
            : undefined;
        const ownOrigin = typeof source?.url === "string" ? new URL(source.url).origin : "";
        if (ownOrigin.startsWith("http://")) {
            const borrowed = `${ownOrigin}:x@${base.slice("http://".length)}/borrowed.html`;
            const navigation = await navigatePage(trustedAttach.client, borrowed);
            const borrowedCall = await invokeFromPage(trustedAttach.client, "window.getCurrentWindowId");
            recorder.assertCase(
                ORIGIN_GATE_CASE_NAMES[1],
                !navigation.errorText &&
                    borrowedCall?.href?.startsWith(`${base}/borrowed.html`) &&
                    borrowedCall.code === "ORIGIN_DENIED",
                { ownOrigin, borrowed, navigation, call: borrowedCall },
                { call: { code: "ORIGIN_DENIED" } },
            );
        } else {
            recorder.addCase(
                ORIGIN_GATE_CASE_NAMES[1],
                true,
                { skipped: "the popup's own origin is not http, so there is no origin this server could borrow", source },
                { skipped: "needs a popup served over http, such as from the development server" },
            );
        }

        // WP-33: the page the opener loads from the development server, opened as an absolute
        // URL. Without a development server there is no http(s) origin the opener trusts that a
        // popup could load: the virtual host is mapped only for relative URLs.
        if (typeof source?.url !== "string") {
            recorder.addCase(
                ORIGIN_GATE_CASE_NAMES[2],
                true,
                { skipped: "the popup did not come from the development server", source },
                { skipped: "needs the development server" },
            );
            return;
        }
        const vouchedUrl = source.url.split("?")[0];
        const vouched = await invoke("window.createPopup", {
            ...placement,
            ...POPUP_GEOMETRY,
            url: vouchedUrl,
            title: `${POPUP_TITLE} vouched`,
            behavior: { noActivate: true },
        });
        const vouchedId = vouched?.windowId;
        if (typeof vouchedId === "string") createdPopups.push(vouchedId);
        const vouchedAttach =
            typeof vouchedId === "string"
                ? await connectPopupPage(port, vouchedId, popupReadyTimeoutMs)
                : { stage: "create" };
        if (vouchedAttach.client) sessions.push(vouchedAttach.client);
        recorder.assertCase(
            ORIGIN_GATE_CASE_NAMES[2],
            vouched?.success === true &&
                vouchedAttach.stage === "ready" &&
                vouchedAttach.identity?.value?.windowId === vouchedId,
            { vouchedUrl, created: vouched, attachStage: vouchedAttach.stage, identity: vouchedAttach.identity?.value },
            { attachStage: "ready", identity: { windowId: "the popup id" } },
        );
    } finally {
        for (const session of sessions) await closeClient(session);
        server.close();
    }
}

function skipGroup(recorder, names, popups) {
    for (const name of names) {
        recorder.addCase(
            name,
            true,
            {
                skipped: "popups are open that this suite did not create; the bridge cannot tell them from stale test popups",
                popupCount: popups.length,
                popupIds: popups.map((item) => item?.windowId),
            },
            { skipped: "needs zero popups" },
        );
    }
}

const recorder = createRecorder();
let client;
let bridge;
let events;
let blocked = false;
let fatalError;
let targets;
let selfId;
let entryPopupCount;
let placementInfo;
const createdPopups = [];
const popupSession = {};

try {
    const connection = await connectBridgePage();
    client = connection.client;
    targets = connection.candidates;
    console.log(`Target: ${connection.page.title} ${connection.page.url}`);
    bridge = createBridge(client.Runtime, { invokeTimeoutMs });
    await requireResponsiveBridge(bridge, { probeId: runId });

    // A leftover popup page ranks like the main page, so the identity is
    // checked rather than assumed: this suite drives the main window only.
    const current = await bridge.invoke("window.getCurrentWindowId", {});
    selfId = current?.windowId;
    if (selfId !== "main") {
        throw blockedError(
            `attached page identifies as ${JSON.stringify(selfId)}, not main; a stale popup page may be listed first`,
            { current, candidates: targets },
        );
    }

    events = await createEventCollector(bridge, MAIN_EVENT_NAMES, { collectorId: runId });

    const entryWindows = await bridge.invoke("window.getAllWindows", {});
    const entryPopups = popupItems(entryWindows);
    entryPopupCount = entryPopups.length;
    await runZeroDisturbanceCases(bridge, recorder, events, selfId, entryPopups);

    const beforeLifecycle = popupItems(await bridge.invoke("window.getAllWindows", {}));
    if (beforeLifecycle.length === 0) {
        placementInfo = await cornerPlacement(bridge);
        await runLifecycleCases(bridge, recorder, events, createdPopups, placementInfo.placement);
    } else {
        skipGroup(recorder, LIFECYCLE_CASE_NAMES, beforeLifecycle);
    }

    const beforeHandshake = popupItems(await bridge.invoke("window.getAllWindows", {}));
    if (beforeHandshake.length === 0) {
        placementInfo ??= await cornerPlacement(bridge);
        await runBeforeCloseCases(
            bridge,
            recorder,
            events,
            createdPopups,
            placementInfo.placement,
            connection.port,
            popupSession,
        );
    } else {
        skipGroup(recorder, BEFORE_CLOSE_CASE_NAMES, beforeHandshake);
    }

    const beforeFallback = popupItems(await bridge.invoke("window.getAllWindows", {}));
    if (beforeFallback.length === 0) {
        placementInfo ??= await cornerPlacement(bridge);
        await runDevServerFallbackCases(
            bridge,
            recorder,
            createdPopups,
            placementInfo.placement,
            connection.port,
        );
    } else {
        skipGroup(recorder, DEV_SERVER_FALLBACK_CASE_NAMES, beforeFallback);
    }

    const beforeOriginGate = popupItems(await bridge.invoke("window.getAllWindows", {}));
    if (beforeOriginGate.length === 0) {
        placementInfo ??= await cornerPlacement(bridge);
        await runOriginGateCases(bridge, recorder, createdPopups, placementInfo.placement, connection.port);
    } else {
        skipGroup(recorder, ORIGIN_GATE_CASE_NAMES, beforeOriginGate);
    }
} catch (error) {
    fatalError = error;
    blocked = Boolean(error?.blocked);
    console.error(`${blocked ? "BLOCKED" : "ERROR"}: ${error?.message || error}`);
} finally {
    // The popup page's collector is not stopped: its page is about to be
    // destroyed with the window, and a dead session would only stall here.
    await closeClient(popupSession.client);
    if (bridge) {
        const quiet = (method, params) => bridge.invokeRaw(method, params).catch(() => undefined);
        for (const windowId of createdPopups) {
            await quiet("window.closePopup", { windowId });
        }
        // A beforeClose popup left pending force-closes itself after 3 s; the
        // wait keeps the next suite from starting with our popup still listed.
        for (const windowId of createdPopups) {
            await waitForPopupGone(bridge, windowId).catch(() => undefined);
        }
        if (events) await events.stop();
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
            eventTimeoutMs,
            popupReadyTimeoutMs,
            selfId,
            entryPopupCount,
            createdPopups,
            placement: placementInfo?.placement,
            screen: placementInfo?.screen,
            popupSession: { stage: popupSession.stage, waitedMs: popupSession.waitedMs },
            eventsSeen: (events?.received ?? []).map((entry) => entry.name),
            popupEventsSeen: (popupSession.events?.received ?? []).map((entry) => entry.name),
            targets,
        },
    }),
);

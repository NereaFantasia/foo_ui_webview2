/**
 * Checks the machine-assertable part of the window namespace: the read-only
 * state surface for internal agreement, and the geometry and state setters for
 * round-trip fidelity.
 *
 * Visual work (Mica, acrylic, corner preference), input work (drag regions,
 * system menu) and multi-monitor DPI stay out: their results need an eye, not an
 * assertion. Popup-only endpoints are out too, since they need a popup first.
 *
 * The mutating phase moves and resizes the real window, so it records the entry
 * state up front and puts everything back in the finally block: bounds, zoom,
 * title, always-on-top, size constraints, resizability, titlebar height, and
 * the entry maximize or minimize state. It runs only in standalone mode,
 * because the setters answer "unsupported" in panel mode.
 *
 * A second mutating group covers size constraints, resizability,
 * toggleMaximize, the four fullscreen endpoints, DPI zoom and focus/blur. Its
 * internal order is load-bearing; see the comment on that function. Fullscreen
 * cleanup always attempts exitFullscreen in the finally block, including when
 * a case fails, before attempting to restore the saved geometry.
 *
 * reload and refreshWebView are covered by e2e-window-reload.mjs, which tests
 * document replacement separately from a refresh that preserves page state.
 *
 * Usage: node mcp/tests/e2e-window-state.mjs
 */

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
const runId = new Date().toISOString().replace(/[-:.TZ]/g, "");

const MOVE_DELTA = 17;
const SIZE_DELTA = 24;
const TEST_ZOOM = 1.1;

// Size constraints travel as physical pixels but are stored as DIP, so a value
// that is not representable at the current scale reads back one pixel off
// (202 -> 203 at 125%). Every physical comparison in that group allows this.
const TOLERANCE_PX = 1;
const SHRINK_RATIO = 0.7;
const MIN_GROWTH = 40;
const MAX_SHRINK = 60;
const TITLEBAR_DELTA = 8;
const TEST_DPI = 120;

function rect(bounds) {
    return {
        x: bounds?.x,
        y: bounds?.y,
        width: bounds?.width,
        height: bounds?.height,
    };
}

function sameRect(a, b) {
    return JSON.stringify(rect(a)) === JSON.stringify(rect(b));
}

function allIntegers(bounds) {
    return ["x", "y", "width", "height"].every((key) => Number.isInteger(bounds?.[key]));
}

function within(actual, expected, tolerance = TOLERANCE_PX) {
    return (
        Number.isInteger(actual) &&
        Number.isInteger(expected) &&
        Math.abs(actual - expected) <= tolerance
    );
}

function near(actual, expected) {
    return typeof actual === "number" && Math.abs(actual - expected) < 1e-6;
}

/**
 * True when the answer has exactly these keys. Several endpoints in this group
 * answer with a success flag that carries no information (focus, blur); pinning
 * the key set is how those shapes become an expectation.
 */
function hasExactKeys(value, keys) {
    if (!value || typeof value !== "object" || Array.isArray(value)) return false;
    return JSON.stringify(Object.keys(value).sort()) === JSON.stringify([...keys].sort());
}

function isNormalState(state) {
    return (
        state?.isMaximized === false &&
        state?.isMinimized === false &&
        state?.isFullscreen === false
    );
}

/**
 * Reads window.getState until one flag reaches the expected value, then returns
 * that reading.
 *
 * minimize, maximize and restore return once the request is posted, not once
 * Windows has applied it, so a read taken immediately afterwards can still
 * describe the old state. Waiting on the condition instead of on a fixed delay
 * returns as soon as the state is genuinely there; on timeout it returns the
 * last reading, which then fails the case with the state it actually saw.
 */
async function waitForState(invoke, flag, expected, { timeoutMs = 3000, intervalMs = 50 } = {}) {
    const deadline = Date.now() + timeoutMs;
    let state = await invoke("window.getState", {});
    while (state?.[flag] !== expected && Date.now() < deadline) {
        await new Promise((resolve) => setTimeout(resolve, intervalMs));
        state = await invoke("window.getState", {});
    }
    return state;
}

async function runReadCases(bridge, recorder) {
    const [
        bounds,
        state,
        mode,
        currentId,
        allWindows,
        zoom,
        dpiScale,
        minSize,
        maxSize,
        titlebarHeight,
        captionButtons,
        titlebarInfo,
        alwaysOnTop,
        fullscreen,
        maximized,
        minimized,
        resizable,
    ] = await Promise.all(
        [
            "window.getBounds",
            "window.getState",
            "window.getMode",
            "window.getCurrentWindowId",
            "window.getAllWindows",
            "window.getZoom",
            "window.getDpiScale",
            "window.getMinSize",
            "window.getMaxSize",
            "window.getTitlebarHeight",
            "window.getCaptionButtonsWidth",
            "window.getTitlebarInfo",
            "window.isAlwaysOnTop",
            "window.isFullscreen",
            "window.isMaximized",
            "window.isMinimized",
            "window.isResizable",
        ].map((method) => bridge.invoke(method, {})),
    );

    recorder.assertCase(
        "W-01 getBounds reports an integer rectangle",
        allIntegers(bounds),
        bounds,
        { x: "int", y: "int", width: "int", height: "int" },
    );

    // getState carries every flag twice, once bare and once is-prefixed. Pages
    // read either spelling, so a divergence would break half of them silently.
    const aliasPairs = [
        ["maximized", "isMaximized"],
        ["minimized", "isMinimized"],
        ["fullscreen", "isFullscreen"],
        ["alwaysOnTop", "isAlwaysOnTop"],
        ["focused", "isFocused"],
    ];
    const aliasMismatches = aliasPairs.filter(([bare, prefixed]) => state[bare] !== state[prefixed]);
    recorder.assertCase(
        "W-02 getState bare and is-prefixed flags agree",
        aliasMismatches.length === 0,
        { state, mismatches: aliasMismatches },
        { mismatches: [] },
    );

    const predicateMismatches = [];
    if (state.isMaximized !== maximized.isMaximized) predicateMismatches.push("isMaximized");
    if (state.isMinimized !== minimized.minimized) predicateMismatches.push("isMinimized");
    if (state.isFullscreen !== fullscreen.isFullscreen) predicateMismatches.push("isFullscreen");
    if (state.isAlwaysOnTop !== alwaysOnTop.isAlwaysOnTop) predicateMismatches.push("isAlwaysOnTop");
    recorder.assertCase(
        "W-03 getState agrees with the single-flag endpoints",
        predicateMismatches.length === 0,
        {
            state: {
                isMaximized: state.isMaximized,
                isMinimized: state.isMinimized,
                isFullscreen: state.isFullscreen,
                isAlwaysOnTop: state.isAlwaysOnTop,
            },
            predicates: {
                isMaximized: maximized.isMaximized,
                isMinimized: minimized.minimized,
                isFullscreen: fullscreen.isFullscreen,
                isAlwaysOnTop: alwaysOnTop.isAlwaysOnTop,
            },
            mismatches: predicateMismatches,
        },
        { mismatches: [] },
    );

    recorder.assertCase(
        "W-04 the rectangle inside getState equals getBounds",
        sameRect(state, bounds),
        { state: rect(state), bounds: rect(bounds) },
        { equal: true },
    );

    const items = Array.isArray(allWindows?.items) ? allWindows.items : [];
    recorder.assertCase(
        "W-05 getCurrentWindowId is listed by getAllWindows",
        items.some((item) => item?.windowId === currentId?.windowId),
        { currentId: currentId?.windowId, listed: items.map((item) => item?.windowId) },
        { currentIdListed: true },
    );

    recorder.assertCase(
        "W-06 getTitlebarInfo agrees with the single-value titlebar endpoints",
        titlebarInfo?.height === titlebarHeight?.height &&
            titlebarInfo?.captionButtonsWidth === captionButtons?.width &&
            titlebarInfo?.captionButtonWidth === captionButtons?.buttonWidth,
        { titlebarInfo, titlebarHeight, captionButtons },
        { equalAcrossEndpoints: true },
    );

    recorder.assertCase(
        "W-07 getZoom and getDpiScale report the same display scale",
        zoom?.dpi === dpiScale?.dpi && zoom?.dpiScale === dpiScale?.scale,
        { zoom, dpiScale },
        { sameDpiAndScale: true },
    );

    // A zero max size means unbounded, which is why it cannot be compared
    // against the minimum the way a real maximum would be.
    const maxUnbounded = maxSize?.width === 0 && maxSize?.height === 0;
    recorder.assertCase(
        "W-08 getMinSize is positive and getMaxSize is unbounded or above it",
        minSize?.width > 0 &&
            minSize?.height > 0 &&
            (maxUnbounded ||
                (maxSize.width >= minSize.width && maxSize.height >= minSize.height)),
        { minSize, maxSize, maxUnbounded },
        { minPositive: true, maxUnboundedOrAboveMin: true },
    );

    // minSize, maxSize, titlebarHeight and resizable are part of the entry
    // snapshot: none of them has a "reset to default" endpoint, and setMinSize
    // without both dimensions collapses the floor to one DIP, so the only way
    // back is to write the entry values.
    return { bounds, state, mode, zoom, minSize, maxSize, titlebarHeight, resizable };
}

async function runMutatingCases(bridge, recorder, entry) {
    const { invoke } = bridge;

    if (entry.state.isFullscreen === true) {
        recorder.assertCase(
            "W-09 the window is in a state the geometry setters can be checked in",
            false,
            { reason: "entry state is fullscreen; leaving it alone" },
            { fullscreen: false },
        );
        return;
    }

    if (entry.state.isMinimized === true || entry.state.isMaximized === true) {
        await invoke("window.restore", {});
    }

    const normal = await invoke("window.getBounds", {});
    recorder.assertCase(
        "W-09 restore produces a normal on-screen rectangle",
        allIntegers(normal) && normal.width > 0 && normal.height > 0,
        { entryState: { minimized: entry.state.isMinimized, maximized: entry.state.isMaximized }, normal: rect(normal) },
        { width: "> 0", height: "> 0" },
    );

    const moved = {
        x: normal.x + MOVE_DELTA,
        y: normal.y + MOVE_DELTA,
        width: normal.width + SIZE_DELTA,
        height: normal.height + SIZE_DELTA,
    };
    await invoke("window.setBounds", moved);
    const afterSetBounds = await invoke("window.getBounds", {});
    recorder.assertCase(
        "W-10 setBounds round-trips through getBounds",
        sameRect(afterSetBounds, moved),
        { requested: moved, observed: rect(afterSetBounds) },
        { equal: true },
    );

    // Omitted keys are documented to keep their current value, which is what
    // lets a page move a window without knowing its size.
    await invoke("window.setBounds", { x: normal.x, y: normal.y });
    const afterPartial = await invoke("window.getBounds", {});
    recorder.assertCase(
        "W-11 setBounds keeps the fields it was not given",
        afterPartial.x === normal.x &&
            afterPartial.y === normal.y &&
            afterPartial.width === moved.width &&
            afterPartial.height === moved.height,
        { observed: rect(afterPartial), expectedSizeFrom: "previous call" },
        { x: normal.x, y: normal.y, width: moved.width, height: moved.height },
    );

    await invoke("window.setSize", { width: normal.width, height: normal.height });
    await invoke("window.setPosition", { x: normal.x + MOVE_DELTA, y: normal.y });
    const afterSplit = await invoke("window.getBounds", {});
    recorder.assertCase(
        "W-12 setSize and setPosition each move only their own axis pair",
        afterSplit.width === normal.width &&
            afterSplit.height === normal.height &&
            afterSplit.x === normal.x + MOVE_DELTA &&
            afterSplit.y === normal.y,
        { observed: rect(afterSplit) },
        {
            x: normal.x + MOVE_DELTA,
            y: normal.y,
            width: normal.width,
            height: normal.height,
        },
    );

    await invoke("window.center", {});
    const afterCenter = await invoke("window.getBounds", {});
    recorder.assertCase(
        "W-13 center changes position without changing size",
        afterCenter.width === afterSplit.width &&
            afterCenter.height === afterSplit.height &&
            (afterCenter.x !== afterSplit.x || afterCenter.y !== afterSplit.y),
        { before: rect(afterSplit), after: rect(afterCenter) },
        { sizeUnchanged: true, positionChanged: true },
    );

    // Reading the minimized rectangle is a known footgun: it reports where
    // Windows parks a minimized window, not the geometry to persist.
    await invoke("window.minimize", {});
    const minimizedState = await waitForState(invoke, "isMinimized", true);
    const minimizedBounds = await invoke("window.getBounds", {});
    await invoke("window.restore", {});
    await waitForState(invoke, "isMinimized", false);
    const restoredBounds = await invoke("window.getBounds", {});
    recorder.assertCase(
        "W-14 bounds read while minimized are not the restored geometry",
        minimizedState.isMinimized === true &&
            !sameRect(minimizedBounds, restoredBounds) &&
            sameRect(restoredBounds, afterCenter),
        { minimizedBounds: rect(minimizedBounds), restoredBounds: rect(restoredBounds) },
        { differsWhileMinimized: true, restoredEqualsPrevious: true },
    );

    await invoke("window.maximize", {});
    const maximizedState = await waitForState(invoke, "isMaximized", true);
    await invoke("window.restore", {});
    await waitForState(invoke, "isMaximized", false);
    const afterRestore = await invoke("window.getBounds", {});
    recorder.assertCase(
        "W-15 maximize is reported by getState and restore brings the rectangle back",
        maximizedState.isMaximized === true && sameRect(afterRestore, afterCenter),
        { maximizedState: rect(maximizedState), afterRestore: rect(afterRestore) },
        { isMaximized: true, restoredEqualsPrevious: true },
    );

    const entryAlwaysOnTop = entry.state.isAlwaysOnTop === true;
    await invoke("window.setAlwaysOnTop", { enabled: !entryAlwaysOnTop });
    const flipped = await invoke("window.isAlwaysOnTop", {});
    await invoke("window.toggleAlwaysOnTop", {});
    const toggledBack = await invoke("window.isAlwaysOnTop", {});
    recorder.assertCase(
        "W-16 setAlwaysOnTop and toggleAlwaysOnTop are both reflected",
        flipped.isAlwaysOnTop === !entryAlwaysOnTop &&
            toggledBack.isAlwaysOnTop === entryAlwaysOnTop,
        { entryAlwaysOnTop, afterSet: flipped.isAlwaysOnTop, afterToggle: toggledBack.isAlwaysOnTop },
        { afterSet: !entryAlwaysOnTop, afterToggle: entryAlwaysOnTop },
    );

    const setZoom = await invoke("window.setZoom", { zoom: TEST_ZOOM });
    const readZoom = await invoke("window.getZoom", {});
    await invoke("window.resetZoom", {});
    const resetZoom = await invoke("window.getZoom", {});
    recorder.assertCase(
        "W-17 setZoom is reflected by getZoom and resetZoom returns to 1",
        Math.abs(setZoom?.zoom - TEST_ZOOM) < 1e-6 &&
            Math.abs(readZoom?.zoom - TEST_ZOOM) < 1e-6 &&
            Math.abs(resetZoom?.zoom - 1) < 1e-6,
        { setZoom: setZoom?.zoom, readZoom: readZoom?.zoom, afterReset: resetZoom?.zoom },
        { setZoom: TEST_ZOOM, readZoom: TEST_ZOOM, afterReset: 1 },
    );

    const testTitle = `foo_ui_webview2 e2e ${runId}`;
    await invoke("window.setTitle", { title: testTitle });
    const readTitle = await invoke("window.getTitle", {});
    recorder.assertCase(
        "W-18 setTitle round-trips through getTitle",
        readTitle?.title === testTitle,
        { readTitle: readTitle?.title },
        { readTitle: testTitle },
    );

    return { normal: rect(afterCenter) };
}

/**
 * Covers the rest of the geometry, state and focus surface. The order is
 * load-bearing:
 *
 * 1. titlebar height first, with each set immediately followed by its read.
 *    Every chrome apply (setResizable, a fullscreen transition, an activation
 *    change) silently resets the height to the DPI default, so nothing may sit
 *    between a set and the read that checks it.
 * 2. size constraints need the normal state: the revalidation that makes
 *    setMinSize grow the window returns early when maximized or fullscreen.
 * 3. setResizable changes non-client thickness, so it runs after the geometry
 *    cases and no rectangle is compared across it.
 * 4. toggleMaximize, then fullscreen. The fullscreen answer is final when the
 *    handler returns (the flag is set before SetWindowPos), so it is read
 *    directly; toggleMaximize posts its message and reports a prediction, so
 *    its state is polled.
 * 5. zoom, then focus/blur last: focus steals the foreground and, like the
 *    other activation changes, resets the titlebar height.
 */
async function runConstraintAndStateCases(bridge, recorder, entry, geometry) {
    const { invoke, invokeRaw } = bridge;
    const mainWindowId = entry.minSize?.windowId;

    const savedBounds = await invoke("window.hasSavedBounds", {});
    recorder.assertCase(
        "W-19 hasSavedBounds answers success, a boolean flag and a description",
        savedBounds?.success === true &&
            typeof savedBounds?.hasSavedBounds === "boolean" &&
            typeof savedBounds?.description === "string" &&
            savedBounds.description.length > 0 &&
            hasExactKeys(savedBounds, ["success", "hasSavedBounds", "description"]),
        savedBounds,
        {
            keys: ["success", "hasSavedBounds", "description"],
            hasSavedBounds: "boolean (true on any instance that has ever been dragged or closed)",
        },
    );

    // ---- titlebar height: set and read stay adjacent ----
    const entryTitlebar = entry.titlebarHeight?.height;
    const testTitlebar = entryTitlebar >= 92 ? entryTitlebar - TITLEBAR_DELTA : entryTitlebar + TITLEBAR_DELTA;
    const setTitlebar = await invoke("window.setTitlebarHeight", { height: testTitlebar });
    const readTitlebar = await invoke("window.getTitlebarHeight", {});
    const readTitlebarInfo = await invoke("window.getTitlebarInfo", {});
    recorder.assertCase(
        "W-20 setTitlebarHeight echoes the height and both titlebar readers report it right away",
        setTitlebar?.success === true &&
            setTitlebar?.height === testTitlebar &&
            hasExactKeys(setTitlebar, ["success", "height"]) &&
            readTitlebar?.height === testTitlebar &&
            readTitlebarInfo?.height === testTitlebar,
        {
            entryHeight: entryTitlebar,
            requested: testTitlebar,
            setTitlebar,
            readTitlebar: readTitlebar?.height,
            readTitlebarInfo: readTitlebarInfo?.height,
        },
        { setTitlebar: { success: true, height: testTitlebar }, readers: testTitlebar },
    );

    const defaultTitlebar = await invoke("window.setTitlebarHeight", {});
    const readDefault = await invoke("window.getTitlebarHeight", {});
    const tooSmall = await invokeRaw("window.setTitlebarHeight", { height: 23 });
    const tooLarge = await invokeRaw("window.setTitlebarHeight", { height: 101 });
    const readAfterRejects = await invoke("window.getTitlebarHeight", {});
    const rejected = (outcome) =>
        outcome?.kind === "result" &&
        outcome.value?.success === false &&
        /between 24 and 100/i.test(outcome.value?.error ?? "") &&
        outcome.value?.code === "INVALID_PARAMS";
    recorder.assertCase(
        "W-21 setTitlebarHeight without a height falls back to 32 rather than the DPI-scaled default, and rejects 23 and 101 with INVALID_PARAMS, leaving the height alone",
        defaultTitlebar?.success === true &&
            defaultTitlebar?.height === 32 &&
            readDefault?.height === 32 &&
            rejected(tooSmall) &&
            rejected(tooLarge) &&
            readAfterRejects?.height === 32,
        {
            dpi: entry.zoom?.dpi,
            dpiScaledDefault: entryTitlebar,
            defaultTitlebar,
            readDefault: readDefault?.height,
            tooSmall: tooSmall?.value,
            tooLarge: tooLarge?.value,
            readAfterRejects: readAfterRejects?.height,
        },
        {
            defaultTitlebar: { success: true, height: 32 },
            rejects: { success: false, error: "Height must be between 24 and 100", code: "INVALID_PARAMS" },
            readAfterRejects: 32,
        },
    );

    const restoreTitlebar = await invoke("window.setTitlebarHeight", { height: entryTitlebar });
    const readRestored = await invoke("window.getTitlebarHeight", {});
    recorder.assertCase(
        "W-22 setTitlebarHeight writes the entry height back and getTitlebarHeight confirms it",
        restoreTitlebar?.success === true &&
            restoreTitlebar?.height === entryTitlebar &&
            readRestored?.height === entryTitlebar,
        { restoreTitlebar, readRestored: readRestored?.height },
        { height: entryTitlebar },
    );

    // ---- size constraints: need the normal state ----
    const entryMin = entry.minSize;
    const entryMax = entry.maxSize;
    const constraintGate = await invoke("window.getState", {});
    if (!isNormalState(constraintGate)) {
        recorder.assertCase(
            "W-23 the window is in the normal state the size-constraint group needs; skipped",
            false,
            {
                isMaximized: constraintGate?.isMaximized,
                isMinimized: constraintGate?.isMinimized,
                isFullscreen: constraintGate?.isFullscreen,
            },
            { isMaximized: false, isMinimized: false, isFullscreen: false },
        );
    } else {
        const startBounds = await invoke("window.getBounds", {});

        const floorSet = await invoke("window.setMinSize", {});
        const floorRead = await invoke("window.getMinSize", {});
        const afterFloor = await invoke("window.getBounds", {});
        recorder.assertCase(
            "W-23 setMinSize without width and height collapses the floor to one DIP and leaves the rectangle alone",
            floorSet?.success === true &&
                floorSet?.windowId === mainWindowId &&
                hasExactKeys(floorSet, ["success", "windowId"]) &&
                within(floorRead?.width, 1) &&
                within(floorRead?.height, 1) &&
                floorRead?.windowId === mainWindowId &&
                sameRect(afterFloor, startBounds),
            { entryMin, floorSet, floorRead, afterFloor: rect(afterFloor), startBounds: rect(startBounds) },
            {
                floorSet: { success: true, windowId: mainWindowId },
                floorRead: { width: "1 DIP in physical px (1..2)", height: "1 DIP in physical px (1..2)" },
                rectUnchanged: true,
            },
        );

        const shrunk = {
            width: Math.max(Math.round(startBounds.width * SHRINK_RATIO), 200),
            height: Math.max(Math.round(startBounds.height * SHRINK_RATIO), 150),
        };
        await invoke("window.setSize", shrunk);
        const afterShrink = await invoke("window.getBounds", {});
        recorder.assertCase(
            "W-24 with the floor lowered setSize shrinks the window to seventy percent without moving it",
            afterShrink?.width === shrunk.width &&
                afterShrink?.height === shrunk.height &&
                afterShrink?.x === startBounds.x &&
                afterShrink?.y === startBounds.y,
            { requested: shrunk, observed: rect(afterShrink) },
            { width: shrunk.width, height: shrunk.height, x: startBounds.x, y: startBounds.y },
        );

        const raisedMin = { width: shrunk.width + MIN_GROWTH, height: shrunk.height + MIN_GROWTH };
        const raiseSet = await invoke("window.setMinSize", raisedMin);
        const raisedRead = await invoke("window.getMinSize", {});
        const afterRaise = await invoke("window.getBounds", {});
        recorder.assertCase(
            "W-25 setMinSize above the current size grows the window at once, without a setSize, and getMinSize echoes the request within one pixel",
            raiseSet?.success === true &&
                within(raisedRead?.width, raisedMin.width) &&
                within(raisedRead?.height, raisedMin.height) &&
                within(afterRaise?.width, raisedMin.width) &&
                within(afterRaise?.height, raisedMin.height) &&
                afterRaise?.x === afterShrink.x &&
                afterRaise?.y === afterShrink.y,
            {
                requested: raisedMin,
                raiseSet,
                raisedRead,
                afterRaise: rect(afterRaise),
                readEqualsRect: raisedRead?.width === afterRaise?.width && raisedRead?.height === afterRaise?.height,
            },
            { getMinSize: "requested ±1", rect: "requested ±1, position unchanged" },
        );

        const belowMin = await invoke("window.setSize", shrunk);
        const afterBelowMin = await invoke("window.getBounds", {});
        recorder.assertCase(
            "W-26 setSize below the minimum answers a bare success with no clamp information, while the window is held at the minimum",
            hasExactKeys(belowMin, ["success"]) &&
                belowMin.success === true &&
                within(afterBelowMin?.width, raisedMin.width) &&
                within(afterBelowMin?.height, raisedMin.height),
            { requested: shrunk, belowMin, afterBelowMin: rect(afterBelowMin), minimum: raisedMin },
            {
                belowMin: { success: true },
                clampInformation: "absent (the promise in the getMinSize comment is not implemented)",
                rect: "minimum ±1",
            },
        );

        const cap = {
            width: afterBelowMin.width - MAX_SHRINK,
            height: afterBelowMin.height - MAX_SHRINK,
        };
        await invoke("window.setMinSize", {});
        const capSet = await invoke("window.setMaxSize", cap);
        const capRead = await invoke("window.getMaxSize", {});
        const afterCap = await invoke("window.getBounds", {});
        recorder.assertCase(
            "W-27 setMaxSize below the current size shrinks the window at once and getMaxSize echoes the request within one pixel",
            capSet?.success === true &&
                capSet?.windowId === mainWindowId &&
                hasExactKeys(capSet, ["success", "windowId"]) &&
                within(capRead?.width, cap.width) &&
                within(capRead?.height, cap.height) &&
                capRead?.windowId === mainWindowId &&
                within(afterCap?.width, cap.width) &&
                within(afterCap?.height, cap.height) &&
                afterCap?.x === afterBelowMin.x &&
                afterCap?.y === afterBelowMin.y,
            { requested: cap, capSet, capRead, afterCap: rect(afterCap) },
            {
                capSet: { success: true, windowId: mainWindowId },
                getMaxSize: "requested ±1",
                rect: "requested ±1, position unchanged",
            },
        );

        const liftSet = await invoke("window.setMaxSize", { width: 0, height: 0 });
        const liftRead = await invoke("window.getMaxSize", {});
        await invoke("window.setSize", { width: startBounds.width, height: startBounds.height });
        const afterRegrow = await invoke("window.getBounds", {});
        recorder.assertCase(
            "W-28 setMaxSize zero lifts the ceiling: getMaxSize reads 0x0 and the window can grow past the old maximum",
            liftSet?.success === true &&
                liftRead?.width === 0 &&
                liftRead?.height === 0 &&
                afterRegrow?.width === startBounds.width &&
                afterRegrow?.height === startBounds.height,
            { liftSet, liftRead, oldMaximum: cap, afterRegrow: rect(afterRegrow) },
            { liftRead: { width: 0, height: 0 }, rect: { width: startBounds.width, height: startBounds.height } },
        );

        await invoke("window.setMinSize", { width: entryMin.width, height: entryMin.height });
        await invoke("window.setMaxSize", { width: entryMax.width, height: entryMax.height });
        const minBack = await invoke("window.getMinSize", {});
        const maxBack = await invoke("window.getMaxSize", {});
        const boundsBack = await invoke("window.getBounds", {});
        recorder.assertCase(
            "W-29 the entry minimum and maximum are written back within one pixel and the rectangle is where the group started",
            within(minBack?.width, entryMin.width) &&
                within(minBack?.height, entryMin.height) &&
                within(maxBack?.width, entryMax.width) &&
                within(maxBack?.height, entryMax.height) &&
                sameRect(boundsBack, startBounds),
            { entryMin, entryMax, minBack, maxBack, boundsBack: rect(boundsBack), startBounds: rect(startBounds) },
            { minBack: "entry ±1", maxBack: "entry ±1", rect: "equal to the group start" },
        );
    }

    // ---- resizability: no rectangle comparison across these calls ----
    const entryResizable = entry.resizable?.resizable === true;
    const flipSet = await invoke("window.setResizable", { resizable: !entryResizable });
    const flipRead = await invoke("window.isResizable", {});
    const flipAgain = await invoke("window.setResizable", { resizable: !entryResizable });
    recorder.assertCase(
        "W-30 setResizable is reflected by isResizable and writing the same value again still succeeds",
        flipSet?.success === true &&
            flipSet?.windowId === mainWindowId &&
            hasExactKeys(flipSet, ["success", "windowId"]) &&
            flipRead?.resizable === !entryResizable &&
            flipRead?.windowId === mainWindowId &&
            hasExactKeys(flipRead, ["success", "resizable", "windowId"]) &&
            flipAgain?.success === true,
        { entryResizable, flipSet, flipRead, flipAgain },
        {
            flipSet: { success: true, windowId: mainWindowId },
            flipRead: { success: true, resizable: !entryResizable, windowId: mainWindowId },
            flipAgain: { success: true },
        },
    );

    const defaultSet = await invoke("window.setResizable", {});
    const defaultRead = await invoke("window.isResizable", {});
    const backSet = await invoke("window.setResizable", { resizable: entryResizable });
    const backRead = await invoke("window.isResizable", {});
    recorder.assertCase(
        "W-31 setResizable without the flag defaults to resizable, and the entry value is written back",
        defaultSet?.success === true &&
            defaultRead?.resizable === true &&
            backSet?.success === true &&
            backRead?.resizable === entryResizable,
        { defaultSet, defaultRead, backSet, backRead },
        { defaultRead: { resizable: true }, backRead: { resizable: entryResizable } },
    );

    // ---- toggleMaximize: the answer is a prediction, so the state is polled ----
    const beforeToggle = await invoke("window.getBounds", {});
    const toggleOn = await invoke("window.toggleMaximize", {});
    const maximizedByToggle = await waitForState(invoke, "isMaximized", true);
    recorder.assertCase(
        "W-32 toggleMaximize from normal predicts maximized true and getState confirms it after the posted message lands",
        hasExactKeys(toggleOn, ["success", "maximized"]) &&
            toggleOn.success === true &&
            toggleOn.maximized === true &&
            maximizedByToggle?.isMaximized === true,
        { toggleOn, polledIsMaximized: maximizedByToggle?.isMaximized },
        { toggleOn: { success: true, maximized: true }, polledIsMaximized: true },
    );

    const toggleOff = await invoke("window.toggleMaximize", {});
    const restoredByToggle = await waitForState(invoke, "isMaximized", false);
    const afterToggle = await invoke("window.getBounds", {});
    recorder.assertCase(
        "W-33 toggleMaximize from maximized predicts maximized false and the rectangle comes back",
        toggleOff?.success === true &&
            toggleOff?.maximized === false &&
            restoredByToggle?.isMaximized === false &&
            sameRect(afterToggle, beforeToggle),
        { toggleOff, polledIsMaximized: restoredByToggle?.isMaximized, before: rect(beforeToggle), after: rect(afterToggle) },
        { toggleOff: { success: true, maximized: false }, polledIsMaximized: false, rectRestored: true },
    );

    // ---- fullscreen: measured just before, answers are final on return ----
    const fullscreenGate = await invoke("window.getState", {});
    if (!isNormalState(fullscreenGate)) {
        recorder.assertCase(
            "W-34 the window is in the normal state the fullscreen group needs; skipped",
            false,
            {
                isMaximized: fullscreenGate?.isMaximized,
                isMinimized: fullscreenGate?.isMinimized,
                isFullscreen: fullscreenGate?.isFullscreen,
            },
            { isMaximized: false, isMinimized: false, isFullscreen: false },
        );
    } else {
        const beforeFullscreen = await invoke("window.getBounds", {});
        const enter = await invoke("window.enterFullscreen", {});
        const enteredFlag = await invoke("window.isFullscreen", {});
        const enteredState = await invoke("window.getState", {});
        const fullscreenBounds = await invoke("window.getBounds", {});
        recorder.assertCase(
            "W-34 enterFullscreen answers success with isFullscreen true, and both readers agree without polling",
            hasExactKeys(enter, ["success", "isFullscreen"]) &&
                enter.success === true &&
                enter.isFullscreen === true &&
                enteredFlag?.isFullscreen === true &&
                enteredState?.isFullscreen === true &&
                fullscreenBounds?.width >= beforeFullscreen?.width &&
                fullscreenBounds?.height >= beforeFullscreen?.height,
            {
                enter,
                enteredFlag: enteredFlag?.isFullscreen,
                enteredState: enteredState?.isFullscreen,
                before: rect(beforeFullscreen),
                fullscreen: rect(fullscreenBounds),
            },
            { enter: { success: true, isFullscreen: true }, readers: true, rectNotSmaller: true },
        );

        // The revalidation returns early while fullscreen, so a minimum larger
        // than the monitor must be stored without touching the rectangle.
        const oversizeMin = {
            width: fullscreenBounds.width + 100,
            height: fullscreenBounds.height + 100,
        };
        const oversizeSet = await invoke("window.setMinSize", oversizeMin);
        const oversizeRead = await invoke("window.getMinSize", {});
        const afterOversize = await invoke("window.getBounds", {});
        await invoke("window.setMinSize", { width: entryMin.width, height: entryMin.height });
        const minRestoredInFullscreen = await invoke("window.getMinSize", {});
        recorder.assertCase(
            "W-35 setMinSize while fullscreen stores the value but leaves the fullscreen rectangle alone",
            oversizeSet?.success === true &&
                within(oversizeRead?.width, oversizeMin.width) &&
                within(oversizeRead?.height, oversizeMin.height) &&
                sameRect(afterOversize, fullscreenBounds) &&
                within(minRestoredInFullscreen?.width, entryMin.width) &&
                within(minRestoredInFullscreen?.height, entryMin.height),
            {
                requested: oversizeMin,
                oversizeRead,
                afterOversize: rect(afterOversize),
                fullscreen: rect(fullscreenBounds),
                minRestoredInFullscreen,
            },
            { getMinSize: "requested ±1", rectUnchanged: true, entryMinWrittenBack: true },
        );

        const enterAgain = await invokeRaw("window.enterFullscreen", {});
        recorder.assertCase(
            "W-36 enterFullscreen while already fullscreen fails with OPERATION_FAILED and no state field",
            enterAgain?.kind === "result" &&
                hasExactKeys(enterAgain.value, ["success", "error", "code"]) &&
                enterAgain.value.success === false &&
                enterAgain.value.code === "OPERATION_FAILED",
            enterAgain,
            { kind: "result", value: { success: false, error: "Window is already fullscreen", code: "OPERATION_FAILED" } },
        );

        const exit = await invoke("window.exitFullscreen", {});
        const exitedFlag = await invoke("window.isFullscreen", {});
        const afterExit = await invoke("window.getBounds", {});
        recorder.assertCase(
            "W-37 exitFullscreen answers success with isFullscreen false and restores the pre-fullscreen rectangle",
            hasExactKeys(exit, ["success", "isFullscreen"]) &&
                exit.success === true &&
                exit.isFullscreen === false &&
                exitedFlag?.isFullscreen === false &&
                sameRect(afterExit, beforeFullscreen),
            { exit, exitedFlag: exitedFlag?.isFullscreen, before: rect(beforeFullscreen), after: rect(afterExit) },
            { exit: { success: true, isFullscreen: false }, exitedFlag: false, rectRestored: true },
        );

        const exitAgain = await invokeRaw("window.exitFullscreen", {});
        recorder.assertCase(
            "W-38 exitFullscreen while not fullscreen fails the same way",
            exitAgain?.kind === "result" &&
                hasExactKeys(exitAgain.value, ["success", "error", "code"]) &&
                exitAgain.value.success === false &&
                exitAgain.value.code === "OPERATION_FAILED",
            exitAgain,
            { kind: "result", value: { success: false, error: "Window is not fullscreen", code: "OPERATION_FAILED" } },
        );

        const setOn = await invoke("window.setFullscreen", {});
        const setOnAgain = await invoke("window.setFullscreen", { enabled: true });
        const setOnFlag = await invoke("window.isFullscreen", {});
        const setOff = await invoke("window.setFullscreen", { enabled: false });
        const setOffFlag = await invoke("window.isFullscreen", {});
        recorder.assertCase(
            "W-39 setFullscreen defaults to enabled, is idempotent, and reports the state under fullscreen rather than isFullscreen",
            hasExactKeys(setOn, ["success", "fullscreen"]) &&
                setOn.success === true &&
                setOn.fullscreen === true &&
                hasExactKeys(setOnAgain, ["success", "fullscreen"]) &&
                setOnAgain.success === true &&
                setOnAgain.fullscreen === true &&
                setOnFlag?.isFullscreen === true &&
                hasExactKeys(setOff, ["success", "fullscreen"]) &&
                setOff.success === true &&
                setOff.fullscreen === false &&
                setOffFlag?.isFullscreen === false,
            { setOn, setOnAgain, setOnFlag: setOnFlag?.isFullscreen, setOff, setOffFlag: setOffFlag?.isFullscreen },
            {
                setOn: { success: true, fullscreen: true },
                setOnAgain: { success: true, fullscreen: true },
                setOff: { success: true, fullscreen: false },
                fieldName: "fullscreen (enter/exit use isFullscreen)",
            },
        );

        const toggleIn = await invoke("window.toggleFullscreen", {});
        const toggleInFlag = await invoke("window.isFullscreen", {});
        const toggleOut = await invoke("window.toggleFullscreen", {});
        const toggleOutFlag = await invoke("window.isFullscreen", {});
        const afterToggles = await invoke("window.getBounds", {});
        recorder.assertCase(
            "W-40 toggleFullscreen twice enters and leaves, reporting fullscreen each time, and the rectangle survives",
            hasExactKeys(toggleIn, ["success", "fullscreen"]) &&
                toggleIn.success === true &&
                toggleIn.fullscreen === true &&
                toggleInFlag?.isFullscreen === true &&
                hasExactKeys(toggleOut, ["success", "fullscreen"]) &&
                toggleOut.success === true &&
                toggleOut.fullscreen === false &&
                toggleOutFlag?.isFullscreen === false &&
                sameRect(afterToggles, beforeFullscreen),
            {
                toggleIn,
                toggleInFlag: toggleInFlag?.isFullscreen,
                toggleOut,
                toggleOutFlag: toggleOutFlag?.isFullscreen,
                before: rect(beforeFullscreen),
                after: rect(afterToggles),
            },
            {
                toggleIn: { success: true, fullscreen: true },
                toggleOut: { success: true, fullscreen: false },
                rectRestored: true,
            },
        );
    }

    // ---- zoom for DPI: always with an explicit dpi ----
    const zoomForTestDpi = await invoke("window.setZoomForDpi", { dpi: TEST_DPI });
    const readZoomTestDpi = await invoke("window.getZoom", {});
    const zoomFor96 = await invoke("window.setZoomForDpi", { dpi: 96 });
    const readZoom96 = await invoke("window.getZoom", {});
    recorder.assertCase(
        "W-41 setZoomForDpi with an explicit dpi sets the controller zoom to dpi/96, getZoom reads the same value, and dpi 96 means exactly 1",
        hasExactKeys(zoomForTestDpi, ["success", "dpi", "zoom"]) &&
            zoomForTestDpi.success === true &&
            zoomForTestDpi.dpi === TEST_DPI &&
            near(zoomForTestDpi.zoom, TEST_DPI / 96) &&
            near(readZoomTestDpi?.zoom, TEST_DPI / 96) &&
            zoomFor96?.success === true &&
            zoomFor96?.dpi === 96 &&
            zoomFor96?.zoom === 1 &&
            readZoom96?.zoom === 1,
        {
            zoomForTestDpi,
            readZoomTestDpi: readZoomTestDpi?.zoom,
            zoomFor96,
            readZoom96: readZoom96?.zoom,
            entryZoom: entry.zoom?.zoom,
        },
        {
            zoomForTestDpi: { success: true, dpi: TEST_DPI, zoom: TEST_DPI / 96 },
            readZoomTestDpi: TEST_DPI / 96,
            zoomFor96: { success: true, dpi: 96, zoom: 1 },
            readZoom96: 1,
        },
    );

    // ---- focus / blur last: success carries no outcome, getState does ----
    const focusResponse = await invoke("window.focus", {});
    const focusedState = await waitForState(invoke, "focused", true);
    recorder.assertCase(
        "W-42 focus answers a bare success that carries no outcome, and getState.focused turns true once the foreground has moved",
        hasExactKeys(focusResponse, ["success"]) &&
            focusResponse.success === true &&
            focusedState?.focused === true,
        { focusResponse, polledFocused: focusedState?.focused, polledIsFocused: focusedState?.isFocused },
        { focusResponse: { success: true }, polledFocused: true },
    );

    const blurResponse = await invoke("window.blur", {});
    const blurredState = await waitForState(invoke, "focused", false, { timeoutMs: 1500 });
    recorder.assertCase(
        "W-43 blur answers the same bare success; focused turning false is observed but not required, since under CDP fb2k is usually not foreground and blur hands focus to an arbitrary z-order neighbour",
        hasExactKeys(blurResponse, ["success"]) && blurResponse.success === true,
        {
            blurResponse,
            focusedBeforeBlur: focusedState?.focused,
            focusedAfterBlur: blurredState?.focused,
            blurTookEffect: focusedState?.focused === true && blurredState?.focused === false,
        },
        {
            blurResponse: { success: true },
            focusedAfterBlur: "observation only",
        },
    );
}

async function restoreEntryState(bridge, entry, geometry) {
    const { invokeRaw } = bridge;
    const quiet = (method, params) => invokeRaw(method, params).catch(() => undefined);

    // Unconditional: when the window is not fullscreen this fails with
    // OPERATION_FAILED, which quiet ignores. Leaving fullscreen first makes every
    // geometry write below land on the normal window.
    await quiet("window.exitFullscreen", {});

    await quiet("window.setTitle", { title: entry.title });
    await quiet("window.setZoom", { zoom: entry.zoom?.zoom ?? 1 });
    await quiet("window.setAlwaysOnTop", { enabled: entry.state.isAlwaysOnTop === true });

    if (typeof entry.resizable?.resizable === "boolean") {
        await quiet("window.setResizable", { resizable: entry.resizable.resizable });
    }
    // Both dimensions must be present: an omitted one collapses that floor to
    // one DIP, which is worse than leaving the test value in place.
    if (Number.isInteger(entry.minSize?.width) && Number.isInteger(entry.minSize?.height)) {
        await quiet("window.setMinSize", { width: entry.minSize.width, height: entry.minSize.height });
    }
    if (Number.isInteger(entry.maxSize?.width) && Number.isInteger(entry.maxSize?.height)) {
        await quiet("window.setMaxSize", { width: entry.maxSize.width, height: entry.maxSize.height });
    }

    if (geometry?.normal) {
        await quiet("window.setBounds", geometry.normal);
    }
    // After the chrome-touching writes above (exitFullscreen, setResizable),
    // each of which resets the titlebar height to the DPI default.
    if (Number.isInteger(entry.titlebarHeight?.height)) {
        await quiet("window.setTitlebarHeight", { height: entry.titlebarHeight.height });
    }
    if (entry.state.isMaximized === true) {
        await quiet("window.maximize", {});
    }
    if (entry.state.isMinimized === true) {
        await quiet("window.minimize", {});
    }
}

const recorder = createRecorder();
let client;
let bridge;
let blocked = false;
let fatalError;
let entry;
let geometry;
let targets;
let mutated = false;

try {
    const connection = await connectBridgePage();
    client = connection.client;
    targets = connection.candidates;
    console.log(`Target: ${connection.page.title} ${connection.page.url}`);
    bridge = createBridge(client.Runtime, { invokeTimeoutMs });
    await requireResponsiveBridge(bridge, { probeId: runId });

    const read = await runReadCases(bridge, recorder);
    entry = {
        state: read.state,
        zoom: read.zoom,
        title: (await bridge.invoke("window.getTitle", {})).title,
        minSize: read.minSize,
        maxSize: read.maxSize,
        titlebarHeight: read.titlebarHeight,
        resizable: read.resizable,
    };

    if (read.mode?.mode === "standalone") {
        mutated = true;
        geometry = await runMutatingCases(bridge, recorder, entry);
        // geometry is undefined when the entry state was fullscreen and the
        // first mutating group left the window alone; the second group then
        // stays out for the same reason.
        if (geometry) {
            await runConstraintAndStateCases(bridge, recorder, entry, geometry);
        }
    } else {
        recorder.assertCase(
            "W-09 the geometry setters are supported in this hosting mode",
            false,
            { mode: read.mode },
            { mode: "standalone" },
        );
    }
} catch (error) {
    fatalError = error;
    blocked = Boolean(error?.blocked);
    console.error(`${blocked ? "BLOCKED" : "ERROR"}: ${error?.message || error}`);
} finally {
    if (bridge && entry && mutated) {
        await restoreEntryState(bridge, entry, geometry);
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
            entryState: entry
                ? {
                      minimized: entry.state.isMinimized,
                      maximized: entry.state.isMaximized,
                      fullscreen: entry.state.isFullscreen,
                      alwaysOnTop: entry.state.isAlwaysOnTop,
                      minSize: entry.minSize ? { width: entry.minSize.width, height: entry.minSize.height } : undefined,
                      maxSize: entry.maxSize ? { width: entry.maxSize.width, height: entry.maxSize.height } : undefined,
                      resizable: entry.resizable?.resizable,
                      titlebarHeight: entry.titlebarHeight?.height,
                      zoom: entry.zoom?.zoom,
                  }
                : undefined,
            targets,
        },
    }),
);

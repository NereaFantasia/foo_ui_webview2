/**
 * Checks the window chrome surface of the window namespace: backdrop materials
 * through the compatibility setters (setMica / setMicaEffect / setAcrylic /
 * setBlur / setDarkMode / setBackgroundTransparency) and through the standard
 * policy (getBackdropPolicy / setBackdropPolicy), corner preference, the
 * frameless toggle, drag-region registration, the two flash endpoints and the
 * dev-server configuration.
 *
 * Almost nothing here has a getter. What can be read back is
 * getBackdropPolicy.resolvedBackdropPolicy, getCornerPreference, the outer
 * window rectangle, and the window:backdropStateChanged event that every chrome
 * apply broadcasts. useBlurBehind, transparentBackground, the drag rectangles
 * and the frameless flag have no read path at all (getAllWindows does not
 * serialise the resolved chrome state), so their cases assert the envelope and
 * say so in their names.
 *
 * Order is load-bearing. An explicit key written by setBackdropPolicy suppresses
 * the compatibility setters until it is deleted with null - writing "inherit"
 * back is not a delete - so the compatibility group runs first, the policy
 * group runs last and deletes what it wrote. Inside the compatibility group
 * setDarkMode goes first (the darkMode parameter of setMica / setAcrylic writes
 * the same slot), setBlur goes after the material setters (they clear blur), and
 * setBackgroundTransparency goes last (any later apply re-pushes it). Every
 * chrome apply also resets the titlebar height to the DPI default; the suite
 * provokes that on purpose with a sentinel height and repairs it at the end.
 *
 * Environment: FB2K_E2E_ALLOW_FLASH=1 lets flash and flashTaskbar really flash
 * once (otherwise only the stop shape is sent); FB2K_E2E_SKIP_FRAMELESS=1 skips
 * the frameless round trip, which a person should watch on its first run.
 *
 * Usage: node mcp/tests/e2e-window-chrome.mjs
 */

import os from "node:os";

import {
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
const runId = new Date().toISOString().replace(/[-:.TZ]/g, "");

const BACKDROP_EVENT = "window:backdropStateChanged";
const ACTIVE_EFFECTS = ["inherit", "none", "mica", "mica-alt", "acrylic"];
const INACTIVE_EFFECTS = [...ACTIVE_EFFECTS, "system"];
const MANAGED_EFFECTS = ["mica", "mica-alt", "acrylic"];
const CORNER_MODES = ["small", "none", "round"];
const POLICY_KEYS_TOUCHED = ["zzz", "activeEffect", "darkMode"];
const TITLEBAR_DEFAULT_DIP = 32;
const SETTLE_EVENT_MS = 800;

function envFlag(name) {
    const raw = process.env[name];
    return raw !== undefined && raw !== "" && raw !== "0" && raw.toLowerCase() !== "false";
}

const allowFlash = envFlag("FB2K_E2E_ALLOW_FLASH");
const skipFrameless = envFlag("FB2K_E2E_SKIP_FRAMELESS");

function canon(value) {
    if (Array.isArray(value)) return `[${value.map(canon).join(",")}]`;
    if (value && typeof value === "object") {
        return `{${Object.keys(value)
            .sort()
            .map((key) => `${JSON.stringify(key)}:${canon(value[key])}`)
            .join(",")}}`;
    }
    return JSON.stringify(value);
}

function deepEqual(a, b) {
    return canon(a) === canon(b);
}

function isPlainObject(value) {
    return value !== null && typeof value === "object" && !Array.isArray(value);
}

function hasKeys(value, keys) {
    return isPlainObject(value) && deepEqual(Object.keys(value).sort(), [...keys].sort());
}

/**
 * The compatibility setters store their setting even when the window does not
 * draw it right away; that answer is a failure envelope carrying the same
 * fields plus error and code, so either shape is accepted here.
 */
function hasApplyKeys(value, keys) {
    if (!isPlainObject(value)) return false;
    if (value.success === true) return hasKeys(value, ["success", ...keys]);
    return value.code === "OPERATION_FAILED" && hasKeys(value, ["success", "error", "code", ...keys]);
}

/** A result envelope that failed with INVALID_PARAMS, as the parameter reader answers. */
function refused(outcome) {
    return (
        outcome?.kind === "result" &&
        outcome.value?.success === false &&
        outcome.value?.code === "INVALID_PARAMS"
    );
}

function explicitKey(policy, key) {
    return isPlainObject(policy) && key in policy && policy[key] !== null;
}

function rect(bounds) {
    return { x: bounds?.x, y: bounds?.y, width: bounds?.width, height: bounds?.height };
}

function sameRect(a, b) {
    return deepEqual(rect(a), rect(b));
}

/** Win32 MulDiv for positive operands: rounds half away from zero. */
function mulDiv(a, b, c) {
    return Math.floor((a * b + Math.floor(c / 2)) / c);
}

function mainItemOf(allWindows) {
    const items = Array.isArray(allWindows?.items) ? allWindows.items : [];
    return items.find((item) => item?.windowId === "main");
}

/**
 * Runs one chrome-mutating call and pairs it with the backdropStateChanged
 * event it broadcasts. The page buffer is drained first so a late event from
 * the previous apply is not mistaken for this one; the wait is bounded because
 * an apply that fails at DWM level broadcasts nothing.
 */
async function applyAndCollect(events, call, timeoutMs) {
    await events.drain();
    const seen = events.received.length;
    const result = await call();
    await events.waitFor((all) => all.length > seen, { timeoutMs });
    const fresh = events.received.slice(seen).filter((entry) => entry.name === BACKDROP_EVENT);
    return { result, event: fresh.at(-1), eventCount: fresh.length };
}

function eventShapeOk(payload) {
    return (
        isPlainObject(payload) &&
        payload.windowId === "main" &&
        typeof payload.active === "boolean" &&
        ["active", "inactive"].includes(payload.mode) &&
        payload.mode === (payload.active ? "active" : "inactive") &&
        typeof payload.effect === "string"
    );
}

/**
 * The event reports the effect for the current activation state. An inactive
 * window with an explicit inactiveEffect other than "inherit" legitimately
 * reports that effect instead of the one the compatibility call just set.
 */
function eventEffectMatches(payload, expectedEffect, resolved) {
    if (!isPlainObject(payload)) return false;
    if (payload.effect === expectedEffect) return true;
    return payload.active === false && resolved?.inactiveEffect !== "inherit";
}

async function readEntry(bridge) {
    const [mode, state, bounds, currentId, allWindows, backdrop, corner, titlebar, dpi, devServer] =
        await Promise.all(
            [
                "window.getMode",
                "window.getState",
                "window.getBounds",
                "window.getCurrentWindowId",
                "window.getAllWindows",
                "window.getBackdropPolicy",
                "window.getCornerPreference",
                "window.getTitlebarHeight",
                "window.getDpiScale",
                "window.getDevServerConfig",
            ].map((method) => bridge.invoke(method, {})),
        );
    return { mode, state, bounds, currentId, allWindows, backdrop, corner, titlebar, dpi, devServer };
}

function runPreconditionCase(recorder, read) {
    const standalone = read.mode?.mode === "standalone" && read.mode?.panelMode === false;
    recorder.assertCase(
        "WC-01 the instance hosts a standalone main window, which every chrome endpoint requires",
        standalone,
        { mode: read.mode },
        { mode: "standalone", panelMode: false },
    );
    return standalone;
}

async function runReadCases(bridge, recorder, entry) {
    const { invokeRaw } = bridge;
    const backdrop = entry.backdrop;
    const resolved = backdrop?.resolvedBackdropPolicy;

    // success is written unconditionally after the resolver succeeded, so it
    // never carries information; what is checkable is the typed resolved policy.
    recorder.assertCase(
        "WC-02 getBackdropPolicy answers its hard-coded success, the caller's windowId, the explicit override object and a fully typed resolved policy",
        backdrop?.success === true &&
            backdrop.windowId === entry.currentId?.windowId &&
            isPlainObject(backdrop.backdropPolicy) &&
            hasKeys(resolved, ["activeEffect", "inactiveEffect", "darkMode", "reapplyOnActivate"]) &&
            ACTIVE_EFFECTS.includes(resolved.activeEffect) &&
            INACTIVE_EFFECTS.includes(resolved.inactiveEffect) &&
            typeof resolved.darkMode === "boolean" &&
            typeof resolved.reapplyOnActivate === "boolean",
        { backdrop },
        {
            success: "always true",
            windowId: entry.currentId?.windowId,
            backdropPolicy: "object of explicit overrides",
            resolvedBackdropPolicy: { activeEffect: ACTIVE_EFFECTS, inactiveEffect: INACTIVE_EFFECTS, darkMode: "boolean", reapplyOnActivate: "boolean" },
        },
    );

    const corner = entry.corner;
    recorder.assertCase(
        "WC-03 getCornerPreference answers success and mode and preference as the same string",
        hasKeys(corner, ["success", "mode", "preference"]) &&
            corner.success === true &&
            typeof corner.mode === "string" &&
            corner.preference === corner.mode,
        { corner },
        { keys: ["success", "mode", "preference"], preferenceEqualsMode: true },
    );

    const main = mainItemOf(entry.allWindows);
    recorder.assertCase(
        "WC-04 getAllWindows lists the main window with the same explicit and resolved backdrop policy getBackdropPolicy answers, and the fixed main-window capabilities",
        Boolean(main) &&
            deepEqual(main.backdropPolicy, backdrop.backdropPolicy) &&
            deepEqual(main.resolvedBackdropPolicy, resolved) &&
            main.capabilities?.supportsBackdropPolicy === true &&
            main.capabilities?.supportsCornerPreference === true &&
            main.capabilities?.supportsFrameless === true &&
            main.capabilities?.supportsMicaAlt === true,
        {
            mainBackdropPolicy: main?.backdropPolicy,
            mainResolved: main?.resolvedBackdropPolicy,
            capabilities: main?.capabilities,
        },
        { equalToGetBackdropPolicy: true, capabilities: { supportsBackdropPolicy: true, supportsCornerPreference: true, supportsFrameless: true, supportsMicaAlt: true } },
    );

    // The shell snapshot has a chrome.resolved member, but the JSON projection
    // leaves it out; that is why the fields below are unobservable from a page.
    const flat = JSON.stringify(main ?? {});
    const leaked = [
        "frameless",
        "useBlurBehind",
        "transparentBackground",
        "effectiveActiveEffect",
        "effectiveInactiveEffect",
        "nativeFrameStrategy",
    ].filter((key) => flat.includes(`"${key}"`));
    recorder.assertCase(
        "WC-05 getAllWindows carries no resolved chrome state: frameless, useBlurBehind, transparentBackground and the effective effects have no read path",
        Boolean(main) && leaked.length === 0 && !("chrome" in (main.shell ?? {})),
        { leaked, shellKeys: Object.keys(main?.shell ?? {}) },
        { leaked: [], shellHasChrome: false },
    );

    const unknown = await invokeRaw("window.getBackdropPolicy", { windowId: `no-such-window-${runId}` });
    recorder.assertCase(
        "WC-06 getBackdropPolicy for an unknown windowId fails with NOT_FOUND and no policy fields",
        unknown?.kind === "result" &&
            deepEqual(unknown.value, { success: false, error: "Window not found", code: "NOT_FOUND" }),
        { unknown },
        { kind: "result", value: { success: false, error: "Window not found", code: "NOT_FOUND" } },
    );
}

/**
 * Picks a titlebar height that is legal (24..100), differs from the entry value
 * and from the DPI default the chrome apply resets to, so that losing it is
 * unambiguous evidence of the reset.
 */
function pickSentinel(entryHeight, dpiDefault) {
    return [entryHeight + 4, entryHeight + 8, entryHeight - 4, entryHeight - 8].find(
        (height) => height >= 24 && height <= 100 && height !== entryHeight && height !== dpiDefault,
    );
}

async function placeTitlebarSentinel(bridge, entry) {
    const dpiDefault = mulDiv(TITLEBAR_DEFAULT_DIP, entry.dpi?.dpi ?? 96, 96);
    const height = pickSentinel(entry.titlebar?.height, dpiDefault);
    if (height === undefined) return { applied: false, dpiDefault };
    const set = await bridge.invoke("window.setTitlebarHeight", { height });
    const read = await bridge.invoke("window.getTitlebarHeight", {});
    return { applied: set?.success === true && read?.height === height, height, dpiDefault, set, read };
}

/** The compatibility call that reproduces an entry resolved activeEffect. */
function compatRestoreCall(effect) {
    if (effect === "acrylic") return ["window.setAcrylic", { enabled: true }];
    if (effect === "mica" || effect === "mica-alt") {
        return ["window.setMica", { enabled: true, variant: effect }];
    }
    return ["window.setMica", { enabled: false }];
}

async function runCompatibilityCases(bridge, recorder, events, entry) {
    const { invoke } = bridge;
    const baseline = entry.backdrop;
    const baseDark = baseline.resolvedBackdropPolicy.darkMode;
    const baseEffect = baseline.resolvedBackdropPolicy.activeEffect;
    // An explicit standard key wins over the compatibility slot for good, so
    // with one present the setters below can only be checked as envelopes.
    const effectLive = !explicitKey(baseline.backdropPolicy, "activeEffect");
    const darkLive = !explicitKey(baseline.backdropPolicy, "darkMode");
    const osRelease = os.release();

    const dark = await applyAndCollect(
        events,
        () => invoke("window.setDarkMode", { enabled: !baseDark }),
        SETTLE_EVENT_MS,
    );
    const afterDark = await invoke("window.getBackdropPolicy", {});
    recorder.assertCase(
        darkLive
            ? "WC-07 setDarkMode flips resolvedBackdropPolicy.darkMode through the compatibility slot, while its success reports the backdrop apply rather than dark mode itself"
            : "WC-07 setDarkMode answers an envelope only: an explicit backdropPolicy.darkMode key on entry suppresses the compatibility slot",
        hasApplyKeys(dark.result, ["enabled"]) &&
            dark.result.enabled === !baseDark &&
            (!darkLive ||
                (afterDark?.resolvedBackdropPolicy?.darkMode === !baseDark &&
                    afterDark.resolvedBackdropPolicy.activeEffect === baseEffect &&
                    deepEqual(afterDark.backdropPolicy, baseline.backdropPolicy))),
        {
            response: dark.result,
            resolvedAfter: afterDark?.resolvedBackdropPolicy,
            event: dark.event?.payload,
            darkLive,
        },
        {
            response: { success: "boolean, from the backdrop HRESULT", enabled: !baseDark },
            resolvedDarkMode: darkLive ? !baseDark : "unchanged, suppressed",
            explicitPolicyUntouched: true,
        },
    );

    const mica = await applyAndCollect(
        events,
        () => invoke("window.setMica", { enabled: true, variant: "mica" }),
        eventTimeoutMs,
    );
    const afterMica = await invoke("window.getBackdropPolicy", {});
    recorder.assertCase(
        effectLive
            ? "WC-08 setMica sets resolvedBackdropPolicy.activeEffect to mica and leaves inactiveEffect, darkMode and the explicit policy alone; without a darkMode parameter the envelope has no darkMode key"
            : "WC-08 setMica answers an envelope only: an explicit backdropPolicy.activeEffect key on entry suppresses the compatibility slot",
        hasApplyKeys(mica.result, ["enabled", "variant"]) &&
            mica.result.enabled === true &&
            mica.result.variant === "mica" &&
            (!effectLive ||
                (afterMica?.resolvedBackdropPolicy?.activeEffect === "mica" &&
                    afterMica.resolvedBackdropPolicy.inactiveEffect ===
                        baseline.resolvedBackdropPolicy.inactiveEffect &&
                    afterMica.resolvedBackdropPolicy.darkMode === afterDark?.resolvedBackdropPolicy?.darkMode &&
                    deepEqual(afterMica.backdropPolicy, baseline.backdropPolicy))),
        {
            response: mica.result,
            resolvedAfter: afterMica?.resolvedBackdropPolicy,
            effectLive,
            osRelease,
        },
        {
            response: { success: "boolean", enabled: true, variant: "mica" },
            resolvedActiveEffect: effectLive ? "mica" : "unchanged, suppressed",
            inactiveEffectUnchanged: true,
        },
    );

    recorder.assertCase(
        "WC-09 the compatibility apply broadcasts backdropStateChanged with {windowId, active, mode, effect}; mode follows the real activation state and effect is the effective backdrop",
        mica.result?.success === true &&
            mica.eventCount >= 1 &&
            eventShapeOk(mica.event?.payload) &&
            (!effectLive ||
                eventEffectMatches(mica.event.payload, "mica", afterMica?.resolvedBackdropPolicy)),
        { eventCount: mica.eventCount, event: mica.event?.payload, success: mica.result?.success },
        {
            eventCount: ">= 1",
            payload: { windowId: "main", active: "boolean", mode: "active or inactive, agreeing with active", effect: effectLive ? "mica" : "the effective effect" },
        },
    );

    const alt = await applyAndCollect(
        events,
        () => invoke("window.setMicaEffect", { enabled: true, variant: "mica-alt" }),
        eventTimeoutMs,
    );
    const afterAlt = await invoke("window.getBackdropPolicy", {});
    recorder.assertCase(
        effectLive
            ? "WC-10 setMicaEffect with variant mica-alt resolves to mica-alt, and the main window (supportsMicaAlt) broadcasts mica-alt as the effective effect rather than a downgrade"
            : "WC-10 setMicaEffect answers an envelope only: an explicit backdropPolicy.activeEffect key on entry suppresses the compatibility slot",
        hasApplyKeys(alt.result, ["enabled", "variant"]) &&
            alt.result.enabled === true &&
            alt.result.variant === "mica-alt" &&
            (!effectLive ||
                (afterAlt?.resolvedBackdropPolicy?.activeEffect === "mica-alt" &&
                    (alt.result.success !== true ||
                        eventEffectMatches(alt.event?.payload, "mica-alt", afterAlt.resolvedBackdropPolicy)))),
        {
            response: alt.result,
            resolvedAfter: afterAlt?.resolvedBackdropPolicy,
            event: alt.event?.payload,
            eventCount: alt.eventCount,
        },
        {
            response: { success: "boolean", enabled: true, variant: "mica-alt" },
            resolvedActiveEffect: effectLive ? "mica-alt" : "unchanged, suppressed",
            eventEffectWhenApplied: effectLive ? "mica-alt" : "the effective effect",
        },
    );

    const altViaMica = await applyAndCollect(
        events,
        () => invoke("window.setMica", { enabled: true, variant: "mica-alt" }),
        SETTLE_EVENT_MS,
    );
    recorder.assertCase(
        "WC-11 setMica and setMicaEffect are one implementation: identical parameters produce field-for-field identical envelopes",
        deepEqual(altViaMica.result, alt.result),
        { viaSetMica: altViaMica.result, viaSetMicaEffect: alt.result },
        { identical: true },
    );

    const bogus = await applyAndCollect(
        events,
        () => invoke("window.setMica", { enabled: true, variant: `bogus-${runId}`, darkMode: baseDark }),
        SETTLE_EVENT_MS,
    );
    const afterBogus = await invoke("window.getBackdropPolicy", {});
    recorder.assertCase(
        "WC-12 an unknown variant is silently coerced to mica, and the darkMode parameter is echoed and written into the same slot setDarkMode uses",
        hasApplyKeys(bogus.result, ["enabled", "variant", "darkMode"]) &&
            bogus.result.variant === "mica" &&
            bogus.result.darkMode === baseDark &&
            (!effectLive || afterBogus?.resolvedBackdropPolicy?.activeEffect === "mica") &&
            (!darkLive || afterBogus?.resolvedBackdropPolicy?.darkMode === baseDark),
        { response: bogus.result, resolvedAfter: afterBogus?.resolvedBackdropPolicy },
        {
            response: { variant: "mica", darkMode: baseDark },
            resolvedActiveEffect: effectLive ? "mica" : "unchanged, suppressed",
            resolvedDarkMode: darkLive ? baseDark : "unchanged, suppressed",
        },
    );

    const acrylic = await applyAndCollect(
        events,
        () => invoke("window.setAcrylic", { enabled: true }),
        eventTimeoutMs,
    );
    const afterAcrylic = await invoke("window.getBackdropPolicy", {});
    recorder.assertCase(
        effectLive
            ? "WC-13 setAcrylic resolves activeEffect to acrylic; its envelope has no variant key and no darkMode key when none was passed"
            : "WC-13 setAcrylic answers an envelope only: an explicit backdropPolicy.activeEffect key on entry suppresses the compatibility slot",
        hasApplyKeys(acrylic.result, ["enabled"]) &&
            acrylic.result.enabled === true &&
            (!effectLive || afterAcrylic?.resolvedBackdropPolicy?.activeEffect === "acrylic"),
        {
            response: acrylic.result,
            resolvedAfter: afterAcrylic?.resolvedBackdropPolicy,
            event: acrylic.event?.payload,
            osRelease,
        },
        {
            response: { success: "boolean", enabled: true },
            resolvedActiveEffect: effectLive ? "acrylic" : "unchanged, suppressed",
        },
    );

    // The blur fallback is only reached when the effective effect is "none";
    // with acrylic active DwmEnableBlurBehindWindow is called with enable=false
    // regardless, so success says nothing about blur. The flag itself has no
    // read path.
    const blurOn = await applyAndCollect(
        events,
        () => invoke("window.setBlur", { enabled: true }),
        SETTLE_EVENT_MS,
    );
    const afterBlurOn = await invoke("window.getBackdropPolicy", {});
    const blurOff = await applyAndCollect(
        events,
        () => invoke("window.setBlur", { enabled: false }),
        SETTLE_EVENT_MS,
    );
    recorder.assertCase(
        "WC-14 setBlur answers {success, enabled} only: useBlurBehind has no read path, the resolved policy is untouched, and the blur fallback is not even reached while a managed effect is active",
        hasApplyKeys(blurOn.result, ["enabled"]) &&
            blurOn.result.enabled === true &&
            hasApplyKeys(blurOff.result, ["enabled"]) &&
            blurOff.result.enabled === false &&
            deepEqual(afterBlurOn?.resolvedBackdropPolicy, afterAcrylic?.resolvedBackdropPolicy),
        {
            on: blurOn.result,
            off: blurOff.result,
            resolvedWhileOn: afterBlurOn?.resolvedBackdropPolicy,
            readPath: "none",
        },
        { on: { success: "boolean", enabled: true }, off: { success: "boolean", enabled: false }, resolvedUnchanged: true },
    );

    const transparentOn = await applyAndCollect(
        events,
        () => invoke("window.setBackgroundTransparency", { transparent: true }),
        SETTLE_EVENT_MS,
    );
    const transparentOff = await applyAndCollect(
        events,
        () => invoke("window.setBackgroundTransparency", { transparent: false }),
        SETTLE_EVENT_MS,
    );
    const afterTransparency = await invoke("window.getBackdropPolicy", {});
    recorder.assertCase(
        "WC-15 setBackgroundTransparency answers {success, transparent, description} only: transparentBackground has no read path, and the resolved policy is not polluted",
        hasApplyKeys(transparentOn.result, ["transparent", "description"]) &&
            transparentOn.result.transparent === true &&
            typeof transparentOn.result.description === "string" &&
            hasApplyKeys(transparentOff.result, ["transparent", "description"]) &&
            transparentOff.result.transparent === false &&
            deepEqual(afterTransparency?.resolvedBackdropPolicy, afterAcrylic?.resolvedBackdropPolicy),
        {
            on: transparentOn.result,
            off: transparentOff.result,
            resolvedAfter: afterTransparency?.resolvedBackdropPolicy,
            readPath: "none",
        },
        { on: { transparent: true }, off: { transparent: false }, resolvedUnchanged: true },
    );

    const [restoreMethod, restoreParams] = compatRestoreCall(baseEffect);
    const restored = await applyAndCollect(
        events,
        () => invoke(restoreMethod, restoreParams),
        SETTLE_EVENT_MS,
    );
    const afterRestore = await invoke("window.getBackdropPolicy", {});
    recorder.assertCase(
        effectLive
            ? "WC-16 turning the compatibility effect back to its entry value returns resolvedBackdropPolicy to exactly what it was on entry"
            : "WC-16 the compatibility restore is an envelope only; the resolved policy never moved because an explicit key suppresses the slot",
        typeof restored.result?.success === "boolean" &&
            restored.result.enabled === MANAGED_EFFECTS.includes(baseEffect) &&
            deepEqual(afterRestore?.resolvedBackdropPolicy, baseline.resolvedBackdropPolicy) &&
            deepEqual(afterRestore?.backdropPolicy, baseline.backdropPolicy),
        {
            restoreCall: { method: restoreMethod, params: restoreParams },
            response: restored.result,
            resolvedAfter: afterRestore?.resolvedBackdropPolicy,
            entryResolved: baseline.resolvedBackdropPolicy,
        },
        { resolvedEqualsEntry: true, explicitPolicyEqualsEntry: true },
    );
}

async function runCornerCases(bridge, recorder, events, entry) {
    const { invoke } = bridge;
    const baseMode = entry.corner.mode;
    const testMode = CORNER_MODES.find((mode) => mode !== baseMode);

    const set = await applyAndCollect(
        events,
        () => invoke("window.setCornerPreference", { mode: testMode }),
        SETTLE_EVENT_MS,
    );
    const read = await invoke("window.getCornerPreference", {});
    recorder.assertCase(
        "WC-17 setCornerPreference answers a hard-coded {success:true} regardless of the DWM result, and getCornerPreference reads the mode back",
        deepEqual(set.result, { success: true }) &&
            deepEqual(read, { success: true, mode: testMode, preference: testMode }),
        { response: set.result, read, event: set.event?.payload },
        { response: { success: true }, read: { success: true, mode: testMode, preference: testMode } },
    );

    const bogusMode = `bogus-${runId}`;
    const setBogus = await applyAndCollect(
        events,
        () => invoke("window.setCornerPreference", { mode: bogusMode }),
        SETTLE_EVENT_MS,
    );
    const readBogus = await invoke("window.getCornerPreference", {});
    const setDefault = await applyAndCollect(
        events,
        () => invoke("window.setCornerPreference", {}),
        SETTLE_EVENT_MS,
    );
    const readDefault = await invoke("window.getCornerPreference", {});
    recorder.assertCase(
        "WC-18 an unknown corner mode is stored and echoed verbatim by getCornerPreference (DWM renders it as round), and an omitted mode means default",
        deepEqual(setBogus.result, { success: true }) &&
            readBogus?.mode === bogusMode &&
            readBogus?.preference === bogusMode &&
            deepEqual(setDefault.result, { success: true }) &&
            readDefault?.mode === "default" &&
            readDefault?.preference === "default",
        { bogus: setBogus.result, readBogus, omitted: setDefault.result, readDefault },
        { readBogus: { mode: bogusMode, preference: bogusMode }, readDefault: { mode: "default", preference: "default" } },
    );

    await applyAndCollect(
        events,
        () => invoke("window.setCornerPreference", { mode: baseMode }),
        SETTLE_EVENT_MS,
    );
    const readRestored = await invoke("window.getCornerPreference", {});
    recorder.assertCase(
        "WC-19 setCornerPreference puts the entry mode back and getCornerPreference confirms it",
        readRestored?.mode === baseMode && readRestored?.preference === baseMode,
        { readRestored, entry: entry.corner },
        { mode: baseMode, preference: baseMode },
    );
}

function policyRestorePatch(entryPolicy) {
    return Object.fromEntries(
        POLICY_KEYS_TOUCHED.map((key) => [key, explicitKey(entryPolicy, key) ? entryPolicy[key] : null]),
    );
}

async function runPolicyCases(bridge, recorder, events, entry) {
    const { invoke, invokeRaw } = bridge;
    const baseline = entry.backdrop;
    const baseDark = baseline.resolvedBackdropPolicy.darkMode;
    const baseEffect = baseline.resolvedBackdropPolicy.activeEffect;

    const missing = await invokeRaw("window.setBackdropPolicy", {});
    const asString = await invokeRaw("window.setBackdropPolicy", { backdropPolicy: "mica" });
    const asNull = await invokeRaw("window.setBackdropPolicy", { backdropPolicy: null });
    const asArray = await invokeRaw("window.setBackdropPolicy", { backdropPolicy: ["mica"] });
    const unknownWindow = await invokeRaw("window.setBackdropPolicy", {
        backdropPolicy: {},
        windowId: `no-such-window-${runId}`,
    });
    const required = { success: false, error: "backdropPolicy is required", code: "INVALID_PARAMS" };
    const mustBeObject = { success: false, error: "backdropPolicy must be an object", code: "INVALID_PARAMS" };
    recorder.assertCase(
        "WC-20 setBackdropPolicy requires its policy: a missing or null one and a non-object one each fail with INVALID_PARAMS and their own message, and an unknown windowId fails with NOT_FOUND",
        [missing, asNull].every((probe) => probe?.kind === "result" && deepEqual(probe.value, required)) &&
            [asString, asArray].every(
                (probe) => probe?.kind === "result" && deepEqual(probe.value, mustBeObject),
            ) &&
            unknownWindow?.kind === "result" &&
            deepEqual(unknownWindow.value, { success: false, error: "Window not found", code: "NOT_FOUND" }),
        {
            missing: missing?.value,
            asString: asString?.value,
            asNull: asNull?.value,
            asArray: asArray?.value,
            unknownWindow: unknownWindow?.value,
        },
        {
            missingOrNull: required,
            nonObject: mustBeObject,
            unknownWindow: { success: false, error: "Window not found", code: "NOT_FOUND" },
        },
    );

    // No key whitelist, no value validation: the unknown key is stored, the
    // invalid effect falls back to the base value inside the resolver, and the
    // explicit key now shadows the compatibility slot.
    const written = await applyAndCollect(
        events,
        () =>
            invoke("window.setBackdropPolicy", {
                backdropPolicy: { zzz: 1, activeEffect: "bogus", darkMode: !baseDark },
            }),
        SETTLE_EVENT_MS,
    );
    const readWritten = await invoke("window.getBackdropPolicy", {});
    recorder.assertCase(
        "WC-21 setBackdropPolicy stores and echoes an unknown key, swallows an invalid activeEffect so backdropPolicy and resolvedBackdropPolicy disagree with no error, and applies a valid darkMode",
        written.result?.success === true &&
            written.result.windowId === "main" &&
            deepEqual(written.result.backdropPolicy, readWritten?.backdropPolicy) &&
            readWritten?.backdropPolicy?.zzz === 1 &&
            readWritten.backdropPolicy.activeEffect === "bogus" &&
            readWritten.backdropPolicy.darkMode === !baseDark &&
            readWritten.resolvedBackdropPolicy?.activeEffect === "inherit" &&
            readWritten.resolvedBackdropPolicy.darkMode === !baseDark &&
            !("error" in written.result),
        {
            response: written.result,
            read: readWritten,
            event: written.event?.payload,
        },
        {
            backdropPolicy: { zzz: 1, activeEffect: "bogus", darkMode: !baseDark },
            resolved: { activeEffect: "inherit (bogus swallowed)", darkMode: !baseDark },
            errorField: "absent",
        },
    );

    const micaWhileExplicit = await applyAndCollect(
        events,
        () => invoke("window.setMica", { enabled: true, variant: "mica" }),
        SETTLE_EVENT_MS,
    );
    const darkWhileExplicit = await applyAndCollect(
        events,
        () => invoke("window.setDarkMode", { enabled: baseDark }),
        SETTLE_EVENT_MS,
    );
    const readSuppressed = await invoke("window.getBackdropPolicy", {});
    // Clears the compatibility effect slot again (still shadowed, so nothing
    // moves) so the later delete returns to the entry state, not to mica.
    const resetLegacy = await applyAndCollect(
        events,
        () => invoke("window.setMica", { enabled: false }),
        SETTLE_EVENT_MS,
    );
    recorder.assertCase(
        "WC-22 while backdropPolicy holds explicit activeEffect and darkMode keys, setMica and setDarkMode still answer their normal envelopes but no longer move the resolved policy",
        micaWhileExplicit.result?.enabled === true &&
            micaWhileExplicit.result.variant === "mica" &&
            darkWhileExplicit.result?.enabled === baseDark &&
            readSuppressed?.resolvedBackdropPolicy?.activeEffect === "inherit" &&
            readSuppressed.resolvedBackdropPolicy.darkMode === !baseDark &&
            deepEqual(readSuppressed.backdropPolicy, readWritten?.backdropPolicy) &&
            hasApplyKeys(resetLegacy.result, ["enabled", "variant"]) &&
            resetLegacy.result.enabled === false,
        {
            setMica: micaWhileExplicit.result,
            setDarkMode: darkWhileExplicit.result,
            resolvedAfter: readSuppressed?.resolvedBackdropPolicy,
            resetLegacy: resetLegacy.result,
        },
        {
            resolved: { activeEffect: "inherit (setMica ignored)", darkMode: `${!baseDark} (setDarkMode ignored)` },
            envelopes: "normal success shapes",
        },
    );

    const inherit = await applyAndCollect(
        events,
        () => invoke("window.setBackdropPolicy", { backdropPolicy: { activeEffect: "inherit" } }),
        SETTLE_EVENT_MS,
    );
    const readInherit = await invoke("window.getBackdropPolicy", {});
    const restorePatch = policyRestorePatch(baseline.backdropPolicy);
    const deleted = await applyAndCollect(
        events,
        () => invoke("window.setBackdropPolicy", { backdropPolicy: restorePatch }),
        SETTLE_EVENT_MS,
    );
    const readDeleted = await invoke("window.getBackdropPolicy", {});
    recorder.assertCase(
        "WC-23 writing inherit keeps the key explicit (still stored, still shadowing); only a null value deletes it, which returns backdropPolicy to the entry object",
        inherit.result?.success === true &&
            explicitKey(readInherit?.backdropPolicy, "activeEffect") &&
            readInherit.backdropPolicy.activeEffect === "inherit" &&
            readInherit.backdropPolicy.zzz === 1 &&
            deleted.result?.success === true &&
            deepEqual(readDeleted?.backdropPolicy, baseline.backdropPolicy),
        {
            afterInherit: readInherit?.backdropPolicy,
            restorePatch,
            afterDelete: readDeleted?.backdropPolicy,
            entry: baseline.backdropPolicy,
        },
        {
            afterInherit: { activeEffect: "inherit (key present)", zzz: 1 },
            afterDelete: baseline.backdropPolicy,
        },
    );

    const [restoreMethod, restoreParams] = compatRestoreCall(baseEffect);
    await applyAndCollect(events, () => invoke(restoreMethod, restoreParams), SETTLE_EVENT_MS);
    const finalRead = await invoke("window.getBackdropPolicy", {});
    const allWindows = await invoke("window.getAllWindows", {});
    const main = mainItemOf(allWindows);
    recorder.assertCase(
        "WC-24 once the explicit keys are deleted the compatibility slot is live again: getBackdropPolicy and getAllWindows both return to the entry baseline and agree with each other",
        deepEqual(finalRead?.resolvedBackdropPolicy, baseline.resolvedBackdropPolicy) &&
            deepEqual(finalRead?.backdropPolicy, baseline.backdropPolicy) &&
            Boolean(main) &&
            deepEqual(main.resolvedBackdropPolicy, finalRead.resolvedBackdropPolicy) &&
            deepEqual(main.backdropPolicy, finalRead.backdropPolicy),
        {
            getBackdropPolicy: { backdropPolicy: finalRead?.backdropPolicy, resolved: finalRead?.resolvedBackdropPolicy },
            getAllWindows: { backdropPolicy: main?.backdropPolicy, resolved: main?.resolvedBackdropPolicy },
            entry: { backdropPolicy: baseline.backdropPolicy, resolved: baseline.resolvedBackdropPolicy },
        },
        { equalToEntry: true, endpointsAgree: true },
    );
}

async function runDragRegionCases(bridge, recorder) {
    const { invoke, invokeRaw } = bridge;
    const dpi = await invoke("window.getDpiScale", {});

    // Two legal rectangles (the second relies on x and y defaulting to 0), one
    // with zero width and one with negative height; the last two are dropped.
    const regions = [
        { x: 0, y: 0, width: 120, height: 32 },
        { width: 40, height: 20 },
        { x: 10, y: 10, width: 0, height: 20 },
        { x: 10, y: 10, width: 20, height: -4 },
    ];

    const drag = await invoke("window.setDragRegions", { regions });
    recorder.assertCase(
        "WC-25 setDragRegions takes {x,y,width,height} in CSS pixels, silently drops elements with width<=0 or height<=0, counts the survivors and reports the dpiScale getDpiScale also reports",
        hasKeys(drag, ["success", "count", "dpiScale"]) &&
            drag.success === true &&
            drag.count === 2 &&
            drag.dpiScale > 0 &&
            drag.dpiScale === dpi?.scale,
        { response: drag, getDpiScale: dpi, submitted: regions.length },
        { success: true, count: 2, dpiScale: dpi?.scale },
    );

    const noDrag = await invoke("window.setNoDragRegions", { regions });
    recorder.assertCase(
        "WC-26 setNoDragRegions applies the same element contract, drop rule, count and dpiScale as setDragRegions",
        hasKeys(noDrag, ["success", "count", "dpiScale"]) &&
            noDrag.success === true &&
            noDrag.count === 2 &&
            noDrag.dpiScale === dpi?.scale,
        { response: noDrag, getDpiScale: dpi },
        { success: true, count: 2, dpiScale: dpi?.scale },
    );

    const clearDrag = await invoke("window.clearDragRegions", {});
    const clearNoDrag = await invoke("window.clearNoDragRegions", {});
    recorder.assertCase(
        "WC-27 clearDragRegions and clearNoDragRegions answer a bare {success:true} with no count or dpiScale; the rectangles themselves have no read path",
        deepEqual(clearDrag, { success: true }) && deepEqual(clearNoDrag, { success: true }),
        { clearDrag, clearNoDrag, readPath: "none" },
        { each: { success: true } },
    );

    const noParam = await invoke("window.setDragRegions", {});
    const notArray = await invokeRaw("window.setNoDragRegions", { regions: "top" });
    recorder.assertCase(
        "WC-28 a missing regions parameter is an empty list (success with count 0); a non-array one fails with INVALID_PARAMS",
        hasKeys(noParam, ["success", "count", "dpiScale"]) &&
            noParam.success === true &&
            noParam.count === 0 &&
            refused(notArray),
        { noParam, notArray },
        {
            noParam: { success: true, count: 0 },
            notArray: { kind: "result", value: { success: false, code: "INVALID_PARAMS" } },
        },
    );

    // The parameter reader checks every element before the handler runs, so a
    // number instead of an object, or a string coordinate, changes nothing.
    const numbers = await invokeRaw("window.setDragRegions", { regions: [1, 2] });
    const stringCoordinate = await invokeRaw("window.setNoDragRegions", { regions: [{ x: "a" }] });
    recorder.assertCase(
        "WC-29 malformed region elements (a bare number, a string coordinate) fail with INVALID_PARAMS",
        refused(numbers) && refused(stringCoordinate),
        { numbers, stringCoordinate },
        { each: { kind: "result", value: { success: false, code: "INVALID_PARAMS" } } },
    );
}

async function runDevServerCases(bridge, recorder, entry) {
    const { invoke, invokeRaw } = bridge;
    const config = entry.devServer;
    recorder.assertCase(
        "WC-30 getDevServerConfig answers {success, useDevServer, devServerUrl} with a boolean flag and a string url",
        hasKeys(config, ["success", "useDevServer", "devServerUrl"]) &&
            config.success === true &&
            typeof config.useDevServer === "boolean" &&
            typeof config.devServerUrl === "string",
        { config },
        { success: true, useDevServer: "boolean", devServerUrl: "string" },
    );

    let current = config;
    if (config?.useDevServer === true) {
        // The only write this suite allows is useDevServer:false, which here
        // would switch the user's dev server off for their next start.
        for (const name of [
            "WC-31 setDevServerConfig with only useDevServer leaves the stored devServerUrl intact and reads the real current values back instead of echoing the request",
            "WC-32 the safe full write (useDevServer:false with the stored url) and an empty patch both answer the unchanged configuration; useDevServer:true stays on the manual list because it only acts on the next start",
        ]) {
            recorder.addCase(
                name,
                true,
                { skipped: "useDevServer is already true on this instance; the suite never writes true and will not switch it off", config },
                { skipped: "needs useDevServer false on entry" },
            );
        }
    } else {
        const partial = await invoke("window.setDevServerConfig", { useDevServer: false });
        const afterPartial = await invoke("window.getDevServerConfig", {});
        recorder.assertCase(
            "WC-31 setDevServerConfig with only useDevServer leaves the stored devServerUrl intact and reads the real current values back instead of echoing the request",
            hasKeys(partial, ["success", "useDevServer", "devServerUrl"]) &&
                partial.success === true &&
                partial.useDevServer === false &&
                partial.devServerUrl === config.devServerUrl &&
                deepEqual(afterPartial, partial),
            { partial, afterPartial, entryUrl: config.devServerUrl, entryUrlEmpty: config.devServerUrl === "" },
            { useDevServer: false, devServerUrl: config.devServerUrl, readBackEqualsResponse: true },
        );

        const full = await invoke("window.setDevServerConfig", {
            useDevServer: false,
            devServerUrl: config.devServerUrl,
        });
        const empty = await invoke("window.setDevServerConfig", {});
        recorder.assertCase(
            "WC-32 the safe full write (useDevServer:false with the stored url) and an empty patch both answer the unchanged configuration; useDevServer:true stays on the manual list because it only acts on the next start",
            deepEqual(full, afterPartial) && deepEqual(empty, afterPartial),
            { full, empty, expected: afterPartial },
            { full: afterPartial, empty: afterPartial },
        );
        current = afterPartial;
    }

    // The parameter reader refuses both probes before anything is stored.
    const badFlag = await invokeRaw("window.setDevServerConfig", { useDevServer: "yes" });
    const badUrl = await invokeRaw("window.setDevServerConfig", { devServerUrl: 123 });
    const afterBad = await invoke("window.getDevServerConfig", {});
    recorder.assertCase(
        "WC-33 wrong parameter types on setDevServerConfig fail with INVALID_PARAMS and leave the stored configuration untouched",
        refused(badFlag) && refused(badUrl) && deepEqual(afterBad, current),
        { badFlag, badUrl, afterBad, before: current },
        {
            badFlag: { kind: "result", value: { success: false, code: "INVALID_PARAMS" } },
            badUrl: { kind: "result", value: { success: false, code: "INVALID_PARAMS" } },
            configUnchanged: true,
        },
    );
}

async function runFramelessCases(bridge, recorder, events, touched) {
    const { invoke } = bridge;
    const state = await invoke("window.getState", {});
    const before = await invoke("window.getBounds", {});
    const stateSummary = {
        minimized: state?.isMinimized,
        maximized: state?.isMaximized,
        fullscreen: state?.isFullscreen,
        focused: state?.focused,
    };

    if (skipFrameless || state?.isFullscreen === true) {
        recorder.addCase(
            "WC-34 setFrameless round trip skipped: its first run must be watched by a person (FB2K_E2E_SKIP_FRAMELESS), and it is not attempted while fullscreen",
            true,
            {
                skipped: skipFrameless ? "FB2K_E2E_SKIP_FRAMELESS is set" : "the window is fullscreen",
                state: stateSummary,
                bounds: rect(before),
            },
            { skipped: "run without FB2K_E2E_SKIP_FRAMELESS on a non-fullscreen window" },
        );
        return;
    }

    // The main window starts frameless (frameless_ defaults to true and only this
    // endpoint writes it), and the flag has no getter, so the round trip goes
    // true -> false -> true and the restore writes true.
    touched.frameless = true;
    const off = await applyAndCollect(
        events,
        () => invoke("window.setFrameless", { frameless: false }),
        SETTLE_EVENT_MS,
    );
    const afterOff = await invoke("window.getBounds", {});
    const on = await applyAndCollect(
        events,
        () => invoke("window.setFrameless", { frameless: true }),
        SETTLE_EVENT_MS,
    );
    const afterOn = await invoke("window.getBounds", {});
    recorder.assertCase(
        "WC-34 setFrameless round trip (true -> false -> true; the main window starts frameless) echoes the shell's frameless flag and leaves the outer rectangle untouched; the flag has no getter of its own",
        deepEqual(off.result, { success: true, frameless: false }) &&
            deepEqual(on.result, { success: true, frameless: true }) &&
            sameRect(afterOff, before) &&
            sameRect(afterOn, before),
        {
            off: off.result,
            on: on.result,
            before: rect(before),
            afterOff: rect(afterOff),
            afterOn: rect(afterOn),
            state: stateSummary,
            offEvent: off.event?.payload,
            onEvent: on.event?.payload,
        },
        {
            off: { success: true, frameless: false },
            on: { success: true, frameless: true },
            outerRectangleUnchanged: true,
        },
    );
}

async function runFlashCases(bridge, recorder) {
    const { invoke, invokeRaw } = bridge;

    // A non-boolean enabled is refused by the parameter reader before
    // FlashWindowEx runs, so this probe never flashes anything.
    const badFlash = await invokeRaw("window.flash", { enabled: "yes" });
    if (allowFlash) {
        const flashOn = await invoke("window.flash", { enabled: true, count: 1 });
        const flashOff = await invoke("window.flash", { enabled: false, count: 0 });
        recorder.assertCase(
            "WC-35 flash really flashes once under FB2K_E2E_ALLOW_FLASH and is then stopped; both calls answer the hard-coded {success:true}, and a non-boolean enabled fails with INVALID_PARAMS",
            deepEqual(flashOn, { success: true }) &&
                deepEqual(flashOff, { success: true }) &&
                refused(badFlash),
            { flashOn, flashOff, badFlash },
            {
                flashOn: { success: true },
                flashOff: { success: true },
                badFlash: { kind: "result", value: { success: false, code: "INVALID_PARAMS" } },
            },
        );
    } else {
        // enabled:false maps to FLASHW_STOP; count:0 is passed because a stop
        // with a non-zero count can leave the taskbar button highlighted.
        const flashOff = await invoke("window.flash", { enabled: false, count: 0 });
        recorder.assertCase(
            "WC-35 without FB2K_E2E_ALLOW_FLASH flash is only sent its stop shape (enabled:false); the answer is the hard-coded {success:true} and says nothing about flashing, and a non-boolean enabled fails with INVALID_PARAMS",
            deepEqual(flashOff, { success: true }) && refused(badFlash),
            { flashOff, badFlash, allowFlash },
            {
                flashOff: { success: true },
                badFlash: { kind: "result", value: { success: false, code: "INVALID_PARAMS" } },
            },
        );
    }

    // flashTaskbar has only count and always sends FLASHW_ALL; count:0 is the
    // least intrusive call it accepts, and flash's stop clears any residue.
    const badTaskbar = await invokeRaw("window.flashTaskbar", { count: "x" });
    const taskbar = await invoke("window.flashTaskbar", { count: allowFlash ? 1 : 0 });
    const stopAfter = await invoke("window.flash", { enabled: false, count: 0 });
    recorder.assertCase(
        allowFlash
            ? "WC-36 flashTaskbar flashes the taskbar button once under FB2K_E2E_ALLOW_FLASH and answers the hard-coded {success:true}; it has no stop shape of its own, so flash stops it, and a non-numeric count fails with INVALID_PARAMS"
            : "WC-36 flashTaskbar has no stop shape of its own, so without FB2K_E2E_ALLOW_FLASH it is sent count:0 and immediately stopped through flash; the answer is the hard-coded {success:true}, and a non-numeric count fails with INVALID_PARAMS",
        deepEqual(taskbar, { success: true }) &&
            deepEqual(stopAfter, { success: true }) &&
            refused(badTaskbar),
        { taskbar, stopAfter, badTaskbar, count: allowFlash ? 1 : 0 },
        {
            taskbar: { success: true },
            stopAfter: { success: true },
            badTaskbar: { kind: "result", value: { success: false, code: "INVALID_PARAMS" } },
        },
    );
}

async function runFinalCases(bridge, recorder, entry, sentinel) {
    const { invoke } = bridge;
    const backdrop = await invoke("window.getBackdropPolicy", {});
    const corner = await invoke("window.getCornerPreference", {});
    const titlebar = await invoke("window.getTitlebarHeight", {});

    recorder.assertCase(
        "WC-37 after every group the explicit and resolved backdrop policy and the corner mode read exactly as they did on entry",
        deepEqual(backdrop?.backdropPolicy, entry.backdrop.backdropPolicy) &&
            deepEqual(backdrop?.resolvedBackdropPolicy, entry.backdrop.resolvedBackdropPolicy) &&
            corner?.mode === entry.corner.mode &&
            corner?.preference === entry.corner.preference,
        {
            backdrop: { backdropPolicy: backdrop?.backdropPolicy, resolved: backdrop?.resolvedBackdropPolicy },
            corner,
        },
        {
            backdrop: { backdropPolicy: entry.backdrop.backdropPolicy, resolved: entry.backdrop.resolvedBackdropPolicy },
            corner: entry.corner,
        },
    );

    // UpdateDpiDependentSizes runs after every backdrop apply and rewrites
    // titlebarHeight_ to MulDiv(32, dpi, 96); nothing announces it.
    const restored = await invoke("window.setTitlebarHeight", { height: entry.titlebar.height });
    const afterRestore = await invoke("window.getTitlebarHeight", {});
    recorder.assertCase(
        sentinel.applied
            ? "WC-38 every chrome apply silently resets the titlebar height to the DPI default: the sentinel written before the material group is gone, and setTitlebarHeight has to put the entry value back"
            : "WC-38 the titlebar height reads as the DPI default after the chrome applies (no sentinel could be placed within 24..100), and setTitlebarHeight puts the entry value back",
        titlebar?.height === sentinel.dpiDefault &&
            (!sentinel.applied || titlebar.height !== sentinel.height) &&
            restored?.success === true &&
            afterRestore?.height === entry.titlebar.height,
        {
            beforeRestore: titlebar,
            sentinel,
            restored,
            afterRestore,
            entry: entry.titlebar,
            dpi: entry.dpi,
        },
        {
            beforeRestore: { height: sentinel.dpiDefault },
            sentinelSurvived: false,
            afterRestore: entry.titlebar,
        },
    );
}

async function restoreEntryState(bridge, entry, touched) {
    const quiet = (method, params) => bridge.invokeRaw(method, params).catch(() => undefined);
    const resolved = entry.backdrop.resolvedBackdropPolicy;

    if (touched.policy) {
        await quiet("window.setBackdropPolicy", { backdropPolicy: policyRestorePatch(entry.backdrop.backdropPolicy) });
    }
    if (touched.compat || touched.policy) {
        // Transparency first: its bare WebView override is only corrected by the
        // next apply, which the calls after it provide.
        await quiet("window.setBackgroundTransparency", { transparent: false });
        await quiet("window.setBlur", { enabled: false });
        await quiet("window.setDarkMode", { enabled: resolved.darkMode });
        const [method, params] = compatRestoreCall(resolved.activeEffect);
        await quiet(method, params);
    }
    if (touched.corner) {
        await quiet("window.setCornerPreference", { mode: entry.corner.mode });
    }
    if (touched.frameless) {
        await quiet("window.setFrameless", { frameless: true });
    }
    if (touched.regions) {
        await quiet("window.clearDragRegions", {});
        await quiet("window.clearNoDragRegions", {});
    }
    // Last, because every call above is a chrome apply that resets it.
    if (touched.titlebar) {
        await quiet("window.setTitlebarHeight", { height: entry.titlebar.height });
    }
}

const recorder = createRecorder();
let client;
let bridge;
let events;
let blocked = false;
let fatalError;
let entry;
let targets;
let sentinel = { applied: false };
const touched = { compat: false, corner: false, policy: false, regions: false, frameless: false, titlebar: false };

try {
    const connection = await connectBridgePage();
    client = connection.client;
    targets = connection.candidates;
    console.log(`Target: ${connection.page.title} ${connection.page.url}`);
    bridge = createBridge(client.Runtime, { invokeTimeoutMs });
    await requireResponsiveBridge(bridge, { probeId: runId });

    const read = await readEntry(bridge);
    if (runPreconditionCase(recorder, read)) {
        entry = read;
        events = await createEventCollector(bridge, [BACKDROP_EVENT], { collectorId: runId });

        await runReadCases(bridge, recorder, entry);

        touched.titlebar = true;
        sentinel = await placeTitlebarSentinel(bridge, entry);

        touched.compat = true;
        await runCompatibilityCases(bridge, recorder, events, entry);

        touched.corner = true;
        await runCornerCases(bridge, recorder, events, entry);

        touched.policy = true;
        await runPolicyCases(bridge, recorder, events, entry);

        touched.regions = true;
        await runDragRegionCases(bridge, recorder);

        await runDevServerCases(bridge, recorder, entry);
        await runFramelessCases(bridge, recorder, events, touched);
        await runFlashCases(bridge, recorder);
        await runFinalCases(bridge, recorder, entry, sentinel);
    }
} catch (error) {
    fatalError = error;
    blocked = Boolean(error?.blocked);
    console.error(`${blocked ? "BLOCKED" : "ERROR"}: ${error?.message || error}`);
} finally {
    if (bridge && entry) {
        await restoreEntryState(bridge, entry, touched);
    }
    if (events) await events.stop();
    await closeClient(client);
}

const backdropEvents = (events?.received ?? []).filter((item) => item.name === BACKDROP_EVENT);

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
            allowFlash,
            skipFrameless,
            osRelease: os.release(),
            entryState: entry
                ? {
                      minimized: entry.state?.isMinimized,
                      maximized: entry.state?.isMaximized,
                      fullscreen: entry.state?.isFullscreen,
                      focused: entry.state?.focused,
                      backdropPolicy: entry.backdrop?.backdropPolicy,
                      resolvedBackdropPolicy: entry.backdrop?.resolvedBackdropPolicy,
                      cornerMode: entry.corner?.mode,
                      titlebarHeight: entry.titlebar?.height,
                      dpi: entry.dpi?.dpi,
                      useDevServer: entry.devServer?.useDevServer,
                  }
                : undefined,
            backdropEvents: {
                count: backdropEvents.length,
                modes: [...new Set(backdropEvents.map((item) => item.payload?.mode))],
                effects: [...new Set(backdropEvents.map((item) => item.payload?.effect))],
            },
            targets,
        },
    }),
);

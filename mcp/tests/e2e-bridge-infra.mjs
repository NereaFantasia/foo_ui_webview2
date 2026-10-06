/**
 * Covers the bridge's own infrastructure namespaces: shared state, named ports,
 * cross-window events, the foobar2000 console, the plugin log file, the plugin
 * registry queries, cursor visibility, and the toast request.
 *
 * These 22 endpoints share a property that makes them a good batch: none of them
 * touches the media library, playback, or window geometry, so the suite leaves
 * nothing behind that a later suite could trip over. What state it does write is
 * namespaced by run id and removed in the finally block.
 *
 * The oracles are mostly the endpoints' own read side (set/get, connect/getPorts,
 * write/read), plus the events each mutation is documented to broadcast. Where an
 * endpoint has no read side at all the case says so rather than pretending:
 * console.log/warn/error write to the foobar2000 console, and nothing in the
 * bridge can read that sink back - log.read reads a different file - so those
 * three are pinned at their envelope and their validation branch only.
 *
 * Several cases record asymmetries rather than wishes. state.get omits the key
 * field when the key is absent but includes it when present; log.read drops its
 * lines and totalLines fields when the file does not exist; an invalid custom log
 * filename is silently ignored instead of refused, so the write lands in the
 * default file.
 * Those are marked, and exist so an intentional change shows up here.
 *
 * The plugin log file belongs to the user, so its bytes are snapshotted and
 * restored: the profile directory is learned from the path that a write to a
 * throwaway file returns, which is also what pins the custom-file behaviour.
 *
 * Usage: node mcp/tests/e2e-bridge-infra.mjs
 * Environment: FB2K_CDP_PORT, FB2K_E2E_TIMEOUT_MS, FB2K_EVENT_TIMEOUT_MS.
 */

import { existsSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";

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

const KEY_PREFIX = `e2e:${runId}:`;
const KEY_ALPHA = `${KEY_PREFIX}alpha`;
const KEY_BETA = `${KEY_PREFIX}beta`;
const KEY_TTL = `${KEY_PREFIX}ttl`;
const PORT_NAME = `e2e-port-${runId}`;
const EVENT_NAME = `e2e:${runId}:ping`;
const PROBE_LOG = `e2e-${runId}.log`;
const TTL_MS = 150;

const EVENT_NAMES = [
    "state:changed",
    "state:deleted",
    "port:connected",
    "port:disconnected",
    "port:message",
    "cursor:hiddenChanged",
    "ui:toast",
    EVENT_NAME,
];

const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms));

/** Events of one name, oldest first. */
function named(events, name) {
    return events.filter((entry) => entry.name === name);
}

/**
 * Events of one name that concern one state key.
 *
 * The page is a live front-end sharing this bridge, and it keeps its own state
 * with its own expiries - the lyrics cache writes `lyrics:payload:*` keys under
 * a ttl. So "the last state:deleted with reason expired" is not necessarily
 * this suite's; only the key makes it ours.
 */
function forKey(events, name, key) {
    return named(events, name).filter((entry) => entry.payload?.key === key);
}

async function runStateCases(bridge, recorder, events) {
    const { invoke, invokeRaw } = bridge;

    const setAlpha = await invoke("state.set", { key: KEY_ALPHA, value: { n: 1 } });
    const getAlpha = await invoke("state.get", { key: KEY_ALPHA });
    recorder.assertCase(
        "BI-01 state.set stores a value that state.get returns with its key",
        setAlpha?.success === true &&
            getAlpha?.success === true &&
            getAlpha.exists === true &&
            getAlpha.key === KEY_ALPHA &&
            getAlpha.value?.n === 1,
        { setAlpha, getAlpha },
        { set: { success: true }, get: { exists: true, key: KEY_ALPHA, value: { n: 1 } } },
    );

    const missing = await invoke("state.get", { key: `${KEY_PREFIX}absent` });
    recorder.assertCase(
        "BI-02 state.get on an absent key succeeds with exists false, and omits the key field",
        missing?.success === true &&
            missing.exists === false &&
            missing.value === null &&
            !("key" in missing),
        { missing, hasKeyField: "key" in missing },
        { success: true, exists: false, value: null, keyFieldPresent: false },
    );

    await invoke("state.set", { key: KEY_BETA, value: "b" });
    const allKeys = await invoke("state.keys", {});
    const prefixKeys = await invoke("state.keys", { pattern: `${KEY_PREFIX}*` });
    const exactKeys = await invoke("state.keys", { pattern: KEY_ALPHA });
    const unmatched = await invoke("state.keys", { pattern: `${KEY_PREFIX}no*` });
    recorder.assertCase(
        "BI-03 state.keys defaults to every key, and supports a trailing-star prefix or an exact name",
        Array.isArray(allKeys?.keys) &&
            allKeys.keys.includes(KEY_ALPHA) &&
            prefixKeys.keys.length === 2 &&
            prefixKeys.keys.includes(KEY_ALPHA) &&
            prefixKeys.keys.includes(KEY_BETA) &&
            exactKeys.keys.length === 1 &&
            exactKeys.keys[0] === KEY_ALPHA &&
            unmatched.keys.length === 0,
        {
            allIncludesOurs: allKeys?.keys?.includes(KEY_ALPHA),
            prefix: prefixKeys?.keys,
            exact: exactKeys?.keys,
            unmatched: unmatched?.keys,
        },
        { prefix: "both run keys", exact: "only alpha", unmatched: "empty" },
    );

    // previousValue carries the value being replaced, so a second write is what
    // proves the field is not just echoing the new one.
    await events.drain();
    await invoke("state.set", { key: KEY_ALPHA, value: { n: 2 } });
    await events.waitFor((all) => forKey(all, "state:changed", KEY_ALPHA).length > 0, {
        timeoutMs: eventTimeoutMs,
    });
    const changed = forKey(events.received, "state:changed", KEY_ALPHA).at(-1);
    recorder.assertCase(
        "BI-04 an overwrite announces the key, the new value and the value it replaced",
        changed?.payload?.key === KEY_ALPHA &&
            changed.payload.value?.n === 2 &&
            changed.payload.previousValue?.n === 1 &&
            typeof changed.payload.sourceWindowId === "string",
        { payload: changed?.payload },
        { key: KEY_ALPHA, value: { n: 2 }, previousValue: { n: 1 }, sourceWindowId: "a string" },
    );

    await events.drain();
    const silent = await invoke("state.set", { key: KEY_ALPHA, value: { n: 3 }, silent: true });
    // A negative needs a real wait: there is no event to wait for, so the only
    // way to be sure none is coming is to give it time and drain again.
    await sleep(300);
    await events.drain();
    const silentEvents = forKey(events.received, "state:changed", KEY_ALPHA).filter(
        (entry) => entry.payload?.value?.n === 3,
    );
    const storedSilently = await invoke("state.get", { key: KEY_ALPHA });
    recorder.assertCase(
        "BI-05 silent true stores the value without announcing it",
        silent?.success === true &&
            silentEvents.length === 0 &&
            storedSilently.value?.n === 3,
        {
            silent,
            announcementsForThisWrite: silentEvents.length,
            storedValue: storedSilently?.value,
        },
        { success: true, announcements: 0, storedValue: { n: 3 } },
    );

    await events.drain();
    const deleted = await invoke("state.delete", { key: KEY_BETA });
    const deletedAgain = await invoke("state.delete", { key: KEY_BETA });
    await events.waitFor((all) => forKey(all, "state:deleted", KEY_BETA).length > 0, {
        timeoutMs: eventTimeoutMs,
    });
    const deleteEvent = forKey(events.received, "state:deleted", KEY_BETA).at(-1);
    recorder.assertCase(
        "BI-06 delete reports whether the key was there, and succeeds either way",
        deleted?.success === true &&
            deleted.existed === true &&
            deletedAgain?.success === true &&
            deletedAgain.existed === false &&
            deleteEvent?.payload?.key === KEY_BETA &&
            deleteEvent.payload.reason === "deleted",
        { deleted, deletedAgain, deleteEvent: deleteEvent?.payload },
        {
            first: { existed: true },
            second: { success: true, existed: false },
            event: { reason: "deleted" },
        },
    );

    await events.drain();
    const ttlSet = await invoke("state.set", { key: KEY_TTL, value: 1, ttlMs: TTL_MS });
    // Expiry is lazy: it is swept on the next read, which is also what emits the
    // event, so the read has to come before the assertion. Poll that read rather
    // than sleeping a fixed span - a fixed wait long enough to be safe on a
    // loaded machine is longer than this case should cost.
    let afterExpiry;
    const sweepDeadline = Date.now() + Math.max(eventTimeoutMs, 2000);
    for (;;) {
        afterExpiry = await invoke("state.keys", { pattern: `${KEY_PREFIX}*` });
        if (!afterExpiry.keys.includes(KEY_TTL) || Date.now() >= sweepDeadline) break;
        await sleep(25);
    }
    await events.waitFor((all) => forKey(all, "state:deleted", KEY_TTL).length > 0, {
        timeoutMs: eventTimeoutMs,
    });
    const expiredEvent = forKey(events.received, "state:deleted", KEY_TTL).at(-1);
    recorder.assertCase(
        "BI-07 a ttl write reports its expiry time, and the key is swept on the next read with reason expired",
        Number.isFinite(ttlSet?.expiresAt) &&
            !afterExpiry.keys.includes(KEY_TTL) &&
            expiredEvent?.payload?.key === KEY_TTL &&
            expiredEvent.payload.sourceWindowId === "" &&
            expiredEvent.payload.reason === "expired",
        {
            expiresAt: ttlSet?.expiresAt,
            keysAfter: afterExpiry?.keys,
            expiredEvent: expiredEvent?.payload,
        },
        {
            expiresAt: "epoch milliseconds",
            keyGone: true,
            event: { reason: "expired", sourceWindowId: "" },
        },
    );

    const noKey = await invokeRaw("state.set", { value: 1 });
    const noValue = await invokeRaw("state.set", { key: KEY_ALPHA });
    const getNoKey = await invokeRaw("state.get", {});
    recorder.assertCase(
        "BI-08 state refuses a missing key or a missing value with INVALID_PARAMS",
        noKey?.value?.code === "INVALID_PARAMS" &&
            noValue?.value?.code === "INVALID_PARAMS" &&
            getNoKey?.value?.code === "INVALID_PARAMS",
        { noKey: noKey?.value, noValue: noValue?.value, getNoKey: getNoKey?.value },
        { each: { code: "INVALID_PARAMS" } },
    );
}

async function runPortCases(bridge, recorder, events, openPorts) {
    const { invoke, invokeRaw } = bridge;

    await events.drain();
    const first = await invoke("port.connect", { name: PORT_NAME });
    openPorts.push(first.portId);
    await events.waitFor((all) => named(all, "port:connected").length > 0, {
        timeoutMs: eventTimeoutMs,
    });
    const connectEvent = named(events.received, "port:connected").at(-1);
    recorder.assertCase(
        "BI-09 port.connect answers with success and the port identity, and announces the connection",
        first?.success === true &&
            typeof first.portId === "string" &&
            first.portId.length > 0 &&
            first.name === PORT_NAME &&
            typeof first.windowId === "string" &&
            connectEvent?.payload?.portId === first.portId,
        { first, connectEvent: connectEvent?.payload },
        { success: true, portId: "a string", name: PORT_NAME },
    );

    const second = await invoke("port.connect", { name: PORT_NAME });
    openPorts.push(second.portId);
    const listed = await invoke("port.getPorts", {});
    const filtered = await invoke("port.getPorts", { name: PORT_NAME });
    recorder.assertCase(
        "BI-10 getPorts lists open ports, and the name filter narrows to that channel",
        listed?.success === true &&
            listed.ports.some((port) => port.portId === first.portId) &&
            filtered.ports.length === 2 &&
            filtered.ports.every((port) => port.name === PORT_NAME),
        {
            listedCount: listed?.ports?.length,
            filtered: filtered?.ports?.map((port) => port.portId),
        },
        { filtered: "exactly the two ports of this channel" },
    );

    // postMessage fans out by channel name and skips the sender, so the second
    // port of the same name is what receives it.
    await events.drain();
    const posted = await invoke("port.postMessage", {
        portId: first.portId,
        message: { hello: runId },
    });
    await events.waitFor((all) => named(all, "port:message").length > 0, {
        timeoutMs: eventTimeoutMs,
    });
    const message = named(events.received, "port:message").at(-1);
    recorder.assertCase(
        "BI-11 postMessage reaches the channel's other ports but not the sender",
        posted?.success === true &&
            posted.recipients === 1 &&
            message?.payload?.portId === second.portId &&
            message.payload.sourcePortId === first.portId &&
            message.payload.message?.hello === runId,
        { posted, delivered: message?.payload },
        {
            recipients: 1,
            delivered: { portId: "the other port", sourcePortId: "the sender" },
        },
    );

    const lonely = await invoke("port.connect", { name: `${PORT_NAME}-lonely` });
    openPorts.push(lonely.portId);
    const noPeers = await invoke("port.postMessage", {
        portId: lonely.portId,
        message: { x: 1 },
    });
    recorder.assertCase(
        "BI-12 a channel with no other port still succeeds, reporting zero recipients",
        noPeers?.success === true && noPeers.recipients === 0,
        { noPeers },
        { success: true, recipients: 0 },
    );

    await events.drain();
    const directed = await invoke("port.postMessageTo", {
        portId: first.portId,
        targetPortId: second.portId,
        message: { direct: runId },
    });
    await events.waitFor(
        (all) => named(all, "port:message").some((e) => e.payload?.message?.direct === runId),
        { timeoutMs: eventTimeoutMs },
    );
    const directMessage = named(events.received, "port:message").find(
        (entry) => entry.payload?.message?.direct === runId,
    );
    const unknownTarget = await invokeRaw("port.postMessageTo", {
        portId: first.portId,
        targetPortId: "port_ffffffff",
        message: {},
    });
    recorder.assertCase(
        "BI-13 postMessageTo delivers to one named port, and refuses an unknown target",
        directed?.success === true &&
            directMessage?.payload?.portId === second.portId &&
            unknownTarget?.value?.code === "TARGET_NOT_FOUND",
        { directed, directMessage: directMessage?.payload, unknownTarget: unknownTarget?.value },
        { directed: { success: true }, unknownTarget: { code: "TARGET_NOT_FOUND" } },
    );

    await events.drain();
    const dropped = await invoke("port.disconnect", { portId: lonely.portId });
    openPorts.splice(openPorts.indexOf(lonely.portId), 1);
    await events.waitFor((all) => named(all, "port:disconnected").length > 0, {
        timeoutMs: eventTimeoutMs,
    });
    const dropEvent = named(events.received, "port:disconnected").at(-1);
    const stillListed = await invoke("port.getPorts", { name: `${PORT_NAME}-lonely` });
    const unknownPort = await invokeRaw("port.disconnect", { portId: "port_ffffffff" });
    recorder.assertCase(
        "BI-14 disconnect removes the port, announces it, and refuses an unknown id",
        dropped?.success === true &&
            dropEvent?.payload?.portId === lonely.portId &&
            stillListed.ports.length === 0 &&
            unknownPort?.value?.code === "PORT_NOT_FOUND",
        { dropped, dropEvent: dropEvent?.payload, remaining: stillListed?.ports?.length },
        { removed: true, unknown: { code: "PORT_NOT_FOUND" } },
    );

    // port:message carries whatever JSON the sender passed, not only objects.
    await events.drain();
    const arraySent = await invoke("port.postMessageTo", {
        portId: first.portId,
        targetPortId: second.portId,
        message: [runId, 2],
    });
    await events.waitFor(
        (all) => named(all, "port:message").some((e) => Array.isArray(e.payload?.message)),
        { timeoutMs: eventTimeoutMs },
    );
    const arrayMessage = named(events.received, "port:message").find((entry) =>
        Array.isArray(entry.payload?.message),
    );
    const keysOf = (payload) => (payload ? Object.keys(payload).sort().join(",") : "");
    recorder.assertCase(
        "BI-34 port events carry exactly their declared keys, and a message of any JSON type arrives as sent",
        keysOf(connectEvent?.payload) === "name,portId,windowId" &&
            keysOf(dropEvent?.payload) === "name,portId,windowId" &&
            keysOf(message?.payload) === "message,portId,sourcePortId,sourceWindowId" &&
            arraySent?.success === true &&
            arrayMessage?.payload?.message?.[0] === runId &&
            arrayMessage.payload.message[1] === 2,
        {
            connected: keysOf(connectEvent?.payload),
            disconnected: keysOf(dropEvent?.payload),
            message: keysOf(message?.payload),
            arrayMessage: arrayMessage?.payload?.message,
        },
        {
            connected: "name,portId,windowId",
            disconnected: "name,portId,windowId",
            message: "message,portId,sourcePortId,sourceWindowId",
            arrayMessage: [runId, 2],
        },
    );

    const noName = await invokeRaw("port.connect", {});
    const noPortId = await invokeRaw("port.postMessage", { message: {} });
    const noMessage = await invokeRaw("port.postMessage", { portId: first.portId });
    recorder.assertCase(
        "BI-15 port refuses a missing name, a missing port id and a missing message",
        noName?.value?.code === "INVALID_PARAMS" &&
            noPortId?.value?.code === "INVALID_PARAMS" &&
            noMessage?.value?.code === "INVALID_PARAMS",
        { noName: noName?.value, noPortId: noPortId?.value, noMessage: noMessage?.value },
        { each: { code: "INVALID_PARAMS" } },
    );

    return { windowId: first.windowId };
}

async function runEventCases(bridge, recorder, events, windowId) {
    const { invoke, invokeRaw } = bridge;

    // event.emit wraps what the caller passed: subscribers see
    // { payload, sourceWindowId }, not the payload on its own.
    await events.drain();
    const emitted = await invoke("event.emit", {
        event: EVENT_NAME,
        payload: { tag: runId },
    });
    await events.waitFor((all) => named(all, EVENT_NAME).length > 0, {
        timeoutMs: eventTimeoutMs,
    });
    const delivered = named(events.received, EVENT_NAME).at(-1);
    recorder.assertCase(
        "BI-16 event.emit broadcasts under the caller's own event name, wrapping the payload in an envelope",
        emitted?.success === true &&
            Number.isInteger(emitted.recipients) &&
            delivered?.payload?.payload?.tag === runId &&
            typeof delivered.payload.sourceWindowId === "string",
        { emitted, delivered: delivered?.payload },
        {
            recipients: "an integer",
            delivered: { payload: { tag: runId }, sourceWindowId: "a string" },
        },
    );

    await events.drain();
    const toSelf = await invoke("event.emitTo", {
        event: EVENT_NAME,
        targetWindowId: windowId,
        payload: { direct: runId },
    });
    await events.waitFor(
        (all) => named(all, EVENT_NAME).some((e) => e.payload?.payload?.direct === runId),
        { timeoutMs: eventTimeoutMs },
    );
    const self = named(events.received, EVENT_NAME).find(
        (entry) => entry.payload?.payload?.direct === runId,
    );
    const unknownWindow = await invoke("event.emitTo", {
        event: EVENT_NAME,
        targetWindowId: `no-such-window-${runId}`,
        payload: {},
    });
    recorder.assertCase(
        "BI-17 emitTo delivers to the window id a port reported, and reports failure for an unknown one",
        toSelf?.success === true &&
            self?.payload?.payload?.direct === runId &&
            unknownWindow?.success === false,
        { toSelf, deliveredToSelf: Boolean(self), unknownWindow },
        { toSelf: { success: true }, unknownWindow: { success: false } },
    );

    const noEvent = await invokeRaw("event.emit", { payload: {} });
    const noTarget = await invokeRaw("event.emitTo", { event: EVENT_NAME });
    recorder.assertCase(
        "BI-18 event refuses a missing event name and a missing target window",
        noEvent?.value?.code === "INVALID_PARAMS" && noTarget?.value?.code === "INVALID_PARAMS",
        { noEvent: noEvent?.value, noTarget: noTarget?.value },
        { each: { code: "INVALID_PARAMS" } },
    );
}

async function runConsoleCases(bridge, recorder) {
    const { invoke, invokeRaw } = bridge;

    const [log, warn, error] = await Promise.all([
        invoke("console.log", { message: `e2e log ${runId}` }),
        invoke("console.warn", { message: `e2e warn ${runId}` }),
        invoke("console.error", { message: `e2e error ${runId}` }),
    ]);
    const fromArgs = await invoke("console.log", { args: ["e2e", "args", runId] });
    const structured = await invoke("console.log", { message: { nested: runId } });
    // Nothing in the bridge reads the foobar2000 console back - log.read reads a
    // different file - so what reached the sink is not assertable here, only that
    // the call shapes were accepted. The text landing is in the manual checklist.
    recorder.assertCase(
        "BI-19 the three console levels accept a message, an args array, or a structure to dump",
        [log, warn, error, fromArgs, structured].every((res) => res?.success === true),
        { log, warn, error, fromArgs, structured },
        { each: { success: true } },
    );

    const empty = await invokeRaw("console.log", {});
    const emptyWarn = await invokeRaw("console.warn", {});
    const emptyError = await invokeRaw("console.error", {});
    recorder.assertCase(
        "BI-20 a call with neither message nor args is refused by all three levels",
        [empty, emptyWarn, emptyError].every(
            (res) => res?.value?.success === false && /message/i.test(res.value.error ?? ""),
        ),
        { empty: empty?.value, emptyWarn: emptyWarn?.value, emptyError: emptyError?.value },
        { each: { success: false, error: "mentions message" } },
    );
}

async function runLogCases(bridge, recorder, logState) {
    const { invoke, invokeRaw } = bridge;

    // Writing to a throwaway file is how the profile directory is learned, which
    // is what would make a byte-exact snapshot of the real log possible.
    const probeWrite = await invokeRaw("log.write", {
        file: PROBE_LOG,
        message: `probe ${runId}`,
    });
    const defaultWrite = await invokeRaw("log.write", { message: `default ${runId}` });
    const probePath = probeWrite?.value?.path;
    const writable = probeWrite?.value?.success === true;

    // Both paths have to resolve, and a named file is the one that proves the
    // profile directory was resolved rather than guessed. This regressed once:
    // deriving the directory from the literal "profile://" yields a path that
    // cannot be opened, which broke every write while log.read masked it (see
    // BI-22). core_api::get_profile_path is the form that works.
    recorder.assertCase(
        "BI-21 log.write can open the file it is asked to write, for both the default and a named file",
        writable === true && defaultWrite?.value?.success === true,
        { probeWrite: probeWrite?.value, defaultWrite: defaultWrite?.value },
        { each: { success: true, path: "a path under the profile directory" } },
    );

    if (!writable || typeof probePath !== "string") {
        for (const skipped of [
            "BI-22 a missing log file reads as an empty one, without the lines and totalLines fields",
            "BI-23 a write to the default file is read back with an upper-cased level and a timestamp",
            "BI-24 lines takes the tail of the file while totalLines keeps reporting the whole of it",
            "BI-25 timestamp false drops the time prefix but keeps the level prefix",
            "BI-26 append false replaces the file instead of adding to it",
            "BI-27 clear empties the file, and reading an empty file is a success with no content",
            "BI-28 an unusable custom filename falls back to the default file",
        ]) {
            recorder.addCase(
                skipped,
                true,
                { skipped: "these need a writable log file; see BI-21" },
                { skipped: "BI-21 did not pass" },
            );
        }
        await runLogRejectionCases(bridge, recorder);
        return;
    }

    logState.probePath = probePath;
    const defaultPath = join(dirname(probePath), "webview_ui.log");
    logState.defaultPath = defaultPath;
    logState.originalBytes = existsSync(defaultPath) ? readFileSync(defaultPath) : null;

    // A file that is not there at all reads as success with empty content, and
    // drops the lines and totalLines fields that a real read carries. That is a
    // defensible reading of "no log yet", but it is also why a log that could
    // not be opened at all once looked exactly like a log with nothing in it.
    rmSync(defaultPath, { force: true });
    const readMissing = await invoke("log.read", {});
    recorder.assertCase(
        "BI-22 a missing log file reads as an empty one, without the lines and totalLines fields",
        readMissing?.success === true &&
            readMissing.content === "" &&
            readMissing.lineCount === 0 &&
            !("lines" in readMissing) &&
            !("totalLines" in readMissing),
        {
            readMissing,
            fieldsPresent: Object.keys(readMissing ?? {}),
        },
        {
            success: true,
            content: "",
            lineCount: 0,
            linesField: "absent",
            totalLinesField: "absent",
        },
    );

    await invoke("log.clear", {});
    const marker = `marker ${runId}`;
    const written = await invoke("log.write", { message: marker, level: "warn" });
    const read = await invoke("log.read", {});
    recorder.assertCase(
        "BI-23 a write to the default file is read back with an upper-cased level and a timestamp",
        written?.success === true &&
            written.path === defaultPath &&
            read?.success === true &&
            read.content.includes(marker) &&
            read.content.includes("[WARN]") &&
            /\[\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}\.\d{3}\]/.test(read.content),
        { written, content: read?.content, lineCount: read?.lineCount },
        { content: `contains ${marker} and [WARN] and a timestamp` },
    );

    await invoke("log.write", { message: `second ${runId}`, timestamp: false });
    await invoke("log.write", { message: `third ${runId}` });
    const tail = await invoke("log.read", { lines: 1 });
    const all = await invoke("log.read", { lines: 100 });
    recorder.assertCase(
        "BI-24 lines takes the tail of the file while totalLines keeps reporting the whole of it",
        tail?.lineCount === 1 &&
            tail.lines.length === 1 &&
            tail.content.includes(`third ${runId}`) &&
            all?.totalLines === 3 &&
            all.lines.length === 3,
        {
            tail: { lineCount: tail?.lineCount, content: tail?.content },
            totalLines: all?.totalLines,
            returnedLines: all?.lines?.length,
        },
        { tail: { lineCount: 1, content: "the last line" }, totalLines: 3, returnedLines: 3 },
    );

    const noTimestampLine = all.lines[1] ?? "";
    recorder.assertCase(
        "BI-25 timestamp false drops the time prefix but keeps the level prefix",
        noTimestampLine.startsWith("[INFO]") && noTimestampLine.includes(`second ${runId}`),
        { line: noTimestampLine },
        { line: "starts with [INFO], no leading timestamp" },
    );

    const truncating = await invoke("log.write", { message: `only ${runId}`, append: false });
    const afterTruncate = await invoke("log.read", {});
    recorder.assertCase(
        "BI-26 append false replaces the file instead of adding to it",
        truncating?.success === true &&
            afterTruncate.totalLines === 1 &&
            afterTruncate.content.includes(`only ${runId}`),
        { totalLines: afterTruncate?.totalLines, content: afterTruncate?.content },
        { totalLines: 1 },
    );

    await invoke("log.clear", {});
    const cleared = await invoke("log.read", {});
    recorder.assertCase(
        "BI-27 clear empties the file, and reading an empty file is a success with no content",
        cleared?.success === true && cleared.content === "" && cleared.totalLines === 0,
        { cleared },
        { success: true, content: "", totalLines: 0 },
    );

    // An unusable custom filename is not refused - it is ignored, and the write
    // lands in the default file. A caller who mistypes gets a success and no hint.
    const traversal = await invoke("log.write", {
        file: "../escape.log",
        message: `traversal ${runId}`,
    });
    const badExtension = await invoke("log.write", {
        file: "notes.exe",
        message: `extension ${runId}`,
    });
    const reserved = await invoke("log.write", { file: "CON.log", message: `reserved ${runId}` });
    recorder.assertCase(
        "BI-28 an unusable custom filename is silently ignored, so the write falls back to the default file",
        [traversal, badExtension, reserved].every(
            (res) => res?.success === true && res.path === defaultPath,
        ),
        {
            traversal: traversal?.path,
            badExtension: badExtension?.path,
            reserved: reserved?.path,
            defaultPath,
        },
        { each: { success: true, path: "the default log file" } },
    );

    await runLogRejectionCases(bridge, recorder);
}

/**
 * The two validation branches, split out because they answer before any file is
 * touched and so stay meaningful while the write path is broken.
 */
async function runLogRejectionCases(bridge, recorder) {
    const { invokeRaw } = bridge;

    const negative = await invokeRaw("log.read", { lines: -1 });
    const noMessage = await invokeRaw("log.write", {});
    // Both are parameter errors, told apart by code rather than by wording: the line count is
    // checked by the generated parser (lines has a minimum of 0), the missing message by the handler.
    recorder.assertCase(
        "BI-29 log refuses a negative line count and a missing message",
        negative?.value?.success === false &&
            negative.value.code === "INVALID_PARAMS" &&
            /lines/.test(negative.value.error ?? "") &&
            noMessage?.value?.success === false &&
            noMessage.value.code === "INVALID_PARAMS",
        { negative: negative?.value, noMessage: noMessage?.value },
        {
            negative: { success: false, code: "INVALID_PARAMS", error: "names lines" },
            noMessage: { success: false, code: "INVALID_PARAMS" },
        },
    );
}

async function runRegistryAndCursorCases(bridge, recorder, events, cursorEntry) {
    const { invoke, invokeRaw } = bridge;

    const plugins = await invoke("system.getRegisteredPlugins", {});
    const unknown = await invoke("system.isPluginRegistered", { namespace: `nope-${runId}` });
    const missingNamespace = await invoke("system.isPluginRegistered", {});
    recorder.assertCase(
        "BI-30 the registry answers with a plugins array in the envelope, reports an unknown namespace as absent, and refuses a missing one with INVALID_PARAMS",
        plugins?.success === true &&
            Array.isArray(plugins.plugins) &&
            unknown?.success === true &&
            unknown.registered === false &&
            missingNamespace?.success === false &&
            missingNamespace.code === "INVALID_PARAMS" &&
            /namespace/i.test(missingNamespace.error ?? ""),
        {
            pluginsIsArray: Array.isArray(plugins?.plugins),
            pluginCount: Array.isArray(plugins?.plugins) ? plugins.plugins.length : undefined,
            unknown,
            missingNamespace,
        },
        {
            plugins: { success: true, plugins: "an array" },
            unknown: { registered: false },
            missingNamespace: { success: false, code: "INVALID_PARAMS", error: "names namespace" },
        },
    );

    await events.drain();
    const hide = await invoke("cursor.setHidden", { hidden: !cursorEntry });
    const readHidden = await invoke("cursor.isHidden", {});
    const again = await invoke("cursor.setHidden", { hidden: !cursorEntry });
    await events.waitFor((all) => named(all, "cursor:hiddenChanged").length > 0, {
        timeoutMs: eventTimeoutMs,
    });
    const hiddenEvents = named(events.received, "cursor:hiddenChanged");
    recorder.assertCase(
        "BI-31 setHidden reports whether it changed anything, isHidden agrees, and only a real change is announced",
        hide?.success === true &&
            hide.changed === true &&
            readHidden?.hidden === !cursorEntry &&
            readHidden?.success === true &&
            again?.changed === false &&
            hiddenEvents.length === 1 &&
            hiddenEvents[0].payload?.hidden === !cursorEntry,
        {
            hide,
            readHidden,
            hasSuccessField: "success" in readHidden,
            again,
            announcements: hiddenEvents.length,
        },
        {
            firstCall: { changed: true },
            repeatCall: { changed: false },
            announcements: 1,
            successFieldOnRead: false,
        },
    );

    const noFlag = await invokeRaw("cursor.setHidden", {});
    const wrongType = await invokeRaw("cursor.setHidden", { hidden: "yes" });
    recorder.assertCase(
        "BI-32 setHidden requires an actual boolean",
        noFlag?.value?.success === false && wrongType?.value?.success === false,
        { noFlag: noFlag?.value, wrongType: wrongType?.value },
        { each: { success: false } },
    );
}

async function runToastCases(bridge, recorder, events) {
    const { invoke } = bridge;
    const TOAST_KEYS = "duration,message,position,type";
    const keysOf = (payload) => (payload ? Object.keys(payload).sort().join(",") : "");

    // The host draws nothing; the payload is the whole contract. A page that renders
    // toasts will show these two, which is harmless.
    await events.drain();
    const plain = await invoke("ui.showToast", { message: `e2e toast ${runId}` });
    const explicit = await invoke("ui.showToast", {
        message: `e2e toast ${runId} explicit`,
        duration: 0,
        type: "warning",
        position: "top-left",
    });
    await events.waitFor((all) => named(all, "ui:toast").length >= 2, { timeoutMs: eventTimeoutMs });
    const [first, second] = named(events.received, "ui:toast").map((entry) => entry.payload);
    recorder.assertCase(
        "BI-33 showToast hands the calling page exactly message, duration, type and position, defaulting to 3000, info and bottom-right, and passes duration and position through unchecked",
        plain?.success === true &&
            explicit?.success === true &&
            keysOf(first) === TOAST_KEYS &&
            first.message === `e2e toast ${runId}` &&
            first.duration === 3000 &&
            first.type === "info" &&
            first.position === "bottom-right" &&
            keysOf(second) === TOAST_KEYS &&
            second.duration === 0 &&
            second.type === "warning" &&
            second.position === "top-left",
        { plain, explicit, first, second },
        {
            first: { duration: 3000, type: "info", position: "bottom-right" },
            second: { duration: 0, type: "warning", position: "top-left" },
            keys: TOAST_KEYS,
        },
    );
}

const recorder = createRecorder();
let client;
let bridge;
let events;
let blocked = false;
let fatalError;
let targets;
let cursorEntry = false;
const openPorts = [];
const logState = {};

try {
    const connection = await connectBridgePage();
    client = connection.client;
    targets = connection.candidates;
    console.log(`Target: ${connection.page.title} ${connection.page.url}`);
    bridge = createBridge(client.Runtime, { invokeTimeoutMs });
    await requireResponsiveBridge(bridge, { probeId: runId });

    cursorEntry = (await bridge.invoke("cursor.isHidden", {})).hidden === true;
    events = await createEventCollector(bridge, EVENT_NAMES, { collectorId: runId });

    await runStateCases(bridge, recorder, events);
    const { windowId } = await runPortCases(bridge, recorder, events, openPorts);
    await runEventCases(bridge, recorder, events, windowId);
    await runConsoleCases(bridge, recorder);
    await runLogCases(bridge, recorder, logState);
    await runRegistryAndCursorCases(bridge, recorder, events, cursorEntry);
    await runToastCases(bridge, recorder, events);
} catch (error) {
    fatalError = error;
    blocked = Boolean(error?.blocked);
    console.error(`${blocked ? "BLOCKED" : "ERROR"}: ${error?.message || error}`);
} finally {
    if (bridge) {
        const quiet = (method, params) =>
            bridge.invokeRaw(method, params).catch(() => undefined);
        for (const key of [KEY_ALPHA, KEY_BETA, KEY_TTL]) {
            await quiet("state.delete", { key });
        }
        for (const portId of openPorts) {
            await quiet("port.disconnect", { portId });
        }
        await quiet("cursor.setHidden", { hidden: cursorEntry });
    }
    // The plugin log belongs to the user, so its bytes go back exactly as found.
    if (logState.defaultPath) {
        try {
            if (logState.originalBytes === null) {
                rmSync(logState.defaultPath, { force: true });
            } else {
                writeFileSync(logState.defaultPath, logState.originalBytes);
            }
        } catch {
            // Reported by the case results; restoration failure must not mask them.
        }
    }
    if (logState.probePath) {
        try {
            rmSync(logState.probePath, { force: true });
        } catch {
            // As above.
        }
    }
    if (events) await events.stop();
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
            entryState: { cursorHidden: cursorEntry },
            eventsSeen: (events?.received ?? []).map((entry) => entry.name),
            targets,
        },
    }),
);

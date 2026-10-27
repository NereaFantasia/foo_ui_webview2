/**
 * Covers the content versions of a playlist and of the queue: which reads report one, which
 * changes advance it, and that a write carrying an outdated `expectedVersion` changes nothing.
 *
 * Everything runs on a scratch playlist built from library tracks, removed at the end. The queue
 * cases need an empty queue to start from and leave it empty; with anything already queued they
 * are recorded as not run rather than touching the user's queue.
 *
 * Usage: node mcp/tests/e2e-content-version.mjs
 * Environment: FB2K_CDP_PORT, FB2K_E2E_TIMEOUT_MS, FB2K_EVENT_TIMEOUT_MS.
 */
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

const invokeTimeoutMs = envInt("FB2K_E2E_TIMEOUT_MS", 15000);
const eventTimeoutMs = envInt("FB2K_EVENT_TIMEOUT_MS", 4000);
const runId = new Date().toISOString().replace(/[-:.TZ]/g, "");
const NAME = `e2e content version ${runId}`;
const SUBJECT_COUNT = 4;

const recorder = createRecorder();
let client;
let bridge;
let collector;
let blocked = false;
let fatalError;
let guid;
let queueTouched = false;
/** A playlist version from before the first change, so it no longer matches. */
let outdated;

async function waitForEvent(name, filter) {
    await collector.waitFor(
        (events) => events.some((event) => event.name === name && filter(event.payload)),
        { timeoutMs: eventTimeoutMs, pollMs: 50 },
    ).catch(() => undefined);
    return collector.received.find((event) => event.name === name && filter(event.payload))?.payload;
}

async function runPlaylistCases(invoke) {
    const version = async () => (await invoke("playlist.getTracks", { playlistGuid: guid, count: 1 }))?.contentVersion;

    const first = await invoke("playlist.getTracks", { playlistGuid: guid, start: 0, count: 2 });
    const second = await invoke("playlist.getTracks", { playlistGuid: guid, start: 2, count: 2 });
    const at = await invoke("playlist.getTracksAt", { playlistGuid: guid, rows: [0, 3] });
    const selection = await invoke("playlist.getSelection", { playlistGuid: guid });
    const selected = await invoke("playlist.getSelectedTracks", { playlistGuid: guid });
    const matching = await invoke("playlist.getMatchingRows", { playlistGuid: guid, query: "ALL" });
    const runs = await invoke("playlist.getGroupRuns", { playlistGuid: guid, patterns: ["%title%"] });
    const versions = [second, at, selection, selected, matching, runs].map((answer) => answer?.contentVersion);
    recorder.assertCase(
        "CV-01 every read that reports row numbers carries the same version while nothing changes",
        typeof first?.contentVersion === "string" && first.contentVersion !== "" &&
            versions.every((value) => value === first.contentVersion),
        { first: first?.contentVersion, others: versions },
        { all: "equal and non-empty" },
    );

    const selectedAnswer = await invoke("playlist.setSelection", { playlistGuid: guid, indices: [1, 2] });
    const afterSelection = await version();
    recorder.assertCase(
        "CV-02 selecting rows leaves the version unchanged and reports it",
        selectedAnswer?.success === true && selectedAnswer.contentVersion === first.contentVersion &&
            afterSelection === first.contentVersion,
        { selectedAnswer, afterSelection },
        { contentVersion: first.contentVersion },
    );

    const reordered = await invoke("playlist.reorder", {
        playlistGuid: guid,
        newOrder: [3, 2, 1, 0],
        expectedVersion: first.contentVersion,
    });
    const afterReorder = await invoke("playlist.getTracks", { playlistGuid: guid, count: 1 });
    recorder.assertCase(
        "CV-03 a reorder keeps the row count but changes the version, and reports the new one",
        reordered?.success === true && afterReorder?.total === SUBJECT_COUNT &&
            reordered.contentVersion !== first.contentVersion &&
            reordered.contentVersion === afterReorder.contentVersion,
        { reordered, after: { total: afterReorder?.total, contentVersion: afterReorder?.contentVersion } },
        { total: SUBJECT_COUNT, contentVersion: "new, equal to the next read" },
    );

    const staleRemove = await invoke("playlist.removeTracks", {
        playlistGuid: guid,
        items: [0],
        expectedVersion: first.contentVersion,
    });
    const afterStale = await invoke("playlist.getTracks", { playlistGuid: guid, count: 1 });
    recorder.assertCase(
        "CV-04 a removal with an outdated version changes nothing and reports the current version",
        staleRemove?.success === false && staleRemove.code === "STALE_TARGET" &&
            staleRemove.details?.current === afterReorder.contentVersion &&
            afterStale?.total === SUBJECT_COUNT && afterStale.contentVersion === afterReorder.contentVersion,
        { staleRemove, afterStale: { total: afterStale?.total, contentVersion: afterStale?.contentVersion } },
        { code: "STALE_TARGET", total: SUBJECT_COUNT },
    );

    const removed = await invoke("playlist.removeTracks", {
        playlistGuid: guid,
        items: [0],
        expectedVersion: afterReorder.contentVersion,
    });
    const event = await waitForEvent("playlist:itemsRemoved", (payload) => payload?.playlistGuid === guid);
    const afterRemove = await invoke("playlist.getTracks", { playlistGuid: guid, count: 1 });
    recorder.assertCase(
        "CV-05 a removal with the current version goes through, and its event carries the version it reports",
        removed?.success === true && afterRemove?.total === SUBJECT_COUNT - 1 &&
            removed.contentVersion === afterRemove.contentVersion &&
            event?.contentVersion === removed.contentVersion,
        { removed, event, afterRemove: { total: afterRemove?.total, contentVersion: afterRemove?.contentVersion } },
        { total: SUBJECT_COUNT - 1, eventVersion: "equal to the reported version" },
    );

    const staleInsert = await invoke("playlist.insertTracks", {
        playlistGuid: guid,
        position: 0,
        handles: [first.tracks?.[0]?.path].filter(Boolean),
        expectedVersion: first.contentVersion,
    });
    const staleMove = await invoke("playlist.moveTracks", {
        playlistGuid: guid,
        items: [0],
        delta: 1,
        expectedVersion: first.contentVersion,
    });
    const staleSelect = await invoke("playlist.setSelection", {
        playlistGuid: guid,
        indices: [0],
        expectedVersion: first.contentVersion,
    });
    const moved = await invoke("playlist.moveTracks", {
        playlistGuid: guid,
        items: [0],
        delta: 1,
        expectedVersion: removed.contentVersion,
    });
    const afterAll = await invoke("playlist.getTracks", { playlistGuid: guid, count: 1 });
    recorder.assertCase(
        "CV-06 insert, move and select refuse an outdated version; a move with the current one goes through",
        [staleInsert, staleMove, staleSelect].every((answer) => answer?.code === "STALE_TARGET") &&
            moved?.success === true && moved.contentVersion === afterAll?.contentVersion &&
            afterAll.total === SUBJECT_COUNT - 1,
        { staleInsert, staleMove, staleSelect, moved, afterAll: { total: afterAll?.total } },
        { stale: "STALE_TARGET", moved: "success with the next read's version" },
    );
    outdated = first.contentVersion;

    const focusBefore = await invoke("playlist.getFocusedTrack", { playlistGuid: guid });
    const staleFocus = await invoke("playlist.setFocusedTrack", { playlistGuid: guid, index: 2, expectedVersion: outdated });
    const staleOldFocus = await invoke("playlist.focusTrack", { playlistGuid: guid, index: 2, expectedVersion: outdated });
    const focusAfterStale = await invoke("playlist.getFocusedTrack", { playlistGuid: guid });
    // Only the refused case is played: a successful playTrack would start audible playback.
    const stalePlay = await invoke("playlist.playTrack", { playlistGuid: guid, index: 0, expectedVersion: outdated });
    const focused = await invoke("playlist.setFocusedTrack", {
        playlistGuid: guid,
        index: 2,
        expectedVersion: afterAll.contentVersion,
    });
    const focusAfter = await invoke("playlist.getFocusedTrack", { playlistGuid: guid });
    recorder.assertCase(
        "CV-09 focusing and playing a row refuse an outdated version; focusing with the current one goes through",
        [staleFocus, staleOldFocus, stalePlay].every((answer) => answer?.code === "STALE_TARGET") &&
            focusAfterStale?.index === focusBefore?.index && focused?.success === true && focusAfter?.index === 2,
        { staleFocus, staleOldFocus, stalePlay, focusBefore: focusBefore?.index, focusAfterStale: focusAfterStale?.index, focusAfter: focusAfter?.index },
        { stale: "STALE_TARGET", focusAfter: 2 },
    );
}

async function runQueueCases(invoke) {
    const start = await invoke("queue.get", {});
    if (!start?.success || start.count !== 0) {
        recorder.assertCase(
            "CV-07 the queue starts empty, so its cases can run without touching queued entries",
            false,
            { count: start?.count },
            { count: 0 },
        );
        return;
    }
    queueTouched = true;
    const added = await invoke("queue.add", { playlistGuid: guid, tracks: [0, 1, 2] });
    const filled = await invoke("queue.get", {});
    const staleRemove = await invoke("queue.remove", { index: 0, expectedVersion: start.contentVersion });
    const afterStale = await invoke("queue.get", {});
    recorder.assertCase(
        "CV-07 adding to the queue changes its version, and a removal with the old one changes nothing",
        added?.success === true && filled?.count === 3 && filled.contentVersion !== start.contentVersion &&
            staleRemove?.code === "STALE_TARGET" && staleRemove.details?.current === filled.contentVersion &&
            afterStale?.count === 3,
        { start: start.contentVersion, filled: filled?.contentVersion, staleRemove, afterStale: afterStale?.count },
        { code: "STALE_TARGET", count: 3 },
    );

    const removed = await invoke("queue.remove", { index: 0, expectedVersion: filled.contentVersion });
    const event = await waitForEvent(
        "playback:queueChanged",
        (payload) => payload?.contentVersion === removed?.contentVersion,
    );
    const moved = await invoke("queue.moveToTop", { index: 1, expectedVersion: removed?.contentVersion });
    const staleSet = await invoke("queue.setContents", { items: [], expectedVersion: filled.contentVersion });
    const end = await invoke("queue.get", {});
    recorder.assertCase(
        "CV-08 queue writes with the current version go through and report it; the event carries it too",
        removed?.success === true && event !== undefined && moved?.success === true &&
            moved.contentVersion === end?.contentVersion && staleSet?.code === "STALE_TARGET" && end?.count === 2,
        { removed, event, moved, staleSet, end: { count: end?.count, contentVersion: end?.contentVersion } },
        { count: 2, staleSet: "STALE_TARGET" },
    );

    const listVersion = (await invoke("playlist.getTracks", { playlistGuid: guid, count: 1 }))?.contentVersion;
    const staleAdd = await invoke("queue.add", { playlistGuid: guid, tracks: [0], expectedPlaylistVersion: outdated });
    const staleNext = await invoke("queue.insertNext", {
        items: [{ playlistGuid: guid, item: 0, expectedPlaylistVersion: outdated }],
    });
    const staleRow = await invoke("queue.setContents", {
        items: [{ queueIndex: 0 }, { playlistGuid: guid, item: 1, expectedPlaylistVersion: outdated }],
    });
    const misplaced = await invoke("queue.setContents", { items: [{ queueIndex: 0, expectedPlaylistVersion: listVersion }] });
    const afterRefused = await invoke("queue.get", {});
    const addedCurrent = await invoke("queue.add", { playlistGuid: guid, tracks: [0], expectedPlaylistVersion: listVersion });
    const final = await invoke("queue.get", {});
    recorder.assertCase(
        "CV-10 queue calls naming playlist rows refuse an outdated playlist version, and accept the current one",
        [staleAdd, staleNext, staleRow].every((answer) => answer?.code === "STALE_TARGET" && answer.details?.current === listVersion) &&
            misplaced?.code === "INVALID_PARAMS" && afterRefused?.contentVersion === end?.contentVersion &&
            addedCurrent?.success === true && final?.count === 3,
        { staleAdd, staleNext, staleRow, misplaced, afterRefused: afterRefused?.count, addedCurrent, final: final?.count },
        { stale: "STALE_TARGET with details.current", misplaced: "INVALID_PARAMS", final: 3 },
    );
}

try {
    const connection = await connectBridgePage();
    client = connection.client;
    console.log(`Target: ${connection.page.title} ${connection.page.url}`);
    bridge = createBridge(client.Runtime, { invokeTimeoutMs });
    await requireResponsiveBridge(bridge, { probeId: runId });
    const { invoke } = bridge;

    const page = await bridge.invoke("library.query", { query: "ALL", limit: 50 });
    const paths = (page?.tracks ?? []).map((track) => track?.absolutePath).filter(Boolean).slice(0, SUBJECT_COUNT);
    if (paths.length < SUBJECT_COUNT) {
        recorder.assertCase(
            "CV-00 the library can supply subject tracks",
            false,
            { available: paths.length },
            { available: SUBJECT_COUNT },
        );
    } else {
        const created = await bridge.invoke("playlist.create", { name: NAME });
        guid = created?.guid;
        await bridge.invoke("playlist.addPathsSequential", { playlistGuid: guid, paths });
        collector = await createEventCollector(bridge, ["playlist:itemsRemoved", "playback:queueChanged"], {
            collectorId: runId,
        });
        await runPlaylistCases(invoke);
        await runQueueCases(invoke);
    }
} catch (error) {
    fatalError = error;
    blocked = Boolean(error?.blocked);
    console.error(`${blocked ? "BLOCKED" : "ERROR"}: ${error?.message || error}`);
} finally {
    if (collector) await collector.stop();
    if (bridge) {
        const quiet = (method, params) => bridge.invokeRaw(method, params).catch(() => undefined);
        if (queueTouched) await quiet("queue.clear", {});
        if (guid) await quiet("playlist.remove", { playlistGuid: guid });
    }
    await closeClient(client);
}

process.exit(
    report({
        recorder,
        blocked,
        fatalError,
        extra: { runId, targetPort: resolvePort(), invokeTimeoutMs, eventTimeoutMs },
    }),
);

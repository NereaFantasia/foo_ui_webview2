/**
 * Checks that playlist mutations actually emit their events, and that each
 * payload carries the fields a page needs.
 *
 * Static scanning can prove an event name exists in the C++ source; it cannot
 * prove the event is ever emitted, reaches the page, or carries the documented
 * fields. Historically that gap was closed by a human noticing a stale view.
 * Here a listener is installed before each trigger and the arriving payload is
 * matched against the mutation that caused it.
 *
 * Everything happens on a scratch playlist created for the run and removed
 * afterwards, and the entry active playlist is put back, so no existing playlist
 * is touched. Track insertion reuses a path already in the library, so no file
 * is written.
 *
 * Usage: node mcp/tests/e2e-playlist-events.mjs
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
const eventTimeoutMs = envInt("FB2K_EVENT_TIMEOUT_MS", 3000);
const runId = new Date().toISOString().replace(/[-:.TZ]/g, "");

const SCRATCH_NAME = `e2e scratch ${runId}`;
const RENAMED_NAME = `${SCRATCH_NAME} renamed`;

const EVENT_NAMES = [
    "playlist:created",
    "playlist:activated",
    "playlist:renamed",
    "playlist:itemsAdded",
    "playlist:itemsRemoved",
    "playlist:removed",
];

function eventsNamed(received, name) {
    return received.filter((event) => event.name === name);
}

async function waitForEvent(collector, name, minCount = 1) {
    await collector.waitFor((received) => eventsNamed(received, name).length >= minCount, {
        timeoutMs: eventTimeoutMs,
    });
    return eventsNamed(collector.received, name);
}

const recorder = createRecorder();
let client;
let bridge;
let collector;
let blocked = false;
let fatalError;
let targets;
let entryActiveIndex;
let scratchIndex;

try {
    const connection = await connectBridgePage();
    client = connection.client;
    targets = connection.candidates;
    console.log(`Target: ${connection.page.title} ${connection.page.url}`);
    bridge = createBridge(client.Runtime, { invokeTimeoutMs });
    await requireResponsiveBridge(bridge, { probeId: runId });

    const entryActive = await bridge.invoke("playlist.getActive", {});
    entryActiveIndex = entryActive?.index;
    const entryAll = await bridge.invoke("playlist.getAll", {});
    const entryCount = Array.isArray(entryAll?.playlists) ? entryAll.playlists.length : undefined;

    collector = await createEventCollector(bridge, EVENT_NAMES, { collectorId: runId });

    const created = await bridge.invoke("playlist.create", { name: SCRATCH_NAME });
    scratchIndex = created?.index;
    const createdEvents = await waitForEvent(collector, "playlist:created");
    const createdMatch = createdEvents.find(
        (event) => event.payload?.name === SCRATCH_NAME,
    );
    recorder.assertCase(
        "PE-01 playlist.create emits playlist:created carrying the new index, GUID and name",
        created?.success === true &&
            createdMatch !== undefined &&
            createdMatch.payload.index === scratchIndex &&
            typeof created?.guid === "string" &&
            createdMatch.payload.guid === created.guid,
        {
            createReturned: { success: created?.success, index: scratchIndex, guid: created?.guid },
            payload: createdMatch?.payload,
            entryCount,
        },
        { payload: { index: scratchIndex, guid: created?.guid, name: SCRATCH_NAME } },
    );

    await bridge.invoke("playlist.setActive", { playlist: scratchIndex });
    const activatedEvents = await waitForEvent(collector, "playlist:activated");
    const activatedMatch = activatedEvents.find(
        (event) => event.payload?.newIndex === scratchIndex,
    );
    recorder.assertCase(
        "PE-02 playlist.setActive emits playlist:activated with both indices and the new playlist's GUID",
        activatedMatch !== undefined &&
            Number.isInteger(activatedMatch.payload.oldIndex) &&
            activatedMatch.payload.oldIndex !== activatedMatch.payload.newIndex &&
            activatedMatch.payload.newGuid === created?.guid,
        { entryActiveIndex, payload: activatedMatch?.payload },
        { payload: { newIndex: scratchIndex, newGuid: created?.guid, oldIndex: "a different integer" } },
    );

    await bridge.invoke("playlist.rename", { playlist: scratchIndex, name: RENAMED_NAME });
    const renamedEvents = await waitForEvent(collector, "playlist:renamed");
    const renamedMatch = renamedEvents.find(
        (event) => event.payload?.index === scratchIndex,
    );
    recorder.assertCase(
        "PE-03 playlist.rename emits playlist:renamed with the new name and the playlist's GUID",
        renamedMatch?.payload?.name === RENAMED_NAME && renamedMatch?.payload?.guid === created?.guid,
        { payload: renamedMatch?.payload },
        { payload: { index: scratchIndex, guid: created?.guid, name: RENAMED_NAME } },
    );

    const librarySample = await bridge.invoke("library.query", { query: "ALL", limit: 2 });
    const paths = (librarySample?.tracks ?? [])
        .map((track) => track.absolutePath)
        .filter((path) => typeof path === "string" && path.length > 0);

    if (paths.length === 0) {
        recorder.assertCase(
            "PE-04 a library path is available to insert into the scratch playlist",
            false,
            { reason: "library.query returned no usable absolutePath" },
            { paths: "at least one" },
        );
    } else {
        const added = await bridge.invoke("playlist.addPaths", {
            playlist: scratchIndex,
            paths,
        });
        const addedEvents = await waitForEvent(collector, "playlist:itemsAdded");
        const addedMatch = addedEvents.find(
            (event) => event.payload?.playlist === scratchIndex,
        );
        recorder.assertCase(
            "PE-04 playlist.addPaths emits playlist:itemsAdded for the target playlist",
            added?.success === true &&
                addedMatch !== undefined &&
                addedMatch.payload.count === paths.length &&
                addedMatch.payload.start === 0,
            { requestedPaths: paths.length, payload: addedMatch?.payload },
            { payload: { playlist: scratchIndex, start: 0, count: paths.length } },
        );

        await bridge.invoke("playlist.clear", { playlist: scratchIndex });
        const removedItemEvents = await waitForEvent(collector, "playlist:itemsRemoved");
        const removedItemMatch = removedItemEvents.find(
            (event) => event.payload?.playlist === scratchIndex,
        );
        recorder.assertCase(
            "PE-05 playlist.clear emits playlist:itemsRemoved with the count transition",
            removedItemMatch !== undefined &&
                removedItemMatch.payload.oldCount === paths.length &&
                removedItemMatch.payload.newCount === 0,
            { payload: removedItemMatch?.payload },
            { payload: { playlist: scratchIndex, oldCount: paths.length, newCount: 0 } },
        );
    }

    // Restoring the entry playlist before the removal keeps the scratch index
    // valid: removing the active playlist would shift activation on its own and
    // make the count transition harder to attribute.
    if (Number.isInteger(entryActiveIndex)) {
        await bridge.invoke("playlist.setActive", { playlist: entryActiveIndex });
    }

    const removedIndex = scratchIndex;
    const removed = await bridge.invoke("playlist.remove", { playlist: scratchIndex });
    const removedEvents = await waitForEvent(collector, "playlist:removed");
    const removedMatch = removedEvents[removedEvents.length - 1];
    recorder.assertCase(
        "PE-06 playlist.remove emits playlist:removed with the count transition and the index and GUID of the playlist removed",
        removed?.success === true &&
            removedMatch !== undefined &&
            removedMatch.payload.newCount === removedMatch.payload.oldCount - 1 &&
            JSON.stringify(removedMatch.payload.indices) === JSON.stringify([removedIndex]) &&
            JSON.stringify(removedMatch.payload.guids) === JSON.stringify([created?.guid]),
        { payload: removedMatch?.payload },
        { payload: { newCount: "oldCount minus one", indices: [removedIndex], guids: [created?.guid] } },
    );
    if (removed?.success === true) {
        scratchIndex = undefined;
    }

    // The whole lifecycle must have arrived in the order it was triggered; an
    // out-of-order or coalesced stream would break any view that rebuilds state
    // from these events.
    const order = collector.received
        .map((event) => event.name)
        .filter((name) => name !== "playlist:activated");
    const expectedOrder = [
        "playlist:created",
        "playlist:renamed",
        "playlist:itemsAdded",
        "playlist:itemsRemoved",
        "playlist:removed",
    ].filter((name) => order.includes(name));
    const firstIndexes = expectedOrder.map((name) => order.indexOf(name));
    recorder.assertCase(
        "PE-07 the lifecycle events arrived in trigger order",
        firstIndexes.every((value, i) => i === 0 || value > firstIndexes[i - 1]),
        { order, expectedOrder },
        { strictlyIncreasing: true },
    );
} catch (error) {
    fatalError = error;
    blocked = Boolean(error?.blocked);
    console.error(`${blocked ? "BLOCKED" : "ERROR"}: ${error?.message || error}`);
} finally {
    if (bridge) {
        if (Number.isInteger(scratchIndex)) {
            await bridge.invokeRaw("playlist.remove", { playlist: scratchIndex }).catch(
                () => undefined,
            );
        }
        if (Number.isInteger(entryActiveIndex)) {
            await bridge
                .invokeRaw("playlist.setActive", { playlist: entryActiveIndex })
                .catch(() => undefined);
        }
    }
    if (collector) {
        await collector.stop();
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
            entryActiveIndex,
            eventsSeen: collector?.received.map((event) => event.name),
            targets,
        },
    }),
);

/**
 * Covers the just-in-time queue (jitQueue.*) and the queue.flush alias.
 *
 * The JIT queue exists so a front-end can feed foobar2000 one streaming track
 * at a time: playNow starts a track, the C++ side asks for the next one through
 * jitQueue:needNext, and enqueueNext answers. That design means the interesting
 * half of the surface only exists while something is actually playing.
 *
 * This suite deliberately never starts playback, and still reaches all nine
 * endpoints, because the state machine is guarded at the front:
 *   - playNow, enqueueNext and preloadBatch validate their arguments and return
 *     before QueueManager is touched at all, so their refusals cost nothing;
 *   - enqueueNext and skip are refused outright while the machine is Idle, which
 *     is a state guard rather than an argument check;
 *   - notifyEmpty drives Idle -> Exhausted and emits jitQueue:listExhausted
 *     without any track;
 *   - clear and stop are the documented way back to Idle.
 *
 * What that leaves untested is stated rather than hidden (JQ-15): the accepted
 * paths of playNow and preloadBatch, and four of the five events
 * (trackChanged, needNext, preloadComplete, error). Exercising those means real
 * playback, which is audible, switches the active and playing playlist, forces
 * playbackOrder to Default, and leaves a locked shadow playlist that
 * query_playlist_remove refuses to delete - it can only be emptied. They belong
 * with the audible playback batch, not here.
 *
 * Cases are guarded on the entry state instead of assuming it, because most of
 * this surface is not read-only:
 *   - jitQueue.stop calls playback_control::stop unconditionally, so it needs a
 *     stopped instance;
 *   - queue.flush empties the play queue, so it needs an empty one;
 *   - notifyEmpty, clear and skip all move the machine, and clear empties the
 *     shadow playlist. If the page is itself mid-session on the JIT queue - a
 *     front-end streaming one track at a time is the whole reason this API
 *     exists - running them would drop the queued next track and break that
 *     stream's auto-advance. So the entry state is checked for that too, and
 *     every case that would move the machine records why it was skipped.
 * What remains in that situation is the argument-validation half, which returns
 * before QueueManager is touched at all and is safe whatever the page is doing.
 *
 * Usage: node mcp/tests/e2e-jit-queue.mjs
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

const invokeTimeoutMs = envInt("FB2K_E2E_TIMEOUT_MS", 8000);
const eventTimeoutMs = envInt("FB2K_EVENT_TIMEOUT_MS", 3000);
const runId = new Date().toISOString().replace(/[-:.TZ]/g, "");

/** ApiLimits::MAX_STREAM_URL_LENGTH; foo_httpstream overflows past it. */
const MAX_URL_LENGTH = 2048;
const TOO_LONG_URL = `http://127.0.0.1/${"a".repeat(MAX_URL_LENGTH)}`;
/** Well-formed but unresolvable, so nothing can start even if a guard slipped. */
const INERT_URL = `http://127.0.0.1:9/e2e-${runId}.mp3`;

const EVENT_NAMES = [
    "jitQueue:listExhausted",
    "jitQueue:trackChanged",
    "jitQueue:needNext",
    "jitQueue:preloadComplete",
    "jitQueue:error",
];

function named(events, name) {
    return events.filter((entry) => entry.name === name);
}

/**
 * The Idle reading of every field getState reports. requireEmptyBuffer is opt-out
 * only for the entry reading, where an empty buffer is a property of the instance
 * rather than of anything this suite did.
 */
function idleShape(state, { requireEmptyBuffer = true } = {}) {
    return (
        state?.isActive === false &&
        state.state === "Idle" &&
        state.currentTrackId === "" &&
        state.nextTrackId === "" &&
        (!requireEmptyBuffer || state.bufferSize === 0)
    );
}

/**
 * True when the page is mid-session on the JIT queue, in which case none of the
 * state-machine cases may run: notifyEmpty and clear are not read-only, and
 * clear empties the shadow playlist and drops the queued next track, which
 * breaks a live stream's auto-advance.
 */
function jitQueueInUse(state) {
    return (
        state?.isActive === true || state?.state !== "Idle" || (state?.bufferSize ?? 0) > 0
    );
}

async function runStateMachineCases(bridge, recorder, events, inUse) {
    const { invoke } = bridge;

    const entryState = await invoke("jitQueue.getState", {});

    // Since the declaration in src/api/schema/jitQueue.ts an uncreated shadow
    // playlist reports -1 (the host's SIZE_MAX marker cast to a signed
    // integer), so "unset" is an exact literal again.
    const shadow = entryState?.shadowPlaylist;
    const shadowUnset = shadow === -1;

    if (inUse) {
        for (const name of [
            "JQ-01 an unused JIT queue reports Idle with no track, no next track and an empty buffer",
            "JQ-03 notifyEmpty drives the machine to Exhausted and announces it, with no track involved",
            "JQ-04 clear brings Exhausted back to Idle",
        ]) {
            recorder.addCase(
                name,
                true,
                {
                    skipped:
                        "the page is using the JIT queue; notifyEmpty and clear would break a live stream's auto-advance",
                    entryState,
                },
                { skipped: "needs an idle JIT queue" },
            );
        }
        recorder.assertCase(
            "JQ-02 an uncreated shadow playlist reports -1, a created one its playlist index",
            Number.isInteger(shadow) && (shadowUnset || shadow >= 0),
            { shadowPlaylist: shadow, unset: shadowUnset },
            { unset: -1, created: "a non-negative playlist index" },
        );
        return;
    }

    // bufferSize is the shadow playlist's item count, and __webview_buffer__
    // outlives the process with whatever it last held. While QueueManager has not
    // bound it the count reads 0 whatever is in it, so the empty reading only
    // means "unused" in the unbound case.
    recorder.assertCase(
        "JQ-01 an unused JIT queue reports Idle with no track, no next track and an empty buffer",
        idleShape(entryState, { requireEmptyBuffer: shadowUnset }),
        { entryState, shadowBound: !shadowUnset },
        {
            isActive: false,
            state: "Idle",
            currentTrackId: "",
            nextTrackId: "",
            bufferSize: shadowUnset
                ? 0
                : "not asserted: a bound shadow playlist reports the contents it kept",
        },
    );

    recorder.assertCase(
        "JQ-02 an uncreated shadow playlist reports -1, a created one its playlist index",
        Number.isInteger(shadow) && (shadowUnset || shadow >= 0),
        { shadowPlaylist: shadow, unset: shadowUnset },
        { unset: -1, created: "a non-negative playlist index" },
    );

    await events.drain();
    const notified = await invoke("jitQueue.notifyEmpty", {});
    await events.waitFor((all) => named(all, "jitQueue:listExhausted").length > 0, {
        timeoutMs: eventTimeoutMs,
    });
    const exhaustedState = await invoke("jitQueue.getState", {});
    const listExhausted = named(events.received, "jitQueue:listExhausted").at(-1);
    recorder.assertCase(
        "JQ-03 notifyEmpty drives the machine to Exhausted and announces it, with no track involved",
        notified?.success === true &&
            exhaustedState?.state === "Exhausted" &&
            listExhausted !== undefined &&
            Object.keys(listExhausted.payload ?? {}).join(",") === "lastTrackId" &&
            typeof listExhausted.payload.lastTrackId === "string",
        { notified, state: exhaustedState?.state, event: listExhausted?.payload },
        {
            success: true,
            state: "Exhausted",
            event: "jitQueue:listExhausted arrived with only lastTrackId, a string",
        },
    );

    const cleared = await invoke("jitQueue.clear", {});
    const afterClear = await invoke("jitQueue.getState", {});
    recorder.assertCase(
        "JQ-04 clear brings Exhausted back to Idle",
        cleared?.success === true && idleShape(afterClear),
        { cleared, afterClear },
        { success: true, state: "Idle" },
    );
}

async function runStopCases(bridge, recorder, playbackStopped) {
    const { invoke } = bridge;

    if (!playbackStopped) {
        recorder.addCase(
            "JQ-05 stop answers for either buffer disposition and leaves the machine Idle",
            true,
            {
                skipped:
                    "the instance is playing; jitQueue.stop calls playback_control::stop unconditionally and would end it",
            },
            { skipped: "needs a stopped instance" },
        );
        return;
    }

    const stopKeeping = await invoke("jitQueue.stop", { clearBuffer: false });
    const midState = await invoke("jitQueue.getState", {});
    const stopClearing = await invoke("jitQueue.stop", {});
    const endState = await invoke("jitQueue.getState", {});
    recorder.assertCase(
        "JQ-05 stop answers for either buffer disposition and leaves the machine Idle",
        stopKeeping?.success === true &&
            stopClearing?.success === true &&
            midState?.state === "Idle" &&
            idleShape(endState),
        { stopKeeping, midState: midState?.state, stopClearing, endState },
        { both: { success: true }, state: "Idle" },
    );
}

async function runGuardedRefusalCases(bridge, recorder, inUse) {
    const { invoke } = bridge;

    if (inUse) {
        for (const name of [
            "JQ-06 enqueueNext is refused while Idle, because a next track only means something during playback",
            "JQ-07 skip is refused while Idle and reports no current track",
            "JQ-08 a refused enqueueNext or skip leaves the machine where it was",
        ]) {
            recorder.addCase(
                name,
                true,
                {
                    skipped:
                        "the page is using the JIT queue, so the machine is not Idle and enqueueNext and skip would be accepted rather than refused - skip would advance a live stream",
                },
                { skipped: "needs an idle JIT queue" },
            );
        }
        return;
    }

    // The state guard, not an argument check: the arguments here are well formed.
    const enqueueIdle = await invoke("jitQueue.enqueueNext", {
        trackId: `t-${runId}`,
        title: "inert",
        url: INERT_URL,
    });
    recorder.assertCase(
        "JQ-06 enqueueNext is refused while Idle, because a next track only means something during playback",
        enqueueIdle?.success === false &&
            enqueueIdle.trackId === `t-${runId}` &&
            enqueueIdle.bufferSize === 0,
        { enqueueIdle },
        { success: false, trackId: `t-${runId}`, bufferSize: 0 },
    );

    const skipIdle = await invoke("jitQueue.skip", {});
    recorder.assertCase(
        "JQ-07 skip is refused while Idle and reports no current track",
        skipIdle?.success === false && skipIdle.currentTrackId === "",
        { skipIdle },
        { success: false, currentTrackId: "" },
    );

    const stillIdle = await invoke("jitQueue.getState", {});
    recorder.assertCase(
        "JQ-08 a refused enqueueNext or skip leaves the machine where it was",
        idleShape(stillIdle),
        { stillIdle },
        { state: "Idle", bufferSize: 0 },
    );
}

async function runArgumentCases(bridge, recorder) {
    const { invoke } = bridge;

    const playNoTrack = await invoke("jitQueue.playNow", { url: INERT_URL });
    const playNoUrl = await invoke("jitQueue.playNow", { trackId: `t-${runId}` });
    const playLongUrl = await invoke("jitQueue.playNow", {
        trackId: `t-${runId}`,
        url: TOO_LONG_URL,
    });
    recorder.assertCase(
        "JQ-09 playNow refuses a missing trackId, a missing url and an over-long url before it touches the queue",
        playNoTrack?.success === false &&
            /trackId/i.test(playNoTrack.error ?? "") &&
            playNoUrl?.success === false &&
            /url/i.test(playNoUrl.error ?? "") &&
            playLongUrl?.success === false &&
            playLongUrl.error === `URL exceeds maximum length (${MAX_URL_LENGTH})`,
        { playNoTrack, playNoUrl, playLongUrl },
        {
            missingTrackId: { success: false, error: "names trackId" },
            missingUrl: { success: false, error: "names url" },
            longUrl: { error: `URL exceeds maximum length (${MAX_URL_LENGTH})` },
        },
    );

    const enqNoTrack = await invoke("jitQueue.enqueueNext", { url: INERT_URL });
    const enqNoUrl = await invoke("jitQueue.enqueueNext", { trackId: `t-${runId}` });
    const enqLongUrl = await invoke("jitQueue.enqueueNext", {
        trackId: `t-${runId}`,
        url: TOO_LONG_URL,
    });
    recorder.assertCase(
        "JQ-10 enqueueNext applies the same three argument checks, ahead of its state guard",
        enqNoTrack?.success === false &&
            /trackId/i.test(enqNoTrack.error ?? "") &&
            enqNoUrl?.success === false &&
            /url/i.test(enqNoUrl.error ?? "") &&
            enqLongUrl?.success === false &&
            enqLongUrl.error === `URL exceeds maximum length (${MAX_URL_LENGTH})`,
        { enqNoTrack, enqNoUrl, enqLongUrl },
        { each: { success: false }, longUrl: { error: "the shared URL_TOO_LONG text" } },
    );

    // These two refusals are returned before the playbackOrder is saved or
    // forced, which is what makes them safe to call on a live instance.
    const emptyList = await invoke("jitQueue.preloadBatch", { urls: [] });
    const missingList = await invoke("jitQueue.preloadBatch", {});
    recorder.assertCase(
        "JQ-11 preloadBatch refuses an empty list, and a missing urls key reads as an empty one",
        emptyList?.success === false &&
            emptyList.error === "Empty URL list" &&
            emptyList.tracksAdded === 0 &&
            missingList?.success === false &&
            missingList.error === "Empty URL list",
        { emptyList, missingList },
        { each: { success: false, error: "Empty URL list", tracksAdded: 0 } },
    );

    const badStart = await invoke("jitQueue.preloadBatch", {
        urls: [INERT_URL],
        startIndex: 5,
    });
    recorder.assertCase(
        "JQ-12 preloadBatch refuses a startIndex past the end of the list",
        badStart?.success === false && badStart.error === "startIndex out of range",
        { badStart },
        { success: false, error: "startIndex out of range" },
    );

    // urls is a declared MediaRead array parameter, so the bridge validates each
    // element before the handler is entered. A non-string never reaches the
    // handler's own invalidCount tally - it is refused outright, and the message
    // names the offending index.
    const nonString = await invoke("jitQueue.preloadBatch", { urls: [42, INERT_URL] });
    recorder.assertCase(
        "JQ-13 a non-string entry is refused by parameter validation before the handler, and the message names its index",
        nonString?.success === false &&
            nonString.code === "INVALID_PARAMS" &&
            /urls\[0\]/.test(nonString.error ?? "") &&
            /must be a string/i.test(nonString.error),
        { nonString },
        { success: false, code: "INVALID_PARAMS", error: "names urls[0] and the string requirement" },
    );

    // Over-long entries do pass the element type check, so they are the only way
    // to reach the handler's invalidCount. With nothing left, the request
    // degrades into the empty-list refusal and invalidCount is the sole trace.
    const allTooLong = await invoke("jitQueue.preloadBatch", {
        urls: [TOO_LONG_URL, TOO_LONG_URL],
    });
    recorder.assertCase(
        "JQ-14 over-long entries are dropped and counted, and a list of nothing but those degrades to the empty-list refusal",
        allTooLong?.success === false &&
            allTooLong.error === "Empty URL list" &&
            allTooLong.tracksAdded === 0 &&
            allTooLong.invalidCount === 2,
        { allTooLong },
        { success: false, error: "Empty URL list", tracksAdded: 0, invalidCount: 2 },
    );

    // The size refusal is returned by the API layer rather than the queue, so
    // unlike the empty-list refusal above it carries no tracksAdded field.
    const oversized = await invoke("jitQueue.preloadBatch", {
        urls: Array.from({ length: 10001 }, (_, index) => `http://127.0.0.1:9/${index}.mp3`),
    });
    recorder.assertCase(
        "JQ-15 a batch over ten thousand entries is refused without a tracksAdded field, so the main thread is never asked to insert it",
        oversized?.success === false &&
            oversized.error === "Batch exceeds maximum size (10000)" &&
            !("tracksAdded" in oversized),
        { oversized, tracksAddedPresent: "tracksAdded" in (oversized ?? {}) },
        {
            success: false,
            error: "Batch exceeds maximum size (10000)",
            tracksAddedPresent: false,
        },
    );
}

async function runFlushCase(bridge, recorder, queueEmpty) {
    const { invoke } = bridge;

    if (!queueEmpty) {
        recorder.addCase(
            "JQ-16 queue.flush is queue.clear under a second name",
            true,
            { skipped: "the play queue is not empty; flushing it would discard the user's queue" },
            { skipped: "needs an empty play queue" },
        );
        return;
    }

    const flushed = await invoke("queue.flush", {});
    const cleared = await invoke("queue.clear", {});
    const count = await invoke("queue.getCount", {});
    recorder.assertCase(
        "JQ-16 queue.flush is queue.clear under a second name: same envelope, same effect",
        flushed?.success === true &&
            flushed.clearedCount === 0 &&
            JSON.stringify(Object.keys(flushed).sort()) ===
                JSON.stringify(Object.keys(cleared).sort()) &&
            count?.count === 0,
        {
            flushed,
            cleared,
            sameKeys:
                JSON.stringify(Object.keys(flushed ?? {}).sort()) ===
                JSON.stringify(Object.keys(cleared ?? {}).sort()),
            count: count?.count,
        },
        { flushed: { success: true, clearedCount: 0 }, sameKeys: true, count: 0 },
    );
}

function recordUnreachableEvents(recorder, events) {
    const seen = new Set(events.received.map((entry) => entry.name));
    const needPlayback = EVENT_NAMES.filter((name) => name !== "jitQueue:listExhausted");
    recorder.addCase(
        "JQ-17 the four events that describe a playing JIT session are out of reach from a stopped instance",
        true,
        {
            asserted: ["jitQueue:listExhausted"],
            notReachableHere: needPlayback,
            seen: [...seen],
            reason:
                "trackChanged and needNext fire on track transitions, preloadComplete and error only after a resolve attempt; all four need an accepted playNow or preloadBatch, which starts audible playback",
        },
        { recorded: "coverage boundary, not a wish" },
    );
}

const recorder = createRecorder();
let client;
let bridge;
let events;
let blocked = false;
let fatalError;
let targets;
let entry;
let inUse = false;

try {
    const connection = await connectBridgePage();
    client = connection.client;
    targets = connection.candidates;
    console.log(`Target: ${connection.page.title} ${connection.page.url}`);
    bridge = createBridge(client.Runtime, { invokeTimeoutMs });
    await requireResponsiveBridge(bridge, { probeId: runId });

    const [playback, queueCount, jitState] = await Promise.all([
        bridge.invoke("playback.getState", {}),
        bridge.invoke("queue.getCount", {}),
        bridge.invoke("jitQueue.getState", {}),
    ]);
    inUse = jitQueueInUse(jitState);
    entry = {
        playbackState: playback?.state,
        queueCount: queueCount?.count,
        jitState: jitState?.state,
        jitQueueInUse: inUse,
    };
    const playbackStopped = playback?.state === "stopped";
    const queueEmpty = queueCount?.count === 0;

    events = await createEventCollector(bridge, EVENT_NAMES, { collectorId: runId });

    await runStateMachineCases(bridge, recorder, events, inUse);
    await runStopCases(bridge, recorder, playbackStopped);
    await runGuardedRefusalCases(bridge, recorder, inUse);
    await runArgumentCases(bridge, recorder);
    await runFlushCase(bridge, recorder, queueEmpty);
    recordUnreachableEvents(recorder, events);
} catch (error) {
    fatalError = error;
    blocked = Boolean(error?.blocked);
    console.error(`${blocked ? "BLOCKED" : "ERROR"}: ${error?.message || error}`);
} finally {
    // When the queue was idle on entry the machine is left Idle whatever
    // happened: clear is the documented reset and is idempotent, and none of the
    // cases above can have started playback. When the page was mid-session the
    // reset is skipped - clearing would empty the shadow playlist and drop the
    // queued next track out from under a live stream.
    if (bridge && !inUse) {
        await bridge.invokeRaw("jitQueue.clear", {}).catch(() => undefined);
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
            entryState: entry,
            eventsSeen: (events?.received ?? []).map((item) => item.name),
            targets,
        },
    }),
);

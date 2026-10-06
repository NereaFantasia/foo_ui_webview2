/**
 * Covers the events a playback transition sends, in the order a page receives them:
 * playback:starting, playback:stateChanged (with canSeek), playback:trackChanged,
 * playback:paused and playback:stopped. The transitions are starting from stopped,
 * playing another playlist row while playing, playback.next, pause and resume, stop,
 * and the last row playing to its end.
 *
 * Playback is audible and changes what the user was listening to, so the suite only runs
 * when playback is stopped on entry, and records a skip otherwise. It mutes foobar2000,
 * sets the playback order to Default and stop-after-current off so playback.next is
 * predictable, plays three library tracks from a scratch playlist, and afterwards stops,
 * removes the playlist and puts back the mute state, the order, stop-after-current and the
 * active playlist.
 *
 * PT-07 checks the hostTime that playback positions carry: it must come from the clock the
 * page reads with Date.now(), so a page can take Date.now() - hostTime as the delivery delay.
 *
 * Several cases record what foobar2000 does rather than a choice this component made:
 * which stop reason a transition reports, and whether a stopped state comes between two
 * tracks. They are pinned so a change shows up here before it surprises a page.
 *
 * Usage: node mcp/tests/e2e-playback-events.mjs
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
const eventTimeoutMs = envInt("FB2K_EVENT_TIMEOUT_MS", 8000);
const runId = new Date().toISOString().replace(/[-:.TZ]/g, "");
const SCRATCH_NAME = `e2e playback events ${runId}`;
const EVENT_NAMES = [
    "playback:starting",
    "playback:stateChanged",
    "playback:trackChanged",
    "playback:paused",
    "playback:stopped",
];
// Long enough that no track ends while the cases run, except where PT-06 seeks to the end.
const MIN_TRACK_SECONDS = 30;
// How far before the end of the last track PT-06 seeks.
const END_LEAD_SECONDS = 2;
// Where PT-07 seeks to; well inside every track the suite picks.
const SEEK_TARGET_SECONDS = 10;
// Date.now() has whole milliseconds and hostTime a fraction, so a reading taken between two
// Date.now() calls can sit just outside them.
const CLOCK_TOLERANCE_MS = 2;
// Longest delivery delay PT-07 accepts for an event, Date.now() on arrival minus hostTime.
const MAX_DELIVERY_MS = 1000;
const HOST_TIME_EVENTS = ["playback:seeked", "playback:timeHighRes"];

const recorder = createRecorder();

const brief = (events) => events.map((event) => ({ name: event.name, payload: summaryOf(event) }));

function summaryOf(event) {
    const p = event.payload ?? {};
    if (event.name === "playback:trackChanged") return { title: p.title, subsong: p.subsong };
    return p;
}

const states = (events) => events.filter((event) => event.name === "playback:stateChanged");
const indexOf = (events, name, from = 0) => events.findIndex((event, i) => i >= from && event.name === name);

/** Waits until a stateChanged arrives after a trackChanged, and returns the events since `from`. */
async function untilStateAfterTrack(collector, from) {
    const all = await collector.waitFor(
        (received) => {
            const since = received.slice(from);
            const track = indexOf(since, "playback:trackChanged");
            return track >= 0 && indexOf(since, "playback:stateChanged", track + 1) >= 0;
        },
        { timeoutMs: eventTimeoutMs },
    );
    return all.slice(from);
}

async function untilEvent(collector, from, name) {
    const all = await collector.waitFor(
        (received) => received.slice(from).some((event) => event.name === name),
        { timeoutMs: eventTimeoutMs },
    );
    return all.slice(from);
}

/** Calls a method from the page, with Date.now() read just before and just after. */
function timedInvoke(bridge, method, params) {
    return bridge.evaluateValue(`(async () => {
        const before = Date.now();
        const result = await window.fb2k.invoke(${JSON.stringify(method)}, ${JSON.stringify(params ?? {})});
        const after = Date.now();
        return { before, after, result };
    })()`);
}

const readBetween = (hostTime, before, after) =>
    typeof hostTime === "number" &&
    hostTime >= before - CLOCK_TOLERANCE_MS &&
    hostTime <= after + CLOCK_TOLERANCE_MS;

/** An event's hostTime precedes its arrival by no more than MAX_DELIVERY_MS. */
const deliveredInTime = (event) => {
    const delay = event?.at - event?.payload?.hostTime;
    return Number.isFinite(delay) && delay >= -CLOCK_TOLERANCE_MS && delay <= MAX_DELIVERY_MS;
};

const delayOf = (event) => ({ name: event?.name, delayMs: event?.at - event?.payload?.hostTime });

/** The stateChanged sent right after trackChanged: the one that carries the new track's canSeek. */
function stateAfterTrack(events) {
    const track = indexOf(events, "playback:trackChanged");
    const at = track < 0 ? -1 : indexOf(events, "playback:stateChanged", track + 1);
    return at < 0 ? undefined : events[at].payload;
}

/**
 * PT-07, while a track plays: getPosition and setPosition read hostTime between the page's
 * Date.now() before and after the call, and stateChanged, seeked and timeHighRes arrive within
 * MAX_DELIVERY_MS of their hostTime.
 */
async function checkHostTime(bridge, recorder, pausedEvent, resumedEvent) {
    const timed = await createEventCollector(bridge, HOST_TIME_EVENTS, { collectorId: `${runId}_hostTime` });
    try {
        const position = await timedInvoke(bridge, "playback.getPosition");
        const seek = await timedInvoke(bridge, "playback.setPosition", { position: SEEK_TARGET_SECONDS });
        const received = await timed.waitFor(
            (events) =>
                events.some((event) => event.name === "playback:seeked") &&
                events.filter((event) => event.name === "playback:timeHighRes").length >= 3,
            { timeoutMs: eventTimeoutMs },
        );
        const seeked = received.find((event) => event.name === "playback:seeked");
        const ticks = received.filter((event) => event.name === "playback:timeHighRes");
        const tickTimes = ticks.map((event) => event.payload?.hostTime);
        recorder.assertCase(
            "PT-07 positions carry hostTime from the clock the page reads with Date.now(): getPosition and setPosition read it during the call, stateChanged, seeked and timeHighRes arrive within a second of it",
            position?.result?.success === true &&
                readBetween(position.result.hostTime, position.before, position.after) &&
                seek?.result?.success === true &&
                readBetween(seek.result.hostTime, seek.before, seek.after) &&
                [pausedEvent, resumedEvent].every(deliveredInTime) &&
                seeked?.payload?.position === seek.result.actualPosition &&
                seeked.payload.hostTime >= seek.before - CLOCK_TOLERANCE_MS &&
                deliveredInTime(seeked) &&
                ticks.length >= 3 &&
                ticks.every(deliveredInTime) &&
                tickTimes.every((time, i) => i === 0 || time > tickTimes[i - 1]),
            {
                getPosition: { before: position?.before, hostTime: position?.result?.hostTime, after: position?.after },
                setPosition: { before: seek?.before, hostTime: seek?.result?.hostTime, after: seek?.after },
                seekedPosition: seeked?.payload?.position,
                actualPosition: seek?.result?.actualPosition,
                delays: [pausedEvent, resumedEvent, seeked, ...ticks.slice(0, 5)].map(delayOf),
            },
            {
                readBetween: `Date.now() before and after, ±${CLOCK_TOLERANCE_MS} ms`,
                deliveryDelayMs: `0 to ${MAX_DELIVERY_MS}`,
                seekedPosition: "the actualPosition setPosition answered",
                timeHighRes: "at least 3 ticks, hostTime increasing",
            },
        );
    } finally {
        await timed.stop();
    }
}

async function runTransitions(bridge, recorder, collector, scratch, tracks) {
    const { invoke } = bridge;
    const mark = () => collector.received.length;

    let from = mark();
    const started = await invoke("playlist.playTrack", { playlistGuid: scratch.guid, index: 0 });
    let events = await untilStateAfterTrack(collector, from);
    let state = await invoke("playback.getState");
    let settled = stateAfterTrack(events);
    const starting = events.find((event) => event.name === "playback:starting")?.payload;
    recorder.assertCase(
        "PT-01 starting from stopped sends starting, then trackChanged, then a stateChanged playing whose canSeek matches getState",
        started?.success === true &&
            starting?.command === "play" &&
            starting?.paused === false &&
            indexOf(events, "playback:starting") < indexOf(events, "playback:trackChanged") &&
            settled?.state === "playing" &&
            settled?.canSeek === true &&
            settled?.duration > 0 &&
            state?.state === "playing" &&
            state?.canSeek === settled?.canSeek &&
            !events.some((event) => event.name === "playback:stopped"),
        { events: brief(events), getState: state },
        { order: ["starting", "trackChanged", "stateChanged playing"], canSeek: "true, as getState" },
    );

    from = mark();
    await invoke("playlist.playTrack", { playlistGuid: scratch.guid, index: 1 });
    events = await untilStateAfterTrack(collector, from);
    settled = stateAfterTrack(events);
    const stoppedAt = indexOf(events, "playback:stopped");
    const stoppedState = states(events).find((event) => event.payload?.state === "stopped")?.payload;
    recorder.assertCase(
        "PT-02 playing another row while playing is reported by foobar2000 as a user stop, so a stateChanged stopped comes before the new track's events",
        events[stoppedAt]?.payload?.reason === "user" &&
            stoppedState?.canSeek === false &&
            stoppedAt < indexOf(events, "playback:starting") &&
            indexOf(events, "playback:starting") < indexOf(events, "playback:trackChanged") &&
            settled?.state === "playing" &&
            settled?.canSeek === true,
        { events: brief(events) },
        {
            order: ["stopped (user)", "stateChanged stopped", "starting", "trackChanged", "stateChanged playing"],
            characterization: "the stop reason is foobar2000's",
        },
    );

    from = mark();
    const next = await invoke("playback.next");
    events = await untilStateAfterTrack(collector, from);
    settled = stateAfterTrack(events);
    const nextStop = events.find((event) => event.name === "playback:stopped")?.payload;
    recorder.assertCase(
        "PT-03 playback.next stops with reason starting_another, which sends no stateChanged stopped, then plays the next track",
        next?.success === true &&
            (nextStop === undefined || nextStop.reason === "starting_another") &&
            !states(events).some((event) => event.payload?.state === "stopped") &&
            events.find((event) => event.name === "playback:starting")?.payload?.command === "next" &&
            settled?.state === "playing" &&
            settled?.canSeek === true,
        { events: brief(events) },
        { stopReason: "starting_another or none", stateStopped: "never", settled: "playing, canSeek true" },
    );

    from = mark();
    await invoke("playback.pause");
    const paused = await untilEvent(collector, from, "playback:stateChanged");
    from = mark();
    await invoke("playback.play");
    const resumed = await untilEvent(collector, from, "playback:stateChanged");
    const pausedState = states(paused)[0]?.payload;
    const resumedState = states(resumed)[0]?.payload;
    recorder.assertCase(
        "PT-04 pause and resume send stateChanged paused and playing, both with the playing track's canSeek, and playback:paused true then false",
        pausedState?.state === "paused" &&
            pausedState?.canSeek === true &&
            paused.find((event) => event.name === "playback:paused")?.payload?.paused === true &&
            resumedState?.state === "playing" &&
            resumedState?.canSeek === true &&
            resumed.find((event) => event.name === "playback:paused")?.payload?.paused === false,
        { paused: brief(paused), resumed: brief(resumed) },
        {
            paused: { state: "paused", canSeek: true, "playback:paused": true },
            resumed: { state: "playing", canSeek: true, "playback:paused": false },
        },
    );

    await checkHostTime(bridge, recorder, states(paused)[0], states(resumed)[0]);

    from = mark();
    await invoke("playback.stop");
    events = await untilEvent(collector, from, "playback:stateChanged");
    state = await invoke("playback.getState");
    const stopState = states(events)[0]?.payload;
    recorder.assertCase(
        "PT-05 stop reports reason user and a stateChanged stopped whose canSeek is false, as getState",
        events.find((event) => event.name === "playback:stopped")?.payload?.reason === "user" &&
            stopState?.state === "stopped" &&
            stopState?.canSeek === false &&
            state?.state === "stopped" &&
            state?.canSeek === false,
        { events: brief(events), getState: state },
        { reason: "user", state: "stopped", canSeek: false },
    );

    // The last row played to its end: order Default and stop-after-current off, so foobar2000
    // stops at the end of the playlist.
    const last = tracks.length - 1;
    from = mark();
    await invoke("playlist.playTrack", { playlistGuid: scratch.guid, index: last });
    await untilStateAfterTrack(collector, from);
    from = mark();
    const seek = await invoke("playback.setPosition", {
        position: Math.max(0, tracks[last].duration - END_LEAD_SECONDS),
    });
    // on_playback_stop sends playback:stopped before the stateChanged stopped.
    const all = await collector.waitFor(
        (received) => states(received.slice(from)).some((event) => event.payload?.state === "stopped"),
        { timeoutMs: eventTimeoutMs + END_LEAD_SECONDS * 1000 },
    );
    events = all.slice(from);
    state = await invoke("playback.getState");
    const eofState = states(events).find((event) => event.payload?.state === "stopped")?.payload;
    recorder.assertCase(
        "PT-06 the last row playing to its end stops with reason eof and a stateChanged stopped whose canSeek is false",
        seek?.success === true &&
            events.find((event) => event.name === "playback:stopped")?.payload?.reason === "eof" &&
            eofState?.canSeek === false &&
            !events.some((event) => event.name === "playback:trackChanged") &&
            state?.state === "stopped",
        { events: brief(events), getState: state },
        { reason: "eof", state: "stopped", canSeek: false, trackChanged: "none" },
    );
}

let client;
let bridge;
let collector;
let blocked = false;
let fatalError;
let entry;
let scratch;

try {
    const connection = await connectBridgePage();
    client = connection.client;
    console.log(`Target: ${connection.page.title} ${connection.page.url}`);
    bridge = createBridge(client.Runtime, { invokeTimeoutMs });
    await requireResponsiveBridge(bridge, { probeId: runId });
    const { invoke } = bridge;

    entry = {
        state: await invoke("playback.getState"),
        volume: await invoke("playback.getVolume"),
        order: await invoke("playback.getPlaybackOrder"),
        stopAfter: await invoke("playback.getStopAfterCurrent"),
        active: await invoke("playlist.getActive", {}),
    };

    if (entry.state?.state !== "stopped") {
        recorder.addCase(
            "PT-01 to PT-07 playback transitions: skipped, not run",
            true,
            { skipped: `playback was ${entry.state?.state} on entry` },
        );
    } else {
        const page = await invoke("library.query", { query: "ALL", limit: 200 });
        const seen = new Set();
        const tracks = (page?.tracks ?? []).filter((track) => {
            const path = track?.absolutePath ?? "";
            if (!path || /^[a-z]+:\/\//i.test(path) || (track.subsong ?? 0) !== 0) return false;
            if (!(track.duration >= MIN_TRACK_SECONDS) || seen.has(path)) return false;
            seen.add(path);
            return true;
        });
        if (tracks.length < 3) {
            recorder.assertCase(
                "PT-00 the library can supply three local tracks of at least 30 seconds",
                false,
                { available: tracks.length },
                { available: 3 },
            );
        } else {
            await invoke("playback.mute", { muted: true });
            await invoke("playback.setPlaybackOrder", { order: 0 });
            await invoke("playback.setStopAfterCurrent", { enabled: false });
            const picked = tracks.slice(0, 3);
            scratch = await invoke("playlist.create", { name: SCRATCH_NAME });
            await invoke("playlist.addHandles", {
                playlistGuid: scratch.guid,
                handles: picked.map((track) => track.absolutePath),
            });
            collector = await createEventCollector(bridge, EVENT_NAMES, { collectorId: runId });
            await runTransitions(bridge, recorder, collector, scratch, picked);
        }
    }
} catch (error) {
    fatalError = error;
    blocked = Boolean(error?.blocked);
    console.error(`${blocked ? "BLOCKED" : "ERROR"}: ${error?.message || error}`);
} finally {
    if (collector) await collector.stop();
    if (bridge && entry?.state?.state === "stopped") {
        const quiet = (method, params) => bridge.invokeRaw(method, params).catch(() => undefined);
        await quiet("playback.stop", {});
        if (entry.active?.guid) await quiet("playlist.setActive", { playlistGuid: entry.active.guid });
        if (scratch?.guid) await quiet("playlist.remove", { playlistGuid: scratch.guid });
        await quiet("playback.setStopAfterCurrent", { enabled: entry.stopAfter?.enabled === true });
        if (Number.isInteger(entry.order?.order)) {
            await quiet("playback.setPlaybackOrder", { order: entry.order.order });
        }
        await quiet("playback.mute", { muted: entry.volume?.muted === true });
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
            entryState: entry && {
                state: entry.state?.state,
                muted: entry.volume?.muted,
                order: entry.order?.order,
                stopAfterCurrent: entry.stopAfter?.enabled,
            },
            eventsSeen: collector?.received.map((event) => event.name),
        },
    }),
);

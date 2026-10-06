/**
 * Covers the machine-assertable part of playback state, the play queue, and the
 * titleformat evaluator.
 *
 * Transport is deliberately untouched: play, pause, stop, next, previous,
 * playPath, setPosition, queue.playNow and queue.flush all change what is
 * currently playing, which is both audible and, on an instance streaming through
 * a proxy playlist, destructive to state this script cannot restore. What is left
 * is still most of the surface: the read-only state endpoints check against each
 * other, volume and the two playback toggles round-trip, the queue is exercised
 * on a scratch playlist, and titleformat is treated as the near-pure function it
 * is.
 *
 * The queue is shared with whoever is using the instance. Whatever it holds on
 * entry is recorded as the baseline and left in place: counts are read relative
 * to it, the suite's own entries are told apart by identity and are the only
 * ones ever removed, and the cases that act on the whole queue - moving an entry
 * to the head, setContents, clear, and the insertNext block that resets with
 * clear - run only when the queue was empty on entry and otherwise record why
 * they stood down.
 *
 * Volume, playback order and stop-after-current are real user settings. Each is
 * recorded on entry and written back in the finally block.
 *
 * Several cases record behaviour rather than a wish - an out-of-range playback
 * order reporting success, volume zero being indistinguishable from mute, a
 * malformed titleformat pattern yielding an empty string instead of an error.
 * Those are marked; they exist so an intentional change shows up here rather than
 * surprising a page.
 *
 * Usage: node mcp/tests/e2e-playback-queue.mjs
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
const eventTimeoutMs = envInt("FB2K_EVENT_TIMEOUT_MS", 3000);
const runId = new Date().toISOString().replace(/[-:.TZ]/g, "");

const SCRATCH_NAME = `e2e playback scratch ${runId}`;
const TEST_VOLUME = 42;
const EVENT_NAMES = [
    "playback:queueChanged",
    "playback:volumeChanged",
    "playback:orderChanged",
    "playback:stopAfterCurrentChanged",
];

const PLAYBACK_STATES = ["playing", "paused", "stopped"];

function titles(queue) {
    return (queue?.items ?? []).map((item) => item.title);
}

/**
 * The part of a queue entry the coordinate cases compare on. A missing
 * coordinate key is spelled out rather than folded into null, because the
 * contract is that all three keys are always present and null is the value
 * that means "this entry has no playlist coordinates".
 */
function queueShape(queue) {
    return (queue?.items ?? []).map((item) => ({
        queueIndex: item.queueIndex,
        playlist: "playlist" in item ? item.playlist : "<key missing>",
        playlistGuid: "playlistGuid" in item ? item.playlistGuid : "<key missing>",
        playlistItem: "playlistItem" in item ? item.playlistItem : "<key missing>",
        absolutePath: item.absolutePath,
    }));
}

function queueChangedCount(collector) {
    return collector.received.filter((event) => event.name === "playback:queueChanged").length;
}

/**
 * Waits out a quiet period and reports the queue-event total after it.
 * waitFor drains until its predicate holds or the deadline passes, so a
 * predicate that never holds is how a "nothing more arrived" window is
 * spelled.
 */
async function settleQueueEvents(collector, timeoutMs) {
    await collector.waitFor(() => false, { timeoutMs });
    return queueChangedCount(collector);
}

async function awaitQueueEvents(collector, atLeast, timeoutMs) {
    await collector.waitFor(() => queueChangedCount(collector) >= atLeast, { timeoutMs });
    return queueChangedCount(collector);
}

/**
 * A refusal arrives in the response body rather than as a rejection, so the
 * refusal cases read the raw envelope: a rejection is itself a contract
 * change and has to fail its own case instead of aborting the run. The
 * method name is passed by the caller so the coverage ledger attributes the
 * endpoint to the case that exercises it.
 */
async function rawEnvelope(bridge, method, params) {
    const outcome = await bridge.invokeRaw(method, params);
    return outcome?.kind === "result"
        ? outcome.value
        : { success: outcome?.kind, error: outcome?.error };
}

async function runReadCases(bridge, recorder) {
    const [state, volume, position, track, trackIndex, order, playingPlaylist, stopAfter] =
        await Promise.all(
            [
                "playback.getState",
                "playback.getVolume",
                "playback.getPosition",
                "playback.getCurrentTrack",
                "playback.getCurrentTrackIndex",
                "playback.getPlaybackOrder",
                "playback.getPlayingPlaylist",
                "playback.getStopAfterCurrent",
            ].map((method) => bridge.invoke(method, {})),
        );

    recorder.assertCase(
        "PQ-01 getState reports one of the documented states with its capability flags",
        PLAYBACK_STATES.includes(state?.state) &&
            typeof state?.canPause === "boolean" &&
            typeof state?.canSeek === "boolean",
        state,
        { state: PLAYBACK_STATES, canPause: "boolean", canSeek: "boolean" },
    );

    // Both spellings of the mute flag, and the pair of scales volume is reported
    // on. A page reading either spelling must see the same thing.
    recorder.assertCase(
        "PQ-02 getVolume agrees with itself across both spellings and both scales",
        volume?.muted === volume?.isMuted &&
            volume?.volume >= 0 &&
            volume?.volume <= 100 &&
            volume?.volumeDb <= 0 &&
            (volume.volume === 0) === (volume.volumeDb === -100),
        volume,
        { muted: "equals isMuted", volume: "0..100", volumeDb: "<= 0, -100 exactly when volume is 0" },
    );

    recorder.assertCase(
        "PQ-03 getPlaybackOrder reports the same order under all four field names",
        Number.isInteger(order?.order) &&
            order.order === order.orderIndex &&
            typeof order?.orderName === "string" &&
            order.orderName === order.name,
        order,
        { order: "equals orderIndex", orderName: "equals name" },
    );

    // While something is loaded these three describe the same track from three
    // angles. The location is the conditional one: get_playing_item_location
    // answers only when playback started from a playlist item, so a stream the
    // JIT queue put into its shadow playlist plays with no location at all,
    // while get_playing_playlist still names a playlist. Asserting found === true
    // would therefore be asserting how playback happened to be started.
    const isPlaying = state?.state !== "stopped";
    if (isPlaying) {
        const located = trackIndex?.found === true;
        recorder.assertCase(
            "PQ-04 something is loaded, so there is a current track; its location is reported only when playback started from a playlist item, and then it agrees with the playing playlist",
            track?.found === true &&
                typeof track.track?.path === "string" &&
                track.track.path.length > 0 &&
                (located
                    ? playingPlaylist?.found === true &&
                      trackIndex.playlist === playingPlaylist.playlist &&
                      typeof trackIndex.playlistGuid === "string" &&
                      trackIndex.playlistGuid === playingPlaylist.playlistGuid &&
                      Number.isInteger(trackIndex.index)
                    : trackIndex?.index === null &&
                      trackIndex?.playlist === null &&
                      trackIndex?.playlistGuid === null),
            {
                trackIndex,
                playingPlaylist: {
                    playlist: playingPlaylist?.playlist,
                    playlistGuid: playingPlaylist?.playlistGuid,
                    name: playingPlaylist?.name,
                },
                trackPath: track?.track?.path,
                locatedInAPlaylist: located,
            },
            located
                ? { samePlaylist: true, samePlaylistGuid: true, index: "an integer", path: "non-empty" }
                : {
                      path: "non-empty",
                      location: "absent, playlist, playlistGuid and index all null - playback did not start from a playlist item",
                  },
        );

        recorder.assertCase(
            "PQ-05 getPosition stays inside the duration the current track reports",
            position?.position >= 0 &&
                position.position <= position.duration + 1 &&
                Math.abs(position.duration - track.track.duration) < 0.5 &&
                position.path === track.track.path,
            { position, trackDuration: track?.track?.duration, trackPath: track?.track?.path },
            { position: "0..duration", duration: "matches getCurrentTrack", path: "matches getCurrentTrack" },
        );
    } else {
        // getPlayingPlaylist is a passthrough of fb2k's get_playing_playlist,
        // whose pointer outlives playback: once anything has played, a stopped
        // instance still names that playlist. Asserting found === false here
        // would only hold on an instance that has never played, and the queue
        // cases below create the very playlist that breaks it. So the contract
        // is: no current track, and a playing playlist that is either absent or
        // a usable pointer.
        recorder.assertCase(
            "PQ-04 nothing is loaded, so there is no current track, and any playing playlist is a retained pointer",
            track?.found === false &&
                trackIndex?.found === false &&
                (playingPlaylist?.found === false ||
                    (playingPlaylist?.found === true &&
                        Number.isInteger(playingPlaylist.playlist) &&
                        playingPlaylist.playlist >= 0 &&
                        typeof playingPlaylist.name === "string")),
            { state: state?.state, trackFound: track?.found, trackIndex, playingPlaylist },
            {
                trackFound: false,
                trackIndexFound: false,
                playingPlaylist: "absent, or found with an integer index and a name",
            },
        );
        recorder.addCase(
            "PQ-05 getPosition stays inside the duration the current track reports",
            true,
            { skipped: "nothing loaded" },
            { skipped: "nothing loaded" },
        );
    }

    recorder.assertCase(
        "PQ-06 getStopAfterCurrent answers with a boolean flag",
        typeof stopAfter?.enabled === "boolean",
        stopAfter,
        { enabled: "boolean" },
    );

    return { state, volume, order, stopAfter };
}

async function runVolumeCases(bridge, recorder, collector, entry) {
    const { invoke } = bridge;

    await invoke("playback.setVolume", { volume: TEST_VOLUME });
    const set = await invoke("playback.getVolume", {});
    recorder.assertCase(
        "PQ-07 setVolume is reflected by getVolume",
        Math.abs(set?.volume - TEST_VOLUME) < 0.01,
        { requested: TEST_VOLUME, observed: set?.volume },
        { volume: TEST_VOLUME },
    );

    // Out-of-range input is clamped rather than refused, so a slider that
    // overshoots cannot put the host into an invalid state. Omitting the key is
    // refused by the parameter reader, so a caller that forgets it cannot make
    // the host loud.
    await invoke("playback.setVolume", { volume: 500 });
    const high = await invoke("playback.getVolume", {});
    await invoke("playback.setVolume", { volume: -500 });
    const low = await invoke("playback.getVolume", {});
    await invoke("playback.setVolume", { volume: TEST_VOLUME });
    const noKeyResponse = await invoke("playback.setVolume", {});
    const noKey = await invoke("playback.getVolume", {});
    recorder.assertCase(
        "PQ-08 unusable volume input is clamped, and a missing key is refused without touching the level",
        high?.volume === 100 &&
            high?.volumeDb === 0 &&
            low?.volume === 0 &&
            noKeyResponse?.success === false &&
            noKeyResponse?.code === "INVALID_PARAMS" &&
            /^volume is required/.test(noKeyResponse?.error ?? "") &&
            Math.abs(noKey?.volume - TEST_VOLUME) < 0.01,
        {
            above: { volume: high?.volume, volumeDb: high?.volumeDb },
            below: { volume: low?.volume },
            missingKey: { response: noKeyResponse, volume: noKey?.volume },
        },
        {
            above: { volume: 100, volumeDb: 0 },
            below: { volume: 0 },
            missingKey: { code: "INVALID_PARAMS", volume: TEST_VOLUME },
        },
    );

    // Zero volume and mute are one state, not two: silencing by dragging a slider
    // to zero makes the mute flag come back true. A page showing a separate mute
    // button has to expect that.
    recorder.assertCase(
        "PQ-09 volume zero and muted are the same state",
        low?.muted === true && low?.isMuted === true && low?.volumeDb === -100,
        { atZero: low },
        { muted: true, isMuted: true, volumeDb: -100 },
    );

    // The step endpoints move in whole decibels, and they land on a whole
    // decibel: stepping up from -6.02 dB gives -5, not -5.02. So one up and one
    // down do not return to a fractional starting point, which is why this checks
    // the dB grid rather than a round trip. The linear reading moves unevenly for
    // the same reason - the scale is logarithmic.
    await invoke("playback.setVolume", { volume: TEST_VOLUME });
    const before = await invoke("playback.getVolume", {});
    await invoke("playback.volumeUp", {});
    const afterUp = await invoke("playback.getVolume", {});
    await invoke("playback.volumeUp", {});
    const afterUpTwice = await invoke("playback.getVolume", {});
    await invoke("playback.volumeDown", {});
    const afterDown = await invoke("playback.getVolume", {});
    recorder.assertCase(
        "PQ-10 the volume steps move one decibel at a time and snap to whole decibels",
        afterUp.volumeDb > before.volumeDb &&
            afterUp.volumeDb === Math.ceil(before.volumeDb) &&
            afterUpTwice.volumeDb === afterUp.volumeDb + 1 &&
            afterDown.volumeDb === afterUp.volumeDb &&
            afterUp.volume > before.volume,
        {
            beforeDb: before.volumeDb,
            afterUpDb: afterUp.volumeDb,
            afterUpTwiceDb: afterUpTwice.volumeDb,
            afterDownDb: afterDown.volumeDb,
            linearBefore: before.volume,
            linearAfterUp: afterUp.volume,
        },
        {
            afterUpDb: Math.ceil(before.volumeDb),
            afterUpTwiceDb: "one more decibel",
            afterDownDb: "back on the previous whole decibel",
        },
    );

    // Muting has to preserve the level, since restoring it is the whole point.
    // The reading is exact, fractional decibels included, unlike the stepped path
    // above.
    await invoke("playback.setVolume", { volume: TEST_VOLUME });
    const beforeMute = await invoke("playback.getVolume", {});
    const toggledOn = await invoke("playback.toggleMute", {});
    const readOn = await invoke("playback.getVolume", {});
    const toggledOff = await invoke("playback.toggleMute", {});
    const readOff = await invoke("playback.getVolume", {});
    recorder.assertCase(
        "PQ-11 toggleMute reports the new state and restores the exact level when undone",
        toggledOn?.muted === true &&
            readOn?.muted === true &&
            readOn?.volume === 0 &&
            toggledOff?.muted === false &&
            Math.abs(readOff?.volume - beforeMute.volume) < 0.01 &&
            Math.abs(readOff?.volumeDb - beforeMute.volumeDb) < 0.01,
        {
            toggleOn: toggledOn,
            readOn: { muted: readOn?.muted, volume: readOn?.volume },
            toggleOff: toggledOff,
            readOff: { muted: readOff?.muted, volume: readOff?.volume, volumeDb: readOff?.volumeDb },
            beforeMute: { volume: beforeMute.volume, volumeDb: beforeMute.volumeDb },
        },
        {
            toggleOn: { muted: true, volume: 0 },
            afterUndo: { muted: false, volume: beforeMute.volume, volumeDb: beforeMute.volumeDb },
        },
    );

    await invoke("playback.mute", { muted: true });
    const explicitOn = await invoke("playback.getVolume", {});
    await invoke("playback.mute", { muted: false });
    const explicitOff = await invoke("playback.getVolume", {});
    recorder.assertCase(
        "PQ-12 mute takes an explicit state instead of flipping the current one",
        explicitOn?.muted === true && explicitOff?.muted === false,
        { afterMuteTrue: explicitOn?.muted, afterMuteFalse: explicitOff?.muted },
        { afterMuteTrue: true, afterMuteFalse: false },
    );

    const volumeEvents = (
        await collector.waitFor(
            (received) => received.some((event) => event.name === "playback:volumeChanged"),
            { timeoutMs: eventTimeoutMs },
        )
    ).filter((event) => event.name === "playback:volumeChanged");
    const lastVolumeEvent = volumeEvents[volumeEvents.length - 1];
    recorder.assertCase(
        "PQ-13 volume changes are announced with the same payload shape getVolume returns",
        volumeEvents.length > 0 &&
            typeof lastVolumeEvent.payload?.volume === "number" &&
            typeof lastVolumeEvent.payload?.volumeDb === "number" &&
            lastVolumeEvent.payload?.muted === lastVolumeEvent.payload?.isMuted,
        { count: volumeEvents.length, lastPayload: lastVolumeEvent?.payload },
        { payload: { volume: "number", volumeDb: "number", muted: "equals isMuted" } },
    );

    await invoke("playback.setVolume", { volume: entry.volume.volume });
}

async function runOrderCases(bridge, recorder, collector, entry) {
    const { invoke } = bridge;
    const entryOrder = entry.order.order;
    const otherOrder = entryOrder === 1 ? 2 : 1;

    const setByIndex = await invoke("playback.setPlaybackOrder", { order: otherOrder });
    const readByIndex = await invoke("playback.getPlaybackOrder", {});
    recorder.assertCase(
        "PQ-14 setPlaybackOrder by index is reflected and names the order it selected",
        setByIndex?.success === true &&
            setByIndex.order === otherOrder &&
            readByIndex?.order === otherOrder &&
            readByIndex.orderName === setByIndex.orderName &&
            readByIndex.orderName.length > 0,
        { requested: otherOrder, response: setByIndex, read: readByIndex },
        { order: otherOrder, orderName: "the same non-empty name in both" },
    );

    // The name goes under its own "name" key. Responses carry "orderName", and
    // passing that back as a key is refused as unknown - the case below pins it.
    const setByName = await invoke("playback.setPlaybackOrder", {
        name: readByIndex.orderName,
    });
    const readByName = await invoke("playback.getPlaybackOrder", {});
    recorder.assertCase(
        "PQ-15 the name key selects the order by name",
        setByName?.success === true &&
            setByName.order === otherOrder &&
            readByName?.order === otherOrder,
        { name: readByIndex.orderName, response: setByName, read: readByName },
        { order: otherOrder },
    );

    // Four ways to get this wrong, all refused before the host is touched: an
    // index past the range, a name no order has, the name under the wrong key,
    // and neither key at all. The order stays where it was after each.
    const badIndex = await invoke("playback.setPlaybackOrder", { order: 9999 });
    const afterBadIndex = await invoke("playback.getPlaybackOrder", {});
    const badName = await invoke("playback.setPlaybackOrder", { name: "no such order" });
    const afterBadName = await invoke("playback.getPlaybackOrder", {});
    const wrongKey = await invoke("playback.setPlaybackOrder", { orderName: "random" });
    const afterWrongKey = await invoke("playback.getPlaybackOrder", {});
    const noKey = await invoke("playback.setPlaybackOrder", {});
    const afterNoKey = await invoke("playback.getPlaybackOrder", {});
    const refused = (response, pattern) =>
        response?.success === false &&
        response.code === "INVALID_PARAMS" &&
        pattern.test(response.error ?? "");
    recorder.assertCase(
        "PQ-16 an unusable order is refused as INVALID_PARAMS and leaves the order alone",
        refused(badIndex, /order is out of range/) &&
            afterBadIndex?.order === otherOrder &&
            refused(badName, /name has an unsupported value/) &&
            afterBadName?.order === otherOrder &&
            refused(wrongKey, /unknown parameter 'orderName'/) &&
            afterWrongKey?.order === otherOrder &&
            refused(noKey, /order or name is required/) &&
            afterNoKey?.order === otherOrder,
        {
            outOfRangeIndex: { response: badIndex, orderAfter: afterBadIndex?.order },
            unknownName: { response: badName, orderAfter: afterBadName?.order },
            wrongKeyName: { response: wrongKey, orderAfter: afterWrongKey?.order },
            noKey: { response: noKey, orderAfter: afterNoKey?.order },
        },
        {
            outOfRangeIndex: { code: "INVALID_PARAMS", orderAfter: otherOrder },
            unknownName: { code: "INVALID_PARAMS", orderAfter: otherOrder },
            wrongKeyName: { code: "INVALID_PARAMS", orderAfter: otherOrder, note: "orderName is not a parameter key" },
            noKey: { code: "INVALID_PARAMS", orderAfter: otherOrder },
        },
    );

    const orderEvents = (
        await collector.waitFor(
            (received) => received.some((event) => event.name === "playback:orderChanged"),
            { timeoutMs: eventTimeoutMs },
        )
    ).filter((event) => event.name === "playback:orderChanged");
    recorder.assertCase(
        "PQ-17 an order change is announced with the index that is now in effect",
        orderEvents.length > 0 &&
            orderEvents.every(
                (event) =>
                    Number.isInteger(event.payload?.order) &&
                    event.payload.order === event.payload.orderIndex,
            ),
        { count: orderEvents.length, payloads: orderEvents.map((event) => event.payload) },
        { payload: { order: "an integer equal to orderIndex" } },
    );

    await invoke("playback.setPlaybackOrder", { order: entryOrder });
}

async function runStopAfterCurrentCases(bridge, recorder, collector, entry) {
    const { invoke } = bridge;
    const entryEnabled = entry.stopAfter.enabled === true;

    const set = await invoke("playback.setStopAfterCurrent", { enabled: !entryEnabled });
    const read = await invoke("playback.getStopAfterCurrent", {});
    const toggled = await invoke("playback.toggleStopAfterCurrent", {});
    const readBack = await invoke("playback.getStopAfterCurrent", {});
    recorder.assertCase(
        "PQ-18 stop-after-current round-trips through both the setter and the toggle",
        set?.enabled === !entryEnabled &&
            read?.enabled === !entryEnabled &&
            toggled?.enabled === entryEnabled &&
            readBack?.enabled === entryEnabled,
        {
            entryEnabled,
            afterSet: read?.enabled,
            toggleResponse: toggled?.enabled,
            afterToggle: readBack?.enabled,
        },
        { afterSet: !entryEnabled, afterToggle: entryEnabled },
    );

    // Each change of foobar2000's option is announced once, with the value now in effect and
    // no other key. The final restore sets the value it already has, so it adds nothing.
    const changes = (
        await collector.waitFor(
            (received) =>
                received.filter((event) => event.name === "playback:stopAfterCurrentChanged").length >= 2,
            { timeoutMs: eventTimeoutMs },
        )
    ).filter((event) => event.name === "playback:stopAfterCurrentChanged");
    recorder.assertCase(
        "PQ-54 the setter and the toggle are each announced with the value now in effect",
        changes.length === 2 &&
            changes[0].payload?.enabled === !entryEnabled &&
            changes[1].payload?.enabled === entryEnabled &&
            changes.every((event) => Object.keys(event.payload ?? {}).join(",") === "enabled"),
        { count: changes.length, payloads: changes.map((event) => event.payload) },
        { payloads: [{ enabled: !entryEnabled }, { enabled: entryEnabled }] },
    );

    await invoke("playback.setStopAfterCurrent", { enabled: entryEnabled });
}

/**
 * What identifies a queue entry across reads: its playlist coordinates plus
 * the file behind them. Two entries for the same coordinate are legitimately
 * distinct queue positions, so ownership below is a multiset subtraction, not
 * a set lookup.
 */
function signatureOf(item) {
    return `${item?.playlist ?? "null"}|${item?.playlistItem ?? "null"}|${item?.absolutePath ?? ""}`;
}

/**
 * Returns a filter that keeps only what this suite added to the queue: an
 * entry is ours when it was not there on entry (the baseline, consumed one
 * signature at a time so a foreign entry naming the same track stays
 * foreign) and it points at the scratch playlist or at one of the four
 * scratch files - everything this suite queues does, whichever endpoint
 * queued it. Entries the user adds while the suite runs fail the second test
 * and are left alone.
 */
function ownEntriesFilter(baselineItems, scratch) {
    const counts = new Map();
    for (const item of baselineItems ?? []) {
        const key = signatureOf(item);
        counts.set(key, (counts.get(key) ?? 0) + 1);
    }
    const scratchPaths = new Set(scratch?.paths ?? []);
    return (queue) => {
        const left = new Map(counts);
        return (queue?.items ?? []).filter((item) => {
            const key = signatureOf(item);
            const n = left.get(key) ?? 0;
            if (n > 0) {
                left.set(key, n - 1);
                return false;
            }
            return item.playlist === scratch?.playlist || scratchPaths.has(item.absolutePath);
        });
    };
}

/**
 * Takes this suite's own entries out of the queue and nothing else: whatever
 * was queued before the suite ran stays, in its order. Used both for the
 * mid-run resets and for the teardown, so a user's queue survives a run.
 */
async function removeOwnQueueEntries(bridge, baselineItems, scratch) {
    const queue = await bridge.invokeRaw("queue.get", {}).catch(() => undefined);
    const own = ownEntriesFilter(baselineItems, scratch)(queue?.value ?? queue);
    if (own.length === 0) return 0;
    await bridge
        .invokeRaw("queue.remove", { indices: own.map((item) => item.queueIndex) })
        .catch(() => undefined);
    return own.length;
}

async function runQueueCases(bridge, recorder, collector, scratch) {
    const { invoke } = bridge;
    const { playlist, paths } = scratch;

    // Whatever the queue holds on entry is the baseline: the user may be
    // listening with a queue of their own, and this suite must neither count
    // those entries as its own nor drop them. Every count below is read
    // relative to the baseline and every entry is told apart by identity, so
    // the additive cases run either way. The cases that by definition act on
    // the whole queue - reorder to the head, setContents, clear, and the
    // insertNext block that resets with clear - only run when the queue was
    // empty on entry; otherwise they record why they stood down.
    const baseline = await invoke("queue.get", {});
    const baselineItems = baseline?.items ?? [];
    scratch.queueBaseline = baselineItems;
    const foreign = baselineItems.length;
    const own = ownEntriesFilter(baselineItems, scratch);
    const exclusive = foreign === 0;
    const entryCount = await invoke("queue.getCount", {});
    recorder.assertCase(
        exclusive
            ? "PQ-19 the queue is empty on entry, so this suite owns it and every case below runs"
            : "PQ-19 the queue already holds entries on entry, so they are recorded as the baseline, counts are read relative to it, and the whole-queue cases stand down",
        Number.isInteger(baseline?.count) &&
            baseline.count === foreign &&
            entryCount?.count === foreign &&
            entryCount?.hasItems === (foreign > 0),
        { baseline: entryCount, foreignEntries: queueShape(baseline) },
        { count: "matches the entries listed", hasItems: foreign > 0 },
    );

    // The plural key takes playlist item indices. Note it is "tracks", while
    // queue.remove below takes "indices" and moveToTop takes "index" - three
    // different spellings across one namespace.
    const added = await invoke("queue.add", { playlist, tracks: [0, 1] });
    const afterAdd = await invoke("queue.get", {});
    const ownAfterAdd = own(afterAdd);
    recorder.assertCase(
        "PQ-20 queue.add appends the named playlist items in the order given",
        added?.success === true &&
            added.addedCount === 2 &&
            added.queueCount === foreign + 2 &&
            afterAdd?.count === foreign + 2 &&
            ownAfterAdd.length === 2 &&
            ownAfterAdd[0].playlistItem === 0 &&
            ownAfterAdd[1].playlistItem === 1 &&
            ownAfterAdd.every(
                (item, index) => item.queueIndex === foreign + index && item.playlist === playlist,
            ),
        {
            response: added,
            baseline: foreign,
            own: ownAfterAdd.map((i) => ({ queueIndex: i.queueIndex, playlistItem: i.playlistItem })),
        },
        {
            addedCount: 2,
            queueCount: foreign + 2,
            playlistItems: [0, 1],
            queueIndexes: [foreign, foreign + 1],
        },
    );

    const addedSingle = await invoke("queue.add", { playlist, track: 2 });
    const afterSingle = await invoke("queue.get", {});
    const ownAfterSingle = own(afterSingle);
    recorder.assertCase(
        "PQ-21 queue.add also takes one item under the singular key",
        addedSingle?.success === true &&
            addedSingle.addedCount === 1 &&
            afterSingle?.count === foreign + 3 &&
            ownAfterSingle.length === 3 &&
            ownAfterSingle[2].playlistItem === 2,
        { response: addedSingle, lastOwnItem: ownAfterSingle[2]?.playlistItem },
        { addedCount: 1, count: foreign + 3, lastPlaylistItem: 2 },
    );

    let mutations = 2;
    if (exclusive) {
        const queuedTitles = titles(afterSingle);
        const moved = await invoke("queue.moveToTop", { index: 2 });
        const afterMove = await invoke("queue.get", {});
        recorder.assertCase(
            "PQ-22 moveToTop lifts one entry to the front and keeps the rest in order",
            moved?.success === true &&
                moved.movedIndex === 2 &&
                moved.queueCount === 3 &&
                JSON.stringify(titles(afterMove)) ===
                    JSON.stringify([queuedTitles[2], queuedTitles[0], queuedTitles[1]]),
            { response: moved, before: queuedTitles, after: titles(afterMove) },
            { order: [queuedTitles[2], queuedTitles[0], queuedTitles[1]] },
        );

        // An entry already in the queue is moved rather than duplicated, which is why
        // insertedCount is zero and movedCount is one. The path comes from a queued
        // entry itself: playlist.addPaths sorts and de-duplicates on the way in, so
        // paths[i] is not guaranteed to be playlist item i, let alone a queued one.
        const insertExisting = await invoke("queue.insertNext", { paths: [afterMove.items[1].absolutePath] });
        const afterInsert = await invoke("queue.get", {});
        recorder.assertCase(
            "PQ-23 insertNext moves a track already queued instead of adding a second copy",
            insertExisting?.success === true &&
                insertExisting.insertedCount === 0 &&
                insertExisting.movedCount === 1 &&
                afterInsert?.count === 3,
            { response: insertExisting, order: titles(afterInsert) },
            { insertedCount: 0, movedCount: 1, count: 3 },
        );
        mutations += 2;
    } else {
        // Both calls put a scratch track at the head of the queue, which is the
        // user's next track while their entries are there.
        for (const name of [
            "PQ-22 moveToTop lifts one entry to the front and keeps the rest in order",
            "PQ-23 insertNext moves a track already queued instead of adding a second copy",
        ]) {
            recorder.addCase(
                name,
                true,
                {
                    skipped: `the queue holds ${foreign} foreign entries; moving a scratch track to the head would make it the user's next track`,
                },
                { skipped: "needs a queue this suite owns" },
            );
        }
    }

    const addedByPath = await invoke("queue.addPaths", { paths: [paths[3]] });
    const afterPaths = await invoke("queue.get", {});
    const ownAfterPaths = own(afterPaths);
    const lastOwn = ownAfterPaths[ownAfterPaths.length - 1];
    mutations += 1;
    recorder.assertCase(
        "PQ-24 addPaths resolves a filesystem path to a queue entry with playlist coordinates",
        addedByPath?.success === true &&
            addedByPath.addedCount === 1 &&
            addedByPath.invalidCount === 0 &&
            afterPaths?.count === foreign + 4 &&
            ownAfterPaths.length === 4 &&
            lastOwn?.queueIndex === afterPaths.count - 1 &&
            lastOwn?.absolutePath === paths[3] &&
            Number.isInteger(lastOwn?.playlistItem),
        {
            response: addedByPath,
            lastItem: {
                queueIndex: lastOwn?.queueIndex,
                absolutePath: lastOwn?.absolutePath,
                playlist: lastOwn?.playlist,
                playlistItem: lastOwn?.playlistItem,
            },
        },
        { addedCount: 1, invalidCount: 0, count: foreign + 4, lastAbsolutePath: paths[3] },
    );

    // Entries carry the same metadata a playlist row does, so a queue view needs
    // no second lookup to render.
    const first = ownAfterPaths[0];
    recorder.assertCase(
        "PQ-25 a queue entry carries the track metadata a view needs to render it",
        typeof first?.title === "string" &&
            typeof first?.artist === "string" &&
            typeof first?.absolutePath === "string" &&
            first.absolutePath.length > 0 &&
            typeof first?.duration === "number" &&
            first.path !== first.absolutePath,
        {
            keys: Object.keys(first ?? {}).sort(),
            logicalPathDiffersFromNative: first?.path !== first?.absolutePath,
        },
        { has: ["title", "artist", "absolutePath", "duration"], logicalAndNativePathsBothPresent: true },
    );

    if (exclusive) {
        const replaced = await invoke("queue.setContents", {
            items: [
                { playlist, item: 1 },
                { playlist, item: 0 },
            ],
        });
        const afterReplace = await invoke("queue.get", {});
        recorder.assertCase(
            "PQ-26 setContents replaces the whole queue with exactly the list it was given",
            replaced?.success === true &&
                replaced.queueCount === 2 &&
                afterReplace?.count === 2 &&
                afterReplace.items[0].playlistItem === 1 &&
                afterReplace.items[1].playlistItem === 0,
            { response: replaced, playlistItems: afterReplace.items.map((i) => i.playlistItem) },
            { queueCount: 2, playlistItems: [1, 0] },
        );

        // The same queue again with the first row named by the playlist's GUID, so the cases
        // below see the queue PQ-26 left. Both keys on one entry, and a GUID no playlist has, are
        // refused before anything is written.
        const absentGuid = "{00000000-0000-0000-0000-000000000000}";
        const byGuid = await invoke("queue.setContents", {
            items: [
                { playlistGuid: scratch.guid, item: 1 },
                { playlist, item: 0 },
            ],
        });
        const afterGuid = await invoke("queue.get", {});
        const [bothKeys, unknownGuid, insertUnknown] = await Promise.all([
            rawEnvelope(bridge, "queue.setContents", {
                items: [{ playlist, playlistGuid: scratch.guid, item: 0 }],
            }),
            rawEnvelope(bridge, "queue.setContents", { items: [{ playlistGuid: absentGuid, item: 0 }] }),
            rawEnvelope(bridge, "queue.insertNext", { items: [{ playlistGuid: absentGuid, item: 0 }] }),
        ]);
        const afterRefusals = await invoke("queue.get", {});
        const itemsOf = (snapshot) => (snapshot?.items ?? []).map((i) => [i.playlist, i.playlistItem]);
        recorder.assertCase(
            "PQ-26b setContents takes a row by the playlist's GUID, and refuses both keys or an unknown GUID without touching the queue",
            byGuid?.success === true &&
                JSON.stringify(itemsOf(afterGuid)) === JSON.stringify([[playlist, 1], [playlist, 0]]) &&
                bothKeys?.success === false &&
                bothKeys?.code === "INVALID_PARAMS" &&
                /^items\[0\]: /.test(bothKeys?.error ?? "") &&
                unknownGuid?.success === false &&
                unknownGuid?.code === "NOT_FOUND" &&
                insertUnknown?.success === false &&
                insertUnknown?.code === "NOT_FOUND" &&
                JSON.stringify(itemsOf(afterRefusals)) === JSON.stringify(itemsOf(afterGuid)),
            { byGuid, entries: itemsOf(afterGuid), bothKeys, unknownGuid, insertUnknown, after: itemsOf(afterRefusals) },
            {
                entries: [[playlist, 1], [playlist, 0]],
                bothKeys: "INVALID_PARAMS naming items[0]",
                unknownGuid: "NOT_FOUND",
                queueUnchanged: true,
            },
        );

        const removed = await invoke("queue.remove", { indices: [0] });
        const afterRemove = await invoke("queue.get", {});
        recorder.assertCase(
            "PQ-27 remove takes queue indices and leaves the other entry behind",
            removed?.success === true &&
                removed.removedCount === 1 &&
                removed.queueCount === 1 &&
                afterRemove?.count === 1 &&
                afterRemove.items[0].playlistItem === 0,
            { response: removed, remaining: afterRemove.items.map((i) => i.playlistItem) },
            { removedCount: 1, remaining: [0] },
        );

        const cleared = await invoke("queue.clear", {});
        const afterClear = await invoke("queue.getCount", {});
        recorder.assertCase(
            "PQ-28 clear empties the queue and reports how many entries it dropped",
            cleared?.success === true && cleared.clearedCount === 1 && afterClear?.count === 0,
            { response: cleared, afterClear },
            { clearedCount: 1, count: 0 },
        );

        const emptyContents = await invoke("queue.setContents", { items: [] });
        const afterEmpty = await invoke("queue.getCount", {});
        recorder.assertCase(
            "PQ-29 setContents with an empty list is a valid way to empty the queue",
            emptyContents?.success === true &&
                emptyContents.queueCount === 0 &&
                afterEmpty?.count === 0,
            { response: emptyContents, afterEmpty },
            { success: true, queueCount: 0 },
        );

        const badPlaylist = await invoke("queue.add", { playlist: 9999, tracks: [0] });
        const badTrack = await invoke("queue.add", { playlist, tracks: [9999] });
        const removeFromEmpty = await invoke("queue.remove", { indices: [0] });
        const stillEmpty = await invoke("queue.getCount", {});
        recorder.assertCase(
            "PQ-30 unusable queue requests fail without changing the queue",
            badPlaylist?.success === false &&
                typeof badPlaylist?.error === "string" &&
                badTrack?.success === false &&
                badTrack.addedCount === 0 &&
                removeFromEmpty?.success === false &&
                stillEmpty?.count === 0,
            { badPlaylist, badTrack, removeFromEmpty, count: stillEmpty?.count },
            { each: "success false", count: 0 },
        );
        mutations += 4;
    } else {
        // setContents and clear act on the whole queue and would drop the
        // foreign entries; the refusal checks in PQ-30 are read against an
        // empty queue. remove is still exercised, on one of this suite's own
        // entries, found by identity rather than by an assumed index.
        recorder.addCase(
            "PQ-26 setContents replaces the whole queue with exactly the list it was given",
            true,
            { skipped: `the queue holds ${foreign} foreign entries that setContents would drop` },
            { skipped: "needs a queue this suite owns" },
        );

        const beforeRemove = await invoke("queue.get", {});
        const ownBeforeRemove = own(beforeRemove);
        const target = ownBeforeRemove[0];
        const removed = await invoke("queue.remove", { indices: [target?.queueIndex ?? -1] });
        const afterRemove = await invoke("queue.get", {});
        const ownAfterRemove = own(afterRemove);
        mutations += 1;
        recorder.assertCase(
            "PQ-27 remove takes queue indices and leaves the other entries behind, the foreign ones included",
            target !== undefined &&
                removed?.success === true &&
                removed.removedCount === 1 &&
                removed.queueCount === beforeRemove.count - 1 &&
                afterRemove?.count === beforeRemove.count - 1 &&
                ownAfterRemove.length === ownBeforeRemove.length - 1 &&
                !ownAfterRemove.some((item) => signatureOf(item) === signatureOf(target)) &&
                afterRemove.count - ownAfterRemove.length === foreign,
            {
                response: removed,
                removedEntry: signatureOf(target),
                ownBefore: ownBeforeRemove.length,
                ownAfter: ownAfterRemove.length,
                foreignAfter: afterRemove?.count - ownAfterRemove.length,
            },
            { removedCount: 1, ownAfter: ownBeforeRemove.length - 1, foreignAfter: foreign },
        );

        for (const name of [
            "PQ-28 clear empties the queue and reports how many entries it dropped",
            "PQ-29 setContents with an empty list is a valid way to empty the queue",
            "PQ-30 unusable queue requests fail without changing the queue",
        ]) {
            recorder.addCase(
                name,
                true,
                { skipped: `the queue holds ${foreign} foreign entries; these cases empty the queue or read against an empty one` },
                { skipped: "needs a queue this suite owns" },
            );
        }
    }

    // The collector buffers in the page and only accumulates when drained, so the
    // whole run of mutations above has to be pulled across before counting. With
    // the queue owned, the bound stays the loose five it always was: a mutation
    // that changes nothing (an empty setContents on an already empty queue) need
    // not announce. Alongside foreign entries every mutation made was a real
    // change, so the bound is exactly that number.
    const eventFloor = exclusive ? 5 : mutations;
    const queueEvents = (
        await collector.waitFor(
            (received) =>
                received.filter((event) => event.name === "playback:queueChanged").length >= eventFloor,
            { timeoutMs: eventTimeoutMs },
        )
    ).filter((event) => event.name === "playback:queueChanged");
    recorder.assertCase(
        "PQ-31 every queue mutation announced a count and where the change came from",
        queueEvents.length >= eventFloor &&
            queueEvents.every(
                (event) =>
                    Number.isInteger(event.payload?.count) &&
                    typeof event.payload?.origin === "string" &&
                    event.payload.origin.length > 0,
            ),
        {
            count: queueEvents.length,
            mutationsMade: mutations,
            origins: [...new Set(queueEvents.map((event) => event.payload?.origin))],
            counts: queueEvents.map((event) => event.payload?.count),
        },
        { atLeast: eventFloor, payload: { count: "an integer", origin: "a non-empty string" } },
    );

    if (!exclusive) {
        // The insertNext block below resets with queue.clear between cases and
        // asserts absolute counts, so it needs a queue of its own.
        for (const name of [
            "PQ-41 A17 an items entry queues the track at that coordinate and the entry keeps it",
            "PQ-42 A19 a path entry carries both coordinate keys, both null, so one read tells a caller it has none",
            "PQ-43 A20 a mixed call lands its items block first and its paths block second, each in the order given",
            "PQ-44 A21 an out-of-range coordinate fails the whole call, naming the entry, and the paths sent with it stay out of the queue",
            "PQ-45 A22 a coordinate for a track already queued by path moves that entry and gives it the coordinate",
            "PQ-46 A23 a call with nothing to insert is refused by name and announces nothing",
            "PQ-47 A24 three fresh coordinates appended past the end arrive with their coordinates and announce exactly once",
            "PQ-48 A25 one track sent as both a coordinate and a path is queued once, and it is the coordinate that survives",
            "PQ-49 A26 a coordinate that matches a queued entry exactly moves that entry, leaving the other copy of the same track alone",
            "PQ-50 A27 a coordinate with no exact match moves the same track's entry and rewrites it to the coordinate given",
            "PQ-51 A28 two coordinates for one track fold into the first, and a malformed items argument is refused by name",
            "PQ-52 A30 folding two coordinates for one track keeps the one already queued instead of rewriting the entry",
        ]) {
            recorder.addCase(
                name,
                true,
                { skipped: `the queue holds ${foreign} foreign entries; this block resets with queue.clear between cases` },
                { skipped: "needs a queue this suite owns" },
            );
        }
        await removeOwnQueueEntries(bridge, baselineItems, scratch);
        return;
    }

    // insertNext takes two entry forms: paths, which queue a track with no
    // playlist coordinates, and items, which queue playlist coordinates the
    // playback cursor can follow. Everything below is that second form and the
    // rules where the two meet. The numbering picks up at 41 because 32 through
    // 40 are already handed out to the titleformat and self-test blocks; ids
    // that shipped stay where they are.
    //
    // Coordinates are read back from the playlist rather than assumed from the
    // order paths were added in: the incoming-item filter behind
    // playlist.addPaths may sort and de-duplicate what it inserts.
    const listing = await invoke("playlist.getTracks", { playlist, start: 0, count: 200 });
    const itemIndexOf = (path) =>
        (listing?.tracks ?? []).findIndex((row) => row.absolutePath === path);
    const coord = (path) => ({ playlist, item: itemIndexOf(path) });

    await invoke("queue.clear", {});
    const byCoordinate = await invoke("queue.insertNext", { items: [coord(paths[0])] });
    const afterCoordinate = await invoke("queue.get", {});
    recorder.assertCase(
        "PQ-41 A17 an items entry queues the track at that coordinate and the entry keeps it",
        byCoordinate?.success === true &&
            byCoordinate.insertedCount === 1 &&
            afterCoordinate?.count === 1 &&
            afterCoordinate.items[0].playlist === playlist &&
            typeof listing?.playlistGuid === "string" &&
            afterCoordinate.items[0].playlistGuid === listing.playlistGuid &&
            afterCoordinate.items[0].playlistItem === itemIndexOf(paths[0]) &&
            afterCoordinate.items[0].absolutePath === paths[0],
        {
            response: byCoordinate,
            entries: queueShape(afterCoordinate),
            fixture: { playlist, playlistGuid: listing?.playlistGuid, itemIndices: paths.map(itemIndexOf) },
        },
        {
            insertedCount: 1,
            playlist,
            playlistGuid: listing?.playlistGuid,
            playlistItem: itemIndexOf(paths[0]),
        },
    );

    const byPath = await invoke("queue.insertNext", { paths: [paths[1]] });
    const afterPathForm = await invoke("queue.get", {});
    const pathEntry = (afterPathForm?.items ?? []).find(
        (item) => item.absolutePath === paths[1],
    );
    recorder.assertCase(
        "PQ-42 A19 a path entry carries all three coordinate keys, all null, so one read tells a caller it has none",
        byPath?.success === true &&
            pathEntry !== undefined &&
            "playlist" in pathEntry &&
            "playlistGuid" in pathEntry &&
            "playlistItem" in pathEntry &&
            pathEntry.playlist === null &&
            pathEntry.playlistGuid === null &&
            pathEntry.playlistItem === null,
        { response: byPath, entries: queueShape(afterPathForm) },
        { playlist: null, playlistGuid: null, playlistItem: null, keys: "all three present" },
    );

    // Shifting the rows under a queued coordinate: whether foobar2000 moves the
    // entry's coordinate along with the row is its own business, but a reported
    // coordinate must still hold the queued track. The inserted row is taken
    // out again so the coordinates the later cases compute stay valid.
    await invoke("queue.clear", {});
    await invoke("queue.insertNext", { items: [coord(paths[0])] });
    const shift = await invoke("playlist.insertTracks", { playlist, position: 0, handles: [paths[1]] });
    const afterShift = await invoke("queue.get", {});
    const shiftedEntry = afterShift?.items?.[0];
    const reportsRow = Number.isInteger(shiftedEntry?.playlistItem);
    const rowAtCoordinate = reportsRow
        ? await invoke("playlist.getTracksAt", { playlist, rows: [shiftedEntry.playlistItem] })
        : null;
    const shifted = shift?.addedCount === 1 && shift.insertIndex === 0;
    if (shifted) await invoke("playlist.removeTracks", { playlist, items: [0] });
    const restored = await invoke("playlist.getTracks", { playlist, start: 0, count: 200 });
    recorder.assertCase(
        "PQ-42b a queued coordinate whose rows shifted either still names the queued track or is reported as all null, never as another row",
        shifted &&
            afterShift?.count === 1 &&
            shiftedEntry.absolutePath === paths[0] &&
            (reportsRow
                ? shiftedEntry.playlist === playlist &&
                  shiftedEntry.playlistGuid === listing?.playlistGuid &&
                  rowAtCoordinate?.tracks?.[0]?.absolutePath === paths[0]
                : shiftedEntry.playlist === null &&
                  shiftedEntry.playlistGuid === null &&
                  shiftedEntry.playlistItem === null) &&
            JSON.stringify((restored?.tracks ?? []).map((row) => row.absolutePath)) ===
                JSON.stringify((listing?.tracks ?? []).map((row) => row.absolutePath)),
        {
            insert: shift,
            entry: queueShape(afterShift)[0],
            rowAtCoordinate: rowAtCoordinate?.tracks?.[0]?.absolutePath,
            listingRestored: restored?.total === listing?.total,
        },
        {
            entry: "the queued track's row, or playlist, playlistGuid and playlistItem all null",
            listingRestored: true,
        },
    );

    await invoke("queue.clear", {});
    const mixed = await invoke("queue.insertNext", {
        items: [coord(paths[0])],
        paths: [paths[1], paths[2]],
        position: 0,
    });
    const afterMixed = await invoke("queue.get", {});
    recorder.assertCase(
        "PQ-43 A20 a mixed call lands its items block first and its paths block second, each in the order given",
        mixed?.success === true &&
            afterMixed?.count === 3 &&
            JSON.stringify((afterMixed.items ?? []).map((item) => item.absolutePath)) ===
                JSON.stringify([paths[0], paths[1], paths[2]]) &&
            afterMixed.items[0].playlist === playlist &&
            afterMixed.items[1].playlist === null &&
            afterMixed.items[2].playlist === null,
        { response: mixed, entries: queueShape(afterMixed) },
        { order: [paths[0], paths[1], paths[2]], coordinates: [playlist, null, null] },
    );

    const beforeBadCoordinate = await invoke("queue.get", {});
    const badCoordinate = await rawEnvelope(bridge, "queue.insertNext", {
        items: [{ playlist, item: 99999 }],
        paths: [paths[3]],
    });
    const afterBadCoordinate = await invoke("queue.get", {});
    recorder.assertCase(
        "PQ-44 A21 an out-of-range coordinate fails the whole call, naming the entry, and the paths sent with it stay out of the queue",
        badCoordinate?.success === false &&
            typeof badCoordinate?.error === "string" &&
            badCoordinate.error.includes("items[0]") &&
            JSON.stringify(queueShape(afterBadCoordinate)) ===
                JSON.stringify(queueShape(beforeBadCoordinate)) &&
            !(afterBadCoordinate?.items ?? []).some(
                (item) => item.absolutePath === paths[3],
            ),
        {
            response: badCoordinate,
            before: queueShape(beforeBadCoordinate),
            after: queueShape(afterBadCoordinate),
        },
        { success: false, error: "names items[0]", queue: "unchanged entry for entry" },
    );

    await invoke("queue.clear", {});
    await invoke("queue.insertNext", { paths: [paths[3]] });
    const handleOnly = await invoke("queue.get", {});
    const promoted = await invoke("queue.insertNext", {
        items: [coord(paths[3])],
        position: 0,
    });
    const afterPromotion = await invoke("queue.get", {});
    recorder.assertCase(
        "PQ-45 A22 a coordinate for a track already queued by path moves that entry and gives it the coordinate",
        handleOnly?.items?.[0]?.playlist === null &&
            promoted?.success === true &&
            promoted.movedCount === 1 &&
            promoted.insertedCount === 0 &&
            afterPromotion?.count === 1 &&
            afterPromotion.items[0].playlist === playlist &&
            afterPromotion.items[0].playlistItem === itemIndexOf(paths[3]),
        {
            before: queueShape(handleOnly),
            response: promoted,
            after: queueShape(afterPromotion),
        },
        { movedCount: 1, insertedCount: 0, playlistItem: itemIndexOf(paths[3]) },
    );

    const beforeEmptyCalls = await settleQueueEvents(collector, 300);
    const noArguments = await rawEnvelope(bridge, "queue.insertNext", {});
    const emptyArrays = await rawEnvelope(bridge, "queue.insertNext", { paths: [], items: [] });
    const afterEmptyCalls = await settleQueueEvents(collector, 400);
    const afterEmptyCount = await invoke("queue.getCount", {});
    recorder.assertCase(
        "PQ-46 A23 a call with nothing to insert is refused by name and announces nothing",
        noArguments?.success === false &&
            noArguments.error === "No paths or items specified" &&
            emptyArrays?.success === false &&
            emptyArrays.error === "No paths or items specified" &&
            afterEmptyCalls === beforeEmptyCalls &&
            afterEmptyCount?.count === 1,
        {
            noArguments,
            emptyArrays,
            queueChanged: { before: beforeEmptyCalls, after: afterEmptyCalls },
            count: afterEmptyCount?.count,
        },
        {
            error: "No paths or items specified",
            queueChanged: "no new event",
            count: 1,
        },
    );

    await invoke("queue.clear", {});
    const beforeAppend = await settleQueueEvents(collector, 300);
    const appended = await invoke("queue.insertNext", {
        items: [coord(paths[0]), coord(paths[1]), coord(paths[2])],
        position: 999,
    });
    await awaitQueueEvents(collector, beforeAppend + 1, eventTimeoutMs);
    const afterAppendEvents = await settleQueueEvents(collector, 400);
    const afterAppend = await invoke("queue.get", {});
    recorder.assertCase(
        "PQ-47 A24 three fresh coordinates appended past the end arrive with their coordinates and announce exactly once",
        appended?.success === true &&
            appended.insertedCount === 3 &&
            afterAppend?.count === 3 &&
            (afterAppend.items ?? []).every(
                (item, index) =>
                    item.playlist === playlist &&
                    item.playlistItem === itemIndexOf(paths[index]),
            ) &&
            afterAppendEvents === beforeAppend + 1,
        {
            response: appended,
            entries: queueShape(afterAppend),
            queueChanged: { before: beforeAppend, after: afterAppendEvents },
        },
        { insertedCount: 3, coordinates: "all three present", queueChanged: "exactly one" },
    );

    await invoke("queue.clear", {});
    const foldedAcrossForms = await invoke("queue.insertNext", {
        items: [coord(paths[0])],
        paths: [paths[0]],
    });
    const afterFoldAcrossForms = await invoke("queue.get", {});
    recorder.assertCase(
        "PQ-48 A25 one track sent as both a coordinate and a path is queued once, and it is the coordinate that survives",
        foldedAcrossForms?.success === true &&
            afterFoldAcrossForms?.count === 1 &&
            afterFoldAcrossForms.items[0].playlist === playlist &&
            afterFoldAcrossForms.items[0].playlistItem === itemIndexOf(paths[0]),
        { response: foldedAcrossForms, entries: queueShape(afterFoldAcrossForms) },
        { count: 1, playlistItem: itemIndexOf(paths[0]) },
    );

    // The three cases below need one track sitting at two positions in the same
    // playlist. addPathsSequential is the append that survives it: the filter
    // behind addPaths would sort and de-duplicate the row away. Where the row
    // landed is read back from the playlist rather than taken on trust.
    await invoke("playlist.addPathsSequential", { playlist, paths: [paths[0]] });
    const grownListing = await invoke("playlist.getTracks", { playlist, start: 0, count: 200 });
    const grownRows = grownListing?.tracks ?? [];
    const firstItem = itemIndexOf(paths[0]);
    let secondItem = -1;
    for (let index = grownRows.length - 1; index >= 0; index -= 1) {
        if (grownRows[index].absolutePath === paths[0]) {
            secondItem = index;
            break;
        }
    }
    const twoCopies = firstItem >= 0 && secondItem > firstItem;

    await invoke("queue.setContents", {
        items: [
            { playlist, item: firstItem },
            coord(paths[1]),
            { playlist, item: secondItem },
        ],
    });
    const beforeExactMatch = await invoke("queue.get", {});
    const exactMatch = await invoke("queue.insertNext", {
        items: [{ playlist, item: secondItem }],
        position: 0,
    });
    const afterExactMatch = await invoke("queue.get", {});
    recorder.assertCase(
        "PQ-49 A26 a coordinate that matches a queued entry exactly moves that entry, leaving the other copy of the same track alone",
        twoCopies &&
            beforeExactMatch?.count === 3 &&
            exactMatch?.success === true &&
            exactMatch.movedCount === 1 &&
            exactMatch.insertedCount === 0 &&
            afterExactMatch?.count === 3 &&
            afterExactMatch.items[0].playlistItem === secondItem &&
            afterExactMatch.items[1].playlistItem === firstItem &&
            afterExactMatch.items[2].playlistItem === itemIndexOf(paths[1]),
        {
            fixture: { firstItem, secondItem, rows: grownRows.length },
            before: queueShape(beforeExactMatch),
            response: exactMatch,
            after: queueShape(afterExactMatch),
        },
        {
            movedCount: 1,
            insertedCount: 0,
            playlistItems: [secondItem, firstItem, itemIndexOf(paths[1])],
        },
    );

    await invoke("queue.setContents", {
        items: [{ playlist, item: firstItem }, coord(paths[1])],
    });
    const rewritten = await invoke("queue.insertNext", {
        items: [{ playlist, item: secondItem }],
        position: 0,
    });
    const afterRewrite = await invoke("queue.get", {});
    recorder.assertCase(
        "PQ-50 A27 a coordinate with no exact match moves the same track's entry and rewrites it to the coordinate given",
        twoCopies &&
            rewritten?.success === true &&
            rewritten.movedCount === 1 &&
            rewritten.insertedCount === 0 &&
            afterRewrite?.count === 2 &&
            afterRewrite.items[0].playlistItem === secondItem &&
            afterRewrite.items[1].playlistItem === itemIndexOf(paths[1]),
        {
            fixture: { firstItem, secondItem },
            response: rewritten,
            after: queueShape(afterRewrite),
        },
        { movedCount: 1, count: 2, playlistItems: [secondItem, itemIndexOf(paths[1])] },
    );

    await invoke("queue.clear", {});
    const foldedCoordinates = await invoke("queue.insertNext", {
        items: [
            { playlist, item: firstItem },
            { playlist, item: secondItem },
        ],
    });
    const afterCoordinateFold = await invoke("queue.get", {});
    const notAnArray = await rawEnvelope(bridge, "queue.insertNext", { items: "x" });
    const missingItemKey = await rawEnvelope(bridge, "queue.insertNext", { items: [{ playlist }] });
    recorder.assertCase(
        "PQ-51 A28 two coordinates for one track fold into the first, and a malformed items argument is refused by name",
        twoCopies &&
            foldedCoordinates?.success === true &&
            foldedCoordinates.insertedCount === 1 &&
            afterCoordinateFold?.count === 1 &&
            afterCoordinateFold.items[0].playlistItem === firstItem &&
            notAnArray?.success === false &&
            notAnArray.error === "items must be an array" &&
            missingItemKey?.success === false &&
            typeof missingItemKey?.error === "string" &&
            missingItemKey.error.includes("items[0]"),
        {
            fixture: { firstItem, secondItem },
            response: foldedCoordinates,
            entries: queueShape(afterCoordinateFold),
            notAnArray,
            missingItemKey,
        },
        {
            insertedCount: 1,
            playlistItem: firstItem,
            notAnArray: "items must be an array",
            missingItemKey: "names items[0]",
        },
    );

    await invoke("queue.setContents", {
        items: [{ playlist, item: secondItem }, coord(paths[1])],
    });
    const foldKeepsQueued = await invoke("queue.insertNext", {
        items: [
            { playlist, item: firstItem },
            { playlist, item: secondItem },
        ],
        position: 0,
    });
    const afterFoldKeepsQueued = await invoke("queue.get", {});
    recorder.assertCase(
        "PQ-52 A30 folding two coordinates for one track keeps the one already queued instead of rewriting the entry",
        twoCopies &&
            foldKeepsQueued?.success === true &&
            foldKeepsQueued.movedCount === 1 &&
            foldKeepsQueued.insertedCount === 0 &&
            afterFoldKeepsQueued?.count === 2 &&
            afterFoldKeepsQueued.items[0].playlistItem === secondItem &&
            afterFoldKeepsQueued.items[1].playlistItem === itemIndexOf(paths[1]),
        {
            fixture: { firstItem, secondItem },
            response: foldKeepsQueued,
            after: queueShape(afterFoldKeepsQueued),
        },
        { movedCount: 1, insertedCount: 0, playlistItems: [secondItem, itemIndexOf(paths[1])] },
    );

    await removeOwnQueueEntries(bridge, baselineItems, scratch);
}

async function runTitleformatCases(bridge, recorder, scratch) {
    const { invoke } = bridge;
    const path = scratch.paths[0];
    const track = scratch.tracks[0];

    const builtins = await invoke("titleformat.getBuiltinFields", {});
    recorder.assertCase(
        "PQ-32 getBuiltinFields maps names to patterns that look like patterns",
        builtins?.fields &&
            Object.keys(builtins.fields).length > 10 &&
            builtins.fields.artist === "%artist%" &&
            Object.values(builtins.fields).every(
                (pattern) => typeof pattern === "string" && pattern.includes("%"),
            ),
        { count: Object.keys(builtins?.fields ?? {}).length, artist: builtins?.fields?.artist },
        { count: "> 10", artist: "%artist%", everyValue: "contains a % field reference" },
    );

    const evaluated = await invoke("titleformat.eval", {
        path,
        pattern: "%artist% / %title%",
    });
    recorder.assertCase(
        "PQ-33 eval formats a library track against its own tags",
        evaluated?.success === true &&
            evaluated.infoAvailable === true &&
            evaluated.result === `${track.artist} / ${track.title}` &&
            evaluated.path === path,
        { observed: evaluated.result, fromLibrary: `${track.artist} / ${track.title}` },
        { result: `${track.artist} / ${track.title}`, infoAvailable: true },
    );

    // The evaluator is close to a pure function once track info is out of the
    // picture, so these are golden cases: no fixture, no library, one right
    // answer each. $if is in the list on purpose - its condition tests whether a
    // field resolved, so a literal is false, which reads as a bug until you know.
    const golden = {
        "$add(2,3)": "5",
        "$sub(10,4)": "6",
        "$mul(3,4)": "12",
        "$div(9,2)": "4",
        "$max(1,7)": "7",
        "$min(1,7)": "1",
        "$upper(abc)": "ABC",
        "$lower(ABC)": "abc",
        "$len(hello)": "5",
        "$left(abcdef,3)": "abc",
        "$num(7,3)": "007",
        "$replace(a-b-c,-,+)": "a+b+c",
        "$strchr(abcdef,c)": "3",
        "$repeat(ab,3)": "ababab",
        "$abbr(Hello World Again)": "HWA",
        "$trim( padded )": "padded",
        "$puts(v,42)$get(v)": "42",
        "$if(1,yes,no)": "no",
    };
    const goldenResults = await Promise.all(
        Object.keys(golden).map((pattern) =>
            bridge.invoke("titleformat.eval", { path, pattern }),
        ),
    );
    const goldenMismatches = Object.keys(golden)
        .map((pattern, index) => ({
            pattern,
            expected: golden[pattern],
            observed: goldenResults[index]?.result,
        }))
        .filter((entry) => entry.observed !== entry.expected);
    recorder.assertCase(
        "PQ-34 the evaluator's own functions produce their documented results",
        goldenMismatches.length === 0,
        { checked: Object.keys(golden).length, mismatches: goldenMismatches },
        { mismatches: [] },
    );

    const fields = await invoke("titleformat.evalFields", {
        path,
        fields: { who: "%artist%", what: "%title%", math: "$add(1,2)" },
    });
    recorder.assertCase(
        "PQ-35 evalFields returns one result per requested name, keyed by that name",
        fields?.success === true &&
            fields.who === track.artist &&
            fields.what === track.title &&
            fields.math === "3",
        { who: fields?.who, what: fields?.what, math: fields?.math },
        { who: track.artist, what: track.title, math: "3" },
    );

    const batch = await invoke("titleformat.evalBatch", {
        paths: scratch.paths.slice(0, 2),
        pattern: "%title%",
    });
    const fieldsBatch = await invoke("titleformat.evalFieldsBatch", {
        paths: scratch.paths.slice(0, 2),
        fields: { what: "%title%" },
    });
    recorder.assertCase(
        "PQ-36 both batch forms answer per path, with totals that add up",
        batch?.total === 2 &&
            batch.successCount === 2 &&
            batch.errorCount === 0 &&
            batch.results.map((row) => row.result).join("|") ===
                scratch.tracks.slice(0, 2).map((t) => t.title).join("|") &&
            fieldsBatch?.total === 2 &&
            fieldsBatch.results.map((row) => row.what).join("|") ===
                scratch.tracks.slice(0, 2).map((t) => t.title).join("|"),
        {
            batch: { total: batch?.total, results: batch?.results?.map((r) => r.result) },
            fieldsBatch: { total: fieldsBatch?.total, results: fieldsBatch?.results?.map((r) => r.what) },
        },
        { titles: scratch.tracks.slice(0, 2).map((t) => t.title) },
    );

    // A path with no track info still evaluates, so infoAvailable is the only
    // thing separating a real value from the evaluator's placeholder.
    const missing = await invoke("titleformat.eval", {
        path: "E:\\no-such-directory\\no-such-file.flac",
        pattern: "%artist%",
    });
    recorder.assertCase(
        "PQ-37 an unknown path evaluates to a placeholder and says info was unavailable",
        missing?.success === true && missing.infoAvailable === false && missing.result === "?",
        missing,
        { success: true, infoAvailable: false, result: "?" },
    );

    // Recorded, not endorsed: a malformed pattern is not a reported error.
    const malformed = await invoke("titleformat.eval", { path, pattern: "$add(" });
    recorder.assertCase(
        "PQ-38 a malformed pattern yields an empty string rather than an error",
        malformed?.success === true && malformed.result === "",
        malformed,
        { success: true, result: "" },
    );

    const missingArgs = await Promise.all([
        bridge.invoke("titleformat.eval", { path: "", pattern: "%artist%" }),
        bridge.invoke("titleformat.eval", { path, pattern: "" }),
        bridge.invoke("titleformat.evalFields", { path, fields: ["artist"] }),
    ]);
    recorder.assertCase(
        "PQ-39 an empty path, an empty pattern and a fields array are each refused",
        missingArgs.every(
            (response) => response?.success === false && typeof response?.error === "string",
        ),
        missingArgs.map((response) => response?.error),
        { each: "success false with an error message" },
    );

    // Without a path the playing track is evaluated through the playback
    // formatter. Transport is left alone here, so the expectation follows what
    // the instance is doing: a loaded track answers with its own path, a stopped
    // instance answers NO_ACTIVE_ITEM.
    const [nowState, nowTrack, nowEval] = await Promise.all([
        bridge.invoke("playback.getState", {}),
        bridge.invoke("playback.getCurrentTrack", {}),
        bridge.invoke("titleformat.eval", { pattern: "%isplaying%" }),
    ]);
    const loaded = nowState?.state !== "stopped";
    recorder.assertCase(
        "PQ-53 eval without a path evaluates the playing track, and answers NO_ACTIVE_ITEM when stopped",
        loaded
            ? nowEval?.success === true &&
                  nowEval.path === nowTrack?.path &&
                  nowEval.result === "1" &&
                  nowEval.infoAvailable === true
            : nowEval?.success === false && nowEval?.code === "NO_ACTIVE_ITEM",
        { state: nowState?.state, eval: nowEval, trackPath: nowTrack?.path },
        loaded
            ? { success: true, path: "the current track's path", result: "1", infoAvailable: true }
            : { success: false, code: "NO_ACTIVE_ITEM" },
    );
}

// Writes into a locked playlist are refused before anything changes. An
// autoplaylist carries foobar2000's own lock, the one lock a suite can place
// without a third-party component; its query matches nothing, so the list starts
// empty and a track in it afterwards came from the call under test. playPath and
// playPaths write into the active playlist, so the autoplaylist is made active
// for them; the finally block restores the user's active playlist.
async function runLockedPlaylistCases(bridge, recorder, scratch) {
    const { invoke } = bridge;
    const names = [
        "PQ-55 queue.addPaths into a locked playlist is refused with LOCKED, and nothing is added or queued",
        "PQ-56 playback.playPaths on a locked active playlist is refused with LOCKED, and nothing is cleared, added or played",
        "PQ-57 playback.playPath on a locked active playlist is refused with LOCKED, and nothing is added or played",
    ];
    const auto = await invoke("playlist.createAutoplaylist", {
        name: `${SCRATCH_NAME} locked`,
        query: `title IS e2e-no-match-${runId}`,
    });
    lockedIndex = auto?.index;
    const lock = Number.isInteger(lockedIndex)
        ? await invoke("playlist.isLocked", { playlist: lockedIndex })
        : undefined;
    if (lock?.isLocked !== true) {
        for (const name of names) {
            recorder.assertCase(
                name,
                false,
                { autoplaylist: auto, lock, reason: "no locked playlist to write into" },
                { isLocked: true },
            );
        }
        return;
    }

    const isLockedFailure = (response) =>
        response?.success === false &&
        response?.code === "LOCKED" &&
        response?.details?.playlist === lockedIndex &&
        response?.details?.isLocked === true;
    const trackCount = async () =>
        (await invoke("playlist.getTrackCount", { playlist: lockedIndex }))?.count;
    // A track that ends between the two reads changes the handle; the calls here
    // take well under a second, so that is left as a known source of a false failure.
    const transport = async () => {
        const [state, current] = await Promise.all([
            invoke("playback.getState", {}),
            invoke("playback.getCurrentTrack", {}),
        ]);
        return { state: state?.state, handle: current?.track?.handle ?? null };
    };
    const sameTransport = (a, b) => a.state === b.state && a.handle === b.handle;
    const refusal = { success: false, code: "LOCKED", details: { playlist: lockedIndex, isLocked: true } };
    const countBefore = await trackCount();

    const queueBefore = (await invoke("queue.getCount", {}))?.count;
    const queued = await invoke("queue.addPaths", {
        paths: [scratch.paths[0]],
        useQueuePlaylist: false,
        playlist: lockedIndex,
    });
    const queueAfter = (await invoke("queue.getCount", {}))?.count;
    const countAfterQueue = await trackCount();
    recorder.assertCase(
        names[0],
        isLockedFailure(queued) &&
            Number.isInteger(queueBefore) &&
            queueAfter === queueBefore &&
            Number.isInteger(countBefore) &&
            countAfterQueue === countBefore,
        {
            response: queued,
            queueCount: { before: queueBefore, after: queueAfter },
            trackCount: { before: countBefore, after: countAfterQueue },
        },
        { ...refusal, queueCount: "unchanged", trackCount: "unchanged" },
    );

    const active = await invoke("playlist.getActive", {});
    activeBefore = active?.found === true ? active.index : undefined;
    await invoke("playlist.setActive", { playlist: lockedIndex });

    const beforeMany = await transport();
    const many = await invoke("playback.playPaths", { paths: scratch.paths, replace: true });
    const afterMany = await transport();
    const countAfterMany = await trackCount();
    recorder.assertCase(
        names[1],
        isLockedFailure(many) && countAfterMany === countBefore && sameTransport(beforeMany, afterMany),
        {
            response: many,
            trackCount: { before: countBefore, after: countAfterMany },
            transport: { before: beforeMany, after: afterMany },
        },
        { ...refusal, trackCount: "unchanged", transport: "unchanged" },
    );

    const beforeOne = await transport();
    const one = await invoke("playback.playPath", { path: scratch.paths[0] });
    const afterOne = await transport();
    const countAfterOne = await trackCount();
    recorder.assertCase(
        names[2],
        isLockedFailure(one) && countAfterOne === countBefore && sameTransport(beforeOne, afterOne),
        {
            response: one,
            trackCount: { before: countBefore, after: countAfterOne },
            transport: { before: beforeOne, after: afterOne },
        },
        { ...refusal, trackCount: "unchanged", transport: "unchanged" },
    );
}

async function runSelfTestCases(bridge, recorder) {
    const [ping, echo] = await Promise.all([
        bridge.invoke("test.ping", {}),
        bridge.invoke("test.echo", { hello: "world", n: 42 }),
    ]);
    recorder.assertCase(
        "PQ-40 the bridge self-test endpoints answer and echo what they were sent",
        ping?.pong === true &&
            Number.isInteger(ping?.timestamp) &&
            JSON.stringify(echo?.hello ?? echo?.params?.hello ?? echo) !== "{}",
        { ping, echo },
        { pong: true, timestamp: "an integer", echo: "carries the sent params" },
    );
}

const recorder = createRecorder();
let client;
let bridge;
let collector;
let blocked = false;
let fatalError;
let targets;
let entry;
let scratchIndex;
let scratch;
let lockedIndex;
let activeBefore;

try {
    const connection = await connectBridgePage();
    client = connection.client;
    targets = connection.candidates;
    console.log(`Target: ${connection.page.title} ${connection.page.url}`);
    bridge = createBridge(client.Runtime, { invokeTimeoutMs });
    await requireResponsiveBridge(bridge, { probeId: runId });

    entry = await runReadCases(bridge, recorder);
    collector = await createEventCollector(bridge, EVENT_NAMES, { collectorId: runId });

    await runVolumeCases(bridge, recorder, collector, entry);
    await runOrderCases(bridge, recorder, collector, entry);
    await runStopAfterCurrentCases(bridge, recorder, collector, entry);

    // The queue holds playlist coordinates, so its cases need a playlist of their
    // own: queueing items out of a playlist the user might edit would leave the
    // queue pointing at moving targets.
    // Cue-sheet tracks share one file, and a bare path to that file expands to
    // every track it contains - which breaks any assertion that counts queue
    // entries. Plain files report subsong 0; cue tracks report their 1-based
    // index, so keeping subsong 0 and one track per file leaves only whole
    // files. The library's iteration order is not stable across restarts, so
    // the window is wider than the four tracks needed.
    const page = await bridge.invoke("library.query", { query: "ALL", limit: 64 });
    const seenPaths = new Set();
    const tracks = [];
    for (const t of page?.tracks ?? []) {
        if (!t.absolutePath || !t.title || (t.subsong ?? 0) !== 0 || seenPaths.has(t.absolutePath)) {
            continue;
        }
        seenPaths.add(t.absolutePath);
        tracks.push(t);
        if (tracks.length === 4) break;
    }
    if (tracks.length < 4) {
        recorder.assertCase(
            "PQ-19 the library can supply four tracks for the queue cases",
            false,
            { available: tracks.length },
            { available: 4 },
        );
    } else {
        const created = await bridge.invoke("playlist.create", { name: SCRATCH_NAME });
        scratchIndex = created?.index;
        const paths = tracks.map((t) => t.absolutePath);
        await bridge.invoke("playlist.addPaths", { playlist: scratchIndex, paths });
        scratch = { playlist: scratchIndex, guid: created?.guid, paths, tracks };
        await runQueueCases(bridge, recorder, collector, scratch);
        await runTitleformatCases(bridge, recorder, scratch);
        await runLockedPlaylistCases(bridge, recorder, scratch);
    }

    await runSelfTestCases(bridge, recorder);
} catch (error) {
    fatalError = error;
    blocked = Boolean(error?.blocked);
    console.error(`${blocked ? "BLOCKED" : "ERROR"}: ${error?.message || error}`);
} finally {
    if (bridge) {
        const quiet = (method, params) => bridge.invokeRaw(method, params).catch(() => undefined);
        // Only this suite's own entries leave the queue. Before the queue cases
        // recorded a baseline nothing was queued by this run, so there is
        // nothing to take out either.
        if (scratch?.queueBaseline) {
            await removeOwnQueueEntries(bridge, scratch.queueBaseline, scratch);
        }
        if (entry) {
            await quiet("playback.setVolume", { volume: entry.volume.volume });
            await quiet("playback.mute", { muted: entry.volume.muted === true });
            await quiet("playback.setPlaybackOrder", { order: entry.order.order });
            await quiet("playback.setStopAfterCurrent", { enabled: entry.stopAfter.enabled === true });
        }
        if (Number.isInteger(activeBefore)) {
            await quiet("playlist.setActive", { playlist: activeBefore });
        }
        // The autoplaylist was created after the scratch playlist, so it goes first:
        // removing the scratch playlist would shift its index. Its lock is taken off
        // before the playlist itself is removed.
        if (Number.isInteger(lockedIndex)) {
            await quiet("playlist.removeAutoplaylist", { playlist: lockedIndex });
            await quiet("playlist.remove", { playlist: lockedIndex });
        }
        if (Number.isInteger(scratchIndex)) {
            await quiet("playlist.remove", { playlist: scratchIndex });
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
            entryState: entry
                ? {
                      state: entry.state?.state,
                      volume: entry.volume?.volume,
                      muted: entry.volume?.muted,
                      order: entry.order?.order,
                      stopAfterCurrent: entry.stopAfter?.enabled,
                  }
                : undefined,
            eventsSeen: collector?.received.map((event) => event.name),
            targets,
        },
    }),
);

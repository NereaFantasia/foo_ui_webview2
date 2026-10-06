/**
 * Covers the global selection (selection.*), the drag-out token
 * (dnd.prepareDrag), the panel config pair, and the three discovery endpoints
 * that execute a menu command.
 *
 * The thread joining them is "what is the current target": a selection viewer,
 * a drag source and a context menu all have to answer that question, and this
 * suite pins the fact that they do not all answer it from the same place.
 * selection.set writes ui_selection_manager, while the context-menu endpoints
 * read the now-playing track or activeplaylist_get_selected_items - so a page
 * that sets the global selection and then executes a context command finds it
 * has no target (SC-17).
 *
 * No command is ever actually executed. An arbitrary foobar2000 command can
 * open a native modal, which would stall every later invoke, so the three
 * discovery endpoints are driven only through the checks that precede dispatch:
 * a malformed or unknown GUID, and the missing-target refusal. Executing a real
 * command belongs in the manual checklist.
 *
 * Selection state is restored by re-pointing the holder rather than by writing
 * a value back, because selection.set refuses an empty array and therefore
 * cannot express "nothing selected" (SC-08). setPlaylistTracking with mode
 * selection returns the reading to what it was on entry. There is no getter for
 * the tracking mode itself, so that restoration is verified by its effect.
 *
 * Three cases depend on nothing being loaded, and measure that right where they
 * read rather than at entry: with a track loaded, the now-playing holder answers
 * selection.get and getType whatever set wrote or whichever tracking mode is
 * on, so SC-07, SC-09 and SC-11 assert the shadowing instead of the
 * write-through in that state. SC-09 also depends on what the front end keeps selected in the
 * active playlist, and reads that from playlist.getSelection at the same
 * moment instead of assuming an empty selection. The instance may be in use
 * while this runs - a front end streaming through the JIT queue leaves a track
 * loaded, another may keep a focused row selected - and nothing here starts or
 * stops playback or changes the selection.
 *
 * Usage: node mcp/tests/e2e-selection-context.mjs
 * Environment: FB2K_CDP_PORT, FB2K_E2E_TIMEOUT_MS.
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

const invokeTimeoutMs = envInt("FB2K_E2E_TIMEOUT_MS", 20000);
const runId = new Date().toISOString().replace(/[-:.TZ]/g, "");

/** Well-formed, and deliberately not the GUID of any registered command. */
const UNKNOWN_GUID = "{DEADBEEF-0000-4000-8000-000000000000}";
const ABSENT_PATH = `Z:\\no-such-directory-${runId}\\absent.flac`;
const VIEWER_MODES = ["prefer_playing", "prefer_selection"];

async function runSelectionReadCases(bridge, recorder) {
    const { invoke } = bridge;

    const [viewerMode, type, selection, viewing, playlists] = await Promise.all([
        invoke("selection.getViewerMode", {}),
        invoke("selection.getType", {}),
        invoke("selection.get", {}),
        invoke("selection.getViewingTrack", {}),
        invoke("playlist.getAll", {}),
    ]);

    // Since their declaration in src/api/schema/selection.ts the three reads
    // answer inside the success envelope like every other endpoint.
    recorder.assertCase(
        "SC-01 getViewerMode answers with one of the two documented preferences, inside the success envelope",
        viewerMode?.success === true && VIEWER_MODES.includes(viewerMode.mode),
        { viewerMode },
        { success: true, mode: VIEWER_MODES.join(" or ") },
    );

    recorder.assertCase(
        "SC-02 getType answers with a matching index and name pair, inside the success envelope",
        type?.success === true &&
            Number.isInteger(type.type) &&
            type.type >= 0 &&
            typeof type.typeName === "string" &&
            type.typeName.length > 0,
        { type },
        { success: true, type: "an integer", typeName: "a non-empty string" },
    );

    recorder.assertCase(
        "SC-03 get answers with a count, a type, a handle page and its paging flags, inside the success envelope",
        selection?.success === true &&
            Number.isInteger(selection.count) &&
            typeof selection.type === "string" &&
            Array.isArray(selection.handles) &&
            selection.offset === 0 &&
            typeof selection.hasMore === "boolean",
        {
            keys: Object.keys(selection ?? {}).sort(),
            count: selection?.count,
            handles: selection?.handles?.length,
        },
        {
            has: ["success", "count", "type", "handles", "offset", "hasMore"],
        },
    );

    // getViewingTrack reports the mode it resolved under even when it found
    // nothing.
    recorder.assertCase(
        "SC-04 getViewingTrack reports the mode it resolved under, and says so plainly when there is nothing to view",
        viewing?.success === true &&
            typeof viewing.found === "boolean" &&
            VIEWER_MODES.includes(viewing.mode) &&
            (viewing.found === false
                ? !("handle" in viewing)
                : typeof viewing.handle === "string" && ["now_playing", "selection"].includes(viewing.source)),
        { viewing },
        {
            success: true,
            mode: "the resolved preference",
            whenNotFound: "no handle field",
        },
    );

    // The location comes as a set: a list without a row would read as "the track
    // is in this list" when only the active list was searched and missed.
    const locationKeys = ["playlistIndex", "playlistGuid", "itemIndex"];
    const present = locationKeys.filter((key) => key in (viewing ?? {}));
    const listed = (playlists?.playlists ?? []).find(
        (entry) => entry.index === viewing?.playlistIndex,
    );
    recorder.assertCase(
        "SC-04b getViewingTrack reports playlistIndex, playlistGuid and itemIndex together or not at all, and the GUID is that playlist's",
        viewing?.success === true &&
            (present.length === 0 ||
                (present.length === locationKeys.length &&
                    Number.isInteger(viewing.playlistIndex) &&
                    Number.isInteger(viewing.itemIndex) &&
                    typeof viewing.playlistGuid === "string" &&
                    listed?.guid === viewing.playlistGuid)),
        {
            present,
            location: {
                playlistIndex: viewing?.playlistIndex,
                playlistGuid: viewing?.playlistGuid,
                itemIndex: viewing?.itemIndex,
            },
            listedGuid: listed?.guid,
        },
        { present: "all three or none", playlistGuid: "the getAll guid at playlistIndex" },
    );

    return { entryCount: selection?.count };
}

async function runSelectionSetCases(bridge, recorder, realPath) {
    const { invoke } = bridge;

    const noHandles = await invoke("selection.set", {});
    const emptyHandles = await invoke("selection.set", { handles: [] });
    recorder.assertCase(
        "SC-05 set distinguishes a missing handles array from an empty one, both as INVALID_PARAMS",
        noHandles?.success === false &&
            noHandles.code === "INVALID_PARAMS" &&
            /^handles is required/i.test(noHandles.error ?? "") &&
            emptyHandles?.success === false &&
            emptyHandles.code === "INVALID_PARAMS" &&
            /^handles must have at least 1 item/i.test(emptyHandles.error ?? ""),
        { noHandles, emptyHandles },
        {
            missing: { code: "INVALID_PARAMS", error: "handles is required" },
            empty: { code: "INVALID_PARAMS", error: "handles must have at least 1 item(s)" },
        },
    );

    // The handle is minted from the string, not looked up, so a path that is
    // not there is selected just as readily as one that is - the same trap
    // library.addToPlaylist has (LR-44).
    const absent = await invoke("selection.set", { handles: [ABSENT_PATH] });
    recorder.assertCase(
        "SC-06 set mints handles without checking that the files exist, so an absent path is selected and counted",
        absent?.success === true && absent.count === 1,
        { absent, path: ABSENT_PATH },
        { success: true, count: 1 },
    );

    const setReal = await invoke("selection.set", { handles: [realPath] });
    const readBack = await invoke("selection.get", {});
    const typeAfter = await invoke("selection.getType", {});
    // Measured here, not at entry: the instance can start or stop part way
    // through a run. set writes through a holder it releases on return, so with
    // a track loaded and the viewer preferring the playing track, the now-playing
    // holder answers get and getType instead of what was just written.
    const [playbackNow, viewerMode] = await Promise.all([
        invoke("playback.getState", {}),
        invoke("selection.getViewerMode", {}),
    ]);
    const nothingLoaded = playbackNow?.state === "stopped";
    const playingWins = !nothingLoaded && viewerMode?.mode === "prefer_playing";
    recorder.assertCase(
        nothingLoaded
            ? "SC-07 what set writes is readable through get, but the selection type is left untouched by this path"
            : playingWins
              ? "SC-07 with a track loaded and the viewer preferring it, the now-playing holder answers get and getType, so what set wrote is not what comes back"
              : "SC-07 with a track loaded under prefer_selection the readback is not characterised; readings recorded",
        setReal?.success === true &&
            setReal.count === 1 &&
            (nothingLoaded
                ? readBack?.count === 1 &&
                  readBack.handles[0] === realPath &&
                  typeAfter?.typeName === "unknown" &&
                  typeAfter.type === 0
                : playingWins
                  ? readBack?.count === 1 &&
                    readBack.handles[0] !== realPath &&
                    typeAfter?.typeName === "now_playing"
                  : true),
        {
            setReal,
            readBack: { count: readBack?.count, handles: readBack?.handles },
            typeAfter,
            playbackState: playbackNow?.state,
            viewerMode: viewerMode?.mode,
        },
        nothingLoaded
            ? {
                  set: { count: 1 },
                  get: { count: 1, firstHandle: realPath },
                  type: { typeName: "unknown", note: "not updated by selection.set" },
              }
            : playingWins
              ? {
                    set: { count: 1 },
                    get: { count: 1, firstHandle: "the loaded track, not what set wrote" },
                    type: { typeName: "now_playing" },
                }
              : { set: { count: 1 }, skipped: "needs a stopped instance or prefer_playing" },
    );

    recorder.addCase(
        "SC-08 set cannot express an empty selection, so there is no way back to nothing selected through it",
        true,
        {
            emptyArray: "refused with 'handles must have at least 1 item(s)'",
            consequence:
                "restoring an entry state of nothing selected needs setPlaylistTracking, not selection.set",
        },
        { recorded: "an API limitation, not a defect" },
    );
}

// selection:changed carries the handles and, for a single track, the shared Track row. What the
// callback reports as selected depends on which holder wins (see SC-07), so only the shape is
// pinned here, not which track it names. SC-07 left realPath selected, and setting it again
// changes nothing and announces nothing, so this selects the absent path instead.
async function runSelectionEventCases(bridge, recorder) {
    const { invoke } = bridge;
    // A change less than 50 ms after the previous event is dropped; let the throttle lapse.
    await new Promise((resolve) => setTimeout(resolve, 150));
    const collector = await createEventCollector(bridge, ["selection:changed"], { collectorId: `sc-event-${runId}` });
    try {
        await invoke("selection.set", { handles: [ABSENT_PATH] });
        const received = await collector.waitFor((r) => r.some((e) => e.name === "selection:changed"), {
            timeoutMs: 3000,
        });
        const payload = received.find((e) => e.name === "selection:changed")?.payload ?? null;
        const playback = await invoke("playback.getState", {});
        const loaded = playback?.state !== "stopped";
        const allowed = new Set(["count", "type", "handles", "truncated", "track", "nowPlaying"]);
        const isTrack = (t) => typeof t?.handle === "string" && Array.isArray(t.artists) && !("id" in t) && !("fullPath" in t);
        recorder.assertCase(
            "SC-18 selection:changed after set carries count, type, string handles and truncated; a single track and the loaded track come as Track rows without id or fullPath",
            payload !== null &&
                Object.keys(payload).every((k) => allowed.has(k)) &&
                typeof payload.count === "number" &&
                typeof payload.type === "string" &&
                Array.isArray(payload.handles) &&
                payload.handles.every((h) => typeof h === "string") &&
                payload.truncated === payload.count > 100 &&
                (payload.count === 1 ? isTrack(payload.track) : !("track" in payload)) &&
                (loaded ? isTrack(payload.nowPlaying) : !("nowPlaying" in payload)),
            {
                keys: Object.keys(payload ?? {}),
                count: payload?.count,
                type: payload?.type,
                handles: payload?.handles,
                trackHandle: payload?.track?.handle,
                nowPlayingHandle: payload?.nowPlaying?.handle,
                playbackState: playback?.state,
            },
            {
                keys: [...allowed],
                track: "a Track when count is 1",
                nowPlaying: "a Track while a track is loaded",
            },
        );
    } finally {
        await collector.stop();
    }
}

async function runTrackingCases(bridge, recorder, playlistTrackCount) {
    const { invoke } = bridge;

    // Playlist tracking points the holder at the whole active playlist; selection
    // tracking points it at that playlist's own selected items.
    const toPlaylist = await invoke("selection.setPlaylistTracking", { mode: "playlist" });
    const wholePlaylist = await invoke("selection.get", { limit: 1 });
    const toSelection = await invoke("selection.setPlaylistTracking", { mode: "selection" });
    const justSelection = await invoke("selection.get", {});

    // Both preconditions are read after the two selection readings rather than
    // at suite entry, so they describe the same moment those were taken: an
    // instance can start or stop on its own part way through a run, and the
    // front end driving the page decides what is selected in the active
    // playlist. With a track loaded the holder reports that track under either
    // tracking mode, so neither count reading is about the playlist and only
    // the mode round-trip is asserted. When stopped, selection tracking is
    // expected to report exactly the active playlist's own selected rows - the
    // bundled front end keeps none, other front ends keep the focused row - so
    // the oracle is playlist.getSelection taken here, not a hard-coded zero.
    const activeSelection = await invoke("playlist.getSelection", {});
    const selectedInActive = activeSelection?.count ?? 0;
    const playback = await invoke("playback.getState", {});
    const nothingPlaying = playback?.state === "stopped";

    recorder.assertCase(
        nothingPlaying
            ? selectedInActive === 0
                ? "SC-09 playlist tracking follows the whole active playlist, selection tracking follows only what is selected in it (nothing at this moment)"
                : "SC-09 playlist tracking follows the whole active playlist, selection tracking follows only what is selected in it (the front end keeps rows selected)"
            : "SC-09 both tracking modes round-trip, but with a track loaded the holder reports that track under either, so the playlist-count reading is out of reach",
        toPlaylist?.success === true &&
            toPlaylist.mode === "playlist" &&
            toSelection?.success === true &&
            toSelection.mode === "selection" &&
            (nothingPlaying
                ? wholePlaylist?.count === playlistTrackCount &&
                  justSelection?.count === selectedInActive
                : true),
        {
            toPlaylist,
            underPlaylistTracking: wholePlaylist?.count,
            playlistTrackCount,
            toSelection,
            underSelectionTracking: justSelection?.count,
            selectedInActivePlaylist: selectedInActive,
            playbackState: playback?.state,
            stoppedBranchReached: nothingPlaying,
        },
        nothingPlaying
            ? {
                  underPlaylistTracking: "the active playlist's track count",
                  underSelectionTracking: selectedInActive,
              }
            : {
                  modes: "playlist then selection, each echoed",
                  counts: "not asserted: a loaded track is the holder's fallback under both modes",
              },
    );

    // The declared parameter limits mode to its two values, so a typo is
    // refused instead of being echoed back as if it had applied; omitting it
    // means selection tracking.
    const bogus = await invoke("selection.setPlaylistTracking", { mode: `nope-${runId}` });
    const defaulted = await invoke("selection.setPlaylistTracking", {});
    recorder.assertCase(
        "SC-10 an unrecognised tracking mode is refused with INVALID_PARAMS, and an omitted one means selection tracking",
        bogus?.success === false &&
            bogus.code === "INVALID_PARAMS" &&
            /^mode /i.test(bogus.error ?? "") &&
            defaulted?.success === true &&
            defaulted.mode === "selection",
        { bogus, defaulted },
        {
            bogus: { success: false, code: "INVALID_PARAMS", error: "names mode" },
            omitted: { success: true, mode: "selection" },
        },
    );
}

async function runPagingCases(bridge, recorder, playlistTrackCount) {
    const { invoke } = bridge;

    if (playlistTrackCount <= 100) {
        recorder.addCase(
            "SC-11 a selection larger than a page reports it, and offset, limit and limit zero window it",
            true,
            {
                skipped: `the active playlist holds ${playlistTrackCount} tracks; the truncation branch needs more than 100`,
            },
            { skipped: "needs an active playlist of over 100 tracks" },
        );
        return;
    }

    // Playlist tracking is the only way to get a selection this large without
    // clicking, which is what makes the truncation branch reachable at all.
    await invoke("selection.setPlaylistTracking", { mode: "playlist" });
    const [implicit, windowed, explicitLimit, everything] = await Promise.all([
        invoke("selection.get", {}),
        invoke("selection.get", { offset: 5, limit: 2 }),
        invoke("selection.get", { limit: 150 }),
        invoke("selection.get", { limit: 0 }),
    ]);
    // Both preconditions are read right after the four readings, not taken
    // from suite entry: with a track loaded the now-playing holder answers
    // selection.get under playlist tracking as well (the same shadowing SC-07
    // and SC-09 assert), so the four readings describe that one track and not
    // the playlist; and the active playlist itself can change under a run.
    const playback = await invoke("playback.getState", {});
    const currentCount = await invoke("playlist.getTrackCount", {});
    await invoke("selection.setPlaylistTracking", { mode: "selection" });

    const trackLoaded = playback?.state !== "stopped";
    const playlistNow = currentCount?.count ?? playlistTrackCount;
    // What the holder is expected to hold at the moment of the readings: the
    // whole active playlist when stopped, the loaded track otherwise. The
    // paging arithmetic is asserted against that size either way; the
    // truncation flags are only reachable when it exceeds a page.
    const held = trackLoaded ? implicit?.count : playlistNow;
    const largerThanPage = Number.isInteger(held) && held > 100;
    const windowExpected = Math.max(0, Math.min(2, (held ?? 0) - 5));

    recorder.assertCase(
        trackLoaded
            ? "SC-11 with a track loaded the now-playing holder answers under playlist tracking too, so offset, limit and limit zero window that one track and the truncation branch is out of reach"
            : largerThanPage
              ? "SC-11 a selection larger than a page reports it, and offset, limit and limit zero window it"
              : "SC-11 the active playlist changed under the run and no longer exceeds a page, so offset, limit and limit zero are asserted on what it holds now",
        Number.isInteger(implicit?.count) &&
            implicit.count === held &&
            windowed?.offset === 5 &&
            !("truncated" in (windowed ?? {})) &&
            !("truncated" in (explicitLimit ?? {})) &&
            everything?.handles.length === held &&
            everything.hasMore === false &&
            (largerThanPage
                ? implicit.handles.length === 100 &&
                  implicit.truncated === true &&
                  implicit.hasMore === true &&
                  windowed.handles.length === 2 &&
                  windowed.hasMore === true &&
                  explicitLimit?.handles.length === 150
                : implicit.handles.length === held &&
                  implicit.hasMore === false &&
                  !("truncated" in implicit) &&
                  windowed.handles.length === windowExpected &&
                  explicitLimit?.handles.length === held),
        {
            playbackState: playback?.state,
            activePlaylistTrackCount: playlistNow,
            held,
            implicit: {
                count: implicit?.count,
                returned: implicit?.handles?.length,
                truncated: implicit?.truncated,
                hasMore: implicit?.hasMore,
            },
            windowed: {
                returned: windowed?.handles?.length,
                offset: windowed?.offset,
                hasMore: windowed?.hasMore,
                truncatedPresent: "truncated" in (windowed ?? {}),
            },
            explicitLimit: { returned: explicitLimit?.handles?.length },
            everything: { returned: everything?.handles?.length, hasMore: everything?.hasMore },
        },
        largerThanPage
            ? {
                  implicit: { count: held, returned: 100, truncated: true, hasMore: true },
                  windowed: { returned: 2, offset: 5, hasMore: true, truncatedPresent: false },
                  explicitLimit: { returned: 150, truncatedPresent: false },
                  limitZero: { returned: "every handle", hasMore: false },
              }
            : {
                  implicit: { count: held, returned: held, truncatedPresent: false, hasMore: false },
                  windowed: { returned: windowExpected, offset: 5, truncatedPresent: false },
                  explicitLimit: { returned: held, truncatedPresent: false },
                  limitZero: { returned: held, hasMore: false },
              },
    );
}

async function runDragCases(bridge, recorder, realPath) {
    const { invoke } = bridge;

    const capabilities = await invoke("dnd.getCapabilities", {});
    const noPaths = await invoke("dnd.prepareDrag", {});
    const notArray = await invoke("dnd.prepareDrag", { paths: "not-an-array" });
    const nonString = await invoke("dnd.prepareDrag", { paths: [42] });
    recorder.assertCase(
        "SC-12 prepareDrag refuses a missing or non-array paths, and a non-string element, with INVALID_PARAMS",
        [noPaths, notArray, nonString].every(
            (res) => res?.success === false && res.code === "INVALID_PARAMS",
        ) &&
            /paths is required/.test(noPaths.error ?? "") &&
            /paths must be an array/.test(notArray.error ?? "") &&
            /paths\[0\] must be a string/.test(nonString.error ?? ""),
        { noPaths, notArray, nonString },
        { each: { success: false, code: "INVALID_PARAMS" } },
    );

    if (capabilities?.dragOut !== true) {
        recorder.addCase(
            "SC-13 prepareDrag mints a token for a real path, and for one that does not exist",
            true,
            { skipped: "this window reports dragOut false", capabilities },
            { skipped: "needs a drag-capable window" },
        );
        return;
    }

    const real = await invoke("dnd.prepareDrag", { paths: [realPath] });
    const absent = await invoke("dnd.prepareDrag", { paths: [ABSENT_PATH] });
    recorder.assertCase(
        "SC-13 prepareDrag mints a token for a real path, and for one that does not exist just the same",
        real?.success === true &&
            /^[0-9a-f]{32}$/.test(real.token ?? "") &&
            absent?.success === true &&
            /^[0-9a-f]{32}$/.test(absent.token ?? "") &&
            real.token !== absent.token,
        {
            real: { success: real?.success, tokenShape: real?.token?.length },
            absent: { success: absent?.success, tokenShape: absent?.token?.length },
            distinct: real?.token !== absent?.token,
        },
        {
            each: { success: true, token: "32 lowercase hex characters" },
            distinct: true,
            note: "existence is not checked at token time",
        },
    );
}

async function runPanelCases(bridge, recorder) {
    const { invoke } = bridge;

    const get = await invoke("panel.getConfig", {});
    const set = await invoke("panel.setConfig", { panelName: `e2e-${runId}` });
    recorder.assertCase(
        "SC-14 both panel endpoints report no panel on a standalone window, because a panel config only exists for a DUI element or a CUI panel",
        get?.success === false &&
            get.code === "NOT_FOUND" &&
            get.error === "Panel not found" &&
            set?.success === false &&
            set.code === "NOT_FOUND" &&
            set.error === "Panel not found",
        { get, set },
        { each: { success: false, code: "NOT_FOUND", error: "Panel not found" } },
    );
}

async function runExecuteCases(bridge, recorder, hasTarget) {
    const { invoke } = bridge;

    const noGuid = await invoke("discovery.executeMainMenuCommand", {});
    const badGuid = await invoke("discovery.executeMainMenuCommand", { guid: "not-a-guid" });
    const badSubGuid = await invoke("discovery.executeMainMenuCommand", {
        guid: UNKNOWN_GUID,
        subGuid: "not-a-guid",
    });
    const unknownGuid = await invoke("discovery.executeMainMenuCommand", { guid: UNKNOWN_GUID });
    recorder.assertCase(
        "SC-15 main menu execution refuses a missing or malformed GUID with INVALID_PARAMS, and an unknown one with NOT_FOUND that still echoes the address",
        noGuid?.success === false &&
            noGuid.code === "INVALID_PARAMS" &&
            /guid is required/i.test(noGuid.error ?? "") &&
            badGuid?.success === false &&
            badGuid.code === "INVALID_PARAMS" &&
            /invalid guid/i.test(badGuid.error ?? "") &&
            badSubGuid?.success === false &&
            badSubGuid.code === "INVALID_PARAMS" &&
            /invalid subguid/i.test(badSubGuid.error ?? "") &&
            unknownGuid?.success === false &&
            unknownGuid.code === "NOT_FOUND" &&
            /no main-menu command owns/i.test(unknownGuid.error ?? "") &&
            unknownGuid.guid === UNKNOWN_GUID &&
            unknownGuid.dynamic === false,
        { noGuid, badGuid, badSubGuid, unknownGuid },
        {
            malformed: { code: "INVALID_PARAMS", error: "names the argument" },
            unknown: { success: false, code: "NOT_FOUND", guid: UNKNOWN_GUID, dynamic: false },
        },
    );

    const cmNoGuid = await invoke("discovery.executeContextMenuCommand", {});
    const cmBadGuid = await invoke("discovery.executeContextMenuCommand", { guid: "not-a-guid" });
    const cpNoPath = await invoke("discovery.executeContextMenuByPath", {});
    recorder.assertCase(
        "SC-16 the two context executions validate their own argument first: a GUID for one, a path for the other",
        cmNoGuid?.success === false &&
            /guid is required/i.test(cmNoGuid.error ?? "") &&
            cmBadGuid?.success === false &&
            /invalid guid/i.test(cmBadGuid.error ?? "") &&
            cpNoPath?.success === false &&
            /path is required/i.test(cpNoPath.error ?? ""),
        { cmNoGuid, cmBadGuid, cpNoPath },
        { each: { success: false, error: "names the missing or malformed argument" } },
    );

    if (hasTarget) {
        recorder.addCase(
            "SC-17 a global selection is not a context-menu target: both context executions look at the playing track or the playlist selection instead",
            true,
            {
                skipped:
                    "something is playing or selected in the active playlist, so the missing-target branch is not reachable",
            },
            { skipped: "needs nothing playing and nothing selected in the active playlist" },
        );
        return;
    }

    // The global selection is set here on purpose: selection.set writes
    // ui_selection_manager, and these two endpoints read the now-playing track
    // or activeplaylist_get_selected_items. Setting one does not feed the other.
    const globalSelection = await invoke("selection.set", { handles: [ABSENT_PATH] });
    const cmWithGlobal = await invoke("discovery.executeContextMenuCommand", {
        guid: UNKNOWN_GUID,
    });
    const cpWithGlobal = await invoke("discovery.executeContextMenuByPath", {
        path: `no-such-menu-${runId}/no-such-item`,
    });
    await invoke("selection.setPlaylistTracking", { mode: "selection" });
    recorder.assertCase(
        "SC-17 a global selection is not a context-menu target: both context executions look at the playing track or the playlist selection instead",
        globalSelection?.success === true &&
            cmWithGlobal?.success === false &&
            /no track selected or playing/i.test(cmWithGlobal.error ?? "") &&
            cpWithGlobal?.success === false &&
            /no track selected or playing/i.test(cpWithGlobal.error ?? ""),
        {
            globalSelectionCount: globalSelection?.count,
            contextByGuid: cmWithGlobal,
            contextByPath: cpWithGlobal,
        },
        {
            globalSelection: "set to one handle",
            both: { success: false, error: "No track selected or playing" },
        },
    );
}

const recorder = createRecorder();
let client;
let bridge;
let blocked = false;
let fatalError;
let targets;
let entry;

try {
    const connection = await connectBridgePage();
    client = connection.client;
    targets = connection.candidates;
    console.log(`Target: ${connection.page.title} ${connection.page.url}`);
    bridge = createBridge(client.Runtime, { invokeTimeoutMs });
    await requireResponsiveBridge(bridge, { probeId: runId });

    const [playback, playlistSelection, trackCount, page] = await Promise.all([
        bridge.invoke("playback.getState", {}),
        bridge.invoke("playlist.getSelection", {}),
        bridge.invoke("playlist.getTrackCount", {}),
        bridge.invoke("library.getAll", { limit: 1 }),
    ]);
    const realPath = page?.tracks?.[0]?.absolutePath;
    if (typeof realPath !== "string" || realPath.length === 0) {
        throw new Error("library.getAll returned no absolute path to drag or select");
    }
    const hasTarget = playback?.state !== "stopped" || (playlistSelection?.count ?? 0) > 0;
    entry = {
        playbackState: playback?.state,
        playlistSelectionCount: playlistSelection?.count,
        playlistTrackCount: trackCount?.count,
        hasContextTarget: hasTarget,
    };

    await runSelectionReadCases(bridge, recorder);
    await runSelectionSetCases(bridge, recorder, realPath);
    await runSelectionEventCases(bridge, recorder);
    await runTrackingCases(bridge, recorder, trackCount?.count ?? 0);
    await runPagingCases(bridge, recorder, trackCount?.count ?? 0);
    await runDragCases(bridge, recorder, realPath);
    await runPanelCases(bridge, recorder);
    await runExecuteCases(bridge, recorder, hasTarget);
} catch (error) {
    fatalError = error;
    blocked = Boolean(error?.blocked);
    console.error(`${blocked ? "BLOCKED" : "ERROR"}: ${error?.message || error}`);
} finally {
    // Re-point the holder at playlist-selection tracking, which is the only way
    // back to the entry reading: selection.set cannot express an empty selection.
    if (bridge) {
        await bridge
            .invokeRaw("selection.setPlaylistTracking", { mode: "selection" })
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
            entryState: entry,
            targets,
        },
    }),
);

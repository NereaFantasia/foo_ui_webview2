/**
 * Covers the playlist namespace beyond the lifecycle events: track insertion and
 * removal, selection, focus, ordering, undo/redo, autoplaylists, the
 * playlist-level operations, naming a playlist by its GUID, and the windowing
 * primitives - the `fields` projection on getTracks and the getGroupRuns group
 * runs.
 *
 * Everything happens on scratch playlists created for the run and removed
 * afterwards, and the entry active playlist is restored, so no playlist of the
 * user's is read or touched. The tracks come from the library; the only file
 * written is a two-line .m3u8 in a per-run folder under the first monitored root
 * (see fixture-area.mjs), removed at the end, which PO-62 and PO-63 need to make
 * addPathsAsync expand in the background.
 *
 * Ordering is asserted without depending on the host's collation: sorting
 * ascending and descending must be exact reverses, reverse must flip the current
 * order, shuffle must preserve the multiset, and `reorder` gets an explicit
 * permutation whose expected result is known. The six subject tracks are picked
 * for distinct titles, so no sort has ties to resolve.
 *
 * Not covered, with reasons: `playTrack` starts playback, which cannot be put
 * back the way it was found, the same reason track changes stay out of the
 * playback suite; `replaceAllAndPlay` is therefore exercised with autoPlay and
 * stopFirst off, so it never starts or stops anything. The locked-playlist
 * refusal (`PlaylistLocked`, code LOCKED) needs a lock owned by another
 * component and cannot be created from the bridge. `playlist:itemsReplaced` fires
 * on item replacement rather than on any bridge call, and `playlist:lockChanged`
 * and `playlist:defaultFormatChanged` have no bridge trigger either.
 *
 * Usage: node mcp/tests/e2e-playlist-ops.mjs
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
import { createFixtureArea } from "./lib/fixture-area.mjs";

const invokeTimeoutMs = envInt("FB2K_E2E_TIMEOUT_MS", 15000);
const eventTimeoutMs = envInt("FB2K_EVENT_TIMEOUT_MS", 4000);
const runId = new Date().toISOString().replace(/[-:.TZ]/g, "");

/** Every playlist this run creates carries the prefix, so cleanup is by name. */
const PREFIX = `e2e ops ${runId}`;
const SUBJECT_COUNT = 6;

const EVENT_NAMES = [
    "playlist:itemsAdded",
    "playlist:itemsRemoved",
    "playlist:itemsReordered",
    "playlist:selectionChanged",
    "playlist:focusChanged",
    "playlist:reordered",
    "playlist:addComplete",
];

function matching(collector, name, filter) {
    return collector.received.filter(
        (event) => event.name === name && (filter === undefined || filter(event.payload)),
    );
}

/** Waits for one more event than the caller has already consumed. */
async function waitForMore(collector, name, alreadySeen, filter) {
    await collector.waitFor(
        (events) =>
            events.filter(
                (event) => event.name === name && (filter === undefined || filter(event.payload)),
            ).length > alreadySeen,
        { timeoutMs: eventTimeoutMs, pollMs: 50 },
    );
    return matching(collector, name, filter);
}

/** A GUID as the host writes it, the shape playlistGuid accepts back. */
const GUID_SHAPE = /^\{[0-9A-F]{8}-[0-9A-F]{4}-[0-9A-F]{4}-[0-9A-F]{4}-[0-9A-F]{12}\}$/;

function keysOf(value) {
    return Object.keys(value ?? {}).sort();
}

function pathsOf(rows) {
    return (rows ?? []).map((row) => String(row?.absolutePath ?? ""));
}

function sameSet(left, right) {
    return JSON.stringify([...left].sort()) === JSON.stringify([...right].sort());
}

async function trackPaths(bridge, playlist) {
    const page = await bridge.invoke("playlist.getTracks", { playlist, count: 500 });
    return pathsOf(page?.tracks);
}

/** Resolves a playlist index by name, which is how a consumer survives reordering. */
async function indexOfName(bridge, name) {
    const all = await bridge.invoke("playlist.getAll", {});
    const position = (all?.playlists ?? []).findIndex((entry) => entry?.name === name);
    return position === -1 ? undefined : position;
}

async function runReadShapeCases(bridge, recorder, scratch, subjects) {
    const { invoke } = bridge;

    const playing = await invoke("playlist.getPlaying", {});
    recorder.assertCase(
        "PO-01 getPlaying either names the playlist playback comes from or reports there is none",
        playing?.found === false
            ? playing?.success === true
            : playing?.found === true &&
              typeof playing?.index === "number" &&
              typeof playing?.name === "string" &&
              typeof playing?.trackCount === "number" &&
              typeof playing?.isActive === "boolean" &&
              typeof playing?.isPlaying === "boolean" &&
              typeof playing?.isLocked === "boolean" &&
              typeof playing?.duration === "number",
        { response: playing },
        { either: "found false", or: "found true, index, name, trackCount, isActive, isPlaying, isLocked, duration" },
    );

    const [listCount, list] = await Promise.all([
        invoke("playlist.getCount", {}),
        invoke("playlist.getAll", {}),
    ]);
    recorder.assertCase(
        "PO-01b getCount agrees with the number of playlists getAll enumerates, and getAll counts its own list",
        Array.isArray(list?.playlists) &&
            listCount?.count === list.playlists.length &&
            list.count === list.playlists.length &&
            listCount.count > scratch,
        { getCount: listCount?.count, getAllCount: list?.count, getAllLength: list?.playlists?.length },
        { equal: true },
    );

    const window = await invoke("playlist.getTracks", { playlist: scratch, start: 2, count: 2 });
    recorder.assertCase(
        "PO-02 getTracks pages with start and count, echoing the window it served",
        window?.playlist === scratch &&
            window?.start === 2 &&
            window?.count === 2 &&
            window?.total === SUBJECT_COUNT &&
            JSON.stringify(pathsOf(window?.tracks)) ===
                JSON.stringify(subjects.slice(2, 4).map((track) => track.absolutePath)),
        {
            start: window?.start,
            count: window?.count,
            total: window?.total,
            paths: pathsOf(window?.tracks),
        },
        { start: 2, count: 2, total: SUBJECT_COUNT },
    );

    // The library namespace pages with offset/limit. Here those keys are neither
    // aliases of start and count nor ignored: the declared reader refuses them, so a
    // caller who reaches for the library spelling finds out at once instead of being
    // served the whole playlist.
    const aliasAttempt = await invoke("playlist.getTracks", {
        playlist: scratch,
        offset: 2,
        limit: 2,
    });
    recorder.assertCase(
        "PO-03 offset and limit are refused as unknown parameters rather than treated as aliases of start and count",
        aliasAttempt?.success === false &&
            aliasAttempt?.code === "INVALID_PARAMS" &&
            /^unknown parameter '(offset|limit)'$/.test(aliasAttempt?.error ?? "") &&
            aliasAttempt?.tracks === undefined,
        { requested: { offset: 2, limit: 2 }, response: aliasAttempt },
        { success: false, code: "INVALID_PARAMS", error: "unknown parameter 'limit' or 'offset'" },
    );

    // Two namespaces describe the same track, and a view written against library
    // rows reads playlist rows too: the playlist row is the library row plus two
    // tags and the play statistics. The statistics appear only where foo_playcount
    // gives a value, so they bound the difference rather than fix it.
    const libraryRow = ((await invoke("library.getAll", { limit: 1 }))?.tracks ?? [])[0];
    const playlistRow = (window?.tracks ?? [])[0];
    const onlyInPlaylist = keysOf(playlistRow).filter((key) => !keysOf(libraryRow).includes(key));
    const onlyInLibrary = keysOf(libraryRow).filter((key) => !keysOf(playlistRow).includes(key));
    recorder.assertCase(
        "PO-04 the playlist row is the library row plus composer, comment and whatever play statistics exist",
        ["comment", "composer"].every((key) => onlyInPlaylist.includes(key)) &&
            onlyInPlaylist.every((key) =>
                ["comment", "composer", ...PLAYCOUNT_FIELDS].includes(key),
            ) &&
            onlyInLibrary.length === 0 &&
            (playlistRow?.playCount === undefined || Number.isInteger(playlistRow.playCount)),
        {
            onlyInPlaylistRow: onlyInPlaylist,
            onlyInLibraryRow: onlyInLibrary,
            playCount: playlistRow?.playCount,
        },
        {
            onlyInPlaylistRow: "comment, composer and any of the play statistics",
            onlyInLibraryRow: [],
            playCount: "absent or an integer",
        },
    );

    await invoke("playlist.selectAll", { playlist: scratch });
    const selectedTracks = await invoke("playlist.getSelectedTracks", { playlist: scratch });
    const selectedRow = (selectedTracks?.tracks ?? [])[0];
    const missingFromSelected = keysOf(playlistRow).filter(
        (key) => !keysOf(selectedRow).includes(key),
    );
    recorder.assertCase(
        "PO-05 the selected-tracks row is the playlist row without the play statistics",
        selectedTracks?.success === true &&
            selectedTracks?.count === SUBJECT_COUNT &&
            missingFromSelected.every((key) => PLAYCOUNT_FIELDS.includes(key)) &&
            keysOf(selectedRow).every((key) => keysOf(playlistRow).includes(key)),
        {
            count: selectedTracks?.count,
            missingFromSelectedRow: missingFromSelected,
            selectedRowKeys: keysOf(selectedRow),
        },
        { missingFromSelectedRow: "play statistics only", keysBeyondThePlaylistRow: "none" },
    );

    const columnList = await invoke("playlist.getAvailableColumns", {});
    const columns = columnList?.columns;
    recorder.assertCase(
        "PO-06 getAvailableColumns answers with the column definitions and their count",
        Array.isArray(columns) &&
            columns.length > 0 &&
            columnList.count === columns.length &&
            columns.every(
                (column) =>
                    typeof column?.id === "string" &&
                    typeof column?.name === "string" &&
                    typeof column?.pattern === "string" &&
                    typeof column?.sortPattern === "string" &&
                    typeof column?.numeric === "boolean" &&
                    typeof column?.alignment === "string",
            ),
        { count: columnList?.count, length: columns?.length, first: columns?.[0] },
        { countMatchesLength: true, everyColumn: "id, name, pattern, sortPattern, numeric, alignment" },
    );

    // The same fact under two envelopes: only getLockInfo echoes the playlist
    // index.
    const [lockInfo, locked] = await Promise.all([
        invoke("playlist.getLockInfo", { playlist: scratch }),
        invoke("playlist.isLocked", { playlist: scratch }),
    ]);
    recorder.assertCase(
        "PO-07 the two lock endpoints agree on the fact, and only getLockInfo echoes the playlist",
        lockInfo?.isLocked === false &&
            lockInfo?.playlist === scratch &&
            lockInfo?.success === true &&
            locked?.isLocked === false &&
            locked?.success === true &&
            locked?.playlist === undefined,
        { getLockInfo: lockInfo, isLocked: locked },
        { sameIsLocked: false, bothSucceed: true, isLockedHasNoPlaylist: true },
    );

    const [isAuto, autoInfo, autoQuery] = await Promise.all([
        invoke("playlist.isAutoplaylist", { playlist: scratch }),
        invoke("playlist.getAutoplaylistInfo", { playlist: scratch }),
        invoke("playlist.getAutoplaylistQuery", { playlist: scratch }),
    ]);
    recorder.assertCase(
        "PO-08 a hand-made playlist is not an autoplaylist, and all three endpoints say so",
        isAuto?.isAutoplaylist === false &&
            autoInfo?.isAutoplaylist === false &&
            autoQuery?.isAutoplaylist === false &&
            autoQuery?.query === null,
        { isAutoplaylist: isAuto, info: autoInfo, query: autoQuery },
        { allThree: "isAutoplaylist false", query: null },
    );
}

async function runSelectionCases(bridge, recorder, collector, scratch) {
    const { invoke } = bridge;
    const forPlaylist = (payload) => payload?.playlist === scratch;

    let selectionSeen = matching(collector, "playlist:selectionChanged", forPlaylist).length;
    await invoke("playlist.deselectAll", { playlist: scratch });
    const cleared = await invoke("playlist.getSelection", { playlist: scratch });
    recorder.assertCase(
        "PO-09 deselectAll leaves an empty selection",
        cleared?.success === true &&
            cleared?.count === 0 &&
            Array.isArray(cleared?.items) &&
            cleared.items.length === 0 &&
            cleared?.playlist === scratch,
        { response: cleared },
        { count: 0, items: [] },
    );

    await invoke("playlist.selectAll", { playlist: scratch });
    const all = await invoke("playlist.getSelection", { playlist: scratch });
    recorder.assertCase(
        "PO-10 selectAll selects every row and reports their indices",
        all?.count === SUBJECT_COUNT &&
            JSON.stringify(all?.items) ===
                JSON.stringify([...Array(SUBJECT_COUNT).keys()]),
        { count: all?.count, items: all?.items },
        { count: SUBJECT_COUNT, items: [...Array(SUBJECT_COUNT).keys()] },
    );

    const setResponse = await invoke("playlist.setSelection", {
        playlist: scratch,
        indices: [1, 3],
    });
    const narrowed = await invoke("playlist.getSelection", { playlist: scratch });
    const narrowedTracks = await invoke("playlist.getSelectedTracks", { playlist: scratch });
    const paths = await trackPaths(bridge, scratch);
    recorder.assertCase(
        "PO-11 setSelection replaces the selection, and the selected rows are the ones at those indices",
        setResponse?.success === true &&
            JSON.stringify(narrowed?.items) === JSON.stringify([1, 3]) &&
            narrowedTracks?.count === 2 &&
            JSON.stringify(pathsOf(narrowedTracks?.tracks)) ===
                JSON.stringify([paths[1], paths[3]]),
        {
            items: narrowed?.items,
            selectedPaths: pathsOf(narrowedTracks?.tracks),
            expectedPaths: [paths[1], paths[3]],
        },
        { items: [1, 3], selectedRowsMatchIndices: true },
    );

    const selectionEvents = await waitForMore(
        collector,
        "playlist:selectionChanged",
        selectionSeen,
        forPlaylist,
    );
    recorder.assertCase(
        "PO-12 selection changes are announced, naming the playlist by index and GUID and nothing else",
        selectionEvents.length > selectionSeen &&
            sameSet(keysOf(selectionEvents[selectionEvents.length - 1].payload), ["playlist", "playlistGuid"]) &&
            GUID_SHAPE.test(selectionEvents[selectionEvents.length - 1].payload?.playlistGuid ?? ""),
        {
            newEvents: selectionEvents.length - selectionSeen,
            payload: selectionEvents[selectionEvents.length - 1]?.payload,
        },
        { atLeastOneNewEvent: true, payloadKeys: ["playlist", "playlistGuid"] },
    );

    // clearOthers defaults to true; false is the additive spelling.
    await invoke("playlist.setSelection", { playlist: scratch, indices: [0] });
    await invoke("playlist.setSelection", {
        playlist: scratch,
        indices: [2],
        clearOthers: false,
    });
    const additive = await invoke("playlist.getSelection", { playlist: scratch });
    recorder.assertCase(
        "PO-13 clearOthers false adds to the selection instead of replacing it",
        JSON.stringify(additive?.items) === JSON.stringify([0, 2]),
        { items: additive?.items },
        { items: [0, 2] },
    );

    const focusSeen = matching(collector, "playlist:focusChanged", forPlaylist).length;
    await invoke("playlist.focusTrack", { playlist: scratch, index: 2 });
    const [focusA, focusB] = await Promise.all([
        invoke("playlist.getFocusTrack", { playlist: scratch }),
        invoke("playlist.getFocusedTrack", { playlist: scratch }),
    ]);
    await invoke("playlist.setFocusedTrack", { playlist: scratch, index: 4 });
    const moved = await invoke("playlist.getFocusedTrack", { playlist: scratch });
    recorder.assertCase(
        "PO-14 focus is one fact reachable under two setter and two getter names",
        focusA?.index === 2 &&
            focusB?.index === 2 &&
            focusA?.playlist === scratch &&
            moved?.index === 4,
        {
            afterFocusTrack: { getFocusTrack: focusA?.index, getFocusedTrack: focusB?.index },
            afterSetFocusedTrack: moved?.index,
        },
        { afterFocusTrack: 2, afterSetFocusedTrack: 4 },
    );

    const focusEvents = await waitForMore(
        collector,
        "playlist:focusChanged",
        focusSeen,
        forPlaylist,
    );
    const lastFocus = focusEvents[focusEvents.length - 1];
    recorder.assertCase(
        "PO-15 a focus change reports where it moved from and to",
        focusEvents.length > focusSeen &&
            sameSet(keysOf(lastFocus?.payload), ["playlist", "playlistGuid", "from", "to"]) &&
            lastFocus?.payload?.to === 4 &&
            typeof lastFocus?.payload?.from === "number",
        { payload: lastFocus?.payload },
        { payloadKeys: ["from", "playlist", "playlistGuid", "to"], to: 4 },
    );

    // The `index` key used to be an alias for the playlist on the endpoints that
    // resolved it through a shared helper, and was silently ignored on the others,
    // which then fell back to the active playlist. Declared endpoints refuse it as
    // an unknown parameter instead.
    const [refusedPage, refused] = await Promise.all([
        invoke("playlist.getTracks", { index: scratch, count: 1 }),
        invoke("playlist.getSelection", { index: scratch }),
    ]);
    recorder.assertCase(
        "PO-16 declared endpoints refuse the index key instead of falling back to the active playlist",
        [refusedPage, refused].every(
            (response) =>
                response?.success === false &&
                response?.code === "INVALID_PARAMS" &&
                response?.error === "unknown parameter 'index'",
        ),
        { getTracksWithIndexKey: refusedPage, getSelectionWithIndexKey: refused, scratch },
        { both: { success: false, code: "INVALID_PARAMS", error: "unknown parameter 'index'" } },
    );
}

async function runOrderCases(bridge, recorder, collector, scratch) {
    const { invoke } = bridge;
    const forPlaylist = (payload) => payload?.playlist === scratch;

    const before = await trackPaths(bridge, scratch);
    let reorderSeen = matching(collector, "playlist:itemsReordered", forPlaylist).length;

    await invoke("playlist.sort", { playlist: scratch, pattern: "%title%" });
    const ascending = await trackPaths(bridge, scratch);
    await invoke("playlist.sort", { playlist: scratch, pattern: "%title%", descending: true });
    const descending = await trackPaths(bridge, scratch);
    recorder.assertCase(
        "PO-17 sorting keeps the same rows, and descending is the exact reverse of ascending",
        sameSet(ascending, before) &&
            JSON.stringify(descending) === JSON.stringify([...ascending].reverse()),
        { ascending, descending },
        { sameRows: true, descendingIsReverse: true },
    );

    // selectedOnly narrows the sort to the selected rows, but descending is
    // applied afterwards as a reversal of the whole playlist - so the two options
    // together do not mean "sort the selected rows downwards", they mean "sort
    // them upwards, then turn the playlist upside down".
    await invoke("playlist.setSelection", { playlist: scratch, indices: [0, 1, 2] });
    await invoke("playlist.sort", {
        playlist: scratch,
        pattern: "%title%",
        selectedOnly: true,
    });
    const selectedAscending = await trackPaths(bridge, scratch);
    await invoke("playlist.sort", {
        playlist: scratch,
        pattern: "%title%",
        selectedOnly: true,
        descending: true,
    });
    const selectedDescending = await trackPaths(bridge, scratch);
    recorder.assertCase(
        "PO-17b descending reverses the whole playlist even when the sort itself was limited to the selection",
        JSON.stringify(selectedDescending) === JSON.stringify([...selectedAscending].reverse()),
        {
            selection: [0, 1, 2],
            afterSelectedAscending: selectedAscending,
            afterSelectedDescending: selectedDescending,
            characterization: "the unselected rows move too",
        },
        { afterSelectedDescending: [...selectedAscending].reverse() },
    );

    const beforeReverse = await trackPaths(bridge, scratch);
    await invoke("playlist.reverse", { playlist: scratch });
    const reversed = await trackPaths(bridge, scratch);
    recorder.assertCase(
        "PO-18 reverse flips the current order",
        JSON.stringify(reversed) === JSON.stringify([...beforeReverse].reverse()),
        { beforeReverse, afterReverse: reversed },
        { afterReverse: [...beforeReverse].reverse() },
    );

    const shuffled = await (async () => {
        await invoke("playlist.shuffle", { playlist: scratch });
        return trackPaths(bridge, scratch);
    })();
    recorder.assertCase(
        "PO-19 shuffle keeps every row, changing only the order",
        sameSet(shuffled, before) && shuffled.length === before.length,
        {
            sameRows: sameSet(shuffled, before),
            orderChanged: JSON.stringify(shuffled) !== JSON.stringify(reversed),
        },
        { sameRows: true },
    );

    // moveTracks is not "move these rows": it sets the selection to the given
    // indices and then moves the selection. So a caller that had rows selected
    // loses that selection to the call, and a caller that omits items moves
    // whatever happened to be selected. Both halves are asserted, because the
    // second one is how an unrelated selection turns into a silent edit.
    await invoke("playlist.setSelection", { playlist: scratch, indices: [0] });
    const beforeMove = await trackPaths(bridge, scratch);
    const moved = await invoke("playlist.moveTracks", {
        playlist: scratch,
        items: [1],
        delta: 2,
    });
    const afterMove = await trackPaths(bridge, scratch);
    const selectionAfterMove = await invoke("playlist.getSelection", { playlist: scratch });
    const expectedAfterMove = [...beforeMove];
    expectedAfterMove.splice(3, 0, ...expectedAfterMove.splice(1, 1));
    recorder.assertCase(
        "PO-19b moveTracks shifts the named rows and takes the selection over from the caller",
        moved?.success === true &&
            JSON.stringify(afterMove) === JSON.stringify(expectedAfterMove) &&
            JSON.stringify(selectionAfterMove?.items) !== JSON.stringify([0]),
        {
            selectionBefore: [0],
            movedItems: [1],
            delta: 2,
            after: afterMove,
            expected: expectedAfterMove,
            selectionAfter: selectionAfterMove?.items,
            characterization: "the selection the caller had set is replaced by the moved rows",
        },
        { rowMovedByDelta: true, selectionAfter: "no longer the caller's [0]" },
    );

    // An explicit permutation is the one ordering operation with a known expected
    // result, so it is the one that pins what the order array means: position i
    // takes the row currently at newOrder[i].
    const beforeReorder = await trackPaths(bridge, scratch);
    const permutation = [3, 0, 5, 1, 4, 2];
    const reordered = await invoke("playlist.reorder", {
        playlist: scratch,
        newOrder: permutation,
    });
    const afterReorder = await trackPaths(bridge, scratch);
    recorder.assertCase(
        "PO-20 reorder applies the permutation it was given, position by position",
        reordered?.success === true &&
            reordered?.itemCount === SUBJECT_COUNT &&
            JSON.stringify(afterReorder) ===
                JSON.stringify(permutation.map((from) => beforeReorder[from])),
        {
            permutation,
            response: reordered,
            after: afterReorder,
            expected: permutation.map((from) => beforeReorder[from]),
        },
        { itemCount: SUBJECT_COUNT, orderFollowsPermutation: true },
    );

    const [wrongLength, outOfRange, notNumbers] = await Promise.all([
        invoke("playlist.reorder", { playlist: scratch, newOrder: [0, 1] }),
        invoke("playlist.reorder", {
            playlist: scratch,
            newOrder: [0, 1, 2, 3, 4, SUBJECT_COUNT],
        }),
        invoke("playlist.reorder", {
            playlist: scratch,
            newOrder: ["0", 1, 2, 3, 4, 5],
        }),
    ]);
    const unchanged = await trackPaths(bridge, scratch);
    recorder.assertCase(
        "PO-21 an unusable permutation is refused with what was expected, and the order is left alone",
        wrongLength?.success === false &&
            wrongLength?.code === "INVALID_PARAMS" &&
            wrongLength?.expected === SUBJECT_COUNT &&
            wrongLength?.got === 2 &&
            outOfRange?.success === false &&
            outOfRange?.code === "INVALID_INDEX" &&
            outOfRange?.index === SUBJECT_COUNT &&
            notNumbers?.success === false &&
            notNumbers?.code === "INVALID_PARAMS" &&
            JSON.stringify(unchanged) === JSON.stringify(afterReorder),
        {
            wrongLength,
            outOfRange,
            notNumbers: notNumbers?.error,
            orderUnchanged: JSON.stringify(unchanged) === JSON.stringify(afterReorder),
        },
        {
            wrongLength: "INVALID_PARAMS with expected and got",
            outOfRange: "INVALID_INDEX with index",
            notNumbers: "INVALID_PARAMS",
            orderUnchanged: true,
        },
    );

    const reorderEvents = await waitForMore(
        collector,
        "playlist:itemsReordered",
        reorderSeen,
        forPlaylist,
    );
    const lastReorder = reorderEvents[reorderEvents.length - 1];
    recorder.assertCase(
        "PO-22 reordering is announced with the number of rows involved",
        reorderEvents.length > reorderSeen &&
            sameSet(keysOf(lastReorder?.payload), ["playlist", "playlistGuid", "count"]) &&
            lastReorder?.payload?.count === SUBJECT_COUNT,
        {
            newEvents: reorderEvents.length - reorderSeen,
            payload: lastReorder?.payload,
        },
        { payloadKeys: ["count", "playlist", "playlistGuid"], count: SUBJECT_COUNT },
    );

    // undo/redo report whether a step was there to restore, which is the only
    // signal a caller gets: there is no "can undo" query.
    const beforeUndo = await trackPaths(bridge, scratch);
    const undone = await invoke("playlist.undo", { playlist: scratch });
    const afterUndo = await trackPaths(bridge, scratch);
    const redone = await invoke("playlist.redo", { playlist: scratch });
    const afterRedo = await trackPaths(bridge, scratch);
    recorder.assertCase(
        "PO-23 undo restores the order before the last change and redo puts it back",
        undone?.success === true &&
            JSON.stringify(afterUndo) !== JSON.stringify(beforeUndo) &&
            redone?.success === true &&
            JSON.stringify(afterRedo) === JSON.stringify(beforeUndo),
        {
            undo: undone,
            redo: redone,
            beforeUndo,
            afterUndo,
            afterRedo,
        },
        { undoChangedTheOrder: true, redoRestoredIt: true },
    );
}

async function runContentCases(bridge, recorder, collector, scratch, subjects) {
    const { invoke } = bridge;
    const forPlaylist = (payload) => payload?.playlist === scratch;
    const extra = subjects[0].absolutePath;

    const addedSeen = matching(collector, "playlist:itemsAdded", forPlaylist).length;
    const inserted = await invoke("playlist.insertTracks", {
        playlist: scratch,
        position: 1,
        handles: [extra],
    });
    const afterInsert = await trackPaths(bridge, scratch);
    recorder.assertCase(
        "PO-24 insertTracks puts the handles at the position it was given and counts what it did",
        inserted?.success === true &&
            inserted?.insertIndex === 1 &&
            inserted?.requestedCount === 1 &&
            inserted?.addedCount === 1 &&
            inserted?.invalidCount === 0 &&
            inserted?.countBefore === SUBJECT_COUNT &&
            inserted?.totalCount === SUBJECT_COUNT + 1 &&
            afterInsert[1] === extra,
        { response: inserted, rowAtPosition1: afterInsert[1] },
        { insertIndex: 1, addedCount: 1, totalCount: SUBJECT_COUNT + 1, rowAtPosition1: extra },
    );

    // The parameter is `handles`, not `paths`. A caller reaching for the spelling
    // every other add endpoint uses is told the key is unknown, and an empty list
    // is refused before anything is resolved.
    const [withPathsKey, empty] = await Promise.all([
        invoke("playlist.insertTracks", { playlist: scratch, position: 0, paths: [extra] }),
        invoke("playlist.addHandles", { playlist: scratch, handles: [] }),
    ]);
    recorder.assertCase(
        "PO-25 insertTracks refuses a paths key as unknown, and addHandles refuses an empty list, both with INVALID_PARAMS",
        withPathsKey?.success === false &&
            withPathsKey?.code === "INVALID_PARAMS" &&
            withPathsKey?.error === "unknown parameter 'paths'" &&
            empty?.success === false &&
            empty?.code === "INVALID_PARAMS" &&
            empty?.error === "handles must have at least 1 item(s)",
        {
            insertTracksWithPathsKey: withPathsKey,
            addHandlesWithEmptyList: empty,
        },
        {
            insertTracksWithPathsKey: { code: "INVALID_PARAMS", error: "unknown parameter 'paths'" },
            addHandlesWithEmptyList: { code: "INVALID_PARAMS", error: "handles must have at least 1 item(s)" },
        },
    );

    const appended = await invoke("playlist.addHandles", {
        playlist: scratch,
        handles: [extra],
    });
    const afterAppend = await trackPaths(bridge, scratch);
    recorder.assertCase(
        "PO-26 addHandles appends and reports the count before and after",
        appended?.success === true &&
            appended?.addedCount === 1 &&
            appended?.countBefore === SUBJECT_COUNT + 1 &&
            appended?.totalCount === SUBJECT_COUNT + 2 &&
            afterAppend[afterAppend.length - 1] === extra,
        { response: appended, lastRow: afterAppend[afterAppend.length - 1] },
        { countBefore: SUBJECT_COUNT + 1, totalCount: SUBJECT_COUNT + 2, lastRow: extra },
    );

    // Being in a playlist makes a path trusted for later media reads and writes, so a
    // handle given as a path string or as { path } has to pass the media read check.
    // A system directory is refused whether or not the file exists.
    const denied = `C:\\Windows\\e2e-no-such-${runId}.flac`;
    const [insertString, insertObject, appendString, appendObject] = await Promise.all([
        invoke("playlist.insertTracks", { playlist: scratch, position: 0, handles: [extra, denied] }),
        invoke("playlist.insertTracks", { playlist: scratch, position: 0, handles: [{ path: denied, subsong: 0 }] }),
        invoke("playlist.addHandles", { playlist: scratch, handles: [denied] }),
        invoke("playlist.addHandles", { playlist: scratch, handles: [extra, { path: denied }] }),
    ]);
    const afterDenied = await trackPaths(bridge, scratch);
    const refused = (res) => res?.success === false && res?.code === "PERMISSION_DENIED" && typeof res?.error === "string";
    recorder.assertCase(
        "PO-56 insertTracks and addHandles refuse a denied path given as a string or as { path } with PERMISSION_DENIED, and add none of the batch",
        refused(insertString) &&
            refused(insertObject) &&
            refused(appendString) &&
            refused(appendObject) &&
            afterDenied.length === SUBJECT_COUNT + 2,
        { insertString, insertObject, appendString, appendObject, rowCount: afterDenied.length },
        { each: { success: false, code: "PERMISSION_DENIED" }, rowCount: SUBJECT_COUNT + 2 },
    );

    const addedEvents = await waitForMore(
        collector,
        "playlist:itemsAdded",
        addedSeen,
        forPlaylist,
    );
    const lastAdded = addedEvents[addedEvents.length - 1];
    recorder.assertCase(
        "PO-27 an insertion is announced with where it landed and how many rows arrived",
        addedEvents.length > addedSeen &&
            sameSet(keysOf(lastAdded?.payload), ["playlist", "playlistGuid", "start", "count"]) &&
            lastAdded?.payload?.count === 1,
        { newEvents: addedEvents.length - addedSeen, payload: lastAdded?.payload },
        { payloadKeys: ["count", "playlist", "playlistGuid", "start"], count: 1 },
    );

    // Sequential adding exists so that the order of the given paths survives; the
    // reported positions are what a view needs to place the rows.
    const orderedPaths = [subjects[1].absolutePath, subjects[2].absolutePath, subjects[3].absolutePath];
    const countBefore = (await trackPaths(bridge, scratch)).length;
    const sequential = await invoke("playlist.addPathsSequential", {
        playlist: scratch,
        paths: orderedPaths,
    });
    const afterSequential = await trackPaths(bridge, scratch);
    recorder.assertCase(
        "PO-28 addPathsSequential keeps the given order and reports the position of each row",
        sequential?.success === true &&
            sequential?.addedCount === orderedPaths.length &&
            JSON.stringify(sequential?.order) ===
                JSON.stringify(orderedPaths.map((_, offset) => countBefore + offset)) &&
            JSON.stringify(afterSequential.slice(countBefore)) === JSON.stringify(orderedPaths),
        {
            response: sequential,
            tail: afterSequential.slice(countBefore),
            expectedTail: orderedPaths,
        },
        {
            addedCount: orderedPaths.length,
            order: orderedPaths.map((_, offset) => countBefore + offset),
        },
    );

    // The two add spellings differ in the one way a caller is most likely to
    // assume they agree: plain addPaths resolves the batch and lands the rows
    // in an order of its own, while the sequential spelling keeps the given
    // sequence. Asserted as a pair on the same input, because either half alone
    // reads like an incidental detail. The addPaths order is not asserted at
    // all: it changes from run to run and now and then happens to match the
    // order given, so only the set of rows is checked on that side.
    const orderProbe = [
        subjects[0].absolutePath,
        subjects[1].absolutePath,
        subjects[2].absolutePath,
        subjects[3].absolutePath,
        subjects[4].absolutePath,
        subjects[5].absolutePath,
    ];
    const batchList = await invoke("playlist.create", { name: `${names.scratch} batch` });
    const sequentialList = await invoke("playlist.create", {
        name: `${names.scratch} sequential`,
    });
    await invoke("playlist.addPaths", { playlist: batchList?.index, paths: orderProbe });
    await invoke("playlist.addPathsSequential", {
        playlist: sequentialList?.index,
        paths: orderProbe,
    });
    const batchOrder = await trackPaths(bridge, batchList?.index);
    const sequentialOrder = await trackPaths(bridge, sequentialList?.index);
    recorder.assertCase(
        "PO-28b addPaths lands the same rows in no promised order, which is what the sequential spelling exists to avoid",
        sameSet(batchOrder, orderProbe) &&
            JSON.stringify(sequentialOrder) === JSON.stringify(orderProbe),
        {
            given: orderProbe.map((path) => orderProbe.indexOf(path)),
            addPathsLandedAs: batchOrder.map((path) => orderProbe.indexOf(path)),
            addPathsSequentialLandedAs: sequentialOrder.map((path) => orderProbe.indexOf(path)),
        },
        {
            addPaths: "same rows, any order",
            addPathsSequential: "the order given",
        },
    );

    // The async spelling returns a receipt and finishes on an event, so the two
    // have to be matched by the operation id; without that a page cannot tell
    // whose add completed.
    const asyncPath = [subjects[4].absolutePath];
    const receipt = await invoke("playlist.addPathsAsync", {
        playlist: scratch,
        paths: asyncPath,
    });
    recorder.assertCase(
        "PO-29 addPathsAsync answers with a pending receipt carrying an operation id",
        receipt?.success === true &&
            receipt?.status === "pending" &&
            typeof receipt?.operationId === "string" &&
            receipt.operationId !== "" &&
            receipt?.totalCount === 1 &&
            receipt?.invalidCount === 0,
        { receipt },
        { status: "pending", operationId: "a non-empty id", totalCount: 1 },
    );

    await collector.waitFor(
        (events) =>
            events.some(
                (event) =>
                    event.name === "playlist:addComplete" &&
                    event.payload?.operationId === receipt?.operationId,
            ),
        { timeoutMs: eventTimeoutMs, pollMs: 50 },
    );
    const completion = matching(
        collector,
        "playlist:addComplete",
        (payload) => payload?.operationId === receipt?.operationId,
    )[0];
    recorder.assertCase(
        "PO-30 the completion arrives under the same operation id and playlist GUID as the receipt, with the counts it added",
        completion !== undefined &&
            sameSet(keysOf(completion.payload), [
                "operationId",
                "playlistGuid",
                "success",
                "addedCount",
                "totalCount",
            ]) &&
            GUID_SHAPE.test(receipt?.playlistGuid ?? "") &&
            completion.payload?.playlistGuid === receipt?.playlistGuid &&
            completion.payload?.success === true &&
            completion.payload?.addedCount === 1 &&
            completion.payload?.totalCount === 1,
        { payload: completion?.payload, receipt },
        {
            payloadKeys: ["addedCount", "operationId", "playlistGuid", "success", "totalCount"],
            playlistGuid: "the receipt's",
            addedCount: 1,
        },
    );

    const removedSeen = matching(collector, "playlist:itemsRemoved", forPlaylist).length;
    const beforeRemoval = await trackPaths(bridge, scratch);
    const removed = await invoke("playlist.removeTracks", { playlist: scratch, items: [0, 2] });
    const afterRemoval = await trackPaths(bridge, scratch);
    recorder.assertCase(
        "PO-31 removeTracks drops exactly the named indices and leaves the rest in order",
        removed?.success === true &&
            JSON.stringify(afterRemoval) ===
                JSON.stringify(beforeRemoval.filter((_, index) => index !== 0 && index !== 2)),
        { before: beforeRemoval.length, after: afterRemoval.length, response: removed },
        { after: beforeRemoval.length - 2, remainingRowsKeepOrder: true },
    );

    // Compared position by position rather than by membership: this run added the
    // same track more than once, so a path is still present after its row is
    // gone.
    await invoke("playlist.setSelection", { playlist: scratch, indices: [1] });
    const removedSelected = await invoke("playlist.removeSelectedTracks", { playlist: scratch });
    const afterSelectedRemoval = await trackPaths(bridge, scratch);
    const expectedAfterSelected = afterRemoval.filter((_, index) => index !== 1);
    recorder.assertCase(
        "PO-32 removeSelectedTracks drops the selected row, and nothing else",
        removedSelected?.success === true &&
            JSON.stringify(afterSelectedRemoval) === JSON.stringify(expectedAfterSelected),
        {
            removedRow: 1,
            before: afterRemoval.length,
            after: afterSelectedRemoval.length,
            expected: expectedAfterSelected.length,
        },
        { after: afterRemoval.length - 1, remainingRowsKeepOrder: true },
    );

    const removalEvents = await waitForMore(
        collector,
        "playlist:itemsRemoved",
        removedSeen,
        forPlaylist,
    );
    const lastRemoval = removalEvents[removalEvents.length - 1];
    recorder.assertCase(
        "PO-33 a removal is announced with the count transition rather than the indices",
        removalEvents.length > removedSeen &&
            sameSet(keysOf(lastRemoval?.payload), ["playlist", "playlistGuid", "oldCount", "newCount"]) &&
            lastRemoval?.payload?.newCount === afterSelectedRemoval.length,
        { newEvents: removalEvents.length - removedSeen, payload: lastRemoval?.payload },
        {
            payloadKeys: ["newCount", "oldCount", "playlist", "playlistGuid"],
            newCount: afterSelectedRemoval.length,
        },
    );
}

async function runPlaylistLevelCases(bridge, recorder, collector, scratch, subjects, names) {
    const { invoke } = bridge;

    const sourcePaths = await trackPaths(bridge, scratch);
    const duplicated = await invoke("playlist.duplicate", {
        playlist: scratch,
        name: names.copy,
    });
    const copyPaths = await trackPaths(bridge, duplicated?.index);
    recorder.assertCase(
        "PO-34 duplicate copies the rows into a new playlist and reports both indices",
        duplicated?.success === true &&
            duplicated?.sourcePlaylist === scratch &&
            duplicated?.newPlaylist === duplicated?.index &&
            duplicated?.name === names.copy &&
            duplicated?.trackCount === sourcePaths.length &&
            JSON.stringify(copyPaths) === JSON.stringify(sourcePaths),
        { response: duplicated, copyMatchesSource: JSON.stringify(copyPaths) === JSON.stringify(sourcePaths) },
        { trackCount: sourcePaths.length, copyMatchesSource: true },
    );

    const unnamed = await invoke("playlist.create", {});
    await invoke("playlist.rename", { playlist: unnamed?.index, name: names.unnamed });
    const renamed = await invoke("playlist.getAll", {});
    recorder.assertCase(
        "PO-35 create without a name still creates a playlist, under a default name",
        unnamed?.success === true &&
            typeof unnamed?.index === "number" &&
            renamed?.playlists?.[unnamed.index]?.name === names.unnamed,
        {
            response: unnamed,
            characterization: "the default name is New Playlist; renamed here so cleanup can find it",
        },
        { success: true, index: "a number" },
    );

    // Converting to an autoplaylist and back is the pair a consumer can drive.
    // The query it was created with is not readable afterwards, which the endpoint
    // says out loud rather than returning an empty string.
    const auto = await invoke("playlist.createAutoplaylist", {
        name: names.auto,
        query: "%genre% IS Game",
    });
    const autoInfo = await invoke("playlist.getAutoplaylistInfo", { playlist: auto?.index });
    const autoQuery = await invoke("playlist.getAutoplaylistQuery", { playlist: auto?.index });
    recorder.assertCase(
        "PO-36 createAutoplaylist echoes the query, while reading it back afterwards is not supported",
        auto?.success === true &&
            auto?.query === "%genre% IS Game" &&
            auto?.playlist === auto?.index &&
            autoInfo?.isAutoplaylist === true &&
            autoInfo?.source === "sdk" &&
            typeof autoInfo?.keepSorted === "boolean" &&
            autoQuery?.query === null &&
            typeof autoQuery?.note === "string",
        { created: auto, info: autoInfo, query: autoQuery },
        { isAutoplaylist: true, query: null, note: "a message saying the query is not exposed" },
    );

    const converted = await invoke("playlist.convertToAutoplaylist", {
        playlist: unnamed?.index,
        query: "ALL",
    });
    const convertedInfo = await invoke("playlist.isAutoplaylist", { playlist: unnamed?.index });
    const reverted = await invoke("playlist.removeAutoplaylist", { playlist: unnamed?.index });
    const revertedInfo = await invoke("playlist.isAutoplaylist", { playlist: unnamed?.index });
    const stillThere = await indexOfName(bridge, names.unnamed);
    recorder.assertCase(
        "PO-37 convertToAutoplaylist and removeAutoplaylist toggle the flag, and removing keeps the playlist",
        converted?.success === true &&
            convertedInfo?.isAutoplaylist === true &&
            reverted?.success === true &&
            revertedInfo?.isAutoplaylist === false &&
            stillThere !== undefined,
        {
            converted,
            afterConvert: convertedInfo?.isAutoplaylist,
            reverted,
            afterRevert: revertedInfo?.isAutoplaylist,
            playlistStillPresent: stillThere !== undefined,
        },
        { afterConvert: true, afterRevert: false, playlistStillPresent: true },
    );

    // replaceAllAndPlay is the atomic clear+add. Playback is kept out of it:
    // stopFirst false leaves whatever is playing alone and autoPlay false focuses
    // the row instead of starting it.
    const replacementPaths = subjects.slice(0, 3).map((track) => track.absolutePath);
    const replaced = await invoke("playlist.replaceAllAndPlay", {
        playlist: duplicated?.index,
        paths: replacementPaths,
        playIndex: 2,
        stopFirst: false,
        autoPlay: false,
    });
    const afterReplace = await trackPaths(bridge, duplicated?.index);
    const activeAfter = await invoke("playlist.getActive", {});
    const focusAfter = await invoke("playlist.getFocusedTrack", { playlist: duplicated?.index });
    recorder.assertCase(
        "PO-38 replaceAllAndPlay swaps the whole content, activates the playlist and focuses the play index",
        replaced?.success === true &&
            replaced?.clearedCount === sourcePaths.length &&
            replaced?.addedCount === replacementPaths.length &&
            replaced?.totalCount === replacementPaths.length &&
            replaced?.playIndex === 2 &&
            sameSet(afterReplace, replacementPaths) &&
            afterReplace.length === replacementPaths.length &&
            activeAfter?.index === duplicated?.index &&
            focusAfter?.index === 2,
        {
            response: replaced,
            content: afterReplace,
            activeIndex: activeAfter?.index,
            focusIndex: focusAfter?.index,
        },
        {
            clearedCount: sourcePaths.length,
            addedCount: replacementPaths.length,
            playIndex: 2,
            activeIndex: duplicated?.index,
            focusIndex: 2,
        },
    );

    // The rows land as a set, not in the order the paths were given - unlike
    // addPathsSequential, which exists for exactly that guarantee (PO-28). It
    // matters here because playIndex then points at whichever row happens to be
    // at that position: "replace with this list and play the third one" plays a
    // different track than the caller named. Recorded rather than asserted as a
    // contract, since the endpoint never promised an order.
    const orderPreserved = JSON.stringify(afterReplace) === JSON.stringify(replacementPaths);
    recorder.assertCase(
        "PO-39 the replacement order is characterized, so a change to it becomes visible",
        typeof orderPreserved === "boolean" && afterReplace.length === replacementPaths.length,
        {
            given: replacementPaths,
            landed: afterReplace,
            orderPreserved,
            rowAtPlayIndex: afterReplace[2],
            rowTheCallerNamed: replacementPaths[2],
        },
        { orderPreserved: "pinned as observed; addPathsSequential is the ordered spelling" },
    );

    // Reordering the playlist list needs a permutation of every playlist, so the
    // one that only swaps this run's own two playlists is used: a failure cannot
    // scramble the user's order.
    const before = await invoke("playlist.getAll", {});
    const beforeNames = (before?.playlists ?? []).map((entry) => entry?.name);
    const left = beforeNames.indexOf(names.copy);
    const right = beforeNames.indexOf(names.unnamed);
    const swap = [...Array(beforeNames.length).keys()];
    swap[left] = right;
    swap[right] = left;
    const reorderSeen = matching(collector, "playlist:reordered").length;
    const reorderedList = await invoke("playlist.reorderPlaylists", { newOrder: swap });
    const afterNames = ((await invoke("playlist.getAll", {}))?.playlists ?? []).map((entry) => entry?.name);
    const expectedNames = swap.map((from) => beforeNames[from]);
    recorder.assertCase(
        "PO-40 reorderPlaylists applies the permutation to the playlist list itself",
        reorderedList?.success === true &&
            reorderedList?.count === beforeNames.length &&
            JSON.stringify(afterNames) === JSON.stringify(expectedNames),
        {
            swappedPositions: [left, right],
            response: reorderedList,
            after: afterNames.slice(Math.min(left, right), Math.max(left, right) + 1),
            expected: expectedNames.slice(Math.min(left, right), Math.max(left, right) + 1),
        },
        { count: beforeNames.length, listFollowsPermutation: true },
    );

    const listEvents = await waitForMore(collector, "playlist:reordered", reorderSeen);
    const reorderedPayload = listEvents[listEvents.length - 1]?.payload;
    const guidsAfter = ((await invoke("playlist.getAll", {}))?.playlists ?? []).map((entry) => entry?.guid);
    recorder.assertCase(
        "PO-41 the playlist list reordering is announced with the number of playlists and every GUID in the new order",
        listEvents.length > reorderSeen &&
            sameSet(keysOf(reorderedPayload), ["count", "guids"]) &&
            reorderedPayload?.count === beforeNames.length &&
            JSON.stringify(reorderedPayload?.guids) === JSON.stringify(guidsAfter),
        { payload: reorderedPayload, guidsAfter },
        { payloadKeys: ["count", "guids"], count: beforeNames.length, guids: "getAll's order after the swap" },
    );

    const wrongLength = await invoke("playlist.reorderPlaylists", { newOrder: [0, 1] });
    const stillSwapped = ((await invoke("playlist.getAll", {}))?.playlists ?? []).map(
        (entry) => entry?.name,
    );
    recorder.assertCase(
        "PO-42 a permutation that is not the full list is refused, and the order stays as it was",
        wrongLength?.success === false &&
            wrongLength?.code === "INVALID_PARAMS" &&
            wrongLength?.expected === beforeNames.length &&
            wrongLength?.got === 2 &&
            JSON.stringify(stillSwapped) === JSON.stringify(afterNames),
        { response: wrongLength, orderUnchanged: JSON.stringify(stillSwapped) === JSON.stringify(afterNames) },
        { code: "INVALID_PARAMS", expected: beforeNames.length, got: 2, orderUnchanged: true },
    );

    // The GUID spelling names playlists, not positions, so the order read before the swap puts
    // the list back as it was. Both keys, and a GUID listed twice, are refused before anything moves.
    const beforeGuids = (before?.playlists ?? []).map((entry) => entry?.guid);
    const [bothKeys, twice] = await Promise.all([
        invoke("playlist.reorderPlaylists", { newOrder: swap, newOrderGuids: beforeGuids }),
        invoke("playlist.reorderPlaylists", {
            newOrderGuids: beforeGuids.map((guid, i) => (i === 1 ? beforeGuids[0] : guid)),
        }),
    ]);
    const restored = await invoke("playlist.reorderPlaylists", { newOrderGuids: beforeGuids });
    const restoredNames = ((await invoke("playlist.getAll", {}))?.playlists ?? []).map((entry) => entry?.name);
    recorder.assertCase(
        "PO-42b reorderPlaylists by GUID restores the order read before the swap, and refuses both keys or a GUID given twice",
        bothKeys?.success === false &&
            bothKeys?.code === "INVALID_PARAMS" &&
            twice?.success === false &&
            twice?.code === "INVALID_PARAMS" &&
            twice?.details?.index === 1 &&
            restored?.success === true &&
            JSON.stringify(restoredNames) === JSON.stringify(beforeNames),
        { bothKeys, twice, restored, restoredMatches: JSON.stringify(restoredNames) === JSON.stringify(beforeNames) },
        { bothKeys: "INVALID_PARAMS", twice: { code: "INVALID_PARAMS", index: 1 }, restored: true },
    );
}

async function runRefusalCases(bridge, recorder) {
    const { invoke } = bridge;
    const outOfRange = 99999;

    const [sorted, movedTracks, cleared, removedTracks, tracks, count, reversed, selected] =
        await Promise.all([
            invoke("playlist.sort", { playlist: outOfRange, pattern: "%title%" }),
            invoke("playlist.moveTracks", { playlist: outOfRange, items: [0], delta: 1 }),
            invoke("playlist.clear", { playlist: outOfRange }),
            invoke("playlist.removeTracks", { playlist: outOfRange, items: [0] }),
            invoke("playlist.getTracks", { playlist: outOfRange }),
            invoke("playlist.getTrackCount", { playlist: outOfRange }),
            invoke("playlist.reverse", { playlist: outOfRange }),
            invoke("playlist.selectAll", { playlist: outOfRange }),
        ]);
    recorder.assertCase(
        "PO-43 an out-of-range playlist is refused by the mutators but answered as an empty page by the readers",
        sorted?.success === false &&
            /invalid playlist index/i.test(sorted?.error ?? "") &&
            sorted?.code === "INVALID_INDEX" &&
            movedTracks?.success === false &&
            movedTracks?.code === "INVALID_INDEX" &&
            cleared?.success === false &&
            cleared?.code === "INVALID_INDEX" &&
            removedTracks?.success === false &&
            removedTracks?.code === "INVALID_INDEX" &&
            tracks?.success === true &&
            tracks?.total === 0 &&
            tracks?.playlist === outOfRange &&
            count?.success === true &&
            count?.count === 0,
        {
            mutators: {
                sort: sorted?.error,
                moveTracks: movedTracks?.error,
                clear: cleared?.error,
                removeTracks: removedTracks?.error,
            },
            readers: { getTracks: tracks, getTrackCount: count },
            characterization: "readers report an empty playlist instead of a bad index",
        },
        {
            mutators: "refused with INVALID_INDEX",
            readers: { getTracks: "an empty page with success", getTrackCount: "count 0 with success" },
        },
    );

    // These two used to answer a bare false with no message at all; they now
    // refuse the way sort does.
    recorder.assertCase(
        "PO-43b reverse and selectAll refuse a bad index with INVALID_INDEX and the same message as sort",
        [reversed, selected].every(
            (response) =>
                response?.success === false &&
                /invalid playlist index/i.test(response?.error ?? "") &&
                response?.code === "INVALID_INDEX",
        ),
        {
            reverse: reversed,
            selectAll: selected,
            comparedWith: { sort: sorted?.error },
        },
        { both: "INVALID_INDEX" },
    );
}

/**
 * Playlist identity: the `guid` the playlist answers report, and `playlistGuid`, which names
 * a playlist in place of its index. Two playlists of the same name stand for the case a
 * lookup by name and rank gets wrong; swapping them and removing a spacer in front of them
 * move the target away from the index it had when its GUID was taken, as can happen while a
 * menu that listed the playlists is open.
 */
async function runIdentityCases(bridge, recorder, subjects, names) {
    const { invoke } = bridge;
    const guidShape = /^\{[0-9A-F]{8}-[0-9A-F]{4}-[0-9A-F]{4}-[0-9A-F]{4}-[0-9A-F]{12}\}$/;
    const playlists = async () => (await invoke("playlist.getAll", {}))?.playlists ?? [];
    const rowOf = async (guid) => (await playlists()).find((entry) => entry?.guid === guid);
    const countOf = async (params) => (await invoke("playlist.getTrackCount", params))?.count;
    const path = subjects[0].absolutePath;

    const spacer = await invoke("playlist.create", { name: names.spacer });
    const first = await invoke("playlist.create", { name: names.twin });
    const second = await invoke("playlist.create", { name: names.twin });
    const rows = await playlists();
    const malformed = rows.filter((row) => !guidShape.test(row?.guid ?? "")).map((row) => row?.guid);
    recorder.assertCase(
        "PO-57 every playlist row carries a GUID, create reports the same one, and two playlists of the same name get different GUIDs",
        rows.length > 0 &&
            malformed.length === 0 &&
            new Set(rows.map((row) => row.guid)).size === rows.length &&
            rows[first?.index]?.guid === first?.guid &&
            rows[second?.index]?.guid === second?.guid &&
            first?.guid !== second?.guid,
        { created: [spacer, first, second], malformed },
        { shape: "{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}", unique: true, twinsDiffer: true },
    );

    const order = [...rows.keys()];
    order[first.index] = second.index;
    order[second.index] = first.index;
    const swapped = await invoke("playlist.reorderPlaylists", { newOrder: order });
    const removedSpacer = await invoke("playlist.remove", { playlistGuid: spacer?.guid });
    const moved = await rowOf(second?.guid);
    const firstBefore = await countOf({ playlistGuid: first?.guid });
    const added = await invoke("library.addToPlaylist", { paths: [path], playlistGuid: second?.guid });
    const secondAfter = await rowOf(second?.guid);
    const firstAfter = await countOf({ playlistGuid: first?.guid });
    const art = await invoke("artwork.getByPlaylistItem", { playlistGuid: second?.guid, index: 0 });
    recorder.assertCase(
        "PO-58 library.addToPlaylist by GUID lands in that playlist after the twins swapped places and a playlist in front of them was removed",
        swapped?.success === true &&
            removedSpacer?.success === true &&
            moved !== undefined &&
            moved.index !== second.index &&
            added?.success === true &&
            added?.added === 1 &&
            secondAfter?.trackCount === 1 &&
            firstAfter === firstBefore &&
            art?.playlist === secondAfter?.index &&
            art?.playlistGuid === second?.guid &&
            art?.index === 0,
        {
            indexWhenTaken: second?.index,
            indexNow: moved?.index,
            added,
            target: secondAfter,
            twinCount: [firstBefore, firstAfter],
            artwork: art && {
                success: art.success,
                playlist: art.playlist,
                playlistGuid: art.playlistGuid,
                index: art.index,
                code: art.code,
            },
        },
        { lands: "in the playlist the GUID names", twinUntouched: true },
    );

    const renamed = await invoke("playlist.rename", { playlistGuid: first?.guid, name: names.renamed });
    const activated = await invoke("playlist.setActive", { playlistGuid: first?.guid });
    const active = await invoke("playlist.getActive", {});
    recorder.assertCase(
        "PO-59 rename and setActive take a GUID in place of the index, and getActive reports it back",
        renamed?.success === true &&
            activated?.success === true &&
            active?.guid === first?.guid &&
            active?.name === names.renamed,
        { renamed, activated, active: active && { guid: active.guid, name: active.name } },
        { guid: first?.guid, name: names.renamed },
    );

    // The removed twin's GUID must not reach the other twin, which is renamed back to the
    // shared name first, nor the active playlist.
    await invoke("playlist.rename", { playlistGuid: first?.guid, name: names.twin });
    const removedTarget = await invoke("playlist.remove", { playlistGuid: second?.guid });
    const survivorBefore = await countOf({ playlistGuid: first?.guid });
    const gone = await invoke("library.addToPlaylist", { paths: [path], playlistGuid: second?.guid });
    const goneRead = await invoke("playlist.getTrackCount", { playlistGuid: second?.guid });
    // A page asking by GUID tells "removed" from "empty" by playlist -1, not by total 0.
    const gonePage = await invoke("playlist.getTracks", { playlistGuid: second?.guid, start: 3 });
    const survivorAfter = await countOf({ playlistGuid: first?.guid });
    recorder.assertCase(
        "PO-60 a removed playlist's GUID fails with NOT_FOUND, reaches no other playlist of the same name, and reads as a missing playlist",
        removedTarget?.success === true &&
            gone?.success === false &&
            gone?.code === "NOT_FOUND" &&
            gone?.details?.playlistGuid === second?.guid &&
            survivorAfter === survivorBefore &&
            goneRead?.success === true &&
            goneRead?.count === 0 &&
            gonePage?.success === true &&
            gonePage?.playlist === -1 &&
            !("playlistGuid" in gonePage) &&
            gonePage?.start === 3 &&
            gonePage?.count === 0 &&
            gonePage?.total === 0 &&
            Array.isArray(gonePage?.tracks) &&
            gonePage.tracks.length === 0,
        { gone, goneRead, gonePage, survivor: [survivorBefore, survivorAfter] },
        {
            code: "NOT_FOUND",
            survivorUntouched: true,
            read: { count: 0 },
            page: { playlist: -1, playlistGuid: "absent", start: 3, count: 0, total: 0, tracks: [] },
        },
    );

    const [both, badGuid, neither, badRead, queueBoth] = await Promise.all([
        invoke("library.addToPlaylist", { paths: [path], playlist: 0, playlistGuid: first?.guid }),
        invoke("playlist.addHandles", { playlistGuid: "not-a-guid", handles: [path] }),
        invoke("playlist.setActive", {}),
        invoke("playlist.getTracks", { playlistGuid: "not-a-guid" }),
        invoke("queue.add", { playlist: 0, playlistGuid: first?.guid, track: 0 }),
    ]);
    const survivorLast = await countOf({ playlistGuid: first?.guid });
    recorder.assertCase(
        "PO-61 both keys, a malformed GUID, and neither key where one is required are refused with INVALID_PARAMS, readers included",
        [both, badGuid, neither, badRead, queueBoth].every(
            (response) => response?.success === false && response?.code === "INVALID_PARAMS",
        ) && survivorLast === survivorAfter,
        {
            libraryBoth: both?.error,
            addHandlesMalformed: badGuid?.error,
            setActiveNeither: neither?.error,
            getTracksMalformed: badRead?.error,
            queueAddBoth: queueBoth?.error,
            survivor: [survivorAfter, survivorLast],
        },
        { every: "INVALID_PARAMS", survivorUntouched: true },
    );

    // Every answer that names a playlist by index names it by GUID too, so a caller can hold on
    // to the list it acted on. Run one after another: the writers change what the readers see.
    const target = { playlistGuid: first?.guid };
    const answers = {};
    for (const [method, params] of [
        ["playlist.getTracks", { ...target, count: 1 }],
        ["playlist.getTracksAt", { ...target, rows: [0] }],
        ["playlist.getMatchingRows", { ...target, query: "ALL" }],
        ["playlist.getSelection", target],
        ["playlist.getSelectedTracks", target],
        ["playlist.getFocusTrack", target],
        ["playlist.getFocusedTrack", target],
        ["playlist.getLockInfo", target],
        ["playlist.isAutoplaylist", target],
        ["playlist.addHandles", { ...target, handles: [path] }],
        ["playlist.insertTracks", { ...target, position: 0, handles: [path] }],
        ["playlist.addPathsSequential", { ...target, paths: [path] }],
    ]) {
        answers[method] = await invoke(method, params);
    }
    // reorder needs every row exactly once, and how many rows the adds above left is theirs to say.
    const rowCount = (await countOf(target)) ?? 0;
    answers["playlist.reorder"] = await invoke("playlist.reorder", {
        ...target,
        newOrder: [...Array(rowCount).keys()].reverse(),
    });
    answers["playlist.clear"] = await invoke("playlist.clear", target);
    const copy = await invoke("playlist.duplicate", { ...target, name: `${names.twin} copy` });
    const misnamed = Object.entries(answers)
        .filter(([, answer]) => answer?.playlistGuid !== first?.guid)
        .map(([method, answer]) => ({
            method,
            success: answer?.success,
            playlistGuid: answer?.playlistGuid,
            error: answer?.error,
        }));
    recorder.assertCase(
        "PO-61b results that name a playlist by index also carry its GUID, and duplicate names both the source and the copy",
        typeof first?.guid === "string" &&
            misnamed.length === 0 &&
            copy?.success === true &&
            copy.sourcePlaylistGuid === first.guid &&
            typeof copy.guid === "string" &&
            copy.guid !== first.guid &&
            (await rowOf(copy.guid))?.index === copy.index,
        {
            misnamed,
            duplicate: copy && {
                sourcePlaylistGuid: copy.sourcePlaylistGuid,
                guid: copy.guid,
                index: copy.index,
            },
        },
        { playlistGuid: first?.guid, duplicate: "source GUID plus a new GUID at the new index" },
    );
}

/**
 * addPathsAsync expands a playlist file after the call has answered, so the target can move or
 * go away before the tracks are inserted. The call and the change are posted from one page
 * script, back to back, so the change reaches the host before the expansion finishes. Where
 * the tracks went, and the playlist:itemsAdded index, show whether it did; a run where the
 * expansion won fails under a "not exercised" name instead of passing.
 */
async function runMovedTargetCases(bridge, recorder, collector, names, playlistFile) {
    const { invoke } = bridge;
    const countOf = async (guid) => (await invoke("playlist.getTrackCount", { playlistGuid: guid }))?.count;
    const expected = 2;
    const both = (first, second) =>
        bridge.evaluateValue(
            `Promise.all([window.fb2k.invoke(${JSON.stringify(first[0])}, ${JSON.stringify(first[1])}),
                          window.fb2k.invoke(${JSON.stringify(second[0])}, ${JSON.stringify(second[1])})])`,
        );
    const completionOf = async (receipt) => {
        await collector.waitFor(
            (events) =>
                events.some(
                    (event) =>
                        event.name === "playlist:addComplete" &&
                        event.payload?.operationId === receipt?.operationId,
                ),
            { timeoutMs: eventTimeoutMs, pollMs: 50 },
        );
        return matching(collector, "playlist:addComplete", (p) => p?.operationId === receipt?.operationId)[0]
            ?.payload;
    };

    const target = await invoke("playlist.create", { name: names.movedTarget });
    const other = await invoke("playlist.create", { name: names.movedOther });
    const rows = (await invoke("playlist.getAll", {}))?.playlists ?? [];
    const order = [...rows.keys()];
    order[target.index] = other.index;
    order[other.index] = target.index;
    // Drain first: events of the earlier cases still waiting in the page would count as ours.
    let from = (await collector.drain()).length;
    const [receipt, swapped] = await both(
        ["playlist.addPathsAsync", { playlist: target.index, paths: [playlistFile] }],
        ["playlist.reorderPlaylists", { newOrder: order }],
    );
    let completion = await completionOf(receipt);
    let added = collector.received.slice(from).filter((event) => event.name === "playlist:itemsAdded");
    const targetNow = (await invoke("playlist.getAll", {}))?.playlists?.findIndex((row) => row?.guid === target.guid);
    const counts = { target: await countOf(target.guid), other: await countOf(other.guid) };
    // Tracks in the target, inserted at its old index: the expansion beat the move.
    const moveLost =
        counts.target === expected && added.length === 1 && added[0].payload?.playlist === target.index;
    recorder.assertCase(
        moveLost
            ? "PO-62 addPathsAsync with the target moving during the expansion: not exercised, the expansion finished first"
            : "PO-62 tracks expanded by addPathsAsync go into its target after the target moved during the expansion",
        receipt?.success === true &&
            swapped?.success === true &&
            completion?.addedCount === expected &&
            counts.target === expected &&
            counts.other === 0 &&
            added.length === 1 &&
            added[0].payload?.playlist === targetNow &&
            targetNow !== target.index,
        { receipt, swapped, completion, itemsAdded: added.map((event) => event.payload), indexNow: targetNow, counts },
        { addedCount: expected, counts: { target: expected, other: 0 }, itemsAddedAt: "the new index" },
    );

    const doomed = await invoke("playlist.create", { name: names.removedTarget });
    const next = await invoke("playlist.create", { name: names.removedNext });
    from = (await collector.drain()).length;
    const [lateReceipt, removed] = await both(
        ["playlist.addPathsAsync", { playlist: doomed.index, paths: [playlistFile] }],
        ["playlist.remove", { playlistGuid: doomed.guid }],
    );
    completion = await completionOf(lateReceipt);
    added = collector.received.slice(from).filter((event) => event.name === "playlist:itemsAdded");
    const nextCount = await countOf(next.guid);
    // Tracks counted as added while the next playlist got none: they went into the target
    // before it was removed, so the expansion beat the removal.
    const removalLost = completion?.addedCount === expected && nextCount === 0;
    recorder.assertCase(
        removalLost
            ? "PO-63 addPathsAsync with the target removed during the expansion: not exercised, the expansion finished first"
            : "PO-63 tracks expanded by addPathsAsync after its target was removed are dropped, not written into the playlist that took its index",
        lateReceipt?.success === true &&
            removed?.success === true &&
            completion?.success === true &&
            completion?.addedCount === 0 &&
            nextCount === 0 &&
            added.length === 0,
        { receipt: lateReceipt, removed, completion, itemsAdded: added.map((event) => event.payload), nextCount },
        { addedCount: 0, nextCount: 0, itemsAdded: "none" },
    );
}

/** getTracks without `fields`: the keys every whole row carries. */
const BASELINE_FIELDS = [
    "index", "handle", "title", "artist", "artists", "album", "albumArtist", "albumArtists",
    "genre", "date",
    "trackNumber", "discNumber", "duration", "path", "absolutePath", "fileSize",
    "subsong", "rating", "codec", "bitrate", "sampleRate", "channels",
    "composer", "comment",
];

/** Whole getTracks rows only, and each only when foo_playcount gives a value. */
const PLAYCOUNT_FIELDS = ["playCount", "firstPlayed", "lastPlayed", "added"];

/** Runs tile a range when they start at its head, abut, and their counts fill it. */
function tilesRange(runs, start, total) {
    let cursor = start;
    for (const run of runs ?? []) {
        if (run?.start !== cursor || !(run?.count > 0)) return false;
        cursor += run.count;
    }
    return cursor === start + total;
}

/** Mirrors the host's ASCII-only case folding, so expectations track the same rule. */
function foldAscii(value) {
    return String(value ?? "").replace(/[A-Z]/g, (letter) => letter.toLowerCase());
}

/**
 * The windowing primitives: the `fields` projection on getTracks, and getGroupRuns.
 *
 * The key-set assertions live here because they have nowhere else to live. The
 * native playlist handler test reimplements the handler over a mock that returns a
 * canned rows payload, and the wire-snapshot test project is fb2k-free, so neither
 * reaches the playlist row builder or the projection branch. This suite is their only
 * automated defence.
 *
 * Every case builds its own playlist instead of reusing the shared scratch one,
 * whose contents and order the sections above deliberately churn.
 */
async function runWindowingCases(bridge, recorder, subjects, names) {
    const { invoke } = bridge;

    const created = await invoke("playlist.create", { name: names.windowing });
    const list = created?.index;
    await invoke("playlist.addPathsSequential", {
        playlist: list,
        paths: subjects.map((track) => track.absolutePath),
    });

    const defaultPage = await invoke("playlist.getTracks", { playlist: list });
    recorder.assertCase(
        "PO-44 getTracks without fields returns the whole row on every row, the play statistics only where they exist",
        (defaultPage?.tracks ?? []).length === SUBJECT_COUNT &&
            (defaultPage?.tracks ?? []).every(
                (row) =>
                    BASELINE_FIELDS.every((field) => field in row) &&
                    keysOf(row).every(
                        (key) => BASELINE_FIELDS.includes(key) || PLAYCOUNT_FIELDS.includes(key),
                    ) &&
                    (row.playCount === undefined || Number.isInteger(row.playCount)),
            ),
        {
            rows: (defaultPage?.tracks ?? []).length,
            firstRowKeys: keysOf((defaultPage?.tracks ?? [])[0]),
        },
        { rows: SUBJECT_COUNT, keys: [...BASELINE_FIELDS].sort(), optional: PLAYCOUNT_FIELDS },
    );

    const projected = await invoke("playlist.getTracks", {
        playlist: list,
        fields: ["title", "album"],
    });
    recorder.assertCase(
        "PO-45 a projected row carries the requested fields plus index and nothing else, the play-count columns included",
        (projected?.tracks ?? []).length === SUBJECT_COUNT &&
            (projected?.tracks ?? []).every((row) =>
                sameSet(keysOf(row), ["index", "title", "album"]),
            ),
        {
            requested: ["title", "album"],
            firstRowKeys: keysOf((projected?.tracks ?? [])[0]),
            playCountKeysPresent: (projected?.tracks ?? []).some((row) =>
                PLAYCOUNT_FIELDS.some((field) => field in (row ?? {})),
            ),
        },
        { keys: ["album", "index", "title"], playCountKeysPresent: false },
    );

    const projectedWithFormats = await invoke("playlist.getTracks", {
        playlist: list,
        count: 1,
        fields: ["title"],
        formats: { plays: "%play_count%" },
    });
    const formatsRow = (projectedWithFormats?.tracks ?? [])[0];
    recorder.assertCase(
        "PO-45b formats columns survive the projection and arrive under formats, by their own names",
        sameSet(keysOf(formatsRow), ["index", "title", "formats"]) &&
            sameSet(keysOf(formatsRow?.formats), ["plays"]) &&
            typeof formatsRow?.formats?.plays === "string",
        { keys: keysOf(formatsRow), formats: formatsRow?.formats },
        { keys: ["formats", "index", "title"], formats: { plays: "a string" } },
    );

    // artists and artist are two shapes of one tag, and a view that reads playlist
    // rows through a validator written for library rows depends on artists staying
    // a list.
    const artistRows = await invoke("playlist.getTracks", {
        playlist: list,
        fields: ["artist", "artists"],
    });
    recorder.assertCase(
        "PO-46 artists projects as an array of strings, not as the joined artist string",
        (artistRows?.tracks ?? []).length === SUBJECT_COUNT &&
            (artistRows?.tracks ?? []).every(
                (row) =>
                    Array.isArray(row?.artists) &&
                    row.artists.every((entry) => typeof entry === "string") &&
                    typeof row?.artist === "string",
            ),
        {
            firstRow: (artistRows?.tracks ?? [])[0],
            artistsIsArray: Array.isArray((artistRows?.tracks ?? [])[0]?.artists),
        },
        { artists: "array of strings", artist: "string" },
    );

    // composer and comment ship in the default row but are absent from the shared
    // field whitelist, so they cannot be requested. Pinned as the known gap it is.
    const unknownFields = await invoke("playlist.getTracks", {
        playlist: list,
        fields: ["composer", "comment", "bogus"],
    });
    const defaultRow = (defaultPage?.tracks ?? [])[0] ?? {};
    recorder.assertCase(
        "PO-47 composer and comment ship in the default row yet are refused as projection fields",
        unknownFields?.success === false &&
            unknownFields?.code === "INVALID_PARAMS" &&
            sameSet(unknownFields?.details?.unknownFields ?? [], ["composer", "comment", "bogus"]) &&
            ["composer", "comment"].every((field) => field in defaultRow),
        {
            response: unknownFields,
            inDefaultRow: ["composer", "comment"].every((field) => field in defaultRow),
        },
        { unknownFields: ["bogus", "comment", "composer"], inDefaultRow: true },
    );

    const [nullFields, notArray, emptyArray, nonString] = await Promise.all([
        invoke("playlist.getTracks", { playlist: list, fields: null }),
        invoke("playlist.getTracks", { playlist: list, fields: "title" }),
        invoke("playlist.getTracks", { playlist: list, fields: [] }),
        invoke("playlist.getTracks", { playlist: list, fields: [1] }),
    ]);
    // null is the same as leaving fields out, as with every declared parameter.
    const malformed = [notArray, emptyArray, nonString];
    recorder.assertCase(
        "PO-48 every malformed fields argument is refused with INVALID_PARAMS rather than served as a page, and null reads as every field",
        malformed.every(
            (response) =>
                response?.success === false &&
                response?.code === "INVALID_PARAMS" &&
                response?.tracks === undefined,
        ) &&
            nullFields?.success === true &&
            (nullFields?.tracks ?? []).length === SUBJECT_COUNT &&
            (nullFields?.tracks ?? []).every((row) =>
                BASELINE_FIELDS.every((field) => field in row),
            ),
        {
            null: { success: nullFields?.success, keys: keysOf((nullFields?.tracks ?? [])[0]) },
            notAnArray: notArray?.error,
            empty: emptyArray?.error,
            nonString: nonString?.error,
        },
        { null: "whole rows", others: "success false, code INVALID_PARAMS, no tracks" },
    );

    // PO-43 pins what a bad index means to a reader: an empty page and no error.
    // With fields present the field check runs first, so the same index reports the
    // field problem instead of silently serving nothing.
    const badIndexBadFields = await invoke("playlist.getTracks", {
        playlist: 99999,
        fields: ["bogus"],
    });
    recorder.assertCase(
        "PO-49 fields are validated before the playlist index, so a bad index with a bad field reports the field",
        badIndexBadFields?.success === false &&
            badIndexBadFields?.code === "INVALID_PARAMS" &&
            sameSet(badIndexBadFields?.details?.unknownFields ?? [], ["bogus"]),
        {
            response: badIndexBadFields,
            comparedWith: "PO-43, where the same index returns an empty page",
        },
        { code: "INVALID_PARAMS", unknownFields: ["bogus"] },
    );

    // The invalid-handle branch of the playlist row builder is not reachable from the
    // bridge: a path that resolves to nothing still yields a valid handle. This is
    // the closest reachable form, and it carries the same obligation - the
    // projection must not thin the row down for an entry that has no metadata.
    const unresolvableCreated = await invoke("playlist.create", { name: names.unresolvable });
    const unresolvableList = unresolvableCreated?.index;
    await invoke("playlist.addPathsSequential", {
        playlist: unresolvableList,
        paths: [`E:\\__fb2k_e2e_no_such_file_${runId}__.flac`],
    });
    const unresolvedProjected = await invoke("playlist.getTracks", {
        playlist: unresolvableList,
        fields: ["title", "album", "duration", "rating"],
    });
    const unresolvedRow = (unresolvedProjected?.tracks ?? [])[0];
    recorder.assertCase(
        "PO-50 an entry whose file resolves to nothing still projects every requested field, at its type default",
        sameSet(keysOf(unresolvedRow), ["index", "title", "album", "duration", "rating"]) &&
            unresolvedRow?.title === "" &&
            unresolvedRow?.album === "" &&
            unresolvedRow?.duration === 0 &&
            unresolvedRow?.rating === 0,
        { row: unresolvedRow },
        { keys: ["album", "duration", "index", "rating", "title"], values: "type defaults" },
    );

    const [runsByAlbum, trackCount] = await Promise.all([
        invoke("playlist.getGroupRuns", { playlist: list, patterns: ["%album%"] }),
        invoke("playlist.getTrackCount", { playlist: list }),
    ]);
    recorder.assertCase(
        "PO-51 getGroupRuns tiles the whole playlist: runs start at zero, abut, and sum to total",
        runsByAlbum?.success === true &&
            runsByAlbum?.playlist === list &&
            runsByAlbum?.total === trackCount?.count &&
            tilesRange(runsByAlbum?.runs, 0, runsByAlbum?.total),
        { total: runsByAlbum?.total, trackCount: trackCount?.count, runs: runsByAlbum?.runs },
        { total: trackCount?.count, tiles: true },
    );

    // Titles are distinct by construction, so each row is its own parent run and
    // every parent after the first starts at a non-zero index - which is what lets
    // this tell an absolute sub start from a parent-relative one.
    const folded = subjects.map((track) => foldAscii(track.title));
    const expectedParents = folded.filter(
        (key, index) => index === 0 || key !== folded[index - 1],
    ).length;
    const twoLevel = await invoke("playlist.getGroupRuns", {
        playlist: list,
        patterns: ["%title%", "$if2(%discnumber%,none)"],
    });
    const parents = twoLevel?.runs ?? [];
    const second = parents[1];
    recorder.assertCase(
        "PO-52 sub-runs tile their parent and their start is an absolute row index, not an offset inside the parent",
        parents.length === expectedParents &&
            expectedParents >= 2 &&
            second?.start > 0 &&
            second?.sub?.[0]?.start === second?.start &&
            parents.every((parent) => tilesRange(parent?.sub, parent?.start, parent?.count)),
        {
            parents: parents.length,
            secondParentStart: second?.start,
            secondParentFirstSubStart: second?.sub?.[0]?.start,
        },
        { parents: expectedParents, secondParentFirstSubStart: second?.start },
    );

    const blankCreated = await invoke("playlist.create", { name: names.emptyList });
    const blankRuns = await invoke("playlist.getGroupRuns", {
        playlist: blankCreated?.index,
        patterns: ["%album%"],
    });
    recorder.assertCase(
        "PO-53 an empty playlist answers with no runs rather than with an error",
        blankRuns?.success === true &&
            blankRuns?.total === 0 &&
            Array.isArray(blankRuns?.runs) &&
            blankRuns.runs.length === 0,
        { response: blankRuns },
        { success: true, total: 0, runs: [] },
    );

    const [
        noPatterns,
        patternsNotArray,
        patternsEmpty,
        patternsThree,
        patternsNonString,
        patternsBlank,
        patternsBadIndex,
    ] = await Promise.all([
        invoke("playlist.getGroupRuns", { playlist: list }),
        invoke("playlist.getGroupRuns", { playlist: list, patterns: "%album%" }),
        invoke("playlist.getGroupRuns", { playlist: list, patterns: [] }),
        invoke("playlist.getGroupRuns", { playlist: list, patterns: ["a", "b", "c"] }),
        invoke("playlist.getGroupRuns", { playlist: list, patterns: ["%album%", 7] }),
        invoke("playlist.getGroupRuns", { playlist: list, patterns: ["%album%", ""] }),
        invoke("playlist.getGroupRuns", { playlist: 99999, patterns: ["%album%"] }),
    ]);
    // A non-string entry is a type error the declared reader catches, so its message
    // names the entry; details.pattern is left to the checks behind the reader.
    const refusals = [
        noPatterns,
        patternsNotArray,
        patternsEmpty,
        patternsThree,
        patternsNonString,
        patternsBlank,
    ];
    recorder.assertCase(
        "PO-54 getGroupRuns refuses every malformed patterns argument, naming the offending entry where there is one",
        refusals.every(
            (response) => response?.success === false && response?.code === "INVALID_PARAMS",
        ) &&
            patternsNonString?.error === "patterns[1] must be a string" &&
            patternsBlank?.details?.pattern === 1 &&
            noPatterns?.details === undefined &&
            patternsBadIndex?.success === false &&
            patternsBadIndex?.code === "INVALID_INDEX" &&
            patternsBadIndex?.details?.playlist === 99999,
        {
            missing: noPatterns,
            notAnArray: patternsNotArray?.error,
            empty: patternsEmpty?.error,
            threeOfThem: patternsThree?.error,
            nonString: patternsNonString?.error,
            blankAt: patternsBlank?.details?.pattern,
            badIndex: patternsBadIndex,
        },
        {
            malformed: "success false, code INVALID_PARAMS",
            nonString: "patterns[1] must be a string",
            blankAt: 1,
            badIndex: "INVALID_INDEX with details.playlist, unlike getTracks which serves an empty page",
        },
    );

    // The endpoint also has a compile-failure refusal, and nothing here reaches it:
    // the host's title formatting compiler accepted every malformed spelling tried,
    // unterminated fields and unclosed calls included, so a caller who mistypes a
    // pattern gets rows grouped by something they did not mean rather than an error.
    // Pinned as the characterization it is - if a future host build starts rejecting
    // any of these, this case turns red and the refusal path becomes testable.
    const malformedSpecs = ["%album", "$if(", "[", "$unknownfunc(a)", "'", "$"];
    const compiled = await Promise.all(
        malformedSpecs.map((spec) =>
            invoke("playlist.getGroupRuns", { playlist: list, patterns: [spec] }),
        ),
    );
    recorder.assertCase(
        "PO-54b a malformed title formatting pattern compiles and groups, so the compile-failure refusal has no reachable input",
        compiled.every((response) => response?.success === true && Array.isArray(response?.runs)),
        {
            specs: malformedSpecs,
            runCounts: compiled.map((response) => response?.runs?.length),
            refusedAny: compiled.some((response) => response?.success === false),
        },
        { all: "success true", refusedAny: false },
    );

    // total is the weak version a windowing client compares the two responses on.
    // Adding a row is the only lever that moves it: reorder and replace leave it
    // where it was, which would make this assertion hold for the wrong reason.
    const [runsBefore, pageBefore] = await Promise.all([
        invoke("playlist.getGroupRuns", { playlist: list, patterns: ["%album%"] }),
        invoke("playlist.getTracks", { playlist: list, count: 1, fields: ["index"] }),
    ]);
    await invoke("playlist.addPathsSequential", {
        playlist: list,
        paths: [subjects[0].absolutePath],
    });
    const [runsAfter, pageAfter] = await Promise.all([
        invoke("playlist.getGroupRuns", { playlist: list, patterns: ["%album%"] }),
        invoke("playlist.getTracks", { playlist: list, count: 1, fields: ["index"] }),
    ]);
    recorder.assertCase(
        "PO-55 the two endpoints report the same total, and both follow the playlist when a row is added",
        runsBefore?.total === pageBefore?.total &&
            runsAfter?.total === pageAfter?.total &&
            runsAfter?.total === runsBefore?.total + 1,
        {
            before: { runs: runsBefore?.total, page: pageBefore?.total },
            after: { runs: runsAfter?.total, page: pageAfter?.total },
        },
        { equalBefore: true, equalAfter: true, grewBy: 1 },
    );

    // The list now holds the subjects and subjects[0] once more at the end. A path holds no `*`
    // or `?` on Windows and IS matches uppercase letters only in the same case, so the path as
    // the host reports it names exactly the two rows of that track.
    const repeated = subjects[0];
    const lastRow = SUBJECT_COUNT;
    const listing = await invoke("playlist.getTracks", {
        playlistGuid: created?.guid,
        count: 500,
        fields: ["absolutePath"],
    });
    const expectedRows = (listing?.tracks ?? [])
        .filter((row) => row.absolutePath === repeated.absolutePath)
        .map((row) => row.index);
    const matched = await invoke("playlist.getMatchingRows", {
        playlistGuid: created?.guid,
        query: `%path% IS "${repeated.absolutePath}"`,
    });
    const picked = await invoke("playlist.getTracksAt", {
        playlistGuid: created?.guid,
        rows: [lastRow, 0, 99999, lastRow],
        fields: ["title"],
    });
    recorder.assertCase(
        "PO-55c getMatchingRows finds every row of one track in playlist order, and getTracksAt reads rows in the order given, skipping one past the end",
        matched?.success === true &&
            matched?.playlist === list &&
            matched?.playlistGuid === created?.guid &&
            matched?.total === listing?.total &&
            expectedRows.length >= 2 &&
            JSON.stringify(matched?.items) === JSON.stringify(expectedRows) &&
            matched?.count === expectedRows.length &&
            picked?.success === true &&
            picked?.total === SUBJECT_COUNT + 1 &&
            JSON.stringify((picked?.tracks ?? []).map((row) => row.index)) ===
                JSON.stringify([lastRow, 0, lastRow]) &&
            (picked?.tracks ?? []).every(
                (row) => sameSet(keysOf(row), ["index", "title"]) && row.title === repeated.title,
            ),
        { matched, expectedRows, picked: { total: picked?.total, rows: (picked?.tracks ?? []).map((row) => row.index) } },
        { items: "the rows of that path, [0, 6] here", total: SUBJECT_COUNT + 1, pickedRows: [lastRow, 0, lastRow] },
    );

    const absentGuid = "{00000000-0000-0000-0000-000000000000}";
    const [unparsable, sorted, libraryUnparsable, missing, missingRows] = await Promise.all([
        invoke("playlist.getMatchingRows", { playlist: list, query: 'title HAS "a" AND (' }),
        invoke("playlist.getMatchingRows", { playlist: list, query: "ALL SORT BY %title%" }),
        invoke("library.query", { query: 'title HAS "a" AND (' }),
        invoke("playlist.getMatchingRows", { playlistGuid: absentGuid, query: "ALL" }),
        invoke("playlist.getTracksAt", { playlistGuid: absentGuid, rows: [0] }),
    ]);
    const queryRefused = (response) =>
        response?.success === false && response?.code === "INVALID_PARAMS" && response?.details?.param === "query";
    recorder.assertCase(
        "PO-55d a query the parser rejects or one with SORT BY names details.param query, a missing playlist fails getMatchingRows and reads empty in getTracksAt",
        queryRefused(unparsable) &&
            queryRefused(sorted) &&
            queryRefused(libraryUnparsable) &&
            missing?.success === false &&
            missing?.code === "NOT_FOUND" &&
            missingRows?.success === true &&
            missingRows?.playlist === -1 &&
            missingRows?.total === 0 &&
            (missingRows?.tracks ?? []).length === 0,
        { unparsable, sorted, libraryUnparsable, missing, missingRows },
        {
            refused: { code: "INVALID_PARAMS", details: { param: "query" } },
            missing: "NOT_FOUND",
            missingRows: { playlist: -1, total: 0, tracks: [] },
        },
    );
}

const recorder = createRecorder();
let client;
let bridge;
let collector;
let blocked = false;
let fatalError;
let targets;
let context;
let entryActiveIndex;
const names = {
    scratch: `${PREFIX} subject`,
    copy: `${PREFIX} copy`,
    unnamed: `${PREFIX} unnamed`,
    auto: `${PREFIX} auto`,
    windowing: `${PREFIX} windowing`,
    unresolvable: `${PREFIX} unresolvable`,
    emptyList: `${PREFIX} empty`,
    spacer: `${PREFIX} spacer`,
    twin: `${PREFIX} twin`,
    renamed: `${PREFIX} twin renamed`,
    movedTarget: `${PREFIX} moved target`,
    movedOther: `${PREFIX} moved other`,
    removedTarget: `${PREFIX} removed target`,
    removedNext: `${PREFIX} removed next`,
};
let fixture;

try {
    const connection = await connectBridgePage();
    client = connection.client;
    targets = connection.candidates;
    console.log(`Target: ${connection.page.title} ${connection.page.url}`);
    bridge = createBridge(client.Runtime, { invokeTimeoutMs });
    await requireResponsiveBridge(bridge, { probeId: runId });

    const entryActive = await bridge.invoke("playlist.getActive", {});
    entryActiveIndex = entryActive?.index;

    // Distinct titles, so the sort cases have no ties to resolve.
    const page = await bridge.invoke("library.query", { query: "ALL", limit: 200 });
    const seen = new Set();
    const subjects = (page?.tracks ?? [])
        .filter((track) => {
            const title = String(track?.title ?? "");
            if (!track?.absolutePath || title === "" || seen.has(title)) return false;
            seen.add(title);
            return true;
        })
        .slice(0, SUBJECT_COUNT);

    if (subjects.length < SUBJECT_COUNT) {
        recorder.assertCase(
            "PO-00 the library can supply distinctly titled subject tracks",
            false,
            { available: subjects.length, wanted: SUBJECT_COUNT },
            { available: SUBJECT_COUNT },
        );
    } else {
        const created = await bridge.invoke("playlist.create", { name: names.scratch });
        const scratch = created?.index;
        // Built with the sequential spelling, because plain addPaths does not
        // promise to land the rows in the order it was given (PO-28b) and every
        // ordering case below needs a known starting order.
        await bridge.invoke("playlist.addPathsSequential", {
            playlist: scratch,
            paths: subjects.map((track) => track.absolutePath),
        });
        context = {
            scratchIndex: scratch,
            entryActiveIndex,
            subjects: subjects.map((track) => track.title),
        };

        collector = await createEventCollector(bridge, EVENT_NAMES, { collectorId: runId });

        await runReadShapeCases(bridge, recorder, scratch, subjects);
        await runSelectionCases(bridge, recorder, collector, scratch);
        await runOrderCases(bridge, recorder, collector, scratch);
        await runContentCases(bridge, recorder, collector, scratch, subjects);
        await runPlaylistLevelCases(bridge, recorder, collector, scratch, subjects, names);
        await runRefusalCases(bridge, recorder);
        await runIdentityCases(bridge, recorder, subjects, names);
        await runWindowingCases(bridge, recorder, subjects, names);

        // A playlist file, so addPathsAsync takes its background path; a monitored root is where
        // the path policy lets the host read it.
        const root = ((await bridge.invoke("library.getRoots", {}))?.roots ?? [])[0];
        if (root?.absolutePath) {
            fixture = createFixtureArea({ libraryRoot: root.absolutePath, runId, source: undefined });
            const playlistFile = fixture.textFile(
                "moved-target.m3u8",
                `${subjects[0].absolutePath}\r\n${subjects[1].absolutePath}\r\n`,
            );
            await runMovedTargetCases(bridge, recorder, collector, names, playlistFile);
        } else {
            recorder.assertCase(
                "PO-62 the library reports a monitored root to hold the playlist file",
                false,
                { roots: 0 },
                { roots: "at least one" },
            );
        }
    }
} catch (error) {
    fatalError = error;
    blocked = Boolean(error?.blocked);
    console.error(`${blocked ? "BLOCKED" : "ERROR"}: ${error?.message || error}`);
} finally {
    if (collector) await collector.stop();
    if (bridge) {
        const quiet = (method, params) => bridge.invokeRaw(method, params).catch(() => undefined);
        // The active playlist goes back first: removing the active one would make
        // the host pick a replacement of its own.
        if (Number.isInteger(entryActiveIndex)) {
            await quiet("playlist.setActive", { playlist: entryActiveIndex });
        }
        // Removal is by name and from the highest index down, because every
        // removal shifts the indices above it - and because this run may have
        // reordered the list.
        const all = await bridge.invoke("playlist.getAll", {}).catch(() => undefined);
        const mine = (all?.playlists ?? [])
            .map((entry, index) => ({ index, name: String(entry?.name ?? "") }))
            .filter((entry) => entry.name.startsWith(PREFIX))
            .sort((left, right) => right.index - left.index);
        for (const entry of mine) {
            await quiet("playlist.remove", { playlist: entry.index });
        }
        if (context) context.removed = mine.map((entry) => entry.name);
        if (Number.isInteger(entryActiveIndex)) {
            await quiet("playlist.setActive", { playlist: entryActiveIndex });
        }
    }
    if (fixture) {
        const removal = fixture.remove();
        if (context) context.fixtureRemoved = removal;
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
            context,
            eventsSeen: collector?.received.map((event) => event.name),
            targets,
        },
    }),
);

/**
 * Covers the endpoints that change a media file or the library's own database:
 * tag writes and removals, ratings, playcount, and embedded artwork.
 *
 * These are the cases that cannot borrow the user's music. Every subject here is
 * a copy this script made, tagged out of band by editing the VORBIS_COMMENT
 * block directly (lib/flac-fixture.mjs), so the fixture and the endpoint under
 * test share no implementation - a value-loss bug in the write path cannot hide
 * behind a fixture built through the same code.
 *
 * The copies live inside the monitored library root rather than %TEMP%, because
 * rating and playcount are keyed on library membership: a file outside every root
 * is not addressable by them at all. lib/fixture-area.mjs creates the directory
 * and deletes it afterwards, which also drops the copies from the library.
 *
 * A write is dispatched, not applied: metadata.write returns once the request is
 * queued and the outcome arrives later as metadata:writeComplete. Every write
 * case therefore waits for that event before reading anything back, rather than
 * sleeping for a guessed interval.
 *
 * Left out: playcount.set has no implementation to exercise (it is a hardcoded
 * refusal, pinned by MW-16), and artwork removal on formats the album_art_editor
 * workflow does not support, which would assert on the host's format support
 * rather than on this component.
 *
 * Usage: node mcp/tests/e2e-media-write.mjs
 * Environment: FB2K_CDP_PORT, FB2K_E2E_TIMEOUT_MS, FB2K_LIBRARY_FIXTURE_DIR,
 * FB2K_FIXTURE_SOURCE.
 */

import { existsSync } from "node:fs";

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
import { resolveFixtureArea, waitForLibraryEntry } from "./lib/fixture-area.mjs";

const invokeTimeoutMs = envInt("FB2K_E2E_TIMEOUT_MS", 30000);
const runId = new Date().toISOString().replace(/[-:.TZ]/g, "");

// A 1x1 PNG, small enough to inline and still a real decodable image.
const PNG_BASE64 =
    "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mP8z8DwHwAFAAH/q842iQAAAABJRU5ErkJggg==";

function tagEntry(tags, key) {
    const wanted = key.toLowerCase();
    const found = Object.keys(tags ?? {}).find((name) => name.toLowerCase() === wanted);
    return found === undefined ? undefined : tags[found];
}

function tagValues(tags, key) {
    const value = tagEntry(tags, key);
    if (Array.isArray(value)) return value;
    return value === undefined ? [] : [value];
}

/**
 * Waits for the completion event of one dispatched write and returns it.
 *
 * The collector keeps the full ordered history, so a predicate that merely asks
 * whether a matching event exists is satisfied by an earlier write to the same
 * path and returns before the current one has landed. Waiting is therefore
 * expressed as a count: the caller says how many matches it has already
 * consumed, and this waits for one more.
 *
 * Matching is by path and operation because the event is broadcast - a write
 * this script did not start would otherwise be taken for the one under test.
 */
async function awaitWriteComplete(
    collector,
    { path, operation, alreadySeen = 0, timeoutMs = 20000 },
) {
    const matching = (events) =>
        events.filter(
            (event) =>
                event.payload?.path === path &&
                (operation === undefined || event.payload?.operation === operation),
        );
    const seen = await collector.waitFor(
        (events) => matching(events).length > alreadySeen,
        { timeoutMs, pollMs: 100 },
    );
    const found = matching(seen);
    return { event: found[alreadySeen], count: found.length };
}

async function runTagWriteCases(bridge, recorder, area, collector) {
    const { invoke, invokeRaw } = bridge;

    const subject = area.taggedAudio("tag-write.flac", [
        ["ARTIST", "Fixture Artist"],
        ["TITLE", "Tag write subject"],
        ["ALBUM", "E2E Media Write"],
        ["COMMENT", "written out of band"],
    ]);
    const path = subject.targetPath;

    const dispatch = await invoke("metadata.write", {
        path,
        tags: { ARTIST: "Rewritten Artist", GENRE: "Test Genre" },
    });
    recorder.assertCase(
        "MW-01 a tag write reports what it queued rather than claiming persistence",
        dispatch?.success === true &&
            dispatch.dispatched === true &&
            dispatch.tagsSet === 2 &&
            dispatch.tagsRemoved === 0 &&
            dispatch.tagsApplied?.ARTIST === "Rewritten Artist" &&
            typeof dispatch.note === "string" &&
            dispatch.note.length > 0,
        {
            dispatched: dispatch?.dispatched,
            tagsSet: dispatch?.tagsSet,
            tagsRemoved: dispatch?.tagsRemoved,
            tagsApplied: dispatch?.tagsApplied,
            note: dispatch?.note,
        },
        { dispatched: true, tagsSet: 2, tagsRemoved: 0, note: "names the completion event" },
    );

    let writesSeen = 0;
    const first = await awaitWriteComplete(collector, { path, operation: "write" });
    writesSeen = first.count;
    recorder.assertCase(
        "MW-02 the dispatched write announces its own completion",
        first.event?.payload?.success === true &&
            first.event.payload.status === "success" &&
            first.event.payload.code === 0 &&
            first.event.payload.subsong === 0,
        { event: first.event?.name, payload: first.event?.payload },
        {
            event: "metadata:writeComplete",
            payload: { operation: "write", success: true, status: "success", code: 0 },
        },
    );

    const afterWrite = await invoke("metadata.read", { path });
    recorder.assertCase(
        "MW-03 the written values are what a later read returns, and untouched tags survive",
        tagValues(afterWrite?.tags, "artist").join("|") === "Rewritten Artist" &&
            tagValues(afterWrite?.tags, "genre").join("|") === "Test Genre" &&
            tagValues(afterWrite?.tags, "title").join("|") === "Tag write subject" &&
            tagValues(afterWrite?.tags, "comment").join("|") === "written out of band",
        { tags: afterWrite?.tags },
        {
            ARTIST: "Rewritten Artist",
            GENRE: "Test Genre",
            TITLE: "unchanged",
            COMMENT: "unchanged",
        },
    );

    // The key is echoed uppercased regardless of how the caller spelled it, and
    // the value replaces rather than appends.
    await invoke("metadata.write", { path, tags: { artist: "Lowercase Key" } });
    writesSeen = (
        await awaitWriteComplete(collector, {
            path,
            operation: "write",
            alreadySeen: writesSeen,
        })
    ).count;
    const afterCase = await invoke("metadata.read", { path });
    recorder.assertCase(
        "MW-04 a lowercase tag key writes the same field and replaces its value",
        tagValues(afterCase?.tags, "artist").join("|") === "Lowercase Key",
        { artist: tagEntry(afterCase?.tags, "artist") },
        { artist: "Lowercase Key" },
    );

    // Documented removal spellings: null and the empty string. Both are counted
    // as removals, not as writes of an empty value.
    const removalDispatch = await invoke("metadata.write", {
        path,
        tags: { GENRE: null, COMMENT: "" },
    });
    writesSeen = (
        await awaitWriteComplete(collector, {
            path,
            operation: "write",
            alreadySeen: writesSeen,
        })
    ).count;
    const afterRemoval = await invoke("metadata.read", { path });
    recorder.assertCase(
        "MW-05 null and an empty string both remove a tag instead of writing one",
        removalDispatch?.tagsRemoved === 2 &&
            removalDispatch.tagsSet === 0 &&
            removalDispatch.tagsApplied?.GENRE === null &&
            tagEntry(afterRemoval?.tags, "genre") === undefined &&
            tagEntry(afterRemoval?.tags, "comment") === undefined,
        {
            tagsRemoved: removalDispatch?.tagsRemoved,
            tagsSet: removalDispatch?.tagsSet,
            tagsApplied: removalDispatch?.tagsApplied,
            remainingKeys: Object.keys(afterRemoval?.tags ?? {}).sort(),
        },
        { tagsRemoved: 2, tagsSet: 0, genre: "gone", comment: "gone" },
    );

    const values = ["First; Second", "第三, Fourth", "First; Second", "  spaced  "];
    const arrayWrite = await invoke("metadata.write", {
        path,
        tags: { ARTIST: values },
    });
    const arrayComplete = await awaitWriteComplete(collector, {
        path,
        operation: "write",
        alreadySeen: writesSeen,
    });
    writesSeen = arrayComplete.count;
    const afterArrayWrite = await invoke("metadata.readRaw", { path });
    recorder.assertCase(
        "MW-06 string arrays replace all previous values and preserve order, duplicates, Unicode, punctuation and whitespace",
        arrayWrite?.success === true &&
            arrayWrite.dispatched === true &&
            arrayWrite.tagsSet === 1 &&
            arrayWrite.tagsRemoved === 0 &&
            JSON.stringify(arrayWrite.tagsApplied?.ARTIST) === JSON.stringify(values) &&
            arrayComplete.event?.payload?.success === true &&
            JSON.stringify(tagValues(afterArrayWrite?.tags, "artist")) === JSON.stringify(values) &&
            tagEntry(afterArrayWrite?.tags, "title") === "Tag write subject",
        {
            response: arrayWrite,
            completion: arrayComplete.event?.payload,
            tagsAfter: afterArrayWrite?.tags,
        },
        { dispatched: true, tagsSet: 1, artist: values, title: "unchanged" },
    );

    const emptyArrayWrite = await invoke("metadata.write", { path, tags: { ARTIST: [] } });
    const emptyComplete = await awaitWriteComplete(collector, {
        path,
        operation: "write",
        alreadySeen: writesSeen,
    });
    const afterEmpty = await invoke("metadata.readRaw", { path });
    recorder.assertCase(
        "MW-30 an empty array removes the entire tag and is counted as one removal",
        emptyArrayWrite?.success === true &&
            emptyArrayWrite.dispatched === true &&
            emptyArrayWrite.tagsSet === 0 &&
            emptyArrayWrite.tagsRemoved === 1 &&
            emptyArrayWrite.tagsApplied?.ARTIST === null &&
            emptyComplete.event?.payload?.success === true &&
            tagEntry(afterEmpty?.tags, "artist") === undefined &&
            tagEntry(afterEmpty?.tags, "title") === "Tag write subject",
        { response: emptyArrayWrite, completion: emptyComplete.event?.payload, tags: afterEmpty?.tags },
        { tagsSet: 0, tagsRemoved: 1, artist: "gone", title: "unchanged" },
    );

    for (const [kind, invalid] of Object.entries({
        mixed: ["valid", 1],
        empty: ["valid", ""],
        nul: ["valid", "a\u0000b"],
    })) {
        const invalidPath = area.taggedAudio(`tag-invalid-${kind}.flac`, [
            ["TITLE", "untouched"],
        ]).targetPath;
        const rejected = await invoke("metadata.write", {
            path: invalidPath,
            tags: { ALBUM: "must not be written", GENRE: invalid },
        });
        await collector.waitFor(() => false, { timeoutMs: 300 });
        const afterRejected = await invoke("metadata.readRaw", { path: invalidPath });
        const completions = collector.received.filter((event) => event.payload?.path === invalidPath);
        recorder.assertCase(
            "MW-31 invalid array elements reject all fields for the track without dispatching a write",
            rejected?.success === false &&
                rejected.code === "INVALID_PARAMS" &&
                rejected.dispatched === undefined &&
                /GENRE/.test(rejected.error ?? "") &&
                /\[1\]/.test(rejected.error ?? "") &&
                completions.length === 0 &&
                tagEntry(afterRejected?.tags, "album") === undefined &&
                tagEntry(afterRejected?.tags, "genre") === undefined &&
                tagEntry(afterRejected?.tags, "title") === "untouched",
            { kind, response: rejected, completions, tags: afterRejected?.tags },
            { code: "INVALID_PARAMS", index: 1, writes: 0, title: "untouched" },
        );
    }

    const [noPath, noTags, wrongTagsType] = await Promise.all([
        invokeRaw("metadata.write", { tags: { ARTIST: "x" } }),
        invokeRaw("metadata.write", { path }),
        invokeRaw("metadata.write", { path, tags: "ARTIST=x" }),
    ]);
    recorder.assertCase(
        "MW-07 a missing path, missing tags and a non-object tags value are each refused",
        noPath?.kind === "result" &&
            noPath.value?.success === false &&
            noTags?.value?.success === false &&
            /tags/i.test(noTags.value?.error ?? "") &&
            wrongTagsType?.value?.success === false &&
            /tags/i.test(wrongTagsType.value?.error ?? ""),
        {
            noPath: noPath?.value?.error ?? noPath?.error,
            noTags: noTags?.value?.error,
            wrongTagsType: wrongTagsType?.value?.error,
        },
        { each: "refused with a message" },
    );

    return { path, subject };
}

async function runRemoveTagCases(bridge, recorder, area, collector) {
    const { invoke, invokeRaw } = bridge;

    const subject = area.taggedAudio("tag-remove.flac", [
        ["ARTIST", "Removal Subject"],
        ["TITLE", "Removal subject"],
        ["GENRE", "Doomed"],
        ["COMMENT", "also doomed"],
    ]);
    const path = subject.targetPath;

    const dispatch = await invoke("metadata.removeTag", {
        path,
        tags: ["genre", "COMMENT"],
    });
    recorder.assertCase(
        "MW-08 removeTag echoes the fields it queued, uppercased",
        dispatch?.success === true &&
            dispatch.dispatched === true &&
            dispatch.removedCount === 2 &&
            JSON.stringify(dispatch.removedTags) === JSON.stringify(["GENRE", "COMMENT"]),
        {
            removedCount: dispatch?.removedCount,
            removedTags: dispatch?.removedTags,
            requested: ["genre", "COMMENT"],
        },
        { removedCount: 2, removedTags: ["GENRE", "COMMENT"] },
    );

    // The operation name in the event is the handler's own label, "removeTag" -
    // not the "Remove dispatched" wording of the response note. A page routing on
    // this field needs the exact string.
    const removal = await awaitWriteComplete(collector, { path, operation: "removeTag" });
    recorder.assertCase(
        "MW-09 a removal announces completion under the operation name removeTag",
        removal.event?.payload?.operation === "removeTag" &&
            removal.event.payload.success === true &&
            removal.event.payload.code === 0,
        { payload: removal.event?.payload, responseNote: dispatch?.note },
        { payload: { operation: "removeTag", success: true, code: 0 } },
    );

    const after = await invoke("metadata.read", { path });
    recorder.assertCase(
        "MW-10 the named fields are gone and the others are untouched",
        tagEntry(after?.tags, "genre") === undefined &&
            tagEntry(after?.tags, "comment") === undefined &&
            tagValues(after?.tags, "artist").join("|") === "Removal Subject" &&
            tagValues(after?.tags, "title").join("|") === "Removal subject",
        { remainingKeys: Object.keys(after?.tags ?? {}).sort(), tags: after?.tags },
        { genre: "gone", comment: "gone", artist: "kept", title: "kept" },
    );

    // removeField is registered against the same handler, so it has to behave
    // identically; a divergence would mean the two registrations drifted apart.
    const viaAlias = await invoke("metadata.removeField", { path, tags: ["ARTIST"] });
    await awaitWriteComplete(collector, {
        path,
        operation: "removeTag",
        alreadySeen: removal.count,
    });
    const afterAlias = await invoke("metadata.read", { path });
    recorder.assertCase(
        "MW-11 removeField is the same operation under a second name",
        viaAlias?.success === true &&
            viaAlias.removedCount === 1 &&
            tagEntry(afterAlias?.tags, "artist") === undefined,
        { response: viaAlias, artistAfter: tagEntry(afterAlias?.tags, "artist") },
        { removedCount: 1, artistAfter: undefined },
    );

    const [noTags, emptyList] = await Promise.all([
        invokeRaw("metadata.removeTag", { path }),
        invokeRaw("metadata.removeTag", { path, tags: [] }),
    ]);
    recorder.assertCase(
        "MW-12 removeTag requires a tags array, and an empty one removes nothing",
        noTags?.value?.success === false &&
            /tags/i.test(noTags.value?.error ?? "") &&
            emptyList?.value?.success === true &&
            emptyList.value.removedCount === 0 &&
            emptyList.value.dispatched === undefined,
        {
            noTags: noTags?.value?.error,
            emptyList: emptyList?.value,
        },
        { noTags: "refused", emptyList: { success: true, removedCount: 0 } },
    );
}

async function runRatingCases(bridge, recorder, area, collector) {
    const { invoke, invokeRaw } = bridge;

    const subject = area.taggedAudio("rating.flac", [
        ["ARTIST", "Rating Subject"],
        ["TITLE", `Rating subject ${runId}`],
        ["ALBUM", "E2E Media Write"],
    ]);
    const path = subject.targetPath;

    // Rating and playcount answer for library members, so the copy has to be
    // indexed before it can be rated. Without this the endpoints fall back to
    // the now-playing track or the current selection and would silently rate
    // whatever the session happened to have open.
    const arrival = await waitForLibraryEntry(bridge, path);
    recorder.assertCase(
        "MW-13 a fixture dropped into a monitored root becomes library-addressable",
        arrival !== null &&
            arrival.entry?.found === true &&
            tagValues({ artist: arrival.entry.artists }, "artist").join("|") === "Rating Subject",
        {
            found: arrival?.entry?.found ?? false,
            waitedMs: arrival?.waitedMs,
            artists: arrival?.entry?.artists,
            title: arrival?.entry?.title,
        },
        { found: true, artists: ["Rating Subject"] },
    );

    if (arrival === null) return;

    const before = await invoke("rating.get", { path });
    const set = await invoke("rating.set", { path, rating: 4 });
    const after = await invoke("rating.get", { path });
    recorder.assertCase(
        "MW-14 a rating round-trips and reports which store answered",
        before?.rating === 0 &&
            set?.success === true &&
            set.rating === 4 &&
            after?.rating === 4 &&
            (after.storage === "stats" || after.storage === "file"),
        {
            before: { rating: before?.rating, storage: before?.storage },
            set: { rating: set?.rating, storage: set?.storage, menuPath: set?.menuPath },
            after: { rating: after?.rating, storage: after?.storage },
        },
        { before: 0, after: { rating: 4, storage: "stats or file" } },
    );

    const cleared = await invoke("rating.set", { path, rating: 0 });
    const afterClear = await invoke("rating.get", { path });
    recorder.assertCase(
        "MW-15 rating zero clears the rating rather than storing a zero score",
        cleared?.success === true && afterClear?.rating === 0,
        { cleared: { rating: cleared?.rating }, afterClear: { rating: afterClear?.rating } },
        { afterClear: { rating: 0 } },
    );

    const [tooHigh, negative, missing] = await Promise.all([
        invokeRaw("rating.set", { path, rating: 6 }),
        invokeRaw("rating.set", { path, rating: -1 }),
        invokeRaw("rating.set", { path }),
    ]);
    recorder.assertCase(
        "MW-16 a rating outside zero to five is refused, and so is a missing one",
        tooHigh?.value?.success === false &&
            /out of range|0-5|0 through 5/i.test(tooHigh.value?.error ?? "") &&
            negative?.value?.success === false &&
            missing?.value?.success === false,
        {
            tooHigh: tooHigh?.value?.error,
            negative: negative?.value?.error,
            missing: missing?.value?.error,
        },
        { each: "refused, naming the accepted range" },
    );

    const counts = await invoke("playcount.get", { paths: [path] });
    const entry = (counts?.results ?? [])[0];
    recorder.assertCase(
        "MW-17 playcount reports an unplayed library member as zero plays, not as missing",
        counts?.success === true &&
            counts.count === 1 &&
            entry?.inLibrary === true &&
            entry.playCount === 0 &&
            entry.firstPlayed === "N/A" &&
            entry.lastPlayed === "N/A" &&
            typeof entry.added === "string" &&
            entry.added.length > 0,
        { count: counts?.count, entry },
        { inLibrary: true, playCount: 0, firstPlayed: "N/A", lastPlayed: "N/A", added: "a date" },
    );

    // Not a stub to be filled in later from this suite's point of view: the
    // handler is a hardcoded refusal, so the contract is the refusal itself.
    // Only path is declared; an extra key such as playCount would be refused as INVALID_PARAMS
    // before the handler could say NOT_SUPPORTED.
    const playcountSet = await invokeRaw("playcount.set", { path });
    recorder.assertCase(
        "MW-18 playcount.set refuses and points at the endpoint that does work",
        playcountSet?.kind === "result" &&
            playcountSet.value?.success === false &&
            playcountSet.value.code === "NOT_SUPPORTED" &&
            /rating\.set/i.test(playcountSet.value?.error ?? ""),
        { response: playcountSet?.value },
        { success: false, code: "NOT_SUPPORTED", error: "names rating.set as the supported route" },
    );

    await runRatingOrderCases(bridge, recorder, collector, path);
}

// Rating lives in two independent stores: foo_playcount statistics and the file's
// own RATING tag. Reads go to statistics first and fall back to the tag, and the
// playlist row must answer in that same order as rating.get does.
//
// The only input that tells the two orders apart is "both stores filled, values
// disagreeing", and until these cases nothing in the suite fed it: MW-14 accepts
// either store in `storage`. That is how the order came to be flipped for a while
// with every gate green, leaving the same track showing one number in a playlist
// row and another through rating.get.
async function runRatingOrderCases(bridge, recorder, collector, path) {
    const { invoke } = bridge;

    const writesBefore = (await collector.drain()).filter(
        (event) => event.payload?.path === path && event.payload?.operation === "write",
    ).length;
    await invoke("metadata.write", { path, tags: { RATING: "2" } });
    await awaitWriteComplete(collector, {
        path,
        operation: "write",
        alreadySeen: writesBefore,
    });
    await invoke("rating.set", { path, rating: 4 });

    const scratch = await invoke("playlist.create", { name: `e2e-rating-order-${runId}` });
    const playlist = scratch?.index;
    if (!Number.isInteger(playlist)) {
        recorder.addCase(
            "MW-27 with both stores disagreeing, the row and rating.get answer the statistics value",
            true,
            { skipped: "the scratch playlist could not be created", scratch },
            { skipped: "needs a playlist holding the rated fixture" },
        );
        return;
    }

    try {
        await invoke("playlist.addPathsSequential", { playlist, paths: [path] });
        const rowOf = async (fields) => {
            const page = await invoke("playlist.getTracks", {
                playlist,
                start: 0,
                count: 50,
                ...(fields ? { fields } : {}),
            });
            const rows = Array.isArray(page) ? page : (page?.tracks ?? []);
            return rows.length === 1 ? rows[0] : null;
        };

        const row = await rowOf(null);
        const fromGet = await invoke("rating.get", { path });
        recorder.assertCase(
            "MW-27 with both stores disagreeing, the row and rating.get answer the statistics value",
            row?.rating === 4 && fromGet?.rating === 4 && fromGet.storage === "stats",
            {
                tag: 2,
                stats: 4,
                row: { title: row?.title, rating: row?.rating },
                get: { rating: fromGet?.rating, storage: fromGet?.storage },
            },
            { row: { rating: 4 }, get: { rating: 4, storage: "stats" } },
        );

        const projected = await rowOf(["index", "rating"]);
        recorder.assertCase(
            "MW-28 a projected row answers the same rating as the full row",
            projected?.rating === 4 && Object.keys(projected).sort().join(",") === "index,rating",
            { projected },
            { projected: { rating: 4, keys: "index,rating" } },
        );

        // Clearing statistics leaves the tag in place, so the fallback tier takes
        // over. This is what proves reading statistics first did not lose the tag.
        await invoke("rating.set", { path, rating: 0 });
        const afterClear = await rowOf(null);
        const getAfterClear = await invoke("rating.get", { path });
        recorder.assertCase(
            "MW-29 once the statistics value is gone the tag answers instead, in both readers",
            afterClear?.rating === 2 &&
                getAfterClear?.rating === 2 &&
                getAfterClear.storage === "file",
            {
                row: { rating: afterClear?.rating },
                get: { rating: getAfterClear?.rating, storage: getAfterClear?.storage },
            },
            { row: { rating: 2 }, get: { rating: 2, storage: "file" } },
        );
    } finally {
        await bridge.invokeRaw("playlist.remove", { playlist }).catch(() => undefined);
    }
}

async function runArtworkCases(bridge, recorder, area) {
    const { invoke, invokeRaw } = bridge;

    const subject = area.taggedAudio("artwork.flac", [
        ["ARTIST", "Artwork Subject"],
        ["TITLE", "Artwork subject"],
    ]);
    const path = subject.targetPath;

    const before = await invoke("artwork.getByPath", { path, type: "front" });
    recorder.assertCase(
        "MW-19 a fresh copy carries no embedded artwork to begin with",
        before?.available === false,
        { available: before?.available, type: before?.type },
        { available: false },
    );

    const embed = await invoke("metadata.embedArtwork", {
        path,
        imageData: PNG_BASE64,
        type: "front",
    });
    const after = await invoke("artwork.getByPath", { path, type: "front" });
    recorder.assertCase(
        "MW-20 embedded artwork is readable back through the artwork endpoint",
        embed?.success === true && after?.available === true,
        { embed, after: { available: after?.available, type: after?.type, bytes: after?.data?.length } },
        { embed: { success: true }, after: { available: true } },
    );

    const removed = await invoke("metadata.removeEmbeddedArt", { path, removeAll: true });
    const afterRemoval = await invoke("artwork.getByPath", { path, type: "front" });
    recorder.assertCase(
        "MW-21 removing embedded artwork makes it unavailable again",
        removed?.success === true && afterRemoval?.available === false,
        { removed, afterRemoval: { available: afterRemoval?.available } },
        { removed: { success: true }, afterRemoval: { available: false } },
    );

    // The documented input is raw Base64. A Data URL header is the most likely
    // caller mistake, so it must be refused rather than embedded verbatim.
    const [dataUrl, emptyData, noPath, badTarget] = await Promise.all([
        invokeRaw("metadata.embedArtwork", {
            path,
            imageData: `data:image/png;base64,${PNG_BASE64}`,
        }),
        invokeRaw("metadata.embedArtwork", { path, imageData: "" }),
        invokeRaw("metadata.embedArtwork", { imageData: PNG_BASE64 }),
        invokeRaw("metadata.embedArtwork", {
            path,
            imageData: PNG_BASE64,
            target: "nowhere",
        }),
    ]);
    recorder.assertCase(
        "MW-22 empty image data, a missing path and an unknown target are refused",
        emptyData?.value?.success === false &&
            noPath?.value?.success === false &&
            badTarget?.value?.success === false &&
            /target/i.test(badTarget.value?.error ?? ""),
        {
            emptyData: emptyData?.value?.error,
            noPath: noPath?.value?.error,
            badTarget: badTarget?.value?.error,
            dataUrlAccepted: dataUrl?.value?.success,
        },
        { emptyData: "refused", noPath: "refused", badTarget: "refused, naming target" },
    );

    // Recorded rather than asserted as a contract: the endpoint decodes what it
    // was given, and a Data URL header is not documented as accepted. Whichever
    // way it lands, the value is pinned so a change is visible.
    recorder.assertCase(
        "MW-23 a Data URL header is handled one definite way, not intermittently",
        typeof dataUrl?.value?.success === "boolean",
        {
            success: dataUrl?.value?.success,
            error: dataUrl?.value?.error,
            documented: "raw Base64 only; strip a Data URL at its first comma",
        },
        { success: "a definite boolean, pinned as characterization" },
    );

    const targeted = await invoke("metadata.embedArtwork", {
        path,
        imageData: PNG_BASE64,
        target: ["embedded", "file"],
        filename: "cover-probe.png",
    });
    recorder.assertCase(
        "MW-24 asking for two targets reports each one separately",
        targeted?.success === true &&
            typeof targeted.results === "object" &&
            targeted.results !== null &&
            "embedded" in targeted.results &&
            "file" in targeted.results,
        {
            success: targeted?.success,
            resultKeys: Object.keys(targeted?.results ?? {}).sort(),
            embedded: targeted?.results?.embedded?.success,
            file: targeted?.results?.file?.success,
        },
        { resultKeys: ["embedded", "file"] },
    );

    const savedTo = String(targeted?.results?.file?.savedTo ?? "");
    const folderImages = await invoke("artwork.getFolderImages", { directory: area.dir });
    const seenByHost = (folderImages?.images ?? []).map((image) =>
        String(image?.path ?? image?.absolutePath ?? image),
    );
    recorder.assertCase(
        "MW-25 the file target writes into the audio file's own directory, where the host finds it",
        targeted?.results?.file?.success === true &&
            savedTo.startsWith(area.dir) &&
            existsSync(savedTo) &&
            seenByHost.some((image) => image.endsWith("cover-probe.png")),
        {
            savedTo,
            insideFixtureDir: savedTo.startsWith(area.dir),
            existsOnDisk: existsSync(savedTo),
            imagesSeenByHost: seenByHost,
        },
        { savedTo: "inside the fixture directory", seenByHost: "includes cover-probe.png" },
    );

    // A filename without an extension is taken verbatim: the bytes land on disk
    // but the host's folder scan keys on image extensions, so nothing finds them.
    // Recorded because success is reported either way - the caller has no signal
    // that the file it just wrote is invisible.
    const extensionless = await invoke("metadata.embedArtwork", {
        path,
        imageData: PNG_BASE64,
        target: ["file"],
        filename: "no-extension",
    });
    const afterExtensionless = await invoke("artwork.getFolderImages", { directory: area.dir });
    const extensionlessPath = String(extensionless?.savedTo ?? "");
    recorder.assertCase(
        "MW-26 a filename with no extension still succeeds, and the host does not list the result",
        extensionless?.success === true &&
            existsSync(extensionlessPath) &&
            extensionlessPath.endsWith("no-extension") &&
            (afterExtensionless?.images ?? []).every(
                (image) => !String(image?.path ?? image?.absolutePath ?? image).endsWith("no-extension"),
            ),
        {
            success: extensionless?.success,
            savedTo: extensionlessPath,
            existsOnDisk: existsSync(extensionlessPath),
            listedByHost: false,
            characterization: "filename is used verbatim; no extension is appended",
        },
        { success: true, existsOnDisk: true, listedByHost: false },
    );
}

const recorder = createRecorder();
let client;
let bridge;
let area;
let collector;
let blocked = false;
let fatalError;
let targets;
let context;

try {
    const connection = await connectBridgePage();
    client = connection.client;
    targets = connection.candidates;
    console.log(`Target: ${connection.page.title} ${connection.page.url}`);
    bridge = createBridge(client.Runtime, { invokeTimeoutMs });
    await requireResponsiveBridge(bridge, { probeId: runId });

    const resolved = await resolveFixtureArea(bridge, { runId });
    area = resolved.area;
    context = {
        libraryRoot: resolved.root.absolutePath,
        fixtureDir: area.dir,
        source: resolved.source,
    };
    console.log(`Fixture area: ${area.dir}`);

    collector = await createEventCollector(bridge, ["metadata:writeComplete"], {
        collectorId: runId,
    });

    await runTagWriteCases(bridge, recorder, area, collector);
    await runRemoveTagCases(bridge, recorder, area, collector);
    await runRatingCases(bridge, recorder, area, collector);
    await runArtworkCases(bridge, recorder, area);
} catch (error) {
    fatalError = error;
    blocked = Boolean(error?.blocked);
    console.error(`${blocked ? "BLOCKED" : "ERROR"}: ${error?.message || error}`);
} finally {
    if (collector) await collector.stop();
    let cleanup;
    if (area) {
        cleanup = area.remove();
        // The copies were library members, so the library is asked to notice
        // they are gone; otherwise it keeps reporting dead entries.
        if (bridge) await bridge.invokeRaw("library.rescan", {}).catch(() => undefined);
    }
    if (context) context.cleanup = cleanup;
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
            context,
            targets,
        },
    }),
);

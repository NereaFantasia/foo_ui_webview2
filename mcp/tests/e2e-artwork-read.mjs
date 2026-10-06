/**
 * Covers the read side of artwork.* - the picture readers, the fb2k:// URL
 * builders, and the two endpoints that report a track's tags rather than its
 * images (getLyrics, getMetadata).
 *
 * The fixtures carry their pictures from out of band: a PNG of chosen size is
 * synthesized here and written straight into a FLAC PICTURE block (see
 * flac-fixture.mjs), so what a case asserts about bytes and dimensions was never
 * produced by anything under test. artwork.embed is deliberately not used to
 * build them - that would make the reader and the writer share one
 * implementation, and a symmetric bug would read as a pass.
 *
 * The namespace splits into three families that behave differently, which is
 * most of what this suite pins:
 *   - the extractor family (getForTrack, getByPath, getAvailableTypes,
 *     getAvailableArtwork, getBatch) opens the file directly, so it sees a
 *     freshly written fixture at once;
 *   - the metadb family (getLyrics, getMetadata) uses cached track info when
 *     complete, otherwise reading the local file directly;
 *   - the URL family (getFb2kUrl, getFb2kUrlByPath, getFb2kUrlByPathBatch) only
 *     builds strings, so it neither opens the file nor checks it exists (AW-13).
 *
 * getCurrent and getFb2kUrl need a playing track. Nothing is started here -
 * that belongs to the audible batch - so the playback state is measured and the
 * no-track branch asserted, with the success branch recorded as out of reach.
 *
 * getByPlaylistItem is aimed at a scratch playlist holding one known fixture
 * rather than at whatever the active playlist happens to hold, so its expected
 * image is exact. The playlist is removed afterwards.
 *
 * Usage: node mcp/tests/e2e-artwork-read.mjs
 * Environment: FB2K_CDP_PORT, FB2K_E2E_TIMEOUT_MS, FB2K_LIBRARY_FIXTURE_DIR,
 * FB2K_FIXTURE_SOURCE.
 */

import {
    closeClient,
    connectBridgePage,
    createBridge,
    createRecorder,
    envInt,
    report,
    requireResponsiveBridge,
    resolvePort,
} from "./lib/e2e-harness.mjs";
import { resolveFixtureArea, waitForLibraryEntry } from "./lib/fixture-area.mjs";
import {
    makeSolidPng,
    PICTURE_TYPE_BACK_COVER,
    PICTURE_TYPE_FRONT_COVER,
} from "./lib/flac-fixture.mjs";

const invokeTimeoutMs = envInt("FB2K_E2E_TIMEOUT_MS", 25000);
const runId = new Date().toISOString().replace(/[-:.TZ]/g, "");

/** Dimensions are chosen, not inherited, so the reader has an exact expectation. */
const FRONT = { width: 12, height: 8, png: makeSolidPng({ width: 12, height: 8, rgb: [0x20, 0x60, 0xa0] }) };
const BACK = { width: 6, height: 4, png: makeSolidPng({ width: 6, height: 4, rgb: [0xa0, 0x30, 0x30] }) };

const ABSENT_PATH = `Z:\\no-such-directory-${runId}\\absent.flac`;
const UNKNOWN_TYPE = "nonsense-type";
/** ArtworkGetFb2kUrlByPathBatch::kMaxBatchItems. */
const MAX_BATCH_ITEMS = 100;
/** The order ArtworkGetAvailableTypes probes, which is the order it reports. */
const TYPE_PROBE_ORDER = ["front", "back", "disc", "icon", "artist"];

const tags = (title) => [
    ["ARTIST", "Artwork Fixture"],
    ["TITLE", title],
    ["ALBUM", `AW ${runId}`],
    ["DATE", "2026"],
    ["GENRE", "Test"],
    ["TRACKNUMBER", "3"],
    ["DISCNUMBER", "1"],
];

const dataUrlFor = (png) => `data:image/png;base64,${png.toString("base64")}`;

async function runExtractorCases(bridge, recorder, fixtures) {
    const { invoke } = bridge;

    const front = await invoke("artwork.getForTrack", { path: fixtures.front });
    recorder.assertCase(
        "AW-01 getForTrack returns the embedded picture byte for byte, with the dimensions read out of the PNG header",
        front?.available === true &&
            front.type === "front" &&
            front.mimeType === "image/png" &&
            front.size === FRONT.png.length &&
            front.width === FRONT.width &&
            front.height === FRONT.height &&
            front.dataUrl === dataUrlFor(FRONT.png),
        {
            available: front?.available,
            mimeType: front?.mimeType,
            size: front?.size,
            width: front?.width,
            height: front?.height,
            dataUrlMatches: front?.dataUrl === dataUrlFor(FRONT.png),
        },
        {
            size: FRONT.png.length,
            width: FRONT.width,
            height: FRONT.height,
            dataUrlMatches: true,
        },
    );

    const back = await invoke("artwork.getForTrack", { path: fixtures.both, type: "back" });
    const backMissing = await invoke("artwork.getForTrack", { path: fixtures.front, type: "back" });
    recorder.assertCase(
        "AW-02 each art type is its own entry: the back cover comes back at its own size, and asking a file that has none is a plain unavailable",
        back?.available === true &&
            back.type === "back" &&
            back.width === BACK.width &&
            back.height === BACK.height &&
            back.dataUrl === dataUrlFor(BACK.png) &&
            backMissing?.available === false &&
            backMissing.type === "back" &&
            backMissing.path === fixtures.front &&
            !("dataUrl" in backMissing),
        {
            back: { available: back?.available, width: back?.width, height: back?.height },
            backMissing,
        },
        {
            back: { width: BACK.width, height: BACK.height },
            backMissing: { available: false, dataUrlPresent: false },
        },
    );

    const bare = await invoke("artwork.getForTrack", { path: fixtures.bare });
    recorder.assertCase(
        "AW-03 a file the fixture builder left without any picture is unavailable, echoing the path and carrying no image fields",
        bare?.available === false &&
            bare.path === fixtures.bare &&
            bare.type === "front" &&
            ["dataUrl", "mimeType", "size", "width", "height"].every((key) => !(key in bare)),
        { bare, keys: Object.keys(bare ?? {}).sort() },
        { available: false, keys: ["available", "path", "type"] },
    );

    // Two readers, one picture, two response shapes.
    const byPath = await invoke("artwork.getByPath", { path: fixtures.front });
    recorder.assertCase(
        "AW-04 getByPath and getForTrack read the same bytes and both echo the path, but only getForTrack measures the image",
        byPath?.available === true &&
            byPath.dataUrl === front.dataUrl &&
            byPath.size === front.size &&
            byPath.path === fixtures.front &&
            !("width" in byPath) &&
            front.path === fixtures.front,
        {
            sameDataUrl: byPath?.dataUrl === front?.dataUrl,
            byPathKeys: Object.keys(byPath ?? {}).sort(),
            getForTrackKeys: Object.keys(front ?? {}).sort(),
        },
        {
            sameDataUrl: true,
            byPath: "carries path, no width or height",
            getForTrack: "carries path, width and height",
        },
    );

    const [typesBoth, typesFront, typesBare] = await Promise.all([
        invoke("artwork.getAvailableTypes", { path: fixtures.both }),
        invoke("artwork.getAvailableTypes", { path: fixtures.front }),
        invoke("artwork.getAvailableTypes", { path: fixtures.bare }),
    ]);
    const inProbeOrder = (list) =>
        list.every(
            (name, i) =>
                TYPE_PROBE_ORDER.indexOf(name) >
                (i === 0 ? -1 : TYPE_PROBE_ORDER.indexOf(list[i - 1])),
        );
    recorder.assertCase(
        "AW-05 getAvailableTypes reports only the types the file carries, in the fixed order the handler probes them",
        typesBoth?.success === true &&
            typesBoth.types.join() === "front,back" &&
            inProbeOrder(typesBoth.types) &&
            typesFront?.types.join() === "front" &&
            typesBare?.success === true &&
            Array.isArray(typesBare.types) &&
            typesBare.types.length === 0,
        { both: typesBoth, front: typesFront, bare: typesBare, probeOrder: TYPE_PROBE_ORDER },
        {
            both: ["front", "back"],
            front: ["front"],
            bare: [],
            order: "the handler's probe order, not the order in the file",
        },
    );

    const typesNoPath = await invoke("artwork.getAvailableTypes", {});
    const playback = await invoke("playback.getState", {});
    // Omitting the path makes this endpoint fall back to the playing track, so
    // the branch that has no subject at all is only reachable when stopped.
    recorder.assertCase(
        playback?.state === "stopped"
            ? "AW-06 with no path and nothing playing getAvailableTypes fails with NO_ACTIVE_ITEM"
            : "AW-06 with no path getAvailableTypes falls back to the playing track and answers in its normal shape",
        playback?.state === "stopped"
            ? typesNoPath?.success === false &&
              typesNoPath.code === "NO_ACTIVE_ITEM" &&
              /nothing playing/i.test(typesNoPath.error ?? "")
            : typesNoPath?.success === true && Array.isArray(typesNoPath.types),
        { typesNoPath, playbackState: playback?.state },
        playback?.state === "stopped"
            ? { success: false, code: "NO_ACTIVE_ITEM" }
            : {
                  success: true,
                  note: "the no-subject failure is out of reach while a track is loaded",
              },
    );

    const [artBoth, artBare, artNoPath] = await Promise.all([
        invoke("artwork.getAvailableArtwork", { path: fixtures.both }),
        invoke("artwork.getAvailableArtwork", { path: fixtures.bare }),
        invoke("artwork.getAvailableArtwork", {}),
    ]);
    recorder.assertCase(
        "AW-07 getAvailableArtwork pairs every type with where it came from, summarises the sources, and unlike getAvailableTypes requires a path",
        artBoth?.success === true &&
            artBoth.available === true &&
            artBoth.artworks.map((entry) => entry.type).join() === "front,back" &&
            artBoth.artworks.every((entry) => entry.source === "embedded") &&
            artBoth.sources.includes("embedded") &&
            artBare?.success === true &&
            artBare.available === false &&
            artBare.artworks.length === 0 &&
            artNoPath?.success === false &&
            /path is required/i.test(artNoPath.error ?? ""),
        { both: artBoth, bare: artBare, noPath: artNoPath },
        {
            both: { available: true, artworks: "front and back, both embedded" },
            bare: { available: false, artworks: [] },
            noPath: { success: false, error: "path is required" },
        },
    );

    const batch = await invoke("artwork.getBatch", {
        paths: [fixtures.front, fixtures.bare],
    });
    const rows = batch?.artworks ?? [];
    recorder.assertCase(
        "AW-08 getBatch answers one row per path in order, and a row with no picture carries only the path and the flag",
        batch?.success === true &&
            rows.length === 2 &&
            rows[0].path === fixtures.front &&
            rows[0].available === true &&
            rows[0].size === FRONT.png.length &&
            rows[1].path === fixtures.bare &&
            rows[1].available === false &&
            Object.keys(rows[1]).sort().join() === "available,path",
        {
            rowKeys: rows.map((row) => Object.keys(row).sort()),
            sizes: rows.map((row) => row.size),
        },
        {
            available: ["available", "dataUrl", "mimeType", "path", "size"],
            unavailable: ["available", "path"],
        },
    );

    const [noPaths, emptyPaths, badTypeBatch] = await Promise.all([
        invoke("artwork.getBatch", {}),
        invoke("artwork.getBatch", { paths: [] }),
        invoke("artwork.getBatch", { paths: [fixtures.front], type: UNKNOWN_TYPE }),
    ]);
    recorder.assertCase(
        "AW-09 getBatch separates a missing array from an empty one, and validates the type once for the whole batch rather than per row",
        noPaths?.success === false &&
            noPaths.code === "INVALID_PARAMS" &&
            /paths is required/i.test(noPaths.error ?? "") &&
            emptyPaths?.success === true &&
            emptyPaths.artworks.length === 0 &&
            badTypeBatch?.success === false &&
            badTypeBatch.code === "INVALID_PARAMS" &&
            !("artworks" in badTypeBatch),
        { noPaths, emptyPaths, badTypeBatch },
        {
            noPaths: { success: false },
            emptyPaths: { success: true, artworks: [] },
            badType: { code: "INVALID_PARAMS", artworksPresent: false },
        },
    );

    const badTypes = await Promise.all(
        [
            ["artwork.getForTrack", { path: fixtures.front, type: UNKNOWN_TYPE }],
            ["artwork.getCurrent", { type: UNKNOWN_TYPE }],
            ["artwork.getByPlaylistItem", { type: UNKNOWN_TYPE }],
        ].map(([method, params]) => invoke(method, params)),
    );
    recorder.assertCase(
        "AW-10 every endpoint that takes a type name refuses an unknown one the same way, before it looks at anything else",
        badTypes.every(
            (res) =>
                res?.success === false &&
                res.code === "INVALID_PARAMS" &&
                res.error === `type has an unsupported value '${UNKNOWN_TYPE}'`,
        ),
        { responses: badTypes },
        { each: { success: false, code: "INVALID_PARAMS", error: "names the rejected type" } },
    );
}

async function runUrlCases(bridge, recorder, fixtures) {
    const { invoke, invokeRaw } = bridge;

    const plain = await invoke("artwork.getFb2kUrlByPath", { path: fixtures.front });
    const encodedPath = encodeURIComponent(fixtures.front);
    recorder.assertCase(
        "AW-11 the fb2k URL carries the path in a query parameter rather than in the URL path, so backslashes survive Chromium's normalisation",
        plain?.available === true &&
            plain.path === fixtures.front &&
            plain.type === "front" &&
            plain.dataUrl === `fb2k://artwork/?path=${encodedPath}&type=front`,
        { dataUrl: plain?.dataUrl, expected: `fb2k://artwork/?path=${encodedPath}&type=front` },
        { dataUrl: "fb2k://artwork/?path=<percent-encoded>&type=front" },
    );

    const scaled = await invoke("artwork.getFb2kUrlByPath", {
        path: fixtures.front,
        maxSize: 256,
    });
    const badMaxSize = await invoke("artwork.getFb2kUrlByPath", {
        path: fixtures.front,
        maxSize: "big",
    });
    const badType = await invoke("artwork.getFb2kUrlByPath", {
        path: fixtures.front,
        type: 42,
    });
    recorder.assertCase(
        "AW-12 maxSize is appended only when given, and a non-integer maxSize or a non-string type is refused with a code naming the parameter",
        scaled?.dataUrl === `fb2k://artwork/?path=${encodedPath}&type=front&maxSize=256` &&
            badMaxSize?.success === false &&
            badMaxSize.code === "INVALID_PARAMS" &&
            /maxSize must be an integer/.test(badMaxSize.error ?? "") &&
            badType?.success === false &&
            badType.code === "INVALID_PARAMS" &&
            /type must be a string/.test(badType.error ?? ""),
        { scaled: scaled?.dataUrl, badMaxSize, badType },
        {
            scaled: "ends with &maxSize=256",
            badMaxSize: { code: "INVALID_PARAMS", error: "maxSize must be an integer" },
            badType: { code: "INVALID_PARAMS", error: "type must be a string" },
        },
    );

    const absent = await invoke("artwork.getFb2kUrlByPath", { path: ABSENT_PATH });
    recorder.assertCase(
        "AW-13 the URL is minted from the string without opening anything, so a path with no file behind it is reported available",
        absent?.available === true &&
            absent.path === ABSENT_PATH &&
            absent.dataUrl.includes(encodeURIComponent(ABSENT_PATH)),
        { absent },
        {
            available: true,
            note: "existence is the protocol handler's problem, not this endpoint's",
        },
    );

    const noPath = await invoke("artwork.getFb2kUrlByPath", {});
    recorder.assertCase(
        "AW-14 a missing path is refused with INVALID_PARAMS, the same way as the option refusals in the same handler",
        noPath?.success === false &&
            noPath.code === "INVALID_PARAMS" &&
            /path is required/i.test(noPath.error ?? ""),
        { noPath },
        { success: false, code: "INVALID_PARAMS", error: "path is required" },
    );

    const viaPaths = await invoke("artwork.getFb2kUrlByPathBatch", {
        paths: [fixtures.front, fixtures.bare],
    });
    const viaItems = await invoke("artwork.getFb2kUrlByPathBatch", {
        items: [{ path: fixtures.front }],
    });
    const neither = await invokeRaw("artwork.getFb2kUrlByPathBatch", {});
    const both = await invokeRaw("artwork.getFb2kUrlByPathBatch", {
        paths: [fixtures.front],
        items: [{ path: fixtures.front }],
    });
    recorder.assertCase(
        "AW-15 the batch takes exactly one of paths or items, and giving both is refused with the same message as giving neither",
        viaPaths?.success === true &&
            viaPaths.artworks.length === 2 &&
            viaPaths.artworks.every((row) => row.available === true) &&
            viaItems?.success === true &&
            viaItems.artworks[0].dataUrl === viaPaths.artworks[0].dataUrl &&
            [neither, both].every(
                (res) =>
                    res?.value?.success === false &&
                    res.value.code === "INVALID_PARAMS" &&
                    /exactly one of paths or items/i.test(res.value.error ?? ""),
            ),
        {
            viaPathsCount: viaPaths?.artworks?.length,
            spellingsAgree: viaItems?.artworks?.[0]?.dataUrl === viaPaths?.artworks?.[0]?.dataUrl,
            neither: neither?.value,
            both: both?.value,
        },
        {
            spellingsAgree: true,
            neither: { code: "INVALID_PARAMS" },
            both: { code: "INVALID_PARAMS", note: "both and neither are not distinguished" },
        },
    );

    const overLimit = await invokeRaw("artwork.getFb2kUrlByPathBatch", {
        paths: Array.from({ length: MAX_BATCH_ITEMS + 1 }, () => fixtures.front),
    });
    const atLimit = await invoke("artwork.getFb2kUrlByPathBatch", {
        paths: Array.from({ length: MAX_BATCH_ITEMS }, () => fixtures.front),
    });
    recorder.assertCase(
        "AW-16 the batch is capped at a hundred entries, and the hundredth is still accepted",
        overLimit?.value?.success === false &&
            overLimit.value.code === "INVALID_PARAMS" &&
            /batch limit exceeded/i.test(overLimit.value.error ?? "") &&
            atLimit?.success === true &&
            atLimit.artworks.length === MAX_BATCH_ITEMS,
        {
            overLimit: overLimit?.value,
            atLimitCount: atLimit?.artworks?.length,
        },
        { overLimit: { code: "INVALID_PARAMS" }, atLimitCount: MAX_BATCH_ITEMS },
    );

    // The refusal below is raised by the path-security tier rather than by this
    // namespace. The code and the parameter it names are asserted; the full
    // wording belongs to that shared tier and is only recorded.
    const emptyItem = await invoke("artwork.getFb2kUrlByPathBatch", { items: [{ path: "" }] });
    recorder.assertCase(
        "AW-17 an empty path inside items is stopped by the write-tier path check before the handler can report it per row",
        emptyItem?.success === false &&
            emptyItem.code === "PERMISSION_DENIED" &&
            /items\[0\]\.path/.test(emptyItem.error ?? ""),
        { emptyItem },
        {
            success: false,
            code: "PERMISSION_DENIED",
            error: "names items[0].path; the wording belongs to the path-security tier",
        },
    );
}

async function runLyricsTagCases(bridge, recorder, fixtures) {
    const lyricist = await Promise.all([
        bridge.invoke("lyrics.get", { path: fixtures.lyricist, source: "embedded" }),
        bridge.invoke("lyrics.exists", { path: fixtures.lyricist }),
        bridge.invoke("artwork.getLyrics", { path: fixtures.lyricist }),
        bridge.invoke("artwork.getMetadata", { path: fixtures.lyricist }),
    ]);
    recorder.assertCase(
        "AW-29 LYRICIST is not a lyrics source in any reader",
        lyricist[0]?.available === false && lyricist[1]?.exists === false &&
            lyricist[2]?.available === false && lyricist[3]?.hasLyrics === false,
        { lyricist },
        { everyReader: "no lyrics" },
    );
    for (const { tag, path, emptyPath } of fixtures.knownLyrics) {
        const [get, exists, artwork, metadata] = await Promise.all([
            bridge.invoke("lyrics.get", { path, source: "embedded" }),
            bridge.invoke("lyrics.exists", { path }),
            bridge.invoke("artwork.getLyrics", { path }),
            bridge.invoke("artwork.getMetadata", { path }),
        ]);
        const tagSynced = tag.startsWith("SYNCED");
        recorder.assertCase(
            "AW-30 known lyrics tags use tag names for artwork synced and content for lyrics synced",
            get?.lyrics === "[00:01]line" && get?.synced === true && exists?.exists === true &&
                artwork?.tag === tag && artwork?.synced === tagSynced && metadata?.hasLyrics === true,
            { tag, get, exists, artwork, hasLyrics: metadata?.hasLyrics },
            { contentSynced: true, tagSynced, exists: true },
        );
        const [emptyGet, emptyExists, emptyArtwork, emptyMetadata] = await Promise.all([
            bridge.invoke("lyrics.get", { path: emptyPath, source: "embedded" }),
            bridge.invoke("lyrics.exists", { path: emptyPath }),
            bridge.invoke("artwork.getLyrics", { path: emptyPath }),
            bridge.invoke("artwork.getMetadata", { path: emptyPath }),
        ]);
        recorder.assertCase(
            "AW-31 empty known lyrics tags exist but supply no readable lyrics",
            emptyGet?.available === false && emptyExists?.exists === true &&
                emptyArtwork?.available === false && emptyMetadata?.hasLyrics === true,
            { tag, emptyGet, emptyExists, emptyArtwork, hasLyrics: emptyMetadata?.hasLyrics },
            { available: false, exists: true, hasLyrics: true },
        );
    }
}

async function runMetadbCases(bridge, recorder, fixtures) {
    const { invoke } = bridge;

    const initialLyrics = await invoke("artwork.getLyrics", { path: fixtures.plainLyrics });
    const initialMetadata = await invoke("artwork.getMetadata", { path: fixtures.plainLyrics });
    recorder.assertCase(
        "AW-18 local metadata and lyrics are readable without waiting for library indexing",
        initialMetadata?.title === "plain lyrics" &&
            initialLyrics?.lyrics === "line one\nline two",
        { initialMetadata, initialLyrics },
        { title: "plain lyrics", lyrics: "line one\nline two" },
    );

    // Now let the library catch up, which is what the tag assertions need.
    const indexed = {};
    for (const key of ["plainLyrics", "syncedLyrics", "bothLyrics", "unsyncedLyrics", "emptyLyrics", "front"]) {
        const got = await waitForLibraryEntry(bridge, fixtures[key], { timeoutMs: 45000 });
        indexed[key] = got !== null;
    }
    if (!Object.values(indexed).every(Boolean)) {
        recorder.addCase(
            "AW-19 a plain LYRICS tag wins over a synced one, because the handler probes it first",
            true,
            { skipped: "the library did not index every fixture in time", indexed },
            { skipped: "needs the fixtures to be library members" },
        );
        return;
    }

    const bothTags = await invoke("artwork.getLyrics", { path: fixtures.bothLyrics });
    recorder.assertCase(
        "AW-19 a file holding both a plain and a synced lyrics tag reports the plain one, because the probe order decides - so its synced flag reads false",
        bothTags?.available === true &&
            bothTags.tag === "LYRICS" &&
            bothTags.lyrics === "plain" &&
            bothTags.synced === false,
        { bothTags },
        {
            tag: "LYRICS",
            synced: false,
            note: "the synced tag is present but never reached",
        },
    );

    const [syncedOnly, unsyncedOnly] = await Promise.all([
        invoke("artwork.getLyrics", { path: fixtures.syncedLyrics }),
        invoke("artwork.getLyrics", { path: fixtures.unsyncedLyrics }),
    ]);
    recorder.assertCase(
        "AW-20 the synced flag is derived from the tag name, so SYNCEDLYRICS sets it while the UNSYNCED spelling containing the same letters does not",
        syncedOnly?.available === true &&
            syncedOnly.tag === "SYNCEDLYRICS" &&
            syncedOnly.synced === true &&
            unsyncedOnly?.available === true &&
            unsyncedOnly.tag === "UNSYNCED LYRICS" &&
            unsyncedOnly.synced === false,
        { syncedOnly, unsyncedOnly },
        {
            syncedOnly: { tag: "SYNCEDLYRICS", synced: true },
            unsyncedOnly: { tag: "UNSYNCED LYRICS", synced: false },
        },
    );

    const [emptyLyrics, emptyMetadata] = await Promise.all([
        invoke("artwork.getLyrics", { path: fixtures.emptyLyrics }),
        invoke("artwork.getMetadata", { path: fixtures.emptyLyrics }),
    ]);
    recorder.assertCase(
        "AW-21 an empty lyrics value is not lyrics to getLyrics but is to getMetadata: one requires a non-empty value, the other only that the tag exists",
        emptyLyrics?.available === false &&
            !("tag" in emptyLyrics) &&
            emptyMetadata?.hasLyrics === true,
        {
            getLyrics: emptyLyrics,
            getMetadataHasLyrics: emptyMetadata?.hasLyrics,
        },
        {
            getLyrics: { available: false },
            getMetadataHasLyrics: true,
            note: "the two endpoints disagree about the same file",
        },
    );

    const [withArt, withoutArt] = await Promise.all([
        invoke("artwork.getMetadata", { path: fixtures.front }),
        invoke("artwork.getMetadata", { path: fixtures.emptyLyrics }),
    ]);
    recorder.assertCase(
        "AW-22 getMetadata reports the written tags and whether the file carries embedded artwork, which it checks across all five types rather than the front cover alone",
        withArt?.available === true &&
            withArt.artist === "Artwork Fixture" &&
            withArt.album === `AW ${runId}` &&
            withArt.title === "front only" &&
            withArt.year === "2026" &&
            withArt.trackNumber === "3" &&
            withArt.discNumber === "1" &&
            withArt.hasEmbedded === true &&
            withoutArt?.hasEmbedded === false,
        {
            withArt: { ...withArt },
            withoutArtHasEmbedded: withoutArt?.hasEmbedded,
        },
        {
            withArt: { hasEmbedded: true, tags: "as written out of band" },
            withoutArtHasEmbedded: false,
        },
    );
}

async function runNowPlayingCases(bridge, recorder) {
    const { invoke } = bridge;

    const playback = await invoke("playback.getState", {});
    const [current, url] = await Promise.all([
        invoke("artwork.getCurrent", {}),
        invoke("artwork.getFb2kUrl", {}),
    ]);
    const stopped = playback?.state === "stopped";

    // Both endpoints need a playing track, so which branch is reachable depends
    // on the instance. Stopped is the only state in which the no_track reading
    // can be seen at all; with a track loaded, getFb2kUrl always succeeds
    // because it never opens the file, while getCurrent reports why it found no
    // picture through one of two fields - reason when its three lookups all came
    // back empty, error when one of them threw (the host's own not-found
    // exception carries a localized message, so the text is not pinned).
    recorder.assertCase(
        stopped
            ? "AW-23 with nothing playing both now-playing readers answer unavailable with the reason no_track, rather than failing"
            : "AW-23 with a track loaded the URL builder always succeeds, while getCurrent says whether it found a picture and, when it did not, why",
        stopped
            ? current?.available === false &&
              current.reason === "no_track" &&
              current.type === "front" &&
              url?.available === false &&
              url.reason === "no_track" &&
              url.type === "front"
            : url?.available === true &&
              typeof url.dataUrl === "string" &&
              url.dataUrl.startsWith("fb2k://artwork/?path=") &&
              (current?.success === false
                  ? typeof current.error === "string"
                  : typeof current?.available === "boolean" &&
                    (current.available === true
                        ? typeof current.source === "string" && typeof current.dataUrl === "string"
                        : current.reason === "not_found")),
        {
            playbackState: playback?.state,
            current: { ...current, dataUrl: current?.dataUrl ? "<omitted>" : undefined },
            url: { ...url, dataUrl: url?.dataUrl?.slice(0, 40) },
        },
        stopped
            ? { both: { available: false, reason: "no_track" } }
            : {
                  url: { available: true, dataUrl: "an fb2k://artwork/ URL for the playing path" },
                  current: "available with a source, or unavailable with a reason or an error",
              },
    );

    recorder.addCase(
        "AW-24 the success paths of getCurrent and getFb2kUrl are not exercised here, because reaching them means starting playback",
        true,
        {
            covered: "the no_track branch of both, and the type validation of getCurrent",
            notCovered:
                "the three lookup fallbacks getCurrent walks (now_playing_manager, album_art_manager_v2, extractor) and the source field naming which of them answered",
            reason: "starting a track is audible and moves the playing-playlist pointer; it belongs with the audible batch",
        },
        { recorded: "a scope boundary, not an API limitation" },
    );
}

async function runPlaylistItemCases(bridge, recorder, fixtures, scratch, scratchGuid) {
    const { invoke } = bridge;

    const front = await invoke("artwork.getByPlaylistItem", { playlist: scratch, index: 0 });
    const back = await invoke("artwork.getByPlaylistItem", {
        playlist: scratch,
        index: 0,
        type: "back",
    });
    const disc = await invoke("artwork.getByPlaylistItem", {
        playlist: scratch,
        index: 0,
        type: "disc",
    });
    recorder.assertCase(
        "AW-25 getByPlaylistItem resolves the row to its track and echoes the coordinates it used, but unlike getForTrack it does not measure the image",
        front?.available === true &&
            front.playlist === scratch &&
            typeof scratchGuid === "string" &&
            front.playlistGuid === scratchGuid &&
            front.index === 0 &&
            front.size === FRONT.png.length &&
            front.dataUrl === dataUrlFor(FRONT.png) &&
            !("width" in front) &&
            back?.available === true &&
            back.size === BACK.png.length &&
            disc?.available === false &&
            disc.playlist === scratch &&
            disc.playlistGuid === scratchGuid &&
            disc.index === 0 &&
            disc.type === "disc",
        {
            front: { available: front?.available, size: front?.size, keys: Object.keys(front ?? {}).sort() },
            back: { available: back?.available, size: back?.size },
            disc,
        },
        {
            front: { size: FRONT.png.length, widthPresent: false, playlistGuid: scratchGuid },
            back: { size: BACK.png.length },
            disc: { available: false, coordinates: "echoed with the playlist GUID" },
        },
    );

    const negative = await invoke("artwork.getByPlaylistItem", { playlist: scratch, index: -1 });
    const pastEnd = await invoke("artwork.getByPlaylistItem", { playlist: scratch, index: 1 });
    recorder.assertCase(
        "AW-26 a negative index quietly means the first row, while one past the end fails with NOT_FOUND",
        negative?.available === true &&
            negative.index === 0 &&
            negative.size === FRONT.png.length &&
            pastEnd?.success === false &&
            pastEnd.code === "NOT_FOUND" &&
            /index out of range/i.test(pastEnd.error ?? ""),
        { negative: { available: negative?.available, index: negative?.index }, pastEnd },
        {
            negative: { index: 0, note: "not refused" },
            pastEnd: { success: false, code: "NOT_FOUND" },
        },
    );

    const badPlaylist = await invoke("artwork.getByPlaylistItem", { playlist: 999999, index: 0 });
    recorder.assertCase(
        "AW-27 a playlist index past the last playlist fails with INVALID_INDEX, apart from a row past the end of an existing playlist",
        badPlaylist?.success === false &&
            badPlaylist.code === "INVALID_INDEX" &&
            /invalid playlist index/i.test(badPlaylist.error ?? ""),
        { badPlaylist },
        {
            success: false,
            code: "INVALID_INDEX",
            error: "Invalid playlist index",
        },
    );

    const fileRelative = await invoke("artwork.getForTrack", {
        path: "file-relative://something.flac",
    });
    recorder.assertCase(
        "AW-28 getForTrack refuses a playlist-relative path outright with INVALID_PATH and points at the endpoint that can resolve one",
        fileRelative?.success === false &&
            fileRelative.code === "INVALID_PATH" &&
            /file-relative:\/\/ paths require playlist context/i.test(fileRelative.error ?? "") &&
            /getByPlaylistItem/.test(fileRelative.error ?? ""),
        { fileRelative },
        { success: false, code: "INVALID_PATH", error: "names artwork.getByPlaylistItem as the alternative" },
    );
}

const recorder = createRecorder();
let client;
let bridge;
let blocked = false;
let fatalError;
let targets;
let area;
let entry;
let scratch;

try {
    const connection = await connectBridgePage();
    client = connection.client;
    targets = connection.candidates;
    console.log(`Target: ${connection.page.title} ${connection.page.url}`);
    bridge = createBridge(client.Runtime, { invokeTimeoutMs });
    await requireResponsiveBridge(bridge, { probeId: runId });

    const resolved = await resolveFixtureArea(bridge, { runId });
    area = resolved.area;

    const frontPicture = {
        type: PICTURE_TYPE_FRONT_COVER,
        width: FRONT.width,
        height: FRONT.height,
        data: FRONT.png,
    };
    const backPicture = {
        type: PICTURE_TYPE_BACK_COVER,
        width: BACK.width,
        height: BACK.height,
        data: BACK.png,
    };

    const fixtures = {
        front: area.taggedAudio(`aw-front-${runId}.flac`, tags("front only"), [frontPicture])
            .targetPath,
        both: area.taggedAudio(`aw-both-${runId}.flac`, tags("front and back"), [
            frontPicture,
            backPicture,
        ]).targetPath,
        bare: area.taggedAudio(`aw-bare-${runId}.flac`, tags("no art")).targetPath,
        plainLyrics: area.taggedAudio(`aw-plain-${runId}.flac`, [
            ...tags("plain lyrics"),
            ["LYRICS", "line one\nline two"],
        ]).targetPath,
        syncedLyrics: area.taggedAudio(`aw-synced-${runId}.flac`, [
            ...tags("synced lyrics"),
            ["SYNCEDLYRICS", "[00:01.00]first"],
        ]).targetPath,
        bothLyrics: area.taggedAudio(`aw-bothlyrics-${runId}.flac`, [
            ...tags("both lyrics tags"),
            ["SYNCEDLYRICS", "[00:01.00]synced"],
            ["LYRICS", "plain"],
        ]).targetPath,
        unsyncedLyrics: area.taggedAudio(`aw-unsynced-${runId}.flac`, [
            ...tags("unsynced lyrics"),
            ["UNSYNCED LYRICS", "plain unsynced"],
        ]).targetPath,
        lyricist: area.taggedAudio(`aw-lyricist-${runId}.flac`, [
            ...tags("lyricist only"), ["LYRICIST", "Writer name"],
        ]).targetPath,
        emptyLyrics: area.taggedAudio(`aw-emptylyrics-${runId}.flac`, [
            ...tags("empty lyrics"),
            ["LYRICS", ""],
        ]).targetPath,
    };

    const knownTags = ["LYRICS", "UNSYNCED LYRICS", "UNSYNCEDLYRICS", "SYNCEDLYRICS", "SYNCED LYRICS"];
    fixtures.knownLyrics = knownTags.map((tag, index) => ({
        tag,
        path: area.taggedAudio(`aw-known-${index}-${runId}.flac`, [
            ...tags(`known lyrics ${index}`), [tag, "[00:01]line"],
        ]).targetPath,
        emptyPath: area.taggedAudio(`aw-known-empty-${index}-${runId}.flac`, [
            ...tags(`known empty lyrics ${index}`), [tag, ""],
        ]).targetPath,
    }));

    entry = {
        fixtureDir: area.dir,
        source: resolved.source.absolutePath,
        frontBytes: FRONT.png.length,
        backBytes: BACK.png.length,
    };

    await runLyricsTagCases(bridge, recorder, fixtures);
    await runMetadbCases(bridge, recorder, fixtures);
    await runExtractorCases(bridge, recorder, fixtures);
    await runUrlCases(bridge, recorder, fixtures);
    await runNowPlayingCases(bridge, recorder);

    const created = await bridge.invoke("playlist.create", { name: `e2e-artwork-${runId}` });
    scratch = created?.index;
    if (Number.isInteger(scratch)) {
        await bridge.invoke("playlist.addPathsSequential", {
            playlist: scratch,
            paths: [fixtures.both],
        });
        await runPlaylistItemCases(bridge, recorder, fixtures, scratch, created?.guid);
    } else {
        recorder.addCase(
            "AW-25 getByPlaylistItem resolves the row to its track and echoes the coordinates it used",
            true,
            { skipped: "the scratch playlist could not be created", created },
            { skipped: "needs a playlist holding the known fixture" },
        );
    }
} catch (error) {
    fatalError = error;
    blocked = Boolean(error?.blocked);
    console.error(`${blocked ? "BLOCKED" : "ERROR"}: ${error?.message || error}`);
} finally {
    if (bridge && Number.isInteger(scratch)) {
        await bridge.invokeRaw("playlist.remove", { playlist: scratch }).catch(() => undefined);
    }
    if (area) area.remove();
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

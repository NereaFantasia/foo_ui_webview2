/**
 * Covers the read side of the library namespace against a controlled album that
 * this script puts into the monitored root.
 *
 * The ground truth is deliberately outside the library. Tag values come from
 * VORBIS_COMMENT blocks written here (lib/flac-fixture.mjs) and the file list
 * comes from the filesystem, so no assertion is checked against another library
 * query. Verifying one library query with another only proves the two agree; it
 * cannot see the case where both drop the same value, which is what the
 * multi-value tag bug did.
 *
 * Every fixture value carries the run id, so aggregate endpoints can be asserted
 * on exact counts rather than "at least one": no other track in the collection
 * can carry those album, artist or genre values.
 *
 * The fixtures live inside the library root rather than %TEMP% because library
 * membership is the subject under test. lib/fixture-area.mjs creates the
 * directory and deletes it afterwards, which also drops the copies from the
 * library.
 *
 * Left out: rescan and refresh are triggered as part of fixture pickup and
 * cleanup, so they are covered there rather than asserted twice; getRandomTracks
 * is asserted on size and membership only, since randomness has no oracle.
 *
 * Usage: node mcp/tests/e2e-library-read.mjs
 * Environment: FB2K_CDP_PORT, FB2K_E2E_TIMEOUT_MS, FB2K_LIBRARY_FIXTURE_DIR,
 * FB2K_FIXTURE_SOURCE.
 */

import { existsSync, statSync } from "node:fs";
import { basename, parse, relative } from "node:path";

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
const eventTimeoutMs = envInt("FB2K_EVENT_TIMEOUT_MS", 60000);
const runId = new Date().toISOString().replace(/[-:.TZ]/g, "");

/** The documented separator for joining multi-value tags into one string. */
const SEPARATOR = ", ";

const ALBUM = `E2E Library Album ${runId}`;
// The album artist tag has two values, and the first one contains the separator
// itself, so the joined albumArtist string cannot be split back into them.
const ALBUM_ARTIST = `E2E Album Artist, Crosby ${runId}`;
const ALBUM_ARTIST_TWO = `E2E Album Artist Two ${runId}`;
const ARTIST_ALPHA = `E2E Artist Alpha ${runId}`;
const ARTIST_BETA = `E2E Artist Beta ${runId}`;
const GENRE_ONE = `E2E Genre One ${runId}`;
const GENRE_TWO = `E2E Genre Two ${runId}`;
const DATE = "2026";
const SCRATCH_NAME = `e2e library ${runId}`;

/**
 * Three tracks of one album, tagged so that each aggregate endpoint has a
 * distinct expected number: alpha is on two tracks, beta on two, one of which
 * carries both, and the second genre exists on a single track. The middle track
 * repeats the ARTIST and GENRE keys, which is how a multi-value tag is spelled
 * in VORBIS_COMMENT; every track repeats ALBUMARTIST the same way.
 */
const FIXTURES = [
    {
        name: "lib-track-1.flac",
        entries: [
            ["TITLE", "Library probe one"],
            ["ALBUM", ALBUM],
            ["ALBUMARTIST", ALBUM_ARTIST],
            ["ALBUMARTIST", ALBUM_ARTIST_TWO],
            ["ARTIST", ARTIST_ALPHA],
            ["GENRE", GENRE_ONE],
            ["DATE", DATE],
            ["TRACKNUMBER", "1"],
        ],
    },
    {
        name: "lib-track-2.flac",
        entries: [
            ["TITLE", "Library probe two"],
            ["ALBUM", ALBUM],
            ["ALBUMARTIST", ALBUM_ARTIST],
            ["ALBUMARTIST", ALBUM_ARTIST_TWO],
            ["ARTIST", ARTIST_ALPHA],
            ["ARTIST", ARTIST_BETA],
            ["GENRE", GENRE_ONE],
            ["GENRE", GENRE_TWO],
            ["DATE", DATE],
            ["TRACKNUMBER", "2"],
        ],
    },
    {
        name: "lib-track-3.flac",
        entries: [
            ["TITLE", "Library probe three"],
            ["ALBUM", ALBUM],
            ["ALBUMARTIST", ALBUM_ARTIST],
            ["ALBUMARTIST", ALBUM_ARTIST_TWO],
            ["ARTIST", ARTIST_BETA],
            ["DATE", DATE],
            ["TRACKNUMBER", "3"],
        ],
    },
];

/**
 * The keys of a declared library track row: the shared Track fields plus
 * `index`. Two serializers produce these rows - the generated one behind the
 * DOM-built endpoints and the deferred direct-write path of query and search -
 * so the set is pinned rather than sampled.
 */
const TRACK_ROW_KEYS = [
    "absolutePath",
    "album",
    "albumArtist",
    "albumArtists",
    "artist",
    "artists",
    "bitrate",
    "channels",
    "codec",
    "date",
    "discNumber",
    "duration",
    "fileSize",
    "genre",
    "handle",
    "index",
    "path",
    "rating",
    "sampleRate",
    "subsong",
    "title",
    "trackNumber",
];

/** getByPath answers with a flat row of its own rather than a track row. */
const BY_PATH_KEYS = [
    "absolutePath",
    "album",
    "artist",
    "artists",
    "date",
    "duration",
    "found",
    "genre",
    "path",
    "success",
    "title",
    "trackNumber",
];

function keysOf(value) {
    return Object.keys(value ?? {}).sort();
}

function sameSet(left, right) {
    return JSON.stringify([...left].sort()) === JSON.stringify([...right].sort());
}

function pathsOf(rows) {
    return (rows ?? []).map((row) => String(row?.absolutePath ?? ""));
}

function valueEntry(list, name) {
    return (list ?? []).find((entry) => entry?.name === name);
}

async function runSurfaceCases(bridge, recorder) {
    const { invoke } = bridge;

    const enabled = await invoke("library.isEnabled", {});
    recorder.assertCase(
        "LR-01 the library is on, so every later case has a subject",
        enabled?.success === true && enabled?.enabled === true,
        { response: enabled },
        { success: true, enabled: true },
    );

    // Three different counting paths: enum_items, a cached stats snapshot, and a
    // full get_all_items. They are read together because a disagreement between
    // them is invisible from any single endpoint.
    const [count, status, stats, page] = await Promise.all([
        invoke("library.getCount", {}),
        invoke("library.getStatus", {}),
        invoke("library.getStats", {}),
        invoke("library.getAll", { limit: 1 }),
    ]);

    recorder.assertCase(
        "LR-02 the four ways of counting the library agree",
        count?.count === status?.itemCount &&
            status?.itemCount === status?.count &&
            count?.count === stats?.totalTracks &&
            count?.count === page?.total,
        {
            getCount: count?.count,
            getStatusItemCount: status?.itemCount,
            getStatusCount: status?.count,
            getStatsTotalTracks: stats?.totalTracks,
            getAllTotal: page?.total,
        },
        { allEqual: true },
    );

    recorder.assertCase(
        "LR-03 the aggregate totals stay inside what the track count allows",
        stats?.totalDuration > 0 &&
            stats?.totalSize > 0 &&
            stats?.totalAlbums > 0 &&
            stats?.totalArtists > 0 &&
            stats?.totalAlbums <= stats?.totalTracks &&
            typeof stats?.cacheValid === "boolean",
        {
            totalTracks: stats?.totalTracks,
            totalAlbums: stats?.totalAlbums,
            totalArtists: stats?.totalArtists,
            totalDuration: stats?.totalDuration,
            totalSize: stats?.totalSize,
            cacheValid: stats?.cacheValid,
        },
        { albumsAtMostTracks: true, positiveDurationAndSize: true },
    );

    // The roots are checked against the filesystem rather than against another
    // library endpoint: a root whose directory is not there would mean the
    // reported path is not the one being watched.
    //
    // A root can sit on a volume this process cannot reach - a mapped network
    // drive is the ordinary case - and then existsSync says nothing about the
    // directory. Those roots are reported rather than asserted, and the volume is
    // probed separately so that "the drive is not mounted" cannot be mistaken for
    // "the directory is gone".
    const roots = await invoke("library.getRoots", {});
    const rootList = roots?.roots ?? [];
    const checked = rootList.map((root) => {
        const path = String(root?.absolutePath ?? "");
        const volume = path === "" ? "" : parse(path).root;
        return {
            path,
            volumeReachable: volume !== "" && existsSync(volume),
            exists: path !== "" && existsSync(path),
        };
    });
    const missingOnDisk = checked.filter((root) => root.volumeReachable && !root.exists);
    const unreachable = checked.filter((root) => !root.volumeReachable);
    const trackSum = rootList.reduce((total, root) => total + (Number(root?.trackCount) || 0), 0);
    recorder.assertCase(
        "LR-04 every reachable monitored root is a real directory and the root tracks add up to the indexed total",
        roots?.success === true &&
            rootList.length > 0 &&
            checked.some((root) => root.exists) &&
            missingOnDisk.length === 0 &&
            roots?.total === rootList.length &&
            trackSum === roots?.indexedTracks,
        {
            roots: rootList.map((root) => ({
                absolutePath: root?.absolutePath,
                trackCount: root?.trackCount,
            })),
            total: roots?.total,
            indexedTracks: roots?.indexedTracks,
            trackSum,
            missingOnDisk: missingOnDisk.map((root) => root.path),
            unverifiable: unreachable.map((root) => root.path),
        },
        { missingOnDisk: [], atLeastOneVerified: true, trackSumEqualsIndexedTracks: true },
    );

    const firstRow = (page?.tracks ?? [])[0];
    recorder.assertCase(
        "LR-05 a track row carries exactly the documented key set",
        sameSet(keysOf(firstRow), TRACK_ROW_KEYS),
        { keys: keysOf(firstRow) },
        { keys: [...TRACK_ROW_KEYS].sort() },
    );

    // start/count used to be aliases of offset/limit; the declared parameters
    // refuse them as unknown instead of paging with a spelling nobody documents.
    const [byOffset, byStart] = await Promise.all([
        invoke("library.getAll", { offset: 1, limit: 2 }),
        invoke("library.getAll", { start: 1, count: 2 }),
    ]);
    recorder.assertCase(
        "LR-06 getAll pages with offset/limit and refuses start/count as unknown parameters",
        byOffset?.offset === 1 &&
            (byOffset?.tracks ?? []).length === 2 &&
            byStart?.success === false &&
            byStart?.code === "INVALID_PARAMS" &&
            /unknown parameter '(start|count)'/.test(byStart?.error ?? ""),
        {
            offsetPaths: pathsOf(byOffset?.tracks),
            offset: byOffset?.offset,
            startCount: byStart,
        },
        { offset: 1, length: 2, startCount: { code: "INVALID_PARAMS", error: "unknown parameter 'start'" } },
    );

    recorder.assertCase(
        "LR-07 items is an alias of tracks, not a second list",
        JSON.stringify(pathsOf(byOffset?.tracks)) === JSON.stringify(pathsOf(byOffset?.items)),
        { tracks: pathsOf(byOffset?.tracks), items: pathsOf(byOffset?.items) },
        { identical: true },
    );

    // The row index is the position in the library, so a page starting at 1 must
    // not renumber from zero.
    recorder.assertCase(
        "LR-08 a page keeps library positions rather than renumbering from zero",
        (byOffset?.tracks ?? []).every((row, position) => row?.index === position + 1),
        { indices: (byOffset?.tracks ?? []).map((row) => row?.index) },
        { indices: [1, 2] },
    );

    return { baselineCount: count?.count, roots: rootList };
}

async function runFixtureCases(bridge, recorder, fixtures, baselineCount) {
    const { invoke } = bridge;
    const expectedPaths = fixtures.map((fixture) => fixture.targetPath);

    const count = await invoke("library.getCount", {});
    recorder.assertCase(
        "LR-09 the three copies raised the library count by exactly three",
        count?.count === baselineCount + FIXTURES.length,
        { before: baselineCount, after: count?.count },
        { after: baselineCount + FIXTURES.length },
    );

    // getByPath is the only endpoint that answers about one file, so it is where
    // the written tags are compared value by value.
    const multi = fixtures[1];
    const row = await invoke("library.getByPath", { path: multi.targetPath });
    recorder.assertCase(
        "LR-10 getByPath returns the tags that were written, joined as documented",
        row?.found === true &&
            row?.title === "Library probe two" &&
            row?.album === ALBUM &&
            row?.date === DATE &&
            row?.trackNumber === "2" &&
            JSON.stringify(row?.artists) === JSON.stringify([ARTIST_ALPHA, ARTIST_BETA]) &&
            row?.artist === [ARTIST_ALPHA, ARTIST_BETA].join(SEPARATOR) &&
            row?.genre === [GENRE_ONE, GENRE_TWO].join(SEPARATOR),
        {
            title: row?.title,
            album: row?.album,
            date: row?.date,
            trackNumber: row?.trackNumber,
            artist: row?.artist,
            artists: row?.artists,
            genre: row?.genre,
        },
        {
            artists: [ARTIST_ALPHA, ARTIST_BETA],
            artist: [ARTIST_ALPHA, ARTIST_BETA].join(SEPARATOR),
            genre: [GENRE_ONE, GENRE_TWO].join(SEPARATOR),
            trackNumber: "2",
        },
    );

    recorder.assertCase(
        "LR-11 getByPath answers with its own flat row, not a track row",
        sameSet(keysOf(row), BY_PATH_KEYS),
        { keys: keysOf(row) },
        { keys: [...BY_PATH_KEYS].sort() },
    );

    // The absolute path and the path the library reports for the same file are
    // different spellings on a portable install; both have to resolve.
    const byReportedPath = await invoke("library.getByPath", { path: row.path });
    recorder.assertCase(
        "LR-12 the reported path form resolves to the same track as the absolute one",
        byReportedPath?.found === true &&
            byReportedPath?.absolutePath === row?.absolutePath &&
            byReportedPath?.title === row?.title,
        {
            reportedPath: row?.path,
            absolutePath: row?.absolutePath,
            found: byReportedPath?.found,
            resolvedAbsolutePath: byReportedPath?.absolutePath,
        },
        { found: true, sameAbsolutePath: true },
    );

    const queried = await invoke("library.query", {
        query: `%album% IS ${ALBUM}`,
        limit: 50,
    });
    recorder.assertCase(
        "LR-13 query on the album returns exactly the three copies",
        queried?.success === true &&
            queried?.total === FIXTURES.length &&
            sameSet(pathsOf(queried?.tracks), expectedPaths),
        { total: queried?.total, paths: pathsOf(queried?.tracks) },
        { total: FIXTURES.length, paths: [...expectedPaths].sort() },
    );

    recorder.assertCase(
        "LR-14 the deferred serializer emits the same key set as the generated one",
        (queried?.tracks ?? []).every((entry) => sameSet(keysOf(entry), TRACK_ROW_KEYS)),
        { keys: keysOf((queried?.tracks ?? [])[0]) },
        { keys: [...TRACK_ROW_KEYS].sort() },
    );

    const searched = await invoke("library.search", { query: ALBUM, limit: 50 });
    recorder.assertCase(
        "LR-15 search on the album finds the same three copies",
        searched?.success === true &&
            searched?.total === FIXTURES.length &&
            searched?.hasMore === false &&
            sameSet(pathsOf(searched?.tracks), expectedPaths),
        {
            total: searched?.total,
            hasMore: searched?.hasMore,
            offset: searched?.offset,
            paths: pathsOf(searched?.tracks),
        },
        { total: FIXTURES.length, hasMore: false, paths: [...expectedPaths].sort() },
    );

    // The album artist of the key is the first ALBUMARTIST value.
    const albumTracks = await invoke("library.getAlbumTracks", {
        album: ALBUM,
        albumArtist: ALBUM_ARTIST,
    });
    recorder.assertCase(
        "LR-16 getAlbumTracks returns the album in written track-number order",
        albumTracks?.success === true &&
            albumTracks?.total === FIXTURES.length &&
            JSON.stringify((albumTracks?.tracks ?? []).map((entry) => entry?.trackNumber)) ===
                JSON.stringify([1, 2, 3]) &&
            JSON.stringify(pathsOf(albumTracks?.tracks)) === JSON.stringify(expectedPaths),
        {
            total: albumTracks?.total,
            trackNumbers: (albumTracks?.tracks ?? []).map((entry) => entry?.trackNumber),
            paths: pathsOf(albumTracks?.tracks),
        },
        { total: FIXTURES.length, trackNumbers: [1, 2, 3], paths: expectedPaths },
    );

    // Alpha is the only value on the first track and the first of two on the
    // second, so a query for it must return both - a reader that only looks at
    // the first value of a multi-value tag returns one.
    const alphaTracks = await invoke("library.getArtistTracks", { artist: ARTIST_ALPHA });
    const betaTracks = await invoke("library.getArtistTracks", { artist: ARTIST_BETA });
    recorder.assertCase(
        "LR-17 getArtistTracks counts a track whose artist tag merely includes the name",
        alphaTracks?.count === 2 &&
            betaTracks?.count === 2 &&
            sameSet(pathsOf(alphaTracks?.tracks), [expectedPaths[0], expectedPaths[1]]) &&
            sameSet(pathsOf(betaTracks?.tracks), [expectedPaths[1], expectedPaths[2]]),
        {
            alpha: { count: alphaTracks?.count, paths: pathsOf(alphaTracks?.tracks) },
            beta: { count: betaTracks?.count, paths: pathsOf(betaTracks?.tracks) },
        },
        { alphaCount: 2, betaCount: 2, eachPairMatchesTheTaggedFiles: true },
    );

    const artists = await invoke("library.getArtists", { limit: 5000 });
    const alphaEntry = valueEntry(artists?.items, ARTIST_ALPHA);
    const betaEntry = valueEntry(artists?.items, ARTIST_BETA);
    recorder.assertCase(
        "LR-18 getArtists lists each written value separately with its own track count",
        alphaEntry?.trackCount === 2 &&
            betaEntry?.trackCount === 2 &&
            alphaEntry?.albumCount === 1 &&
            betaEntry?.albumCount === 1,
        { alpha: alphaEntry, beta: betaEntry },
        { trackCount: 2, albumCount: 1, forBothValues: true },
    );

    const genres = await invoke("library.getGenres", {});
    recorder.assertCase(
        "LR-19 getGenres splits a multi-value genre into one entry per value",
        valueEntry(genres?.genres, GENRE_ONE)?.trackCount === 2 &&
            valueEntry(genres?.genres, GENRE_TWO)?.trackCount === 1,
        {
            genreOne: valueEntry(genres?.genres, GENRE_ONE),
            genreTwo: valueEntry(genres?.genres, GENRE_TWO),
        },
        { genreOne: { trackCount: 2 }, genreTwo: { trackCount: 1 } },
    );

    const fieldValues = await invoke("library.getFieldValues", { field: "artist", limit: 5000 });
    recorder.assertCase(
        "LR-20 getFieldValues counts the same way for an arbitrary field",
        fieldValues?.success === true &&
            fieldValues?.field === "artist" &&
            valueEntry(fieldValues?.values, ARTIST_ALPHA)?.trackCount === 2 &&
            valueEntry(fieldValues?.values, ARTIST_BETA)?.trackCount === 2,
        {
            field: fieldValues?.field,
            total: fieldValues?.total,
            alpha: valueEntry(fieldValues?.values, ARTIST_ALPHA),
            beta: valueEntry(fieldValues?.values, ARTIST_BETA),
        },
        { alpha: { trackCount: 2 }, beta: { trackCount: 2 } },
    );

    // Two endpoints report the same album with different track counts on
    // purpose: getAlbums folds the whole album, getArtistAlbums folds only the
    // tracks the named artist took part in. A consumer that treats them as
    // interchangeable renders a wrong number, so the difference is pinned.
    const albums = await invoke("library.getAlbums", { query: ALBUM, limit: 10 });
    const albumRow = (albums?.albums ?? [])[0];
    recorder.assertCase(
        "LR-21 getAlbums folds the whole album and reports the written album fields",
        albums?.total === 1 &&
            albumRow?.name === ALBUM &&
            albumRow?.albumArtist === ALBUM_ARTIST &&
            albumRow?.year === DATE &&
            albumRow?.trackCount === FIXTURES.length &&
            albumRow?.discCount === 1,
        {
            total: albums?.total,
            name: albumRow?.name,
            albumArtist: albumRow?.albumArtist,
            year: albumRow?.year,
            trackCount: albumRow?.trackCount,
            discCount: albumRow?.discCount,
        },
        { total: 1, trackCount: FIXTURES.length, year: DATE },
    );

    // The joined albumArtist cannot be split back into its values because the
    // first one contains the separator. albumArtists keeps them apart, and the
    // documented rule - the first album artist value, or the first artist when
    // there is none - rebuilds from a track row the album getAlbums files it
    // under. Checked on the full rows (direct-write path) and on a projection.
    const identityOf = (row) => [
        row?.album,
        row?.albumArtists?.length ? row.albumArtists[0] : (row?.artists?.[0] ?? ""),
    ];
    const albumIdentity = JSON.stringify([albumRow?.name, albumRow?.albumArtist]);
    const searchedRows = searched?.tracks ?? [];
    const projectedRows = (
        await invoke("library.search", { query: ALBUM, limit: 50, fields: ["album", "artists", "albumArtists"] })
    )?.tracks ?? [];
    const expectedValues = JSON.stringify([ALBUM_ARTIST, ALBUM_ARTIST_TWO]);
    recorder.assertCase(
        "LR-48 albumArtists keeps each album artist value, and a track row rebuilds the album identity getAlbums uses",
        searchedRows.length === FIXTURES.length &&
            projectedRows.length === FIXTURES.length &&
            searchedRows.every(
                (row) =>
                    JSON.stringify(row?.albumArtists) === expectedValues &&
                    row?.albumArtist === [ALBUM_ARTIST, ALBUM_ARTIST_TWO].join(SEPARATOR),
            ) &&
            [...searchedRows, ...projectedRows].every(
                (row) => JSON.stringify(identityOf(row)) === albumIdentity,
            ) &&
            projectedRows.every((row) => Object.keys(row).sort().join() === "album,albumArtists,artists"),
        {
            album: { name: albumRow?.name, albumArtist: albumRow?.albumArtist },
            full: searchedRows.map((row) => ({ albumArtist: row?.albumArtist, albumArtists: row?.albumArtists })),
            projected: projectedRows,
        },
        {
            albumArtists: [ALBUM_ARTIST, ALBUM_ARTIST_TWO],
            albumArtist: [ALBUM_ARTIST, ALBUM_ARTIST_TWO].join(SEPARATOR),
            identity: [ALBUM, ALBUM_ARTIST],
            projectedKeys: ["album", "albumArtists", "artists"],
        },
    );

    const artistAlbums = await invoke("library.getArtistAlbums", { artist: ARTIST_ALPHA });
    const artistAlbumRow = (artistAlbums?.albums ?? []).find((entry) => entry?.name === ALBUM);
    recorder.assertCase(
        "LR-22 getArtistAlbums counts only the tracks that artist is on",
        artistAlbums?.success === true &&
            artistAlbums?.total === 1 &&
            artistAlbumRow?.trackCount === 2 &&
            albumRow?.trackCount === FIXTURES.length,
        {
            total: artistAlbums?.total,
            artistAlbumTrackCount: artistAlbumRow?.trackCount,
            wholeAlbumTrackCount: albumRow?.trackCount,
        },
        { artistAlbumTrackCount: 2, wholeAlbumTrackCount: FIXTURES.length },
    );

    // The recency order is checked against file modification times taken from
    // the filesystem, so the oracle does not come from the library.
    const recent = await invoke("library.getRecentlyAdded", { limit: 50, sortBy: "modified" });
    const recentPaths = pathsOf(recent?.tracks);
    const timestampGaps = (recent?.tracks ?? [])
        .filter((entry) => expectedPaths.includes(String(entry?.absolutePath)))
        .map((entry) => ({
            path: entry?.absolutePath,
            reported: entry?.modified,
            onDisk: Math.round(statSync(String(entry.absolutePath)).mtimeMs / 1000),
        }))
        .filter((entry) => Math.abs(entry.reported - entry.onDisk) > 120);
    recorder.assertCase(
        "LR-23 the newest files are reported as newest, with the timestamps the disk has",
        recent?.success === true &&
            recent?.sortBy === "modified" &&
            expectedPaths.every((path) => recentPaths.includes(path)) &&
            timestampGaps.length === 0,
        {
            sortBy: recent?.sortBy,
            fallback: recent?.fallback,
            missing: expectedPaths.filter((path) => !recentPaths.includes(path)),
            timestampGaps,
        },
        { allThreePresent: true, timestampGaps: [] },
    );

    // sortBy defaults to the playcount-backed add time, which needs a component
    // that may be absent; the endpoint reports which source answered, so the
    // fallback flag is recorded rather than assumed.
    const recentAdded = await invoke("library.getRecentlyAdded", { limit: 50 });
    const addedPaths = pathsOf(recentAdded?.tracks);
    recorder.assertCase(
        "LR-24 the default recency source also reports the copies, and says which source it used",
        recentAdded?.success === true &&
            recentAdded?.sortBy === "added" &&
            typeof recentAdded?.fallback === "boolean" &&
            expectedPaths.every((path) => addedPaths.includes(path)),
        {
            sortBy: recentAdded?.sortBy,
            fallback: recentAdded?.fallback,
            missing: expectedPaths.filter((path) => !addedPaths.includes(path)),
        },
        { allThreePresent: true, fallback: "a boolean naming the source" },
    );

    const random = await invoke("library.getRandomTracks", { count: 5 });
    const randomKeysOk = (random?.tracks ?? []).every((entry) =>
        sameSet(keysOf(entry), TRACK_ROW_KEYS),
    );
    recorder.assertCase(
        "LR-25 getRandomTracks answers with the asked-for number of well-formed rows",
        random?.success === true &&
            random?.count === 5 &&
            (random?.tracks ?? []).length === 5 &&
            randomKeysOk,
        { count: random?.count, rows: (random?.tracks ?? []).length, keysMatch: randomKeysOk },
        { count: 5, rows: 5, keysMatch: true },
    );

    return { expectedPaths, reportedPath: row.path };
}

async function runTreeCases(bridge, recorder, area, root, fixtures) {
    const { invoke } = bridge;
    const expectedPaths = fixtures.map((fixture) => fixture.targetPath);
    // The directory name comes from the filesystem, so the child row is located
    // by a name the tree did not supply.
    const childName = basename(area.dir);
    const windowsRelative = relative(root.absolutePath, area.dir);
    const insideRoot = windowsRelative !== "" && !windowsRelative.startsWith("..");

    const parent = await invoke("library.browseTree", {
        rootId: root.id,
        pathId: relative(root.absolutePath, area.base).split("\\").join("/"),
    });
    const childRow = (parent?.directories ?? []).find((entry) => entry?.name === childName);
    // The pathId is slash-separated on Windows too. Pinned because it is not the
    // spelling the platform hands you: a consumer that joins path segments with a
    // backslash gets the refusal asserted below instead of a listing.
    recorder.assertCase(
        "LR-26 the parent directory reports the child, its track count, and a slash-separated id",
        insideRoot &&
            childRow?.trackCount === FIXTURES.length &&
            childRow?.pathId === windowsRelative.split("\\").join("/") &&
            childRow?.hasChildren === false,
        {
            insideRoot,
            child: childRow && {
                name: childRow.name,
                pathId: childRow.pathId,
                trackCount: childRow.trackCount,
                hasChildren: childRow.hasChildren,
            },
        },
        {
            trackCount: FIXTURES.length,
            pathId: windowsRelative.split("\\").join("/"),
            hasChildren: false,
        },
    );

    // Descends with the id the tree itself reported, which is how a view
    // navigates; the expected file list still comes from the filesystem.
    const tree = await invoke("library.browseTree", {
        rootId: root.id,
        pathId: childRow?.pathId,
        includeFiles: true,
    });
    recorder.assertCase(
        "LR-27 browseTree lists the copies as the files of that directory",
        tree?.success === true &&
            tree?.pathId === childRow?.pathId &&
            tree?.absolutePath === area.dir &&
            sameSet(pathsOf(tree?.files), expectedPaths) &&
            (tree?.directories ?? []).length === 0,
        {
            pathId: tree?.pathId,
            absolutePath: tree?.absolutePath,
            files: pathsOf(tree?.files),
            directories: (tree?.directories ?? []).length,
        },
        { files: [...expectedPaths].sort(), directories: 0 },
    );

    const backslashed = await invoke("library.browseTree", {
        rootId: root.id,
        pathId: windowsRelative,
        includeFiles: true,
    });
    recorder.assertCase(
        "LR-28 the same directory spelled with backslashes is refused rather than listed",
        backslashed?.success === false && typeof backslashed?.error === "string",
        { pathId: windowsRelative, response: backslashed },
        { success: false, error: "a message naming the missing path" },
    );

    // browseDirectory filters on the raw path the library stores, not on the
    // absolute path every other endpoint accepts. Passing the absolute form
    // returns an empty success, which reads like an empty directory. Both halves
    // are asserted, so the day the filter learns the absolute form this fails and
    // the contract gets restated on purpose.
    const rawDirectory = fixtures[0].reportedPath.slice(
        0,
        fixtures[0].reportedPath.lastIndexOf("\\"),
    );
    const [byRaw, byAbsolute] = await Promise.all([
        invoke("library.browseDirectory", { path: rawDirectory, includeFiles: true }),
        invoke("library.browseDirectory", { path: area.dir, includeFiles: true }),
    ]);
    recorder.assertCase(
        "LR-29 browseDirectory matches the stored path form and answers empty for the absolute one",
        byRaw?.success === true &&
            sameSet(pathsOf(byRaw?.files), expectedPaths) &&
            byAbsolute?.success === true &&
            (byAbsolute?.files ?? []).length === 0,
        {
            rawDirectory,
            byRawFiles: pathsOf(byRaw?.files),
            absoluteDirectory: area.dir,
            byAbsoluteFiles: (byAbsolute?.files ?? []).length,
        },
        { byRawFiles: [...expectedPaths].sort(), byAbsoluteFiles: 0 },
    );
}

async function runCacheCases(bridge, recorder, collector) {
    const { invoke } = bridge;

    // albums, artists and status are kept by the API layer rather than by the
    // core cache; they must be dropped by the same invalidation. getAlbums keeps
    // only a complete list, hence the limit above any library this suite meets.
    await invoke("library.getAlbums", { limit: 100000 });
    await invoke("library.getArtists", {});
    await invoke("library.getStatus", {});
    const warmStats = await invoke("library.getCacheStats", {});

    const invalidated = await invoke("library.invalidateCache", {});
    const coldStats = await invoke("library.getCacheStats", {});
    recorder.assertCase(
        "LR-30 invalidateCache drops the track cache and the directory index together",
        invalidated?.success === true &&
            typeof invalidated?.timestamp === "number" &&
            coldStats?.valid === false &&
            coldStats?.tracksCached === false &&
            coldStats?.treeIndexValid === false,
        {
            timestamp: invalidated?.timestamp,
            valid: coldStats?.valid,
            tracksCached: coldStats?.tracksCached,
            treeIndexValid: coldStats?.treeIndexValid,
        },
        { valid: false, tracksCached: false, treeIndexValid: false },
    );
    recorder.assertCase(
        "LR-45 the albums, artists and status results kept between calls are dropped by the same invalidation",
        warmStats?.albumsCacheEntries >= 1 &&
            warmStats?.artistsCached === true &&
            warmStats?.statsCached === true &&
            coldStats?.albumsCacheEntries === 0 &&
            coldStats?.artistsCached === false &&
            coldStats?.statsCached === false,
        {
            warm: {
                albumsCacheEntries: warmStats?.albumsCacheEntries,
                artistsCached: warmStats?.artistsCached,
                statsCached: warmStats?.statsCached,
            },
            cold: {
                albumsCacheEntries: coldStats?.albumsCacheEntries,
                artistsCached: coldStats?.artistsCached,
                statsCached: coldStats?.statsCached,
            },
        },
        { warm: "all kept", cold: { albumsCacheEntries: 0, artistsCached: false, statsCached: false } },
    );

    const firstRoots = await invoke("library.getRoots", {});
    const secondRoots = await invoke("library.getRoots", {});
    const warmTree = await invoke("library.getCacheStats", {});
    recorder.assertCase(
        "LR-31 the first roots call builds the directory index and the second is served from it",
        firstRoots?.fromCache === false &&
            secondRoots?.fromCache === true &&
            warmTree?.treeIndexValid === true &&
            warmTree?.treeIndexedTracks === secondRoots?.indexedTracks,
        {
            firstFromCache: firstRoots?.fromCache,
            secondFromCache: secondRoots?.fromCache,
            treeIndexValid: warmTree?.treeIndexValid,
            treeIndexedTracks: warmTree?.treeIndexedTracks,
            indexedTracks: secondRoots?.indexedTracks,
        },
        { firstFromCache: false, secondFromCache: true, treeIndexValid: true },
    );

    // The opt-in async path is the only one that serializes the whole library off
    // the main thread: the call returns a receipt and the result arrives as an
    // event. Both halves have to be checked, and the receipt has to match the
    // event, otherwise a page cannot tell whose result it received.
    const total = (await invoke("library.getCount", {}))?.count;
    const receipt = await invoke("library.getAll", {
        asyncResult: true,
        limit: total + 10,
    });
    recorder.assertCase(
        "LR-32 the async full-library read answers with a receipt instead of the data",
        receipt?.pending === true && typeof receipt?.requestId === "string" && receipt.requestId !== "",
        { receipt },
        { pending: true, requestId: "a non-empty id" },
    );

    await collector.waitFor(
        (events) => events.some((event) => event.payload?.requestId === receipt?.requestId),
        { timeoutMs: eventTimeoutMs, pollMs: 100 },
    );
    const delivered = collector.received.find(
        (event) => event.payload?.requestId === receipt?.requestId,
    );
    recorder.assertCase(
        "LR-33 the result arrives on library:getAllResult under the receipt it was promised",
        delivered !== undefined &&
            delivered.payload?.total === total &&
            (delivered.payload?.tracks ?? []).length === total &&
            delivered.payload?.error === undefined &&
            delivered.payload?.fromCache === false &&
            (delivered.payload?.items ?? []).length === total &&
            delivered.payload?.offset === 0 &&
            delivered.payload?.limit === total + 10,
        {
            eventArrived: delivered !== undefined,
            total: delivered?.payload?.total,
            rows: (delivered?.payload?.tracks ?? []).length,
            items: (delivered?.payload?.items ?? []).length,
            offset: delivered?.payload?.offset,
            limit: delivered?.payload?.limit,
            error: delivered?.payload?.error,
            expectedTotal: total,
        },
        { rows: total, items: total, offset: 0, limit: total + 10, error: undefined, fromCache: false },
    );

    const afterAsync = await invoke("library.getCacheStats", {});
    const cachedPage = await invoke("library.getAll", { limit: 2 });
    recorder.assertCase(
        "LR-34 the async read leaves the cache warm, so the next read is served from it",
        afterAsync?.tracksCached === true &&
            cachedPage?.fromCache === true &&
            cachedPage?.total === total &&
            (cachedPage?.tracks ?? []).length === 2,
        {
            tracksCached: afterAsync?.tracksCached,
            fromCache: cachedPage?.fromCache,
            total: cachedPage?.total,
            rows: (cachedPage?.tracks ?? []).length,
        },
        { tracksCached: true, fromCache: true, rows: 2 },
    );
}

async function runRefusalCases(bridge, recorder, root) {
    const { invoke, invokeRaw } = bridge;

    // Same class of mistake, four endpoints, all reading their parameters
    // through the declared reader: one code and one message shape for all.
    const [noPath, noField, noQuery, noRootId] = await Promise.all([
        invoke("library.getByPath", {}),
        invoke("library.getFieldValues", {}),
        invoke("library.query", {}),
        invoke("library.browseTree", {}),
    ]);
    recorder.assertCase(
        "LR-35 a missing required parameter is refused with INVALID_PARAMS by getByPath, getFieldValues, query and browseTree",
        noPath?.success === false &&
            typeof noPath?.error === "string" &&
            noPath?.code === "INVALID_PARAMS" &&
            noField?.success === false &&
            noField?.code === "INVALID_PARAMS" &&
            noQuery?.success === false &&
            noQuery?.code === "INVALID_PARAMS" &&
            noQuery?.error === "query is required" &&
            noRootId?.success === false &&
            noRootId?.code === "INVALID_PARAMS" &&
            noRootId?.error === "rootId is required",
        {
            getByPath: { error: noPath?.error, code: noPath?.code },
            getFieldValues: { error: noField?.error, code: noField?.code },
            query: { error: noQuery?.error, code: noQuery?.code },
            browseTree: { error: noRootId?.error, code: noRootId?.code },
        },
        { everyOne: "INVALID_PARAMS", query: "query is required", browseTree: "rootId is required" },
    );

    const [unknownRoot, unknownPathId] = await Promise.all([
        invoke("library.browseTree", { rootId: "no-such-root" }),
        invoke("library.browseTree", { rootId: root.id, pathId: "no-such-directory" }),
    ]);
    recorder.assertCase(
        "LR-36 an unknown root and an unknown directory are refused with NOT_FOUND",
        unknownRoot?.success === false &&
            unknownRoot?.error === "Unknown rootId" &&
            unknownRoot?.code === "NOT_FOUND" &&
            unknownPathId?.success === false &&
            unknownPathId?.error === "Path not found" &&
            unknownPathId?.code === "NOT_FOUND",
        {
            unknownRoot: { error: unknownRoot?.error, code: unknownRoot?.code },
            unknownPathId: { error: unknownPathId?.error, code: unknownPathId?.code },
        },
        {
            unknownRoot: { error: "Unknown rootId", code: "NOT_FOUND" },
            unknownPathId: { error: "Path not found", code: "NOT_FOUND" },
        },
    );

    // Nothing found is a success with an empty result everywhere, so a page can
    // tell "no matches" from "the call failed".
    const [noAlbum, noArtist, noArtistAlbums, emptySearch] = await Promise.all([
        invoke("library.getAlbumTracks", { album: `nothing-${runId}`, albumArtist: "" }),
        invoke("library.getArtistTracks", { artist: `nothing-${runId}` }),
        invoke("library.getArtistAlbums", { artist: `nothing-${runId}` }),
        invoke("library.search", { query: `nothing-${runId}` }),
    ]);
    recorder.assertCase(
        "LR-37 a name that matches nothing is an empty success, not a failure",
        noAlbum?.success === true &&
            noAlbum?.total === 0 &&
            noAlbum?.album === `nothing-${runId}` &&
            noArtist?.success === true &&
            noArtist?.count === 0 &&
            noArtistAlbums?.success === true &&
            Array.isArray(noArtistAlbums?.albums) &&
            noArtistAlbums?.albums.length === 0 &&
            emptySearch?.success === true &&
            emptySearch?.total === 0 &&
            emptySearch?.hasMore === false,
        {
            getAlbumTracks: { success: noAlbum?.success, total: noAlbum?.total, album: noAlbum?.album },
            getArtistTracks: { success: noArtist?.success, count: noArtist?.count },
            getArtistAlbums: { success: noArtistAlbums?.success, albums: noArtistAlbums?.albums },
            search: { success: emptySearch?.success, total: emptySearch?.total },
        },
        { everyOne: "success true with an empty result" },
    );

    const outsideLibrary = await invoke("library.getByPath", {
        path: `E:\\no-such-file-${runId}.flac`,
    });
    recorder.assertCase(
        "LR-38 a path the library does not hold is reported as not found, with the path echoed back",
        outsideLibrary?.success === true &&
            outsideLibrary?.found === false &&
            outsideLibrary?.path === `E:\\no-such-file-${runId}.flac` &&
            outsideLibrary?.title === undefined,
        { response: outsideLibrary },
        { success: true, found: false, noTrackFields: true },
    );

    // fields is a projection, so a misspelt name must be refused rather than
    // silently answered with every field: the caller would otherwise get more
    // data than it asked for and never learn. null, like an omitted key, asks
    // for every field.
    const projected = await invoke("library.query", {
        query: "ALL",
        limit: 1,
        fields: ["path", "title"],
    });
    const [unknownFields, emptyFields, nullFields] = await Promise.all([
        invokeRaw("library.query", { query: "ALL", limit: 1, fields: ["path", `bogus-${runId}`] }),
        invokeRaw("library.query", { query: "ALL", limit: 1, fields: [] }),
        invokeRaw("library.query", { query: "ALL", limit: 1, fields: null }),
    ]);
    recorder.assertCase(
        "LR-39 a field projection returns only the requested keys",
        projected?.success === true &&
            (projected?.tracks ?? []).every((row) => sameSet(keysOf(row), ["path", "title"])),
        { keys: keysOf((projected?.tracks ?? [])[0]) },
        { keys: ["path", "title"] },
    );

    recorder.assertCase(
        "LR-40 an unknown name and an empty list are refused with INVALID_PARAMS, and null reads as every field",
        unknownFields?.value?.success === false &&
            unknownFields?.value?.code === "INVALID_PARAMS" &&
            JSON.stringify(unknownFields?.value?.details?.unknownFields) ===
                JSON.stringify([`bogus-${runId}`]) &&
            emptyFields?.value?.code === "INVALID_PARAMS" &&
            nullFields?.value?.success === true &&
            (nullFields?.value?.tracks ?? []).every((row) => sameSet(keysOf(row), TRACK_ROW_KEYS)),
        {
            unknown: {
                code: unknownFields?.value?.code,
                details: unknownFields?.value?.details,
            },
            empty: { code: emptyFields?.value?.code, error: emptyFields?.value?.error },
            null: {
                success: nullFields?.value?.success,
                keys: keysOf((nullFields?.value?.tracks ?? [])[0]),
            },
        },
        {
            unknown: { code: "INVALID_PARAMS", unknownFields: [`bogus-${runId}`] },
            empty: { code: "INVALID_PARAMS" },
            null: { success: true, keys: [...TRACK_ROW_KEYS].sort() },
        },
    );

    // Recorded rather than asserted as a contract: a query the host's parser
    // cannot make sense of comes back as an empty match, not as a syntax error,
    // so a page has no way to tell a typo from a genuinely empty result.
    const malformed = await invoke("library.query", { query: "%album% IS(", limit: 1 });
    recorder.assertCase(
        "LR-41 a malformed query is answered as an empty match rather than a syntax error",
        malformed?.success === true && malformed?.total === 0,
        {
            success: malformed?.success,
            total: malformed?.total,
            characterization: "no syntax error is surfaced to the caller",
        },
        { success: true, total: 0 },
    );
}

async function runPlaylistCases(bridge, recorder, fixtures, playlistIndex) {
    const { invoke } = bridge;
    const expectedPaths = fixtures.map((fixture) => fixture.targetPath);

    const added = await invoke("library.addToPlaylist", {
        paths: expectedPaths,
        playlist: playlistIndex,
    });
    // Verified through the playlist namespace: asking the library whether its own
    // insert worked would prove nothing about what the playlist holds.
    const contents = await invoke("playlist.getTracks", { playlist: playlistIndex });
    const rows = Array.isArray(contents) ? contents : (contents?.tracks ?? contents?.items ?? []);
    recorder.assertCase(
        "LR-42 addToPlaylist puts the tracks where the playlist namespace can see them",
        added?.success === true &&
            added?.added === FIXTURES.length &&
            sameSet(pathsOf(rows), expectedPaths),
        { response: added, playlistPaths: pathsOf(rows) },
        { added: FIXTURES.length, playlistPaths: [...expectedPaths].sort() },
    );

    const [noPaths, badPlaylist] = await Promise.all([
        invoke("library.addToPlaylist", { paths: [], playlist: playlistIndex }),
        invoke("library.addToPlaylist", { paths: expectedPaths, playlist: 99999 }),
    ]);
    recorder.assertCase(
        "LR-43 an empty path list is refused as INVALID_PARAMS and an out-of-range playlist as INVALID_INDEX",
        noPaths?.success === false &&
            noPaths?.code === "INVALID_PARAMS" &&
            typeof noPaths?.error === "string" &&
            badPlaylist?.success === false &&
            badPlaylist?.code === "INVALID_INDEX" &&
            typeof badPlaylist?.error === "string",
        { noPaths: { error: noPaths?.error, code: noPaths?.code }, badPlaylist: { error: badPlaylist?.error, code: badPlaylist?.code } },
        { noPaths: "INVALID_PARAMS", badPlaylist: "INVALID_INDEX" },
    );

    // Characterization: a path with no file behind it still becomes a handle, so
    // the insert reports success and the playlist gains an unplayable entry. The
    // endpoint has no existence check, which is worth knowing before treating
    // `added` as "these tracks are playable".
    const ghost = await invoke("library.addToPlaylist", {
        paths: [`E:\\no-such-file-${runId}.flac`],
        playlist: playlistIndex,
    });
    recorder.assertCase(
        "LR-44 a path with no file behind it is still added, without an existence check",
        ghost?.success === true && ghost?.added === 1,
        {
            response: ghost,
            characterization: "handles are created from the spelling, not from the disk",
        },
        { success: true, added: 1 },
    );

    // Being in a playlist makes a path trusted for later media reads and writes, so
    // the insert itself has to pass the media read check. A system directory is
    // refused whether or not the file exists; nothing may reach the playlist.
    const countBefore = (await invoke("playlist.getTrackCount", { playlist: playlistIndex }))?.count;
    const denied = await invoke("library.addToPlaylist", {
        paths: [`C:\\Windows\\e2e-no-such-${runId}.flac`],
        playlist: playlistIndex,
    });
    const countAfter = (await invoke("playlist.getTrackCount", { playlist: playlistIndex }))?.count;
    recorder.assertCase(
        "LR-46 a path the media read check denies is refused with PERMISSION_DENIED and nothing is added",
        denied?.success === false &&
            denied?.code === "PERMISSION_DENIED" &&
            typeof denied?.error === "string" &&
            Number.isInteger(countBefore) &&
            countAfter === countBefore,
        { response: denied, trackCount: { before: countBefore, after: countAfter } },
        { success: false, code: "PERMISSION_DENIED", trackCount: "unchanged" },
    );

    await runLockedPlaylistCase(bridge, recorder, expectedPaths);
}

// An autoplaylist carries foobar2000's own lock, the one lock a suite can place
// without a third-party component. Its query matches nothing, so the list starts
// empty and any track in it afterwards came from the call under test.
async function runLockedPlaylistCase(bridge, recorder, paths) {
    const { invoke } = bridge;
    const name = "LR-47 a locked playlist is refused with LOCKED and nothing is added";
    const auto = await invoke("playlist.createAutoplaylist", {
        name: `${SCRATCH_NAME} locked`,
        query: `title IS e2e-no-match-${runId}`,
    });
    lockedIndex = auto?.index;
    const lock = Number.isInteger(lockedIndex)
        ? await invoke("playlist.isLocked", { playlist: lockedIndex })
        : undefined;
    if (lock?.isLocked !== true) {
        recorder.assertCase(
            name,
            false,
            { autoplaylist: auto, lock, reason: "no locked playlist to add to" },
            { isLocked: true },
        );
        return;
    }

    const before = (await invoke("playlist.getTrackCount", { playlist: lockedIndex }))?.count;
    const refused = await invoke("library.addToPlaylist", { paths, playlist: lockedIndex });
    const after = (await invoke("playlist.getTrackCount", { playlist: lockedIndex }))?.count;
    recorder.assertCase(
        name,
        refused?.success === false &&
            refused?.code === "LOCKED" &&
            refused?.details?.playlist === lockedIndex &&
            refused?.details?.isLocked === true &&
            Number.isInteger(before) &&
            after === before,
        { response: refused, trackCount: { before, after } },
        {
            success: false,
            code: "LOCKED",
            details: { playlist: lockedIndex, isLocked: true },
            trackCount: "unchanged",
        },
    );
}

const recorder = createRecorder();
let client;
let bridge;
let area;
let collector;
let scratchIndex;
let lockedIndex;
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

    const surface = await runSurfaceCases(bridge, recorder);

    const resolved = await resolveFixtureArea(bridge, { runId });
    area = resolved.area;
    const root = resolved.root;
    context = {
        libraryRoot: resolved.root.absolutePath,
        fixtureDir: area.dir,
        source: resolved.source,
        baselineCount: surface.baselineCount,
    };
    console.log(`Fixture area: ${area.dir}`);

    const fixtures = FIXTURES.map((fixture) => ({
        ...fixture,
        ...area.taggedAudio(fixture.name, fixture.entries),
    }));

    // The copies are picked up on the host's own schedule, so membership is
    // waited for rather than assumed. Only the first wait asks for a rescan; the
    // sweep it triggers covers the whole directory.
    const landed = [];
    for (const [position, fixture] of fixtures.entries()) {
        const arrival = await waitForLibraryEntry(bridge, fixture.targetPath, {
            rescan: position === 0,
        });
        landed.push({ path: fixture.targetPath, waitedMs: arrival?.waitedMs ?? null });
        fixture.reportedPath = arrival?.entry?.path;
    }
    context.landed = landed;

    if (landed.some((entry) => entry.waitedMs === null)) {
        recorder.assertCase(
            "LR-09 the three copies raised the library count by exactly three",
            false,
            { landed, reason: "a copy never became a library member" },
            { allThreeInLibrary: true },
        );
    } else {
        const fixtureFacts = await runFixtureCases(
            bridge,
            recorder,
            fixtures,
            surface.baselineCount,
        );
        context.reportedPath = fixtureFacts.reportedPath;
        await runTreeCases(bridge, recorder, area, root, fixtures);

        collector = await createEventCollector(bridge, ["library:getAllResult"], {
            collectorId: runId,
        });
        await runCacheCases(bridge, recorder, collector);
        await runRefusalCases(bridge, recorder, root);

        const created = await bridge.invoke("playlist.create", { name: SCRATCH_NAME });
        scratchIndex = created?.index;
        if (Number.isInteger(scratchIndex)) {
            await runPlaylistCases(bridge, recorder, fixtures, scratchIndex);
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
        if (area) {
            const cleanup = area.remove();
            if (context) context.cleanup = cleanup;
            // The copies were library members, so the library is asked to notice
            // they are gone; otherwise it keeps reporting dead entries.
            await quiet("library.refresh", {});
        }
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
            targets,
        },
    }),
);

/**
 * Covers album identity: library.getAlbumTracks named by a library.getAlbums
 * row must return exactly the tracks that row groups, and the grouping behind
 * both is kept between calls.
 *
 * The fixtures are two albums put into the monitored root. One is a
 * compilation without ALBUMARTIST on most tracks, so getAlbums files it under
 * each track's first ARTIST value; one of its tracks credits two artists and
 * one names an album artist. These are the tracks a looser match (any ALBUM
 * value, any ALBUMARTIST or ARTIST value) pulls into the wrong album. The other
 * album spans two discs with its tracks written out of order.
 *
 * Which album each fixture belongs to is worked out here from the tags this
 * script writes, by the rule the host documents, rather than read back from
 * another library query.
 *
 * Usage: node mcp/tests/e2e-library-albums.mjs
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

const invokeTimeoutMs = envInt("FB2K_E2E_TIMEOUT_MS", 30000);
const runId = new Date().toISOString().replace(/[-:.TZ]/g, "");

const COMPILATION = `E2E Album Key Compilation ${runId}`;
const TWO_DISCS = `E2E Album Key Two Discs ${runId}`;
const ARTIST_ONE = `E2E Album Key Artist One ${runId}`;
const ARTIST_TWO = `E2E Album Key Artist Two ${runId}`;
const VARIOUS = `E2E Album Key Various ${runId}`;
const DISC_ARTIST = `E2E Album Key Disc Artist ${runId}`;

const FIXTURES = [
    {
        name: "key-comp-1.flac",
        entries: [
            ["TITLE", "Compilation one"],
            ["ALBUM", COMPILATION],
            ["ARTIST", ARTIST_ONE],
            ["TRACKNUMBER", "1"],
        ],
    },
    {
        name: "key-comp-2.flac",
        entries: [
            ["TITLE", "Compilation two"],
            ["ALBUM", COMPILATION],
            ["ARTIST", ARTIST_TWO],
            ["TRACKNUMBER", "2"],
        ],
    },
    {
        // First artist two, so it belongs with track 2; artist one is only its
        // second credit.
        name: "key-comp-3.flac",
        entries: [
            ["TITLE", "Compilation three"],
            ["ALBUM", COMPILATION],
            ["ARTIST", ARTIST_TWO],
            ["ARTIST", ARTIST_ONE],
            ["TRACKNUMBER", "3"],
        ],
    },
    {
        // Artist one again, but the album artist decides.
        name: "key-comp-4.flac",
        entries: [
            ["TITLE", "Compilation four"],
            ["ALBUM", COMPILATION],
            ["ALBUMARTIST", VARIOUS],
            ["ARTIST", ARTIST_ONE],
            ["TRACKNUMBER", "4"],
        ],
    },
    {
        name: "key-disc-a.flac",
        entries: [
            ["TITLE", "Disc two, track one"],
            ["ALBUM", TWO_DISCS],
            ["ALBUMARTIST", DISC_ARTIST],
            ["ARTIST", DISC_ARTIST],
            ["DISCNUMBER", "2"],
            ["TRACKNUMBER", "1"],
        ],
    },
    {
        name: "key-disc-b.flac",
        entries: [
            ["TITLE", "Disc one, track two"],
            ["ALBUM", TWO_DISCS],
            ["ALBUMARTIST", DISC_ARTIST],
            ["ARTIST", DISC_ARTIST],
            ["DISCNUMBER", "1"],
            ["TRACKNUMBER", "2"],
        ],
    },
    {
        name: "key-disc-c.flac",
        entries: [
            ["TITLE", "Disc one, track one"],
            ["ALBUM", TWO_DISCS],
            ["ALBUMARTIST", DISC_ARTIST],
            ["ARTIST", DISC_ARTIST],
            ["DISCNUMBER", "1"],
            ["TRACKNUMBER", "1"],
        ],
    },
];

/** The two-disc album in the order the host must return it. */
const DISC_ORDER = ["key-disc-c.flac", "key-disc-b.flac", "key-disc-a.flac"];

function firstValue(entries, key) {
    return entries.find(([name]) => name === key)?.[1];
}

/**
 * The album a fixture is filed under: the first ALBUM value and the first
 * ALBUMARTIST value, or the first ARTIST value when there is no ALBUMARTIST.
 */
function albumKeyOf(entries) {
    const albumArtist = firstValue(entries, "ALBUMARTIST") ?? firstValue(entries, "ARTIST") ?? "";
    return `${firstValue(entries, "ALBUM")}\u0000${albumArtist}`;
}

function rowKey(row) {
    return `${row?.name}\u0000${row?.albumArtist}`;
}

function pathsOf(rows) {
    return (rows ?? []).map((row) => String(row?.absolutePath ?? ""));
}

function sameSet(left, right) {
    return JSON.stringify([...left].sort()) === JSON.stringify([...right].sort());
}

async function timed(action) {
    const started = performance.now();
    const value = await action();
    return { value, ms: Math.round(performance.now() - started) };
}

async function runKeyCases(bridge, recorder, fixtures) {
    const { invoke } = bridge;

    const expected = new Map();
    for (const fixture of fixtures) {
        const key = albumKeyOf(fixture.entries);
        if (!expected.has(key)) expected.set(key, []);
        expected.get(key).push(fixture.targetPath);
    }

    const listed = await invoke("library.getAlbums", { query: runId, limit: 100 });
    const rows = listed?.albums ?? [];
    recorder.assertCase(
        "LA-01 getAlbums files the fixtures under the albums their first album artist or artist names",
        listed?.success === true &&
            sameSet(rows.map(rowKey), [...expected.keys()]) &&
            rows.every((row) => row?.trackCount === expected.get(rowKey(row))?.length),
        rows.map((row) => ({ name: row?.name, albumArtist: row?.albumArtist, trackCount: row?.trackCount })),
        [...expected].map(([key, paths]) => ({ key: key.replace("\u0000", " / "), trackCount: paths.length })),
    );

    const answers = [];
    for (const row of rows) {
        const answer = await invoke("library.getAlbumTracks", {
            album: row.name,
            albumArtist: row.albumArtist,
        });
        answers.push({ row, answer });
    }
    recorder.assertCase(
        "LA-02 getAlbumTracks returns exactly the tracks of the row, so total equals trackCount",
        answers.length === expected.size &&
            answers.every(
                ({ row, answer }) =>
                    answer?.success === true &&
                    answer?.total === row.trackCount &&
                    sameSet(pathsOf(answer?.tracks), expected.get(rowKey(row)) ?? []),
            ),
        answers.map(({ row, answer }) => ({
            album: row.name,
            albumArtist: row.albumArtist,
            trackCount: row.trackCount,
            total: answer?.total,
            paths: pathsOf(answer?.tracks),
        })),
        [...expected].map(([key, paths]) => ({ key: key.replace("\u0000", " / "), paths })),
    );

    const everyPath = answers.flatMap(({ answer }) => pathsOf(answer?.tracks));
    recorder.assertCase(
        "LA-03 no track is returned under two albums, and none is left out",
        everyPath.length === fixtures.length &&
            sameSet(everyPath, fixtures.map((fixture) => fixture.targetPath)),
        { returned: everyPath.length, distinct: new Set(everyPath).size },
        { returned: fixtures.length, distinct: fixtures.length },
    );

    recorder.assertCase(
        "LA-04 the answer carries the same row getAlbums listed, and echoes the key it was given",
        answers.every(
            ({ row, answer }) =>
                JSON.stringify(answer?.row) === JSON.stringify(row) &&
                answer?.album === row.name &&
                answer?.albumArtist === row.albumArtist &&
                answer?.artist === undefined,
        ),
        answers.map(({ answer }) => ({
            album: answer?.album,
            albumArtist: answer?.albumArtist,
            row: answer?.row,
        })),
        { row: "equal to the getAlbums row", album: "row.name", albumArtist: "row.albumArtist" },
    );

    const discs = answers.find(({ row }) => row.name === TWO_DISCS)?.answer;
    const discPaths = DISC_ORDER.map(
        (name) => fixtures.find((fixture) => fixture.name === name)?.targetPath,
    );
    recorder.assertCase(
        "LA-05 tracks come in disc order, then track order, and index counts that order",
        JSON.stringify(pathsOf(discs?.tracks)) === JSON.stringify(discPaths) &&
            JSON.stringify((discs?.tracks ?? []).map((t) => [t?.discNumber, t?.trackNumber])) ===
                JSON.stringify([
                    [1, 1],
                    [1, 2],
                    [2, 1],
                ]) &&
            (discs?.tracks ?? []).every((t, i) => t?.index === i) &&
            JSON.stringify(pathsOf(discs?.items)) === JSON.stringify(discPaths),
        {
            paths: pathsOf(discs?.tracks),
            numbers: (discs?.tracks ?? []).map((t) => [t?.discNumber, t?.trackNumber]),
            index: (discs?.tracks ?? []).map((t) => t?.index),
        },
        { paths: discPaths, numbers: [[1, 1], [1, 2], [2, 1]], index: [0, 1, 2] },
    );

    // The second credit of track 3 and the artist of track 4 are artist one,
    // yet neither track is on the album filed under artist one.
    const [byArtistOne, byNobody, emptyName] = await Promise.all([
        invoke("library.getAlbumTracks", { album: COMPILATION, albumArtist: ARTIST_ONE }),
        invoke("library.getAlbumTracks", { album: COMPILATION, albumArtist: `nobody ${runId}` }),
        invoke("library.getAlbumTracks", { album: "", albumArtist: "" }),
    ]);
    const onlyTrackOne = fixtures.find((fixture) => fixture.name === "key-comp-1.flac")?.targetPath;
    recorder.assertCase(
        "LA-06 an artist credited second, or under another album artist, does not pull a track in",
        byArtistOne?.success === true &&
            byArtistOne?.total === 1 &&
            JSON.stringify(pathsOf(byArtistOne?.tracks)) === JSON.stringify([onlyTrackOne]),
        { total: byArtistOne?.total, paths: pathsOf(byArtistOne?.tracks) },
        { total: 1, paths: [onlyTrackOne] },
    );
    recorder.assertCase(
        "LA-07 a name and album artist no row has is an empty success without a row",
        byNobody?.success === true &&
            byNobody?.total === 0 &&
            Array.isArray(byNobody?.tracks) &&
            byNobody.tracks.length === 0 &&
            byNobody?.row === undefined &&
            emptyName?.success === true &&
            emptyName?.total === 0 &&
            emptyName?.row === undefined,
        {
            unknown: { success: byNobody?.success, total: byNobody?.total, row: byNobody?.row },
            emptyName: { success: emptyName?.success, total: emptyName?.total, row: emptyName?.row },
        },
        { everyOne: "success true, total 0, no row" },
    );

    const [noAlbumArtist, oldKey] = await Promise.all([
        invoke("library.getAlbumTracks", { album: COMPILATION }),
        invoke("library.getAlbumTracks", { album: COMPILATION, albumArtist: "", artist: ARTIST_ONE }),
    ]);
    recorder.assertCase(
        "LA-08 albumArtist is required and the artist key is refused, both as INVALID_PARAMS",
        noAlbumArtist?.success === false &&
            noAlbumArtist?.code === "INVALID_PARAMS" &&
            String(noAlbumArtist?.error).includes("albumArtist") &&
            oldKey?.success === false &&
            oldKey?.code === "INVALID_PARAMS" &&
            String(oldKey?.error).includes("artist"),
        {
            missing: { code: noAlbumArtist?.code, error: noAlbumArtist?.error },
            artistKey: { code: oldKey?.code, error: oldKey?.error },
        },
        { missing: "INVALID_PARAMS naming albumArtist", artistKey: "INVALID_PARAMS naming artist" },
    );

    return rows;
}

async function runCacheCases(bridge, recorder, rows) {
    const { invoke } = bridge;
    const row = rows.find((candidate) => candidate?.name === TWO_DISCS) ?? rows[0];
    const params = { album: row?.name, albumArtist: row?.albumArtist };
    const timings = {};

    // getAlbums keeps the grouping, so the lookups after it are hits.
    await invoke("library.getAlbums", { query: runId, limit: 100 });
    const before = await invoke("library.getCacheStats", {});
    const first = await timed(() => invoke("library.getAlbumTracks", params));
    const second = await timed(() => invoke("library.getAlbumTracks", params));
    const after = await invoke("library.getCacheStats", {});
    timings.warm = [first.ms, second.ms];
    recorder.assertCase(
        "LA-09 after getAlbums, repeated lookups of one album are answered from the kept grouping",
        first.value?.total === row?.trackCount &&
            second.value?.total === row?.trackCount &&
            after?.cacheHits - before?.cacheHits === 2 &&
            after?.cacheMisses === before?.cacheMisses,
        {
            hits: after?.cacheHits - before?.cacheHits,
            misses: after?.cacheMisses - before?.cacheMisses,
            totals: [first.value?.total, second.value?.total],
        },
        { hits: 2, misses: 0, totals: [row?.trackCount, row?.trackCount] },
    );

    // invalidateCache moves the generation like a library change does.
    await invoke("library.invalidateCache", {});
    const cold = await invoke("library.getCacheStats", {});
    const rebuilt = await timed(() => invoke("library.getAlbumTracks", params));
    const warmAgain = await timed(() => invoke("library.getAlbumTracks", params));
    const end = await invoke("library.getCacheStats", {});
    timings.afterInvalidate = [rebuilt.ms, warmAgain.ms];
    recorder.assertCase(
        "LA-10 after an invalidation the first lookup rebuilds the grouping and the next one reuses it",
        rebuilt.value?.total === row?.trackCount &&
            warmAgain.value?.total === row?.trackCount &&
            end?.cacheMisses - cold?.cacheMisses === 1 &&
            end?.cacheHits - cold?.cacheHits === 1,
        {
            hits: end?.cacheHits - cold?.cacheHits,
            misses: end?.cacheMisses - cold?.cacheMisses,
            totals: [rebuilt.value?.total, warmAgain.value?.total],
        },
        { hits: 1, misses: 1, totals: [row?.trackCount, row?.trackCount] },
    );
    return timings;
}

const recorder = createRecorder();
let client;
let bridge;
let area;
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

    const enabled = await bridge.invoke("library.isEnabled", {});
    if (enabled?.enabled !== true) {
        blocked = true;
        throw new Error("the media library is disabled, so no album can be looked up");
    }

    const resolved = await resolveFixtureArea(bridge, { runId });
    area = resolved.area;
    context = { libraryRoot: resolved.root.absolutePath, fixtureDir: area.dir, source: resolved.source };
    console.log(`Fixture area: ${area.dir}`);

    const fixtures = FIXTURES.map((fixture) => ({
        ...fixture,
        ...area.taggedAudio(fixture.name, fixture.entries),
    }));

    const landed = [];
    for (const [position, fixture] of fixtures.entries()) {
        const arrival = await waitForLibraryEntry(bridge, fixture.targetPath, {
            rescan: position === 0,
        });
        landed.push({ path: fixture.targetPath, waitedMs: arrival?.waitedMs ?? null });
    }
    context.landed = landed;

    if (landed.some((entry) => entry.waitedMs === null)) {
        blocked = true;
        throw new Error("a fixture never became a library member");
    }
    const rows = await runKeyCases(bridge, recorder, fixtures);
    context.timingsMs = await runCacheCases(bridge, recorder, rows);
} catch (error) {
    fatalError = error;
    blocked = blocked || Boolean(error?.blocked);
    console.error(`${blocked ? "BLOCKED" : "ERROR"}: ${error?.message || error}`);
} finally {
    if (bridge && area) {
        const cleanup = area.remove();
        if (context) context.cleanup = cleanup;
        // The copies were library members, so the library is asked to notice
        // they are gone; otherwise it keeps reporting dead entries.
        await bridge.invokeRaw("library.refresh", {}).catch(() => undefined);
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
            context,
            targets,
        },
    }),
);

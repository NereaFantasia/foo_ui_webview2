/**
 * Pins the multi-value tag contract on a running instance: a field with several
 * values must report every value, in file order, and its joined form must agree
 * across the three code paths that produce it.
 *
 * The bug this guards against lost every value but the first, which no mock can
 * see: the loss happened while reading real tags. Each case therefore compares
 * two independent producers rather than an API against itself -
 * library.query's snapshot (MetaValuesRaw + JoinMetaValues), metadata.read's own
 * enumeration, and foobar2000's titleformat engine.
 *
 * Library cases run read-only over tracks that already carry multi-value tags.
 * The fixture cases add a controlled probe: a retagged FLAC copy whose value
 * order and duplicates are known before the bridge sees it, written out of band
 * so the fixture shares no code with what it checks. The copy is removed again
 * with node's own fs, not through the bridge.
 *
 * Usage: node mcp/tests/e2e-metadata-multivalue.mjs
 * Environment: FB2K_CDP_PORT, FB2K_E2E_TIMEOUT_MS, FB2K_FIXTURE_DIR,
 * FB2K_LIBRARY_SCAN_LIMIT.
 */

import { existsSync, rmSync, statSync } from "node:fs";
import { join } from "node:path";
import { tmpdir } from "node:os";

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
import { valuesOf, writeTaggedFlacCopy } from "./lib/flac-fixture.mjs";

const invokeTimeoutMs = envInt("FB2K_E2E_TIMEOUT_MS", 30000);
const scanLimit = envInt("FB2K_LIBRARY_SCAN_LIMIT", 3000);
const fixtureDir = process.env.FB2K_FIXTURE_DIR || tmpdir();
const runId = new Date().toISOString().replace(/[-:.TZ]/g, "");
const fixturePath = join(fixtureDir, `fb2k-multivalue-fixture-${runId}.flac`);

const SEPARATOR = ", ";
const SAMPLE_SIZE = 5;

// One repeated value covers the "duplicates are kept" half of the join
// contract, which real library data cannot be relied on to provide.
const FIXTURE_ENTRIES = [
    ["TITLE", "Multi-value fixture"],
    ["ALBUM", "foo_ui_webview2 e2e"],
    ["ARTIST", "Probe Alpha"],
    ["ARTIST", "Probe Beta"],
    ["ARTIST", "Probe Alpha"],
    ["GENRE", "Genre One"],
    ["GENRE", "Genre Two"],
];

function asArray(value) {
    if (Array.isArray(value)) return value;
    return value === undefined ? [] : [value];
}

// metadata.read echoes each key exactly as the file spells it, so a lookup by a
// fixed case finds nothing on a file whose tags are uppercase.
function tagEntry(tags, key) {
    const wanted = key.toLowerCase();
    const found = Object.keys(tags ?? {}).find((name) => name.toLowerCase() === wanted);
    return found === undefined ? undefined : tags[found];
}

function tagValues(tags, key) {
    return asArray(tagEntry(tags, key));
}

async function runLibraryCases(bridge, recorder) {
    const page = await bridge.invoke("library.query", { query: "ALL", limit: scanLimit });
    const tracks = Array.isArray(page?.tracks) ? page.tracks : [];
    const multi = tracks.filter(
        (track) => Array.isArray(track.artists) && track.artists.length > 1,
    );

    recorder.assertCase(
        "MV-01 the scanned library page contains multi-value artist tracks",
        multi.length > 0,
        { scanned: tracks.length, libraryTotal: page?.total, multiValue: multi.length },
        { multiValueAtLeast: 1 },
    );

    const joinMismatches = multi
        .filter((track) => track.artists.join(SEPARATOR) !== track.artist)
        .map((track) => ({
            path: track.absolutePath,
            artist: track.artist,
            artists: track.artists,
        }));
    recorder.assertCase(
        "MV-02 library artist equals artists joined by the documented separator",
        joinMismatches.length === 0,
        { checked: multi.length, mismatches: joinMismatches.slice(0, 5) },
        { mismatches: [] },
    );

    const sample = multi.slice(0, SAMPLE_SIZE);
    const readMismatches = [];
    for (const track of sample) {
        const read = await bridge.invoke("metadata.read", { path: track.absolutePath });
        const readValues = tagValues(read?.tags, "artist");
        if (JSON.stringify(readValues) !== JSON.stringify(track.artists)) {
            readMismatches.push({
                path: track.absolutePath,
                metadataRead: readValues,
                libraryArtists: track.artists,
            });
        }
    }

    recorder.assertCase(
        "MV-03 metadata.read enumerates the same artist values as the library snapshot",
        sample.length > 0 && readMismatches.length === 0,
        { sampled: sample.length, mismatches: readMismatches },
        { sampledAtLeast: 1, mismatches: [] },
    );

    // Sweeping the whole page through foobar2000's own formatter is what makes
    // this a value-loss sentinel rather than a spot check: it needs no hint from
    // the snapshot about which tracks are multi-value, so a snapshot that
    // reported only first values would be caught on every affected track.
    // Rows are restricted to subsong 0 because the batch endpoint always builds
    // handles at index 0.
    const batchTracks = tracks.filter(
        (track) => track.absolutePath && (track.subsong ?? 0) === 0,
    );
    const batch = await bridge.invoke("titleformat.evalBatch", {
        paths: batchTracks.map((track) => track.absolutePath),
        pattern: "%artist%",
    });
    // The path layer validates the whole paths[] before the handler runs, and a
    // single refused entry answers the entire batch with a resolved
    // { success: false, error } that has no results at all. Without echoing that
    // envelope, a refused batch and an empty library look identical here
    // (compared: 0), so the refusal is carried into the observed side verbatim,
    // along with per-row failures and rows whose track info was not ready.
    const rows = Array.isArray(batch?.results) ? batch.results : [];
    const batchEnvelope = {
        success: batch?.success,
        total: batch?.total,
        successCount: batch?.successCount,
        errorCount: batch?.errorCount,
        error: batch?.error,
    };
    const rowFailures = rows.filter((row) => row?.success !== true);
    const rowsWithoutInfo = rows.filter(
        (row) => row?.success === true && row?.infoAvailable !== true,
    );
    const byPath = new Map(batchTracks.map((track) => [track.absolutePath, track]));
    let compared = 0;
    let skippedEmpty = 0;
    const formatMismatches = [];
    for (const row of rows) {
        if (row?.success !== true || row?.infoAvailable !== true) continue;
        const track = byPath.get(row.path);
        if (!track) continue;
        // The two producers spell absence differently: the snapshot yields an
        // empty string, the formatter its own unknown-field marker. Only tracks
        // that carry an artist can be compared value against value.
        if (!track.artist) {
            skippedEmpty += 1;
            continue;
        }
        compared += 1;
        if (row.result !== track.artist) {
            formatMismatches.push({
                path: row.path,
                titleformat: row.result,
                libraryArtist: track.artist,
            });
        }
    }

    recorder.assertCase(
        "MV-04 titleformat %artist% equals the library artist for every scanned track",
        compared > 0 && formatMismatches.length === 0,
        {
            sent: batchTracks.length,
            batchEnvelope,
            rows: rows.length,
            rowFailures: rowFailures.length,
            rowFailureSample: rowFailures.slice(0, 3),
            rowsWithoutInfo: rowsWithoutInfo.length,
            compared,
            skippedEmpty,
            mismatches: formatMismatches.slice(0, 5),
        },
        { batchEnvelope: { success: true }, comparedAtLeast: 1, mismatches: [] },
    );

    return tracks;
}

async function runFixtureCases(bridge, recorder, tracks) {
    const source = tracks
        .filter((track) => String(track.absolutePath || "").toLowerCase().endsWith(".flac"))
        .filter((track) => Number.isFinite(track.fileSize) && track.fileSize > 0)
        .sort((a, b) => a.fileSize - b.fileSize)[0];

    if (!source) {
        recorder.assertCase(
            "MV-05 a FLAC source is available to build the controlled probe",
            false,
            { reason: "no FLAC track with a known size in the scanned page" },
            { flacSource: "found" },
        );
        return;
    }

    const written = writeTaggedFlacCopy({
        sourcePath: source.absolutePath,
        targetPath: fixturePath,
        entries: FIXTURE_ENTRIES,
    });
    const expectedArtists = valuesOf(written.entries, "ARTIST");
    const expectedGenres = valuesOf(written.entries, "GENRE");

    const read = await bridge.invoke("metadata.read", { path: fixturePath });
    const readArtists = tagValues(read?.tags, "artist");
    const readGenres = tagValues(read?.tags, "genre");

    recorder.assertCase(
        "MV-05 metadata.read returns every written artist value in file order",
        JSON.stringify(readArtists) === JSON.stringify(expectedArtists),
        { source: source.absolutePath, sizeBytes: statSync(fixturePath).size, readArtists },
        { readArtists: expectedArtists },
    );

    recorder.assertCase(
        "MV-06 metadata.read keeps duplicate values instead of collapsing them",
        readArtists.length === expectedArtists.length &&
            new Set(readArtists).size < readArtists.length,
        { readArtists },
        { length: expectedArtists.length, containsDuplicate: true },
    );

    recorder.assertCase(
        "MV-07 a second multi-value field behaves the same way",
        JSON.stringify(readGenres) === JSON.stringify(expectedGenres),
        { readGenres },
        { readGenres: expectedGenres },
    );

    recorder.assertCase(
        "MV-08 a single-value field stays a string, not a one-element array",
        typeof tagEntry(read?.tags, "title") === "string",
        { title: tagEntry(read?.tags, "title") },
        { titleType: "string" },
    );

    recorder.assertCase(
        "MV-09 tag keys are echoed with the file's own casing",
        Object.prototype.hasOwnProperty.call(read?.tags ?? {}, "ARTIST") &&
            !Object.prototype.hasOwnProperty.call(read?.tags ?? {}, "artist"),
        { keys: Object.keys(read?.tags ?? {}) },
        { keys: ["as written in the file, here uppercase"] },
    );

    // The fixture lives outside the library, so foobar2000 has no info for it in
    // its cache, where it holds only a placeholder. The host reads a local file
    // like this one from disk instead, so the multi-value artist comes out joined
    // and infoAvailable is true; before that, the placeholder rendered "?".
    const formatted = await bridge.invoke("titleformat.eval", {
        path: fixturePath,
        pattern: "%artist%",
    });
    const joined = expectedArtists.join(SEPARATOR);
    recorder.assertCase(
        "MV-10 titleformat.eval reads the tags of a file outside the library from the file itself",
        formatted?.infoAvailable === true && formatted?.result === joined,
        { infoAvailable: formatted?.infoAvailable, result: formatted?.result },
        { infoAvailable: true, result: joined },
    );
}

const recorder = createRecorder();
let client;
let blocked = false;
let fatalError;
let targets;

try {
    const connection = await connectBridgePage();
    client = connection.client;
    targets = connection.candidates;
    console.log(`Target: ${connection.page.title} ${connection.page.url}`);
    const bridge = createBridge(client.Runtime, { invokeTimeoutMs });
    await requireResponsiveBridge(bridge, { probeId: runId });

    const tracks = await runLibraryCases(bridge, recorder);
    await runFixtureCases(bridge, recorder, tracks);
} catch (error) {
    fatalError = error;
    blocked = Boolean(error?.blocked);
    console.error(`${blocked ? "BLOCKED" : "ERROR"}: ${error?.message || error}`);
} finally {
    if (existsSync(fixturePath)) {
        rmSync(fixturePath, { force: true });
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
            scanLimit,
            fixturePath,
            targets,
        },
    }),
);

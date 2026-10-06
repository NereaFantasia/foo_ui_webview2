/**
 * Covers the metadata.* readers and batch endpoints the write suite left alone
 * (readByPath, readRaw, readBatch, probeBatchAsync, cancelProbe, writeBatch) and
 * the two remaining playcount endpoints (getBatch, getStats).
 *
 * Fixtures are copies inside the monitored library root, tagged out of band by
 * writing the VORBIS_COMMENT block directly (lib/flac-fixture.mjs), so nothing a
 * case asserts about a tag was produced by an endpoint under test. Fresh copies
 * are what most of these endpoints see in practice - a page probes files before
 * the library has indexed them - so the cases read them as soon as they exist,
 * and the one thing that depends on indexing (FILESIZE) names which way it went.
 *
 * Three readers, three answers for the same file, which is most of what this
 * suite pins:
 *   - metadata.read and readRaw return {tags, info}: tag keys keep the case they
 *     were written in, technical fields are numbers. readRaw bypasses the
 *     library cache and says so with source "file".
 *   - readByPath and readBatch return one flat object: tag keys uppercased,
 *     technical fields folded in as strings. readByPath alone synthesises
 *     TRACKNUMBER from a leading number in the filename when the tag is absent;
 *     a readBatch row for the same file has no such key.
 *   - probeBatchAsync reports per path over metadata:probeProgress, with the
 *     flat tag form plus FILESIZE read from the container, and classifies
 *     failures (not-found, read-error) instead of returning an error string.
 *
 * The array endpoints disagree about one edge: readBatch and playcount.get
 * accept an empty list as an empty success, probeBatchAsync refuses it. A
 * missing list is an INVALID_PARAMS envelope from all three, and so is a
 * non-string entry, which the path tier refuses whole before any handler runs.
 *
 * writeBatch counts what it dispatched, not what landed: a path that does not
 * exist counts toward successCount, and the failure arrives later as a
 * metadata:writeComplete with status error. An item without an object tags is
 * that item's own "Missing tags" error: the call fails with OPERATION_FAILED
 * but still carries the counts, and the other items are dispatched. A non-object
 * item is refused whole as INVALID_PARAMS.
 *
 * Cancellation is a race, so the cancel case names its own outcome. When it
 * lands, the remaining items are simply absent from the results - unlike
 * file.*Async, which reports each remaining item as skipped/cancelled.
 *
 * Usage: node mcp/tests/e2e-metadata-batch.mjs
 * Environment: FB2K_CDP_PORT, FB2K_E2E_TIMEOUT_MS, FB2K_LIBRARY_FIXTURE_DIR,
 * FB2K_FIXTURE_SOURCE.
 */

import { statSync } from "node:fs";
import { join } from "node:path";

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

const invokeTimeoutMs = envInt("FB2K_E2E_TIMEOUT_MS", 25000);
const runId = new Date().toISOString().replace(/[-:.TZ]/g, "");
const EVENT_NAMES = ["metadata:probeProgress", "metadata:probeComplete", "metadata:writeComplete"];

/** The technical fields the flat form folds in beside the tags. */
const FLAT_TECH_KEYS = ["BITRATE", "SAMPLERATE", "CHANNELS", "CODEC", "DURATION"];

function tagsOf(flat) {
    const { success, path, ...rest } = flat ?? {};
    return rest;
}

function isFlatForm(flat) {
    return (
        flat &&
        typeof flat === "object" &&
        !("tags" in flat) &&
        !("info" in flat) &&
        FLAT_TECH_KEYS.every((key) => typeof flat[key] === "string") &&
        /^\d+\.\d{3}$/.test(flat.DURATION)
    );
}

async function runProbe(bridge, events, params, { timeoutMs = 15000 } = {}) {
    const dispatch = await bridge.invoke("metadata.probeBatchAsync", params);
    const operationId = dispatch?.operationId;
    await events.waitFor(
        (received) =>
            received.some(
                (e) => e.name === "metadata:probeComplete" && e.payload?.operationId === operationId,
            ),
        { timeoutMs, pollMs: 50 },
    );
    const own = events.received.filter((e) => e.payload?.operationId === operationId);
    const progress = own.filter((e) => e.name === "metadata:probeProgress").map((e) => e.payload);
    const complete = own.find((e) => e.name === "metadata:probeComplete")?.payload;
    return { dispatch, operationId, progress, complete, results: progress.flatMap((p) => p.results) };
}

/** Waits until `count` writeComplete events name one of `paths`, then returns them. */
async function awaitWriteCompletes(events, paths, count, { timeoutMs = 20000 } = {}) {
    const matching = (received) =>
        received.filter((e) => e.name === "metadata:writeComplete" && paths.includes(e.payload?.path));
    const seen = await events.waitFor((received) => matching(received).length >= count, {
        timeoutMs,
        pollMs: 100,
    });
    return matching(seen);
}

/** The file is rewritten after the completion event; reads until the tag lands. */
async function readRawUntil(bridge, path, predicate, { timeoutMs = 5000 } = {}) {
    const deadline = Date.now() + timeoutMs;
    for (;;) {
        const value = await bridge.invoke("metadata.readRaw", { path });
        if (predicate(value) || Date.now() >= deadline) return value;
        await new Promise((resolve) => setTimeout(resolve, 100));
    }
}

async function runReaderCases(bridge, recorder, fixtures) {
    const { invoke, invokeRaw } = bridge;
    const { good, noTrack, withTrack, missing, notAudio } = fixtures;

    const [flat, read, raw] = await Promise.all([
        invoke("metadata.readByPath", { path: good }),
        invoke("metadata.read", { path: good }),
        invoke("metadata.readRaw", { path: good }),
    ]);
    recorder.assertCase(
        "MB-01 readByPath answers one flat object: uppercased tag keys, a multi-value tag as an array, technical fields as strings beside them",
        flat?.success === true &&
            flat.path === good &&
            isFlatForm(flat) &&
            flat.ARTIST === "Probe Artist" &&
            flat.TITLE === "Read subject" &&
            flat.COMMENT === "mixed case key" &&
            !("Comment" in flat) &&
            Array.isArray(flat.GENRE) &&
            flat.GENRE.join() === "Alpha,Beta" &&
            flat.CODEC === "FLAC",
        { flat },
        {
            keys: "uppercase, COMMENT not Comment",
            GENRE: ["Alpha", "Beta"],
            tech: `${FLAT_TECH_KEYS.join("/")} as strings, DURATION with three decimals`,
        },
    );

    const [flatNoTrack, flatWithTrack] = await Promise.all([
        invoke("metadata.readByPath", { path: noTrack }),
        invoke("metadata.readByPath", { path: withTrack }),
    ]);
    recorder.assertCase(
        "MB-02 readByPath alone synthesises TRACKNUMBER from the filename's leading number when the tag is absent; an explicit tag wins, and read / readRaw never add one",
        flat?.TRACKNUMBER === "7" &&
            flatNoTrack?.TRACKNUMBER === "3" &&
            flatWithTrack?.TRACKNUMBER === "9" &&
            read?.tags?.TRACKNUMBER === undefined &&
            raw?.tags?.TRACKNUMBER === undefined,
        {
            fromFilename: { "07 - readme": flat?.TRACKNUMBER, "03 - no-tracknumber": flatNoTrack?.TRACKNUMBER },
            explicit: flatWithTrack?.TRACKNUMBER,
            read: read?.tags?.TRACKNUMBER,
            readRaw: raw?.tags?.TRACKNUMBER,
        },
        { fromFilename: { "07": "7", "03": "3" }, explicit: "9", read: undefined, readRaw: undefined },
    );

    const diskSize = statSync(good).size;
    const hasFileSize = typeof flat?.FILESIZE === "string";
    recorder.assertCase(
        hasFileSize
            ? "MB-03 readByPath reports FILESIZE from the library's cached stats, and it equals the size on disk"
            : "MB-03 readByPath omits FILESIZE for a file the library has not indexed yet - the flat form takes it from cached stats, not from the disk",
        hasFileSize ? flat.FILESIZE === String(diskSize) : !("FILESIZE" in flat),
        { FILESIZE: flat?.FILESIZE, diskSize, indexed: hasFileSize },
        hasFileSize ? { FILESIZE: String(diskSize) } : { FILESIZE: "absent" },
    );

    const [noKey, absent, text] = await Promise.all([
        invokeRaw("metadata.readByPath", {}),
        invokeRaw("metadata.readByPath", { path: missing }),
        invokeRaw("metadata.readByPath", { path: notAudio }),
    ]);
    recorder.assertCase(
        "MB-04 readByPath failures echo no path: a missing key is INVALID_PARAMS saying path is required, a missing or non-audio file says the track info could not be read",
        noKey?.value?.success === false &&
            noKey.value.error === "path is required" &&
            noKey.value.code === "INVALID_PARAMS" &&
            absent?.value?.success === false &&
            absent.value.error === "Failed to get track info" &&
            absent.value.path === undefined &&
            text?.value?.success === false &&
            text.value.error === "Failed to get track info",
        { noKey: noKey?.value, absent: absent?.value, text: text?.value },
        { noKey: { success: false, code: "INVALID_PARAMS" }, absentAndText: "Failed to get track info, no path" },
    );

    recorder.assertCase(
        "MB-05 readRaw reads the file directly and says so with source file; otherwise it agrees with read - keys keep their written case, technical fields are numbers",
        raw?.success === true &&
            raw.source === "file" &&
            raw.path === good &&
            read?.success === true &&
            read.source === undefined &&
            JSON.stringify(raw.tags) === JSON.stringify(read.tags) &&
            JSON.stringify(raw.info) === JSON.stringify(read.info) &&
            raw.tags.Comment === "mixed case key" &&
            raw.tags.GENRE.join() === "Alpha,Beta" &&
            typeof raw.info.bitrate === "number" &&
            typeof raw.info.duration === "number" &&
            raw.info.codec === "FLAC" &&
            raw.info.duration.toFixed(3) === flat.DURATION,
        { raw: { source: raw?.source, tags: raw?.tags, info: raw?.info }, readSource: read?.source },
        {
            source: "file on readRaw, absent on read",
            tags: "identical, Comment keeps its case",
            info: "identical numbers; duration to three decimals equals the flat DURATION string",
        },
    );

    const [suffixed, outOfRange, rawAbsent, rawText, rawNoKey] = await Promise.all([
        invoke("metadata.readRaw", { path: `${good}|subsong:0` }),
        invokeRaw("metadata.readRaw", { path: `${good}|subsong:3` }),
        invokeRaw("metadata.readRaw", { path: missing }),
        invokeRaw("metadata.readRaw", { path: notAudio }),
        invokeRaw("metadata.readRaw", {}),
    ]);
    recorder.assertCase(
        "MB-06 readRaw accepts a |subsong:0 suffix and echoes it back; a subsong past the end, a missing file and a non-audio file all fail with the path echoed, a missing key without",
        suffixed?.success === true &&
            suffixed.path === `${good}|subsong:0` &&
            suffixed.tags?.TITLE === "Read subject" &&
            outOfRange?.value?.success === false &&
            outOfRange.value.error === "Failed to read file directly" &&
            outOfRange.value.path === `${good}|subsong:3` &&
            rawAbsent?.value?.success === false &&
            rawAbsent.value.error === "Failed to read file directly" &&
            rawAbsent.value.path === missing &&
            rawText?.value?.success === false &&
            rawText.value.path === notAudio &&
            rawNoKey?.value?.success === false &&
            rawNoKey.value.error === "path is required" &&
            rawNoKey.value.path === undefined,
        {
            suffixed: { path: suffixed?.path, TITLE: suffixed?.tags?.TITLE },
            outOfRange: outOfRange?.value,
            rawAbsent: rawAbsent?.value,
            rawText: rawText?.value,
            rawNoKey: rawNoKey?.value,
        },
        {
            suffixed: "success, path echoed with the suffix",
            failures: "Failed to read file directly with path echoed - readByPath echoes nothing",
        },
    );
}

async function runReadBatchCases(bridge, recorder, fixtures) {
    const { invoke, invokeRaw } = bridge;
    const { good, missing, notAudio } = fixtures;

    const paths = [good, missing, notAudio];
    const [batch, flat] = await Promise.all([
        invoke("metadata.readBatch", { paths }),
        invoke("metadata.readByPath", { path: good }),
    ]);
    const rows = batch?.results ?? [];
    // readByPath's TRACKNUMBER came from the filename (MB-02); the batch reader
    // has no such fallback, so the comparison is against the flat tags without it.
    const { TRACKNUMBER: synthesised, ...goodTags } = tagsOf(flat);
    recorder.assertCase(
        "MB-07 readBatch answers one row per path in order; a good row carries readByPath's flat tags except the filename-derived TRACKNUMBER, a failed row carries path and error, and the envelope stays a success",
        batch?.success === true &&
            batch.total === 3 &&
            batch.successCount === 1 &&
            batch.errorCount === 2 &&
            rows.length === 3 &&
            rows.every((row, i) => row.path === paths[i]) &&
            rows[0].success === true &&
            synthesised === "7" &&
            rows[0].tags?.TRACKNUMBER === undefined &&
            JSON.stringify(rows[0].tags) === JSON.stringify(goodTags) &&
            rows[1].success === false &&
            rows[1].error === "Failed to get track info" &&
            rows[1].tags === undefined &&
            rows[2].success === false &&
            rows[2].error === "Failed to get track info",
        {
            envelope: { total: batch?.total, successCount: batch?.successCount, errorCount: batch?.errorCount },
            rows: rows.map((row) => ({ path: row.path, success: row.success, error: row.error, tagKeys: Object.keys(row.tags ?? {}).length })),
            readByPathTracknumber: synthesised,
            batchTracknumber: rows[0]?.tags?.TRACKNUMBER,
            sameOtherwise: JSON.stringify(rows[0]?.tags) === JSON.stringify(goodTags),
        },
        {
            total: 3,
            successCount: 1,
            errorCount: 2,
            goodRow: "readByPath's tags minus success/path and minus the synthesised TRACKNUMBER",
            batchTracknumber: undefined,
        },
    );

    const [nonString, noKey, empty] = await Promise.all([
        invokeRaw("metadata.readBatch", { paths: [good, 7] }),
        invokeRaw("metadata.readBatch", {}),
        invokeRaw("metadata.readBatch", { paths: [] }),
    ]);
    recorder.assertCase(
        "MB-08 readBatch: a non-string entry is refused whole by the path tier naming paths[1], so no row reports it; a missing list is INVALID_PARAMS saying paths is required; an empty list is an empty success",
        nonString?.value?.success === false &&
            nonString.value.code === "INVALID_PARAMS" &&
            /paths\[1\]/.test(nonString.value.error ?? "") &&
            noKey?.value?.success === false &&
            noKey.value.error === "paths is required" &&
            noKey.value.code === "INVALID_PARAMS" &&
            empty?.value?.success === true &&
            empty.value.total === 0 &&
            Array.isArray(empty.value.results) &&
            empty.value.results.length === 0,
        { nonString: nonString?.value, noKey: noKey?.value, empty: empty?.value },
        {
            nonString: { code: "INVALID_PARAMS", error: "names paths[1]" },
            noKey: { code: "INVALID_PARAMS", error: "paths is required" },
            empty: { success: true, total: 0 },
        },
    );
}

async function runProbeCases(bridge, recorder, events, fixtures) {
    const { invokeRaw } = bridge;
    const { good, missing, notAudio } = fixtures;

    const paths = [good, missing, notAudio];
    const probe = await runProbe(bridge, events, { paths });
    const [ok, absent, text] = probe.results;
    const diskSize = statSync(good).size;
    recorder.assertCase(
        "MB-09 probeBatchAsync reports per path over probeProgress - a readable file with infoSource direct, numeric info and the flat tags including FILESIZE from the container; a missing file as not-found; a non-audio file as read-error - and probeComplete's counts add up",
        probe.dispatch?.success === true &&
            typeof probe.operationId === "string" &&
            probe.operationId.startsWith("probe_") &&
            probe.dispatch.totalCount === 3 &&
            probe.results.length === 3 &&
            probe.results.every((r, i) => r.path === paths[i]) &&
            ok?.success === true &&
            ok.infoSource === "direct" &&
            typeof ok.info?.bitrate === "number" &&
            ok.info.codec === "FLAC" &&
            isFlatForm(ok.tags) &&
            ok.tags.ARTIST === "Probe Artist" &&
            ok.tags.FILESIZE === String(diskSize) &&
            ok.failure === undefined &&
            absent?.success === false &&
            absent.infoSource === "none" &&
            absent.failure === "not-found" &&
            absent.info === undefined &&
            absent.tags === undefined &&
            text?.success === false &&
            text.infoSource === "none" &&
            text.failure === "read-error" &&
            probe.complete?.total === 3 &&
            probe.complete.successCount === 1 &&
            probe.complete.failureCount === 2 &&
            probe.complete.cancelled === false,
        {
            dispatch: probe.dispatch,
            results: probe.results.map((r) => ({ path: r.path, success: r.success, infoSource: r.infoSource, failure: r.failure, FILESIZE: r.tags?.FILESIZE, hasInfo: r.info !== undefined })),
            complete: probe.complete,
            diskSize,
        },
        {
            ok: { infoSource: "direct", FILESIZE: String(diskSize) },
            absent: { failure: "not-found", infoSource: "none" },
            text: { failure: "read-error" },
            complete: { successCount: 1, failureCount: 2, cancelled: false },
        },
    );

    const noTags = await runProbe(bridge, events, { paths: [good], includeTags: false });
    recorder.assertCase(
        "MB-10 includeTags false drops the tags from a probe result and keeps info",
        noTags.results.length === 1 &&
            noTags.results[0].success === true &&
            noTags.results[0].tags === undefined &&
            typeof noTags.results[0].info?.duration === "number" &&
            noTags.complete?.successCount === 1,
        { result: noTags.results[0], complete: noTags.complete },
        { tags: undefined, info: "present" },
    );

    const [noKey, empty, nonString] = await Promise.all([
        invokeRaw("metadata.probeBatchAsync", {}),
        invokeRaw("metadata.probeBatchAsync", { paths: [] }),
        invokeRaw("metadata.probeBatchAsync", { paths: [3] }),
    ]);
    recorder.assertCase(
        "MB-11 probeBatchAsync refuses a missing list, an empty list and a non-string entry, each as INVALID_PARAMS - where readBatch accepts the empty list",
        noKey?.value?.success === false &&
            noKey.value.code === "INVALID_PARAMS" &&
            empty?.value?.success === false &&
            empty.value.code === "INVALID_PARAMS" &&
            nonString?.value?.success === false &&
            nonString.value.code === "INVALID_PARAMS" &&
            /paths\[0\]/.test(nonString.value.error ?? "") &&
            noKey.value.operationId === undefined &&
            empty.value.operationId === undefined,
        { noKey: noKey?.value, empty: empty?.value, nonString: nonString?.value },
        { each: { code: "INVALID_PARAMS", operationId: undefined } },
    );

    const [unknown, finished, missingId] = await Promise.all([
        invokeRaw("metadata.cancelProbe", { operationId: `probe_${runId}_never` }),
        invokeRaw("metadata.cancelProbe", { operationId: probe.operationId }),
        invokeRaw("metadata.cancelProbe", {}),
    ]);
    recorder.assertCase(
        "MB-12 cancelProbe cannot tell an unknown id from a finished one - both are success with cancelled false - and a missing operationId is INVALID_PARAMS, as file.cancelOp answers it",
        unknown?.value?.success === true &&
            unknown.value.cancelled === false &&
            finished?.value?.success === true &&
            finished.value.cancelled === false &&
            missingId?.value?.success === false &&
            missingId.value.error === "operationId is required" &&
            missingId.value.code === "INVALID_PARAMS",
        { unknown: unknown?.value, finished: finished?.value, missingId: missingId?.value },
        {
            unknownAndFinished: { success: true, cancelled: false },
            missingId: { success: false, code: "INVALID_PARAMS" },
        },
    );
}

async function runCancelProbeCase(bridge, recorder, events, area) {
    const { invoke } = bridge;

    // Enough files that a cancel issued right after dispatch usually lands
    // while the worker is still reading.
    const many = [];
    for (let i = 0; i < 40; i++) {
        many.push(area.copyAudio(`probe-${String(i).padStart(2, "0")}.flac`));
    }
    const dispatch = await invoke("metadata.probeBatchAsync", { paths: many });
    const operationId = dispatch?.operationId;
    const cancel = await invoke("metadata.cancelProbe", { operationId });
    await events.waitFor(
        (received) =>
            received.some(
                (e) => e.name === "metadata:probeComplete" && e.payload?.operationId === operationId,
            ),
        { timeoutMs: 20000, pollMs: 50 },
    );
    const own = events.received.filter((e) => e.payload?.operationId === operationId);
    const results = own
        .filter((e) => e.name === "metadata:probeProgress")
        .flatMap((e) => e.payload.results);
    const complete = own.find((e) => e.name === "metadata:probeComplete")?.payload;
    const reported = (complete?.successCount ?? 0) + (complete?.failureCount ?? 0);
    const landed = cancel?.cancelled === true;

    recorder.assertCase(
        landed
            ? "MB-13 a cancel that lands mid-probe stops the remainder: probeComplete says cancelled with success plus failure short of total, and the unreached paths are simply absent from the results - file.*Async would have listed them as skipped"
            : "MB-13 the probe finished before the cancel arrived, so the cancel reports false and the completion is uncancelled with every path reported",
        landed
            ? complete?.cancelled === true &&
                  complete.total === many.length &&
                  reported < many.length &&
                  complete.failureCount === 0 &&
                  results.length === reported &&
                  results.every((r) => r.success === true && many.includes(r.path))
            : cancel?.cancelled === false &&
                  complete?.cancelled === false &&
                  reported === many.length &&
                  results.length === many.length,
        { cancel, complete, reportedResults: results.length, total: many.length, landed },
        landed
            ? { complete: { cancelled: true, failureCount: 0 }, results: "one per reported path, none for the rest" }
            : { skipped: "the race was lost; readings recorded" },
    );
}

async function runWriteBatchCases(bridge, recorder, events, area, fixtures) {
    const { invoke, invokeRaw } = bridge;
    const { missing, notAudio } = fixtures;

    const a = area.taggedAudio("wb-a.flac", [["TITLE", "A"]]).targetPath;
    const b = area.taggedAudio("wb-b.flac", [["TITLE", "B"]]).targetPath;
    const dispatched = await invoke("metadata.writeBatch", {
        items: [
            { path: a, tags: { ARTIST: "Batch A" } },
            { path: b, tags: { ARTIST: "Batch B" } },
        ],
    });
    const completes = await awaitWriteCompletes(events, [a, b], 2);
    const [rawA, rawB] = await Promise.all([
        readRawUntil(bridge, a, (v) => v?.tags?.ARTIST === "Batch A"),
        readRawUntil(bridge, b, (v) => v?.tags?.ARTIST === "Batch B"),
    ]);
    recorder.assertCase(
        "MB-14 writeBatch dispatches one write per item and reports the dispatch counts; each completes with its own metadata:writeComplete, and readRaw then sees the new value beside the untouched one",
        dispatched?.success === true &&
            dispatched.successCount === 2 &&
            dispatched.failCount === 0 &&
            Array.isArray(dispatched.errors) &&
            dispatched.errors.length === 0 &&
            completes.length === 2 &&
            completes.every(
                (e) =>
                    e.payload.operation === "write" &&
                    e.payload.success === true &&
                    e.payload.status === "success" &&
                    e.payload.code === 0,
            ) &&
            new Set(completes.map((e) => e.payload.path)).size === 2 &&
            rawA?.tags?.ARTIST === "Batch A" &&
            rawA.tags.TITLE === "A" &&
            rawB?.tags?.ARTIST === "Batch B" &&
            rawB.tags.TITLE === "B",
        {
            dispatched,
            completes: completes.map((e) => e.payload),
            rawA: rawA?.tags,
            rawB: rawB?.tags,
        },
        {
            dispatched: { successCount: 2, failCount: 0, errors: [] },
            completes: "two, operation write, status success, code 0, one per path",
            files: { a: { ARTIST: "Batch A", TITLE: "A" }, b: { ARTIST: "Batch B", TITLE: "B" } },
        },
    );

    const seenBefore = events.received.filter((e) => e.name === "metadata:writeComplete").length;
    const badTargets = await invoke("metadata.writeBatch", {
        items: [
            { path: missing, tags: { ARTIST: "nobody" } },
            { path: notAudio, tags: { ARTIST: "nobody" } },
        ],
    });
    const badCompletes = await awaitWriteCompletes(events, [missing, notAudio], 2);
    recorder.assertCase(
        "MB-15 writeBatch counts dispatches, not outcomes: a missing file and a text file both count toward successCount, and each failure arrives only as a writeComplete with status error",
        badTargets?.success === true &&
            badTargets.successCount === 2 &&
            badTargets.failCount === 0 &&
            badCompletes.length === 2 &&
            badCompletes.every(
                (e) =>
                    e.payload.operation === "write" &&
                    e.payload.success === false &&
                    e.payload.status === "error" &&
                    typeof e.payload.code === "number" &&
                    e.payload.code !== 0,
            ),
        { badTargets, completes: badCompletes.map((e) => e.payload), seenBefore },
        {
            badTargets: { success: true, successCount: 2, failCount: 0 },
            completes: "two, success false, status error, non-zero code",
        },
    );

    const c = area.taggedAudio("wb-c.flac", [["TITLE", "C"]]).targetPath;
    const multi = area.taggedAudio("wb-multi.flac", [["TITLE", "Multi"], ["ARTIST", "old"]]).targetPath;
    const invalid = area.taggedAudio("wb-invalid.flac", [["TITLE", "Untouched"]]).targetPath;
    const emptyTags = area.taggedAudio("wb-empty.flac", [["TITLE", "Empty"]]).targetPath;
    const mixed = await invoke("metadata.writeBatch", {
        items: [
            { path: emptyTags, tags: {} },
            { path: multi, tags: { ARTIST: ["x", "y"] } },
            { path: c, tags: { ARTIST: "solo" } },
            { path: c },
            { path: c, tags: "not-an-object" },
            { path: invalid, tags: { ALBUM: "must not be written", GENRE: ["valid", false] } },
        ],
    });
    const mixedCompletes = await awaitWriteCompletes(events, [c, multi], 2);
    await events.waitFor(() => false, { timeoutMs: 500 });
    const mixedCount = events.received.filter(
        (e) => e.name === "metadata:writeComplete" && [c, multi, invalid, emptyTags].includes(e.payload?.path),
    ).length;
    const [rawC, rawMulti, rawInvalid, rawEmpty] = await Promise.all([
        readRawUntil(bridge, c, (v) => v?.tags?.ARTIST === "solo"),
        readRawUntil(bridge, multi, (v) => JSON.stringify(v?.tags?.ARTIST) === '["x","y"]'),
        invoke("metadata.readRaw", { path: invalid }),
        invoke("metadata.readRaw", { path: emptyTags }),
    ]);
    recorder.assertCase(
        "MB-16 writeBatch writes array and scalar entries independently, skips an empty update, and isolates missing tags and an invalid array without partial writes",
        mixed?.success === false &&
            mixed.code === "OPERATION_FAILED" && mixed.successCount === 3 &&
            mixed.failCount === 3 && mixed.errors.length === 3 &&
            mixed.errors.filter((e) => e.path === c && e.error === "Missing tags").length === 2 &&
            mixed.errors.some((e) => e.path === invalid && /GENRE/.test(e.error) && /\[1\]/.test(e.error)) &&
            mixedCompletes.length === 2 && mixedCompletes.every((e) => e.payload?.success === true) &&
            new Set(mixedCompletes.map((e) => e.payload.path)).size === 2 && mixedCount === 2 &&
            rawC?.tags?.ARTIST === "solo" && rawC.tags.TITLE === "C" &&
            JSON.stringify(rawMulti?.tags?.ARTIST) === '["x","y"]' && rawMulti.tags.TITLE === "Multi" &&
            rawInvalid?.tags?.TITLE === "Untouched" && rawInvalid.tags.ALBUM === undefined &&
            rawInvalid.tags.GENRE === undefined && rawEmpty?.tags?.TITLE === "Empty",
        { mixed, completions: mixedCompletes.map((e) => e.payload), mixedCount,
            rawC: rawC?.tags, rawMulti: rawMulti?.tags, rawInvalid: rawInvalid?.tags, rawEmpty: rawEmpty?.tags },
        {
            mixed: { success: false, successCount: 3, failCount: 3 },
            completions: 2, scalar: "solo", multivalue: ["x", "y"], invalidEntry: "unchanged",
        },
    );

    const [noItems, empty, itemNoPath, itemEmptyPath, itemNonObject] = await Promise.all([
        invokeRaw("metadata.writeBatch", {}),
        invokeRaw("metadata.writeBatch", { items: [] }),
        invokeRaw("metadata.writeBatch", { items: [{ tags: { ARTIST: "x" } }] }),
        invokeRaw("metadata.writeBatch", { items: [{ path: "", tags: { ARTIST: "x" } }] }),
        invokeRaw("metadata.writeBatch", { items: ["x"] }),
    ]);
    recorder.assertCase(
        "MB-17 writeBatch shape: a missing list is INVALID_PARAMS saying items is required, an empty list an empty success; an item without path is stopped by the path tier as INVALID_PARAMS naming items[0], an empty path as PERMISSION_DENIED; a non-object item is refused whole as INVALID_PARAMS naming items[0]",
        noItems?.value?.success === false &&
            noItems.value.error === "items is required" &&
            noItems.value.code === "INVALID_PARAMS" &&
            empty?.value?.success === true &&
            empty.value.successCount === 0 &&
            empty.value.failCount === 0 &&
            itemNoPath?.value?.success === false &&
            itemNoPath.value.code === "INVALID_PARAMS" &&
            /items\[0\]/.test(itemNoPath.value.error ?? "") &&
            /'path'/.test(itemNoPath.value.error ?? "") &&
            itemEmptyPath?.value?.success === false &&
            itemEmptyPath.value.code === "PERMISSION_DENIED" &&
            /items\[0\]\.path/.test(itemEmptyPath.value.error ?? "") &&
            itemNonObject?.kind === "result" &&
            itemNonObject.value?.success === false &&
            itemNonObject.value.code === "INVALID_PARAMS" &&
            /items\[0\] must be an object/.test(itemNonObject.value.error ?? ""),
        {
            noItems: noItems?.value,
            empty: empty?.value,
            itemNoPath: itemNoPath?.value,
            itemEmptyPath: itemEmptyPath?.value,
            itemNonObject,
        },
        {
            noItems: { code: "INVALID_PARAMS", error: "items is required" },
            empty: { success: true, successCount: 0, failCount: 0 },
            itemNoPath: { code: "INVALID_PARAMS", error: "names items[0] and 'path'" },
            itemEmptyPath: { code: "PERMISSION_DENIED", error: "names items[0].path; wording belongs to the path tier" },
            itemNonObject: { kind: "result", code: "INVALID_PARAMS", error: "items[0] must be an object" },
        },
    );
}

async function runPlaycountCases(bridge, recorder, fixtures) {
    const { invoke, invokeRaw } = bridge;
    const { good, missing } = fixtures;

    const inLibrary = await waitForLibraryEntry(bridge, good, { timeoutMs: 40000 });
    const paths = [good, missing];
    const [batch, single] = await Promise.all([
        invoke("playcount.getBatch", { paths }),
        invoke("playcount.get", { paths }),
    ]);
    const member = batch?.results?.[0];
    const ghost = batch?.results?.[1];
    recorder.assertCase(
        inLibrary
            ? "MB-18 playcount.getBatch is playcount.get under a second name: identical answer for the same input; a library member reads zero plays with an added date, and a file that does not exist is still a success row with inLibrary false"
            : "MB-18 playcount.getBatch is playcount.get under a second name: identical answer for the same input; the fixture never reached the library, so membership is not asserted",
        batch?.success === true &&
            batch.count === 2 &&
            JSON.stringify(batch) === JSON.stringify(single) &&
            member?.path === good &&
            member.success === true &&
            member.playCount === 0 &&
            member.firstPlayed === "N/A" &&
            member.lastPlayed === "N/A" &&
            ghost?.path === missing &&
            ghost.success === true &&
            ghost.inLibrary === false &&
            ghost.playCount === 0 &&
            ghost.added === "N/A" &&
            (!inLibrary ||
                (member.inLibrary === true && typeof member.added === "string" && member.added !== "N/A")),
        {
            identical: JSON.stringify(batch) === JSON.stringify(single),
            member,
            ghost,
            libraryWaitMs: inLibrary?.waitedMs ?? null,
        },
        {
            identical: true,
            member: inLibrary ? { inLibrary: true, playCount: 0, added: "a date" } : { playCount: 0 },
            ghost: { success: true, inLibrary: false, playCount: 0, added: "N/A" },
        },
    );

    const [nonString, noKey, empty, suffixed] = await Promise.all([
        invokeRaw("playcount.getBatch", { paths: [good, 1] }),
        invokeRaw("playcount.getBatch", {}),
        invokeRaw("playcount.getBatch", { paths: [] }),
        invoke("playcount.getBatch", { paths: [`${good}|subsong:0`] }),
    ]);
    recorder.assertCase(
        "MB-19 getBatch: a non-string entry is refused whole by the path tier naming paths[1]; a missing list is refused as INVALID_PARAMS naming paths; an empty list is count zero; a |subsong suffix is echoed back",
        nonString?.value?.success === false &&
            nonString.value.code === "INVALID_PARAMS" &&
            /paths\[1\]/.test(nonString.value.error ?? "") &&
            noKey?.value?.success === false &&
            noKey.value.code === "INVALID_PARAMS" &&
            /paths/.test(noKey.value.error ?? "") &&
            empty?.value?.success === true &&
            empty.value.count === 0 &&
            suffixed?.results?.[0]?.path === `${good}|subsong:0` &&
            suffixed.results[0].success === true,
        { nonString: nonString?.value, noKey: noKey?.value, empty: empty?.value, suffixedPath: suffixed?.results?.[0]?.path },
        { nonString: "INVALID_PARAMS naming paths[1]", noKey: "INVALID_PARAMS naming paths", empty: { count: 0 }, suffixed: "echoed" },
    );

    const [stats, count] = await Promise.all([
        invoke("playcount.getStats", {}),
        invoke("library.getCount", {}),
    ]);
    const expectedAveragePlays =
        stats?.playedTracks > 0 ? stats.totalPlayCount / stats.playedTracks : 0;
    recorder.assertCase(
        "MB-20 getStats walks the whole library: totalTracks equals library.getCount, unplayed is the remainder, the averages are derived from the counts, and no rating average exists without rated tracks",
        stats?.success === true &&
            stats.totalTracks === count?.count &&
            stats.unplayedTracks === stats.totalTracks - stats.playedTracks &&
            stats.playedTracks >= 0 &&
            stats.maxPlayCount <= stats.totalPlayCount &&
            Math.abs(stats.averagePlayCount - expectedAveragePlays) < 1e-9 &&
            (stats.ratedTracks === 0
                ? stats.averageRating === 0
                : stats.averageRating > 0 && stats.averageRating <= 5),
        { stats, libraryCount: count?.count },
        {
            totalTracks: "equals library.getCount",
            unplayedTracks: "totalTracks - playedTracks",
            averagePlayCount: "totalPlayCount / playedTracks, or 0",
            averageRating: "0 with no rated tracks, otherwise within 1..5",
        },
    );
}

const recorder = createRecorder();
let client;
let bridge;
let events;
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

    const resolved = await resolveFixtureArea(bridge, { runId });
    area = resolved.area;
    context = { libraryRoot: resolved.root.absolutePath, fixtureDir: area.dir, source: resolved.source };
    console.log(`Fixture area: ${area.dir}`);

    // The leading numbers in the names are deliberate: MB-02 asserts what
    // readByPath makes of them.
    const fixtures = {
        good: area.taggedAudio("07 - readme.flac", [
            ["ARTIST", "Probe Artist"],
            ["TITLE", "Read subject"],
            ["ALBUM", "Metadata Batch"],
            ["GENRE", "Alpha"],
            ["GENRE", "Beta"],
            ["Comment", "mixed case key"],
        ]).targetPath,
        noTrack: area.taggedAudio("03 - no-tracknumber.flac", [["TITLE", "no track tag"]]).targetPath,
        withTrack: area.taggedAudio("05 - with-tracknumber.flac", [
            ["TITLE", "with"],
            ["TRACKNUMBER", "9"],
        ]).targetPath,
        missing: join(area.dir, "missing.flac"),
        notAudio: area.textFile("not-audio.flac", "this is not a FLAC stream"),
    };

    events = await createEventCollector(bridge, EVENT_NAMES, { collectorId: runId });

    await runReaderCases(bridge, recorder, fixtures);
    await runReadBatchCases(bridge, recorder, fixtures);
    await runProbeCases(bridge, recorder, events, fixtures);
    await runCancelProbeCase(bridge, recorder, events, area);
    await runWriteBatchCases(bridge, recorder, events, area, fixtures);
    await runPlaycountCases(bridge, recorder, fixtures);
} catch (error) {
    fatalError = error;
    blocked = Boolean(error?.blocked);
    console.error(`${blocked ? "BLOCKED" : "ERROR"}: ${error?.message || error}`);
} finally {
    if (events) await events.stop();
    let cleanup;
    if (area) {
        cleanup = area.remove();
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
            eventsSeen: [...new Set((events?.received ?? []).map((e) => e.name))],
            targets,
        },
    }),
);

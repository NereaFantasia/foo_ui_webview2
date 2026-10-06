/**
 * Checks exact lyrics sidecars, path spellings, filters and cache invalidation.
 * Fixture writes stay in a temporary directory; library media is read-only.
 * Usage: node mcp/tests/e2e-lyrics-sidecar.mjs
 */

import { existsSync, mkdirSync, readdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { tmpdir } from "node:os";
import { performance } from "node:perf_hooks";

import {
    closeClient,
    connectBridgePage,
    createBridge,
    createRecorder,
    createEventCollector,
    envInt,
    report,
    requireResponsiveBridge,
    resolvePort,
} from "./lib/e2e-harness.mjs";

const invokeTimeoutMs = envInt("FB2K_E2E_TIMEOUT_MS", 30000);
const scanLimit = envInt("FB2K_LIBRARY_SCAN_LIMIT", 400);
const runId = new Date().toISOString().replace(/[-:.TZ]/g, "");
const fixtureDir = join(process.env.FB2K_FIXTURE_DIR || tmpdir(), `fb2k-lyrics-e2e-${runId}`);

const CACHE_TTL_MS = 1200;
const SYNCED_BODY = "[00:01.00]first line\n[00:05.50]second line\n";
const PLAIN_BODY = "a line with no timestamp\nanother line\n";

/** Reads only the fields a page would consume, so mismatches print small. */
function shape(result) {
    return {
        success: result?.success,
        available: result?.available,
        source: result?.source,
        synced: result?.synced,
        sourcePath: result?.sourcePath,
        lyrics: result?.lyrics,
    };
}

function sameShape(a, b) {
    return JSON.stringify(shape(a)) === JSON.stringify(shape(b));
}

/** elapsedMs includes the initial read, so it bounds the age of every primed entry. */
function assertCacheInvalidatedBeforeExpiry(recorder, caseId, elapsedMs, condition, observed) {
    const timing = { elapsedMs, ttlMs: CACHE_TTL_MS };
    if (elapsedMs >= CACHE_TTL_MS) {
        recorder.addCase(
            `${caseId} SKIP: cache invalidation within the TTL is not covered`,
            true,
            {
                ...observed,
                ...timing,
                cacheInvalidationCovered: false,
                skipped: "the primed entry may have expired before read-back",
            },
            { skipped: "requires the complete warm-read/write/read-back sequence inside the TTL" },
        );
        return;
    }
    recorder.assertCase(
        `${caseId} cached lyrics are invalidated before their TTL expires`,
        condition,
        { ...observed, ...timing, cacheInvalidationCovered: true },
        { cacheInvalidated: true, elapsedMsLessThan: CACHE_TTL_MS },
    );
}

/** Creates a decodable mono PCM WAV and optional sidecars. */
function makeCase(stem, sidecars = {}) {
    const audioPath = join(fixtureDir, `${stem}.wav`);
    const pcmBytes = 1600;
    const wave = Buffer.alloc(44 + pcmBytes);
    wave.write("RIFF", 0);
    wave.writeUInt32LE(36 + pcmBytes, 4);
    wave.write("WAVEfmt ", 8);
    wave.writeUInt32LE(16, 16);
    wave.writeUInt16LE(1, 20);
    wave.writeUInt16LE(1, 22);
    wave.writeUInt32LE(8000, 24);
    wave.writeUInt32LE(16000, 28);
    wave.writeUInt16LE(2, 32);
    wave.writeUInt16LE(16, 34);
    wave.write("data", 36);
    wave.writeUInt32LE(pcmBytes, 40);
    writeFileSync(audioPath, wave);
    for (const [name, body] of Object.entries(sidecars)) {
        writeFileSync(join(fixtureDir, name), body, "utf8");
    }
    return audioPath;
}

async function runLibraryCases(bridge, recorder) {
    const page = await bridge.invoke("library.query", { query: "ALL", limit: scanLimit });
    const tracks = Array.isArray(page?.tracks) ? page.tracks : [];

    // The oracle for "a sidecar exists" is node's own directory listing, so the
    // library group never verifies the bridge against another bridge call.
    let named;
    let tagged;
    for (const track of tracks) {
        if (named && tagged) break;
        const absolutePath = String(track?.absolutePath || "");
        if (!absolutePath || !track?.path) continue;
        let entries;
        try {
            entries = readdirSync(dirname(absolutePath));
        } catch {
            continue;
        }
        const stem = absolutePath.slice(absolutePath.lastIndexOf("\\") + 1).replace(/\.[^.]*$/, "");
        if (!named) {
            const sidecar = entries.find((name) => name === `${stem}.lrc` || name === `${stem}.txt`);
            if (sidecar) named = { track, sidecar };
        }
        const firstArtist = track.artists?.[0];
        if (!tagged && firstArtist && track.title) {
            const safe = (value) => value.replace(/[\\/:*?"<>|]/g, "_");
            const want = `${safe(firstArtist)} - ${safe(track.title)}.lrc`;
            if (entries.includes(want) && !entries.includes(`${stem}.lrc`)) tagged = { track, sidecar: want };
        }
    }

    const subject = tagged ?? named;
    if (!subject) {
        recorder.assertCase(
            "LY-01 the library holds a track with an external lyrics file on disk",
            false,
            { scanned: tracks.length, reason: "no track directory holds a matching .lrc or .txt" },
            { trackWithSidecar: "found" },
        );
        return;
    }

    const suffix = subject.track.subsong > 0 ? `|subsong:${subject.track.subsong}` : "";
    const rawPath = subject.track.path + suffix;
    const nativePath = subject.track.absolutePath + suffix;
    const rawIsRelative = rawPath.startsWith("file-relative://");

    // The cache key carries the path as it was passed, so these spellings
    // land in separate entries; when the raw path already is the native one,
    // the repeated read comes from cache, which the assertions accept.
    const [byRaw, byNative, byFileUri, byThreeSlash] = await Promise.all([
        bridge.invoke("lyrics.get", { path: rawPath, source: "file", filename: subject.sidecar, type: "any" }),
        bridge.invoke("lyrics.get", { path: nativePath, source: "file", filename: subject.sidecar, type: "any" }),
        bridge.invoke("lyrics.get", { path: `file://${nativePath}`, source: "file", filename: subject.sidecar, type: "any" }),
        bridge.invokeRaw("lyrics.get", {
            path: `file:///${nativePath.replace(/\\/g, "/")}`,
            type: "any",
        }),
    ]);

    recorder.assertCase(
        "LY-01 the logical path a real install reports reaches the sidecar on disk",
        byRaw?.available === true &&
            byRaw?.source === "file" &&
            String(byRaw?.sourcePath || "").endsWith(subject.sidecar),
        {
            rawPath,
            rawFormIsFileRelative: rawIsRelative,
            sidecarOnDisk: subject.sidecar,
            observed: shape(byRaw),
        },
        { available: true, source: "file", sourcePathEndsWith: subject.sidecar },
    );

    recorder.assertCase(
        "LY-02 native, file:// and the reported logical form all read the same file",
        sameShape(byRaw, byNative) && sameShape(byRaw, byFileUri),
        {
            raw: shape(byRaw),
            native: shape(byNative),
            fileUri: shape(byFileUri),
        },
        { allThreeIdentical: true },
    );

    // The response echoes the caller's own spelling. Merging the cache key across
    // path forms would make this value depend on which form was asked first.
    recorder.assertCase(
        "LY-03 path is echoed in the form the caller passed",
        byRaw?.path === rawPath &&
            byNative?.path === nativePath &&
            byFileUri?.path === `file://${nativePath}`,
        { raw: byRaw?.path, native: byNative?.path, fileUri: byFileUri?.path },
        { each: "the string that was passed in" },
    );

    // A three-slash file URI is documented as unsupported. What matters is that
    // it is refused cleanly: either a plain miss or the security envelope, never
    // an unhandled exception surfacing as INTERNAL_ERROR.
    const threeSlashValue = byThreeSlash?.kind === "result" ? byThreeSlash.value : undefined;
    const threeSlashCleanMiss =
        threeSlashValue?.success === true && threeSlashValue?.available === false;
    const threeSlashDenied = threeSlashValue?.code === "PERMISSION_DENIED";
    recorder.assertCase(
        "LY-04 an unsupported three-slash file URI is refused without an internal error",
        threeSlashCleanMiss || threeSlashDenied,
        { kind: byThreeSlash?.kind, observed: threeSlashValue },
        { either: "success with available false", or: "PERMISSION_DENIED" },
    );

    const [existsRaw, existsNative, existsFileUri] = await Promise.all([
        bridge.invoke("lyrics.exists", { path: rawPath, filename: subject.sidecar }),
        bridge.invoke("lyrics.exists", { path: nativePath, filename: subject.sidecar }),
        bridge.invoke("lyrics.exists", { path: `file://${nativePath}`, filename: subject.sidecar }),
    ]);
    const existsShapes = [existsRaw, existsNative, existsFileUri].map((value) =>
        JSON.stringify({ exists: value?.exists, sources: value?.sources }),
    );
    recorder.assertCase(
        "LY-05 exists reports the same file as get, for every path form",
        existsRaw?.exists === true &&
            new Set(existsShapes).size === 1 &&
            (existsRaw?.sources ?? []).includes(`file:${subject.sidecar}`),
        { exists: existsRaw, agreementAcrossForms: new Set(existsShapes).size === 1 },
        { exists: true, sources: [`file:${subject.sidecar}`], identicalAcrossForms: true },
    );

    // The naming most lyrics downloaders use. It needs the track's own artist and
    // title tags, so it is only observable on a library track.
    if (tagged) {
        const taggedPath = tagged.track.path + (tagged.track.subsong > 0 ? `|subsong:${tagged.track.subsong}` : "");
        const byTagged = await bridge.invoke("lyrics.get", { path: taggedPath, source: "file" });
        // Line endings are compared after normalising, because the endpoint reads
        // in text mode and real sidecars come both ways - the two in this library
        // differ from each other. LY-14 is where that translation is pinned.
        const onDisk = readFileSync(join(dirname(tagged.track.absolutePath), tagged.sidecar), "utf8");
        const lyricsMatchDisk =
            String(byTagged?.lyrics ?? "").replace(/\r\n/g, "\n") === onDisk.replace(/\r\n/g, "\n");
        recorder.assertCase(
            "LY-06 a sidecar named '<artist> - <title>' is found and its text read in full",
            byTagged?.available === true &&
                String(byTagged?.sourcePath || "").endsWith(tagged.sidecar) &&
                lyricsMatchDisk,
            {
                artist: tagged.track.artist,
                title: tagged.track.title,
                sidecar: tagged.sidecar,
                sourcePath: byTagged?.sourcePath,
                lyricsMatchDisk,
                diskEol: onDisk.includes("\r\n") ? "CRLF" : "LF",
            },
            { available: true, sourcePathEndsWith: tagged.sidecar, lyrics: "the file's text, EOL aside" },
        );
    } else {
        recorder.assertCase(
            "LY-06 the library holds a sidecar named '<artist> - <title>'",
            false,
            { scanned: tracks.length, reason: "no scanned track directory holds that name" },
            { taggedSidecar: "found" },
        );
    }
}

async function runFixtureCases(bridge, recorder) {
    const perTrack = makeCase("per-track", { "per-track.lrc": SYNCED_BODY });
    const readPerTrack = await bridge.invoke("lyrics.get", { path: perTrack });
    recorder.assertCase(
        "LY-07 a sidecar named after the audio file is read, timestamps making it synced",
        readPerTrack?.available === true &&
            readPerTrack?.source === "file" &&
            readPerTrack?.lyrics === SYNCED_BODY &&
            readPerTrack?.synced === true,
        shape(readPerTrack),
        { available: true, source: "file", lyrics: SYNCED_BODY, synced: true },
    );

    const both = makeCase("both-ext", { "both-ext.lrc": SYNCED_BODY, "both-ext.txt": PLAIN_BODY });
    const [defaultFormat, forcedTxt] = await Promise.all([
        bridge.invoke("lyrics.get", { path: both, format: "any" }),
        bridge.invoke("lyrics.get", { path: both, format: "txt" }),
    ]);
    recorder.assertCase(
        "LY-08 .lrc wins over .txt, and format txt asks for the other one",
        defaultFormat?.lyrics === SYNCED_BODY && forcedTxt?.lyrics === PLAIN_BODY,
        { default: shape(defaultFormat), forcedTxt: shape(forcedTxt) },
        { default: ".lrc content", forcedTxt: ".txt content" },
    );

    const syncedOnly = makeCase("filter-synced", { "filter-synced.lrc": SYNCED_BODY });
    const plainOnly = makeCase("filter-plain", { "filter-plain.txt": PLAIN_BODY });
    const [syncedAsSynced, syncedAsPlain, plainAsPlain, plainAsSynced] = await Promise.all([
        bridge.invoke("lyrics.get", { path: syncedOnly, type: "synced" }),
        bridge.invoke("lyrics.get", { path: syncedOnly, type: "unsynced" }),
        bridge.invoke("lyrics.get", { path: plainOnly, type: "unsynced" }),
        bridge.invoke("lyrics.get", { path: plainOnly, type: "synced" }),
    ]);
    recorder.assertCase(
        "LY-09 the type filter excludes the file that does not match, in both directions",
        syncedAsSynced?.available === true &&
            syncedAsPlain?.available === false &&
            plainAsPlain?.available === true &&
            plainAsSynced?.available === false,
        {
            syncedFile: { askSynced: syncedAsSynced?.available, askUnsynced: syncedAsPlain?.available },
            plainFile: { askUnsynced: plainAsPlain?.available, askSynced: plainAsSynced?.available },
        },
        { syncedFile: { askSynced: true, askUnsynced: false }, plainFile: { askUnsynced: true, askSynced: false } },
    );

    const emptyCase = makeCase("empty-sidecar", { "empty-sidecar.lrc": "" });
    const readEmpty = await bridge.invoke("lyrics.get", { path: emptyCase });
    const existsEmpty = await bridge.invoke("lyrics.exists", { path: emptyCase });
    recorder.assertCase(
        "LY-10 an empty sidecar counts as no lyrics rather than empty lyrics",
        readEmpty?.success === true && readEmpty?.available === false && existsEmpty?.exists === true,
        { get: shape(readEmpty), exists: existsEmpty },
        { success: true, available: false, exists: true },
    );

    const album = makeCase("album", { "album.02.lrc": SYNCED_BODY, "album.lrc": PLAIN_BODY });
    const subsongRead = await bridge.invoke("lyrics.get", { path: `${album}|subsong:1`, source: "file" });
    const subsongExists = await bridge.invoke("lyrics.exists", { path: `${album}|subsong:1` });
    recorder.assertCase(
        "LY-11 a subsong does not automatically use numbered or audio-name sidecars",
        subsongRead?.available === false && subsongExists?.exists === false,
        { get: shape(subsongRead), exists: subsongExists },
        { available: false, exists: false },
    );
    const explicitSubsong = await bridge.invoke("lyrics.get", {
        path: `${album}|subsong:1`, source: "file", filename: "album.02.lrc", format: "txt",
    });
    const explicitExists = await bridge.invoke("lyrics.exists", {
        path: `${album}|subsong:1`, filename: "album.02.lrc",
    });
    recorder.assertCase(
        "LY-12 an explicit subsong filename selects exactly that file regardless of format",
        explicitSubsong?.lyrics === SYNCED_BODY &&
            String(explicitSubsong?.sourcePath || "").endsWith("album.02.lrc") &&
            (explicitExists?.sources ?? []).includes("file:album.02.lrc"),
        { get: shape(explicitSubsong), exists: explicitExists },
        { lyrics: SYNCED_BODY, exists: true },
    );

    const roundTrip = makeCase("round-trip");
    const roundTripStartedAt = performance.now();
    const beforeSave = await bridge.invoke("lyrics.get", { path: roundTrip });
    const saved = await bridge.invoke("lyrics.save", {
        path: roundTrip,
        lyrics: SYNCED_BODY,
        target: ["file"],
    });
    const readBack = await bridge.invoke("lyrics.get", { path: roundTrip });
    const roundTripElapsedMs = performance.now() - roundTripStartedAt;
    recorder.assertCase(
        "LY-13 save writes the sidecar get looks for, and get returns what save was given",
        beforeSave?.available === false && saved?.success === true &&
            existsSync(saved?.savedTo ?? "") &&
            saved.savedTo === join(fixtureDir, "round-trip.lrc") &&
            readBack?.lyrics === SYNCED_BODY,
        { savedTo: saved?.savedTo, onDisk: existsSync(saved?.savedTo ?? ""), readBack: shape(readBack) },
        { success: true, savedTo: join(fixtureDir, "round-trip.lrc"), readBack: SYNCED_BODY },
    );
    assertCacheInvalidatedBeforeExpiry(
        recorder, "LY-13", roundTripElapsedMs,
        beforeSave?.available === false && saved?.success === true && readBack?.lyrics === SYNCED_BODY,
        { beforeSave: shape(beforeSave), saved, readBack: shape(readBack) },
    );

    // Both endpoints use text mode, so a line ending is rewritten on the way out
    // and again on the way in. The pair is lossless through the bridge, but the
    // bytes on disk are not the bytes that were sent, which matters to anything
    // comparing a sidecar against a checksum or a copy made elsewhere.
    // Characterization, not a preference: recorded so a switch to binary mode
    // shows up here.
    const diskBytes = readFileSync(saved.savedTo);
    const crlf = makeCase("crlf-in", { "crlf-in.lrc": SYNCED_BODY.replace(/\n/g, "\r\n") });
    const crlfRead = await bridge.invoke("lyrics.get", { path: crlf });
    recorder.assertCase(
        "LY-14 line endings are translated both ways: CRLF on disk, LF over the bridge",
        diskBytes.toString("utf8") === SYNCED_BODY.replace(/\n/g, "\r\n") &&
            crlfRead?.lyrics === SYNCED_BODY,
        {
            sentLf: Buffer.from(SYNCED_BODY, "utf8").toString("hex"),
            onDisk: diskBytes.toString("hex"),
            crlfSidecarReadBack: Buffer.from(crlfRead?.lyrics ?? "", "utf8").toString("hex"),
        },
        { onDisk: "the sent text with CRLF", readBack: "the file's text with LF" },
    );

    const traversal = makeCase("traversal");
    const escapeTarget = join(fixtureDir, "..", `escaped-${runId}.lrc`);
    const escapedBefore = existsSync(escapeTarget);
    const rejected = await bridge.invoke("lyrics.save", {
        path: traversal,
        lyrics: SYNCED_BODY,
        target: ["file"],
        filename: `..\\escaped-${runId}.lrc`,
    });
    recorder.assertCase(
        "LY-15 a filename that climbs out of the directory is refused and nothing is written",
        rejected?.success === false &&
            typeof rejected?.error === "string" &&
            escapedBefore === false &&
            existsSync(escapeTarget) === false,
        { response: rejected, escapeTarget, existedBefore: escapedBefore, existsAfter: existsSync(escapeTarget) },
        { success: false, error: "a message", fileWritten: false },
    );

    const nonLocal = await bridge.invoke("lyrics.save", {
        path: `archive://${makeCase("archived")}|/inner.flac`,
        lyrics: SYNCED_BODY,
        target: ["file"],
    });
    recorder.assertCase(
        "LY-16 saving beside a track inside a container is refused, not written somewhere odd",
        nonLocal?.success === false && typeof nonLocal?.error === "string" && !nonLocal?.savedTo,
        nonLocal,
        { success: false, error: "a message", savedTo: undefined },
    );

    // The bug that started this was reported on a Japanese file name, where a
    // byte-level path mistake shows up as a miss rather than as mojibake.
    const jpStem = "日本語 テスト";
    const jpAudio = makeCase(jpStem, { [`${jpStem}.lrc`]: SYNCED_BODY });
    const jpRead = await bridge.invoke("lyrics.get", { path: jpAudio });
    const jpFileUri = await bridge.invoke("lyrics.get", { path: `file://${jpAudio}`, type: "any" });
    recorder.assertCase(
        "LY-17 a non-ASCII path is read intact, through both accepted forms",
        jpRead?.available === true &&
            jpRead?.lyrics === SYNCED_BODY &&
            String(jpRead?.sourcePath || "").endsWith(`${jpStem}.lrc`) &&
            sameShape(jpRead, jpFileUri),
        { native: shape(jpRead), fileUri: shape(jpFileUri) },
        { available: true, lyrics: SYNCED_BODY, bothFormsIdentical: true },
    );

    // Results are cached briefly, which is why every case above uses its own
    // path. Pinning it here means a change to the TTL shows up as this case
    // failing rather than as another case turning flaky. LY-18 for the record.
    const cached = makeCase("cache-ttl", { "cache-ttl.lrc": SYNCED_BODY });
    const startedAt = Date.now();
    const firstRead = await bridge.invoke("lyrics.get", { path: cached });
    writeFileSync(join(fixtureDir, "cache-ttl.lrc"), PLAIN_BODY, "utf8");
    const withinTtl = await bridge.invoke("lyrics.get", { path: cached });
    // The second read is only "within the ttl" if it actually got there in
    // time. On a loaded machine these two invokes plus a file write can take
    // longer than the ttl itself, and then the fresh content is the correct
    // answer - asserting the cached one would be testing the machine's speed.
    const elapsedMs = Date.now() - startedAt;
    await new Promise((resolve) => setTimeout(resolve, CACHE_TTL_MS + 400));
    const afterTtl = await bridge.invoke("lyrics.get", { path: cached });

    if (elapsedMs >= CACHE_TTL_MS) {
        recorder.addCase(
            "LY-18 a repeated read is served from cache until the entry expires",
            true,
            {
                skipped: "the second read did not land inside the ttl",
                elapsedMs,
                ttlMs: CACHE_TTL_MS,
            },
            { skipped: "the within-ttl premise did not hold" },
        );
        return;
    }

    recorder.assertCase(
        "LY-18 a repeated read is served from cache until the entry expires",
        firstRead?.lyrics === SYNCED_BODY &&
            withinTtl?.lyrics === SYNCED_BODY &&
            afterTtl?.lyrics === PLAIN_BODY,
        {
            first: firstRead?.lyrics,
            withinTtl: withinTtl?.lyrics,
            afterTtl: afterTtl?.lyrics,
            ttlMs: CACHE_TTL_MS,
            elapsedMs,
        },
        { withinTtl: "the first read's content", afterTtl: "the rewritten content" },
    );
}

async function runWriteCases(bridge, recorder) {
    const path = makeCase("overwrite", { "overwrite.lrc": PLAIN_BODY });
    const alias = `file://${path}`;
    const overwriteStartedAt = performance.now();
    const primed = await Promise.all([path, alias].map((trackPath) =>
        bridge.invoke("lyrics.get", { path: trackPath, source: "file" })));
    const written = await bridge.invoke("lyrics.save", { path, lyrics: SYNCED_BODY });
    const fresh = await Promise.all([path, alias].map((trackPath) =>
        bridge.invoke("lyrics.get", { path: trackPath, source: "file" })));
    const overwriteElapsedMs = performance.now() - overwriteStartedAt;
    recorder.assertCase(
        "LY-19 overwritten lyrics are readable through every path spelling",
        primed.every((r) => r?.lyrics === PLAIN_BODY) && written?.success === true &&
            fresh.every((r) => r?.lyrics === SYNCED_BODY),
        { primed: primed.map(shape), written, fresh: fresh.map(shape) },
        { primed: PLAIN_BODY, fresh: SYNCED_BODY },
    );
    assertCacheInvalidatedBeforeExpiry(
        recorder, "LY-19", overwriteElapsedMs,
        primed.every((r) => r?.lyrics === PLAIN_BODY) && written?.success === true &&
            fresh.every((r) => r?.lyrics === SYNCED_BODY),
        { primed: primed.map(shape), written, fresh: fresh.map(shape) },
    );

    const names = makeCase("explicit", { "one.words": SYNCED_BODY, "two.words": PLAIN_BODY });
    const explicitStartedAt = performance.now();
    const [one, two, missing, wrongType] = await Promise.all([
        bridge.invoke("lyrics.get", { path: names, filename: "one.words", source: "file", format: "txt" }),
        bridge.invoke("lyrics.get", { path: names, filename: "two.words", source: "file" }),
        bridge.invoke("lyrics.get", { path: names, filename: "absent.words", source: "file" }),
        bridge.invoke("lyrics.get", { path: names, filename: "one.words", source: "file", type: "unsynced" }),
    ]);
    const saved = await bridge.invoke("lyrics.save", {
        path: names, filename: "absent.words", format: "txt", lyrics: SYNCED_BODY,
    });
    const after = await bridge.invoke("lyrics.get", { path: names, filename: "absent.words", source: "file" });
    const explicitElapsedMs = performance.now() - explicitStartedAt;
    recorder.assertCase(
        "LY-20 filenames select separate content and an explicitly saved file becomes readable",
        one?.lyrics === SYNCED_BODY && two?.lyrics === PLAIN_BODY &&
            missing?.available === false && wrongType?.available === false &&
            saved?.savedTo === join(fixtureDir, "absent.words") && after?.lyrics === SYNCED_BODY,
        { one: shape(one), two: shape(two), missing: shape(missing), wrongType: shape(wrongType), saved, after: shape(after) },
        { separateNames: true, typeFilterPreserved: true, savedFileReadable: true },
    );
    assertCacheInvalidatedBeforeExpiry(
        recorder, "LY-20", explicitElapsedMs,
        missing?.available === false && saved?.success === true && after?.lyrics === SYNCED_BODY,
        { missing: shape(missing), saved, after: shape(after) },
    );

    const invalid = [];
    for (const filename of ["../escape.lrc", "C:escape.lrc", "track.lrc:stream", "NUL.txt", "trailing.lrc ", "bad?.lrc", "safe.lrc\u0000hidden"]) {
        for (const method of ["lyrics.get", "lyrics.exists", "lyrics.save"]) {
            const params = { path: names, filename };
            if (method === "lyrics.get") params.source = "embedded";
            if (method === "lyrics.save") Object.assign(params, { target: ["embedded"], lyrics: PLAIN_BODY });
            invalid.push({ method, filename, response: await bridge.invoke(method, params) });
        }
    }
    const config = await bridge.invoke("lyrics.save", { path: names, lyrics: PLAIN_BODY, target: ["config"] });
    recorder.assertCase(
        "LY-21 all lyrics handlers reject invalid filenames before source or target work, and config is rejected",
        invalid.every(({ response }) => response?.success === false && response?.code === "INVALID_PARAMS") &&
            config?.code === "INVALID_PARAMS",
        { invalid, config },
        { everyCode: "INVALID_PARAMS" },
    );

    const unknown = join(fixtureDir, "unknown.bin");
    writeFileSync(unknown, "not audio");
    writeFileSync(join(fixtureDir, "unknown.lrc"), SYNCED_BODY);
    const unknownRead = await bridge.invoke("lyrics.get", { path: unknown, source: "file" });
    const unknownSave = await bridge.invoke("lyrics.save", { path: unknown, lyrics: PLAIN_BODY });
    const unknownExplicit = await bridge.invoke("lyrics.get", { path: unknown, filename: "unknown.lrc", source: "file" });
    recorder.assertCase(
        "LY-22 unknown input never assumes an audio-name match and needs a filename for saving",
        unknownRead?.available === false && unknownSave?.code === "INVALID_PARAMS" &&
            /filename/i.test(unknownSave?.error ?? "") && unknownExplicit?.lyrics === SYNCED_BODY,
        { unknownRead: shape(unknownRead), unknownSave, unknownExplicit: shape(unknownExplicit) },
        { automatic: false, saveCode: "INVALID_PARAMS", explicit: true },
    );

    const embedded = makeCase("embedded-cache");
    const events = await createEventCollector(bridge, ["metadata:writeComplete"], { collectorId: `lyrics-${runId}` });
    try {
        const embeddedStartedAt = performance.now();
        const before = await bridge.invoke("lyrics.get", { path: embedded, source: "embedded" });
        const dispatched = await bridge.invoke("lyrics.save", {
            path: embedded, target: ["embedded"], lyrics: SYNCED_BODY,
        });
        const received = await events.waitFor((rows) => rows.some((row) => row.payload?.path === embedded), { timeoutMs: 20000 });
        const complete = received.find((row) => row.payload?.path === embedded)?.payload;
        const afterWrite = await bridge.invoke("lyrics.get", { path: embedded, source: "embedded" });
        const embeddedElapsedMs = performance.now() - embeddedStartedAt;
        recorder.assertCase(
            "LY-23 embedded save is dispatched and fresh lyrics are readable after writeComplete",
            before?.available === false && dispatched?.dispatched === true &&
                complete?.success === true && afterWrite?.lyrics === SYNCED_BODY,
            { before: shape(before), dispatched, complete, after: shape(afterWrite) },
            { dispatched: true, complete: true, lyrics: SYNCED_BODY },
        );
        assertCacheInvalidatedBeforeExpiry(
            recorder, "LY-23", embeddedElapsedMs,
            before?.available === false && dispatched?.dispatched === true &&
                complete?.success === true && afterWrite?.lyrics === SYNCED_BODY,
            { before: shape(before), dispatched, complete, after: shape(afterWrite) },
        );

        const external = makeCase("metadata-cache");
        const metadataStartedAt = performance.now();
        const externalBefore = await bridge.invoke("lyrics.get", { path: external, source: "embedded" });
        const metadataWrite = await bridge.invoke("metadata.write", {
            path: external, tags: { LYRICS: PLAIN_BODY },
        });
        const changed = await events.waitFor((rows) => rows.some((row) => row.payload?.path === external), { timeoutMs: 20000 });
        const metadataComplete = changed.find((row) => row.payload?.path === external)?.payload;
        const externalRead = await bridge.invoke("lyrics.get", { path: external, source: "embedded" });
        const metadataElapsedMs = performance.now() - metadataStartedAt;
        recorder.assertCase(
            "LY-24 lyrics written through metadata are readable after completion",
            externalBefore?.available === false && metadataWrite?.dispatched === true &&
                metadataComplete?.success === true && externalRead?.lyrics === PLAIN_BODY,
            { before: shape(externalBefore), metadataWrite, metadataComplete, externalRead: shape(externalRead) },
            { lyrics: PLAIN_BODY },
        );
        assertCacheInvalidatedBeforeExpiry(
            recorder, "LY-24", metadataElapsedMs,
            externalBefore?.available === false && metadataWrite?.dispatched === true &&
                metadataComplete?.success === true && externalRead?.lyrics === PLAIN_BODY,
            { before: shape(externalBefore), metadataWrite, metadataComplete, externalRead: shape(externalRead) },
        );

        writeFileSync(join(fixtureDir, "embedded-selected.words"), PLAIN_BODY);
        const [preferredEmbedded, selectedFile] = await Promise.all([
            bridge.invoke("lyrics.get", { path: embedded, filename: "embedded-selected.words" }),
            bridge.invoke("lyrics.get", { path: embedded, filename: "embedded-selected.words", source: "file" }),
        ]);
        recorder.assertCase(
            "LY-25 an explicit filename preserves embedded priority unless source is file",
            preferredEmbedded?.source === "embedded" && preferredEmbedded?.lyrics === SYNCED_BODY &&
                selectedFile?.source === "file" && selectedFile?.lyrics === PLAIN_BODY,
            { preferredEmbedded: shape(preferredEmbedded), selectedFile: shape(selectedFile) },
            { defaultSource: "embedded", selectedFile: PLAIN_BODY },
        );

        const allPath = makeCase("all-targets");
        const all = await bridge.invoke("lyrics.save", {
            path: allPath, target: ["all"], filename: "all-targets.words", lyrics: SYNCED_BODY,
        });
        const allEvents = await events.waitFor((rows) => rows.some((row) => row.payload?.path === allPath), { timeoutMs: 20000 });
        const allComplete = allEvents.find((row) => row.payload?.path === allPath)?.payload;
        recorder.assertCase(
            "LY-26 all expands to file and embedded only and preserves asynchronous completion",
            all?.success === true && JSON.stringify(Object.keys(all.results ?? {}).sort()) === '["embedded","file"]' &&
                all.results.file?.success === true && all.results.embedded?.dispatched === true &&
                allComplete?.success === true && existsSync(join(fixtureDir, "all-targets.words")),
            { all, allComplete },
            { targets: ["embedded", "file"], complete: true },
        );
    } finally {
        await events.stop();
    }
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

    mkdirSync(fixtureDir, { recursive: true });
    await runLibraryCases(bridge, recorder);
    await runFixtureCases(bridge, recorder);
    await runWriteCases(bridge, recorder);
} catch (error) {
    fatalError = error;
    blocked = Boolean(error?.blocked);
    console.error(`${blocked ? "BLOCKED" : "ERROR"}: ${error?.message || error}`);
} finally {
    rmSync(fixtureDir, { recursive: true, force: true });
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
            fixtureDir,
            fixtureDirRemoved: existsSync(fixtureDir) === false,
            targets,
        },
    }),
);

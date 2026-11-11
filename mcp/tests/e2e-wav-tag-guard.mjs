/**
 * Covers the guard that keeps tag writes from rewriting a WAV file whose tags
 * hold non-ASCII text.
 *
 * foobar2000 stores WAV tag text as ASCII in the RIFF INFO chunk and turns every
 * other character into '?'; it also reads a GBK INFO chunk as Latin-1. Any write
 * that rewrites such a file therefore loses its tags, so metadata.write,
 * metadata.writeBatch, metadata.removeTag, metadata.removeField and
 * replaygain.clear refuse it with NOT_SUPPORTED before queuing anything.
 *
 * Every subject is a file this script built byte by byte (lib/wav-fixture.mjs)
 * in a scratch directory, so nothing here touches the user's music and the
 * fixtures share no code with the endpoints under test. A refusal is checked
 * against the file's bytes, which must be unchanged after a wait longer than a
 * queued write needs.
 *
 * Usage: node mcp/tests/e2e-wav-tag-guard.mjs
 * Environment: FB2K_CDP_PORT, FB2K_E2E_TIMEOUT_MS, FB2K_WAV_FIXTURE_DIR (scratch
 * directory; default a folder under the system temp directory).
 */

import { mkdtempSync, readFileSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";
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
import { writeAiff, writeWav } from "./lib/wav-fixture.mjs";

const invokeTimeoutMs = envInt("FB2K_E2E_TIMEOUT_MS", 30000);
const runId = new Date().toISOString().replace(/[-:.TZ]/g, "");
const settleMs = 1500;

const CHINESE = { title: "明日世界终结时", artist: "张国荣", album: "国粤翻唱经典 CD2" };

const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms));

function tagValue(tags, key) {
    const wanted = key.toLowerCase();
    const found = Object.keys(tags ?? {}).find((name) => name.toLowerCase() === wanted);
    return found === undefined ? undefined : tags[found];
}

function isRefusal(result, field) {
    return (
        result?.success === false &&
        result.code === "NOT_SUPPORTED" &&
        result.details?.reason === "wavInfoNonAscii" &&
        (field === undefined || result.details?.field === field)
    );
}

/** Subjects: the three shapes of WAV a Chinese-speaking user's library holds. */
function buildSubjects(dir) {
    const paths = {
        plain: join(dir, "plain.wav"),
        gbkInfo: join(dir, "gbk-info.wav"),
        id3Only: join(dir, "id3-only.wav"),
        batchOk: join(dir, "batch-ok.wav"),
        aiff: join(dir, "chinese.aiff"),
    };
    writeWav(paths.plain);
    writeWav(paths.batchOk);
    writeWav(paths.gbkInfo, {
        info: [
            ["INAM", CHINESE.title],
            ["IART", CHINESE.artist],
            ["IPRD", CHINESE.album],
        ],
    });
    writeWav(paths.id3Only, {
        id3: [
            ["TIT2", CHINESE.title],
            ["TPE1", CHINESE.artist],
            ["TALB", CHINESE.album],
        ],
    });
    writeAiff(paths.aiff);
    return paths;
}

async function runGuardCases(bridge, recorder, paths, collector) {
    const { invoke } = bridge;

    // A refused call queues nothing, so unchanged bytes after the settle time mean no write ran.
    const untouchedAfter = async (path, before) => {
        await sleep(settleMs);
        return readFileSync(path).equals(before);
    };

    const plainWrite = await invoke("metadata.write", {
        path: paths.plain,
        tags: { TITLE: "Plain Title", RATING: "4" },
    });
    await collector.waitFor(
        (events) => events.some((e) => e.payload?.path === paths.plain && e.payload?.operation === "write"),
        { timeoutMs: 20000, pollMs: 100 },
    );
    const plainRead = await invoke("metadata.read", { path: paths.plain });
    recorder.assertCase(
        "WG-01 a WAV whose tags stay ASCII is written as before",
        plainWrite?.success === true &&
            plainWrite.dispatched === true &&
            tagValue(plainRead?.tags, "title") === "Plain Title",
        { write: plainWrite, title: tagValue(plainRead?.tags, "title") },
        { dispatched: true, title: "Plain Title" },
    );

    const chineseBefore = readFileSync(paths.plain);
    const chineseWrite = await invoke("metadata.write", {
        path: paths.plain,
        tags: { TITLE: CHINESE.title },
    });
    recorder.assertCase(
        "WG-02 writing Chinese text to a WAV is refused before anything is queued",
        isRefusal(chineseWrite, "TITLE") &&
            chineseWrite.dispatched === undefined &&
            (await untouchedAfter(paths.plain, chineseBefore)),
        { result: chineseWrite },
        { success: false, code: "NOT_SUPPORTED", details: { reason: "wavInfoNonAscii", field: "TITLE" } },
    );

    const gbkBefore = readFileSync(paths.gbkInfo);
    const ratingWrite = await invoke("metadata.write", { path: paths.gbkInfo, tags: { RATING: "3" } });
    recorder.assertCase(
        "WG-03 an ASCII-only write is refused when the file already holds non-ASCII INFO text",
        isRefusal(ratingWrite) && (await untouchedAfter(paths.gbkInfo, gbkBefore)),
        { result: ratingWrite },
        { success: false, code: "NOT_SUPPORTED", fileUnchanged: true },
    );

    const id3Before = readFileSync(paths.id3Only);
    const id3Write = await invoke("metadata.write", { path: paths.id3Only, tags: { RATING: "5" } });
    recorder.assertCase(
        "WG-04 text that lives only in the id3 chunk counts too",
        isRefusal(id3Write) && (await untouchedAfter(paths.id3Only, id3Before)),
        { result: id3Write },
        { success: false, code: "NOT_SUPPORTED", fileUnchanged: true },
    );

    for (const [offset, method] of ["metadata.removeTag", "metadata.removeField"].entries()) {
        const removal = await invoke(method, { path: paths.gbkInfo, tags: ["RATING"] });
        recorder.assertCase(
            `WG-0${5 + offset} ${method} is refused on the same file and leaves it unchanged`,
            isRefusal(removal) && (await untouchedAfter(paths.gbkInfo, gbkBefore)),
            { result: removal },
            { success: false, code: "NOT_SUPPORTED", fileUnchanged: true },
        );
    }

    const cleared = await invoke("replaygain.clear", { paths: [paths.plain, paths.gbkInfo] });
    recorder.assertCase(
        "WG-07 replaygain.clear refuses the whole call when one WAV would be damaged",
        isRefusal(cleared) &&
            cleared.path === paths.gbkInfo &&
            (await untouchedAfter(paths.gbkInfo, gbkBefore)),
        { result: cleared },
        { success: false, code: "NOT_SUPPORTED", path: paths.gbkInfo },
    );

    const batch = await invoke("metadata.writeBatch", {
        items: [
            { path: paths.batchOk, tags: { TITLE: "Batch Plain" } },
            { path: paths.gbkInfo, tags: { RATING: "2" } },
        ],
    });
    recorder.assertCase(
        "WG-08 a batch queues the safe entry and reports the refused one",
        batch?.success === false &&
            batch.code === "OPERATION_FAILED" &&
            batch.successCount === 1 &&
            batch.failCount === 1 &&
            batch.errors?.[0]?.path === paths.gbkInfo &&
            (await untouchedAfter(paths.gbkInfo, gbkBefore)),
        { result: batch },
        { code: "OPERATION_FAILED", successCount: 1, failCount: 1, refused: paths.gbkInfo },
    );

    const aiffWrite = await invoke("metadata.write", {
        path: paths.aiff,
        tags: { TITLE: CHINESE.title, ARTIST: CHINESE.artist },
    });
    const aiffDone = await collector.waitFor(
        (events) => events.some((e) => e.payload?.path === paths.aiff && e.payload?.operation === "write"),
        { timeoutMs: 20000, pollMs: 100 },
    );
    const aiffEvent = aiffDone.find((e) => e.payload?.path === paths.aiff);
    recorder.assertCase(
        "WG-09 other containers are not blocked: Chinese text is written to an AIFF",
        aiffWrite?.success === true && aiffWrite.dispatched === true && aiffEvent?.payload?.success === true,
        { write: aiffWrite, completion: aiffEvent?.payload },
        { dispatched: true, completion: { success: true } },
    );
}

const recorder = createRecorder();
let client;
let bridge;
let collector;
let scratchDir;
let blocked = false;
let fatalError;
let targets;

try {
    const connection = await connectBridgePage();
    client = connection.client;
    targets = connection.candidates;
    console.log(`Target: ${connection.page.title} ${connection.page.url}`);
    bridge = createBridge(client.Runtime, { invokeTimeoutMs });
    await requireResponsiveBridge(bridge, { probeId: runId });

    scratchDir = process.env.FB2K_WAV_FIXTURE_DIR
        ? join(process.env.FB2K_WAV_FIXTURE_DIR, `wav-guard-${runId}`)
        : mkdtempSync(join(tmpdir(), "fb2k-wav-guard-"));
    console.log(`Scratch directory: ${scratchDir}`);
    const paths = buildSubjects(scratchDir);

    collector = await createEventCollector(bridge, ["metadata:writeComplete"], { collectorId: runId });
    await runGuardCases(bridge, recorder, paths, collector);
} catch (error) {
    fatalError = error;
    blocked = Boolean(error?.blocked);
    console.error(`${blocked ? "BLOCKED" : "ERROR"}: ${error?.message || error}`);
} finally {
    if (collector) await collector.stop();
    if (scratchDir) rmSync(scratchDir, { recursive: true, force: true });
    await closeClient(client);
}

process.exit(
    report({
        recorder,
        blocked,
        fatalError,
        extra: { runId, targetPort: resolvePort(), invokeTimeoutMs, scratchDir, targets },
    }),
);

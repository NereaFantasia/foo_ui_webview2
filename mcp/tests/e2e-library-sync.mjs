/**
 * Covers library.getChanges and library.getTracks, the pair a page uses to keep
 * its own copy of the library.
 *
 * One FLAC copy is put into a monitored root and taken out again. Each step is
 * checked against the revision mark taken before it, so the changes reported can
 * only be this suite's own. What happens across a restart (the revision kept, a
 * new epoch after a crash, changes made while foobar2000 was closed) needs the
 * instance stopped and started and is not covered here.
 *
 * Usage: node mcp/tests/e2e-library-sync.mjs
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
const ALBUM = `E2E Library Sync ${runId}`;
const TITLE = `Library sync probe ${runId}`;
const MISSING = `C:\\e2e-library-sync-${runId}\\not-there.flac`;
const LIMIT = 5000;

/** Polls until getByPath no longer finds `path`; true once it is gone. */
async function waitForLibraryExit(bridge, path, { timeoutMs = 40000, pollMs = 1000 } = {}) {
    await bridge.invokeRaw("library.refresh", {}).catch(() => undefined);
    const deadline = Date.now() + timeoutMs;
    for (;;) {
        const found = await bridge.invoke("library.getByPath", { path });
        if (found?.found === false) return true;
        if (Date.now() >= deadline) return false;
        await new Promise((resolve) => setTimeout(resolve, pollMs));
    }
}

async function runBaseCases(bridge, recorder) {
    const { invoke } = bridge;

    const status = await invoke("library.getStatus", {});
    recorder.assertCase(
        "LS-01 getStatus reports the library as loaded",
        status?.success === true && status?.initialized === true,
        { initialized: status?.initialized },
        { initialized: true },
    );

    const [full, count] = await Promise.all([
        invoke("library.getChanges", {}),
        invoke("library.getCount", {}),
    ]);
    recorder.assertCase(
        "LS-02 without a mark, getChanges answers resync with every handle in the library",
        full?.success === true &&
            full?.resync === true &&
            typeof full?.epoch === "string" &&
            full.epoch.length > 0 &&
            Number.isInteger(full?.revision) &&
            Array.isArray(full?.handles) &&
            full.handles.length === count?.count &&
            full?.changed === undefined &&
            full?.removed === undefined,
        {
            resync: full?.resync,
            epoch: full?.epoch,
            revision: full?.revision,
            handles: full?.handles?.length,
            count: count?.count,
        },
        { resync: true, handles: "library.getCount" },
    );

    const mark = { epoch: full?.epoch, since: full?.revision };
    const [same, otherEpoch, ahead] = await Promise.all([
        invoke("library.getChanges", mark),
        invoke("library.getChanges", { epoch: `${mark.epoch}-other`, since: mark.since }),
        invoke("library.getChanges", { epoch: mark.epoch, since: mark.since + 1 }),
    ]);
    recorder.assertCase(
        "LS-03 the current mark answers with change lists and no handle list",
        same?.success === true &&
            same?.resync === false &&
            same?.epoch === mark.epoch &&
            Array.isArray(same?.changed) &&
            Array.isArray(same?.removed) &&
            same?.handles === undefined,
        { resync: same?.resync, changed: same?.changed?.length, removed: same?.removed?.length },
        { resync: false },
    );
    recorder.assertCase(
        "LS-04 another epoch, or a revision ahead of the host, answers resync",
        otherEpoch?.resync === true && ahead?.resync === true,
        { otherEpoch: otherEpoch?.resync, ahead: ahead?.resync },
        { otherEpoch: true, ahead: true },
    );

    const tooMany = await invoke("library.getTracks", {
        handles: Array.from({ length: LIMIT + 1 }, (_, i) => `${MISSING}.${i}`),
    });
    const unknownField = await invoke("library.getTracks", {
        handles: [MISSING],
        fields: ["title", "noSuchField"],
    });
    recorder.assertCase(
        "LS-05 getTracks refuses more than 5000 handles and an unknown field with INVALID_PARAMS",
        tooMany?.success === false &&
            tooMany?.code === "INVALID_PARAMS" &&
            tooMany?.details?.param === "handles" &&
            unknownField?.success === false &&
            unknownField?.code === "INVALID_PARAMS" &&
            Array.isArray(unknownField?.details?.unknownFields) &&
            unknownField.details.unknownFields.includes("noSuchField"),
        { tooMany, unknownField },
        { code: "INVALID_PARAMS" },
    );

    return mark;
}

async function runFixtureCases(bridge, recorder, area, mark, context) {
    const { invoke } = bridge;
    const fixture = area.taggedAudio("sync-probe.flac", [
        ["TITLE", TITLE],
        ["ALBUM", ALBUM],
        ["ARTIST", `E2E Sync Artist ${runId}`],
    ]);
    const arrival = await waitForLibraryEntry(bridge, fixture.targetPath);
    context.arrivalMs = arrival?.waitedMs ?? null;
    if (!arrival) {
        const error = new Error("the fixture never became a library member");
        error.blocked = true;
        throw error;
    }

    const rows = await invoke("library.query", {
        query: `album IS "${ALBUM}"`,
        fields: ["handle"],
    });
    const handle = rows?.tracks?.[0]?.handle;
    context.handle = handle;

    const added = await invoke("library.getChanges", mark);
    recorder.assertCase(
        "LS-06 a track added after the mark is listed in changed, and the revision advanced",
        typeof handle === "string" &&
            added?.resync === false &&
            added?.changed?.includes(handle) &&
            !added?.removed?.includes(handle) &&
            added?.revision > mark.since,
        { handle, resync: added?.resync, changed: added?.changed, revision: added?.revision },
        { changed: "includes the fixture", revision: `> ${mark.since}` },
    );

    const [narrow, whole] = await Promise.all([
        invoke("library.getTracks", { handles: [MISSING, handle], fields: ["handle", "title"] }),
        invoke("library.getTracks", { handles: [handle] }),
    ]);
    const row = narrow?.tracks?.[0];
    recorder.assertCase(
        "LS-07 getTracks returns the row with the fields asked for and lists the unknown handle in missing",
        narrow?.success === true &&
            narrow.tracks.length === 1 &&
            row?.handle === handle &&
            row?.title === TITLE &&
            // As in library.query, a projected row carries only the keys asked for; `index`
            // is one of them and is absent unless requested.
            Object.keys(row).sort().join(",") === "handle,title" &&
            JSON.stringify(narrow.missing) === JSON.stringify([MISSING]),
        { tracks: narrow?.tracks, missing: narrow?.missing },
        { tracks: [{ handle, title: TITLE }], missing: [MISSING] },
    );
    recorder.assertCase(
        "LS-08 without fields, getTracks returns the whole library row",
        whole?.success === true &&
            whole.tracks.length === 1 &&
            whole.tracks[0].album === ALBUM &&
            "rating" in whole.tracks[0] &&
            "absolutePath" in whole.tracks[0] &&
            whole.missing.length === 0,
        { row: whole?.tracks?.[0], missing: whole?.missing },
        { album: ALBUM, keys: "every library row key" },
    );

    const afterAdd = { epoch: added?.epoch, since: added?.revision };
    const cleanup = area.remove();
    context.cleanup = cleanup;
    const gone = await waitForLibraryExit(bridge, fixture.targetPath);
    const removed = await invoke("library.getChanges", afterAdd);
    const fetched = await invoke("library.getTracks", { handles: [handle] });
    recorder.assertCase(
        "LS-09 the removed track is listed in removed, and getTracks then reports it missing",
        gone &&
            removed?.resync === false &&
            removed?.removed?.includes(handle) &&
            !removed?.changed?.includes(handle) &&
            fetched?.tracks?.length === 0 &&
            JSON.stringify(fetched?.missing) === JSON.stringify([handle]),
        { gone, removed: removed?.removed, changed: removed?.changed, missing: fetched?.missing },
        { removed: "includes the fixture", missing: [handle] },
    );
}

const recorder = createRecorder();
let client;
let bridge;
let area;
let blocked = false;
let fatalError;
const context = {};

try {
    const connection = await connectBridgePage();
    client = connection.client;
    console.log(`Target: ${connection.page.title} ${connection.page.url}`);
    bridge = createBridge(client.Runtime, { invokeTimeoutMs });
    await requireResponsiveBridge(bridge, { probeId: runId });

    const mark = await runBaseCases(bridge, recorder);
    const resolved = await resolveFixtureArea(bridge, { runId });
    area = resolved.area;
    context.fixtureDir = area.dir;
    await runFixtureCases(bridge, recorder, area, mark, context);
} catch (error) {
    fatalError = error;
    blocked = Boolean(error?.blocked);
    console.error(`${blocked ? "BLOCKED" : "ERROR"}: ${error?.message || error}`);
} finally {
    if (area && !context.cleanup) {
        context.cleanup = area.remove();
        if (bridge) await bridge.invokeRaw("library.refresh", {}).catch(() => undefined);
    }
    await closeClient(client);
}

process.exit(
    report({
        recorder,
        blocked,
        fatalError,
        extra: { runId, targetPort: resolvePort(), invokeTimeoutMs, context },
    }),
);

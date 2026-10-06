/**
 * Covers file.* inspection and mutation endpoints: getInfo, rename and
 * the synchronous move, plus the asynchronous batch family (copyAsync,
 * moveAsync, deleteAsync, cancelOp) and its two events.
 *
 * Everything runs inside a scratch directory this script creates under the
 * user's TEMP with node, and node is also the oracle: after every operation the
 * disk is read directly rather than through file.exists or file.read, so a
 * symmetric bug in the file endpoints cannot read as a pass. TEMP is on the
 * host's write whitelist, so the paths are accepted as-is with no variable
 * expansion needed; the one %TEMP% case is there to pin how expansion echoes.
 *
 * Most of what this suite pins is the gap between the two generations of the
 * namespace. The synchronous move and delete are thin wrappers over
 * std::filesystem: move onto an existing file replaces it silently, move onto an
 * existing directory fails instead of moving into it, move into a missing parent
 * fails, delete refuses a non-empty directory. The asynchronous family decides
 * each of those the other way: an existing target is skipped as already-exists
 * unless overwrite is set, a directory destination receives the file, a missing
 * parent is created, and delete removes a whole tree. A page that switches from
 * one to the other inherits every one of those differences.
 *
 * The asynchronous endpoints answer with an operationId before anything has
 * happened; the outcome arrives as file:opProgress batches (at most 64 results
 * each) followed by one file:opComplete whose three counts add up to total.
 * Every case that dispatches waits for that completion event and then checks the
 * disk, rather than sleeping.
 *
 * Cancellation is a race, so the cancel case names its own outcome: when the
 * cancel lands mid-flight the remaining items are reported as skipped/cancelled
 * and the number of files on disk equals successCount; when the batch finished
 * first the case is recorded as skipped with the readings.
 *
 * The path length cases pin the MAX_PATH ceiling foobar2000 imposes on the host: past 259
 * characters the synchronous endpoints fail with 206 in details.value instead of reading an
 * existing file as missing, a new folder stops at 247, and an atomic write also needs its
 * temporary file to fit beside the target.
 *
 * Left out, and recorded as boundaries: the recycle-bin branch of deleteAsync
 * (the default), because it would leave files in the user's Recycle Bin that the
 * bridge cannot take back; and the eight-operation concurrency gate.
 *
 * Usage: node mcp/tests/e2e-file-ops.mjs
 * Environment: FB2K_CDP_PORT, FB2K_E2E_TIMEOUT_MS.
 */

import {
    existsSync,
    mkdirSync,
    readdirSync,
    readFileSync,
    realpathSync,
    rmSync,
    statSync,
    writeFileSync,
} from "node:fs";
import { tmpdir } from "node:os";
import { basename, join } from "node:path";

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

const invokeTimeoutMs = envInt("FB2K_E2E_TIMEOUT_MS", 15000);
const runId = new Date().toISOString().replace(/[-:.TZ]/g, "");
const EVENT_NAMES = ["file:opProgress", "file:opComplete"];

/** FileOpProgressEmitter flushes at this many results or 100 ms, whichever first. */
const PROGRESS_BATCH_LIMIT = 64;
/** Win32 error numbers the synchronous endpoints surface in details.value. */
const ERROR_PATH_NOT_FOUND = 3;
const ERROR_ACCESS_DENIED = 5;
const ERROR_DIR_NOT_EMPTY = 145;
const ERROR_FILENAME_EXCED_RANGE = 206;
/** foobar2000.exe does not declare longPathAware, so the host is held to MAX_PATH minus the NUL. */
const MAX_PATH_CHARS = 259;
const MAX_NEW_FOLDER_CHARS = 247;

const scratch = join(tmpdir(), `fb2k-e2e-file-ops-${runId}`);

function file(name, body = name) {
    const path = join(scratch, name);
    mkdirSync(join(path, ".."), { recursive: true });
    writeFileSync(path, body);
    return path;
}

function dir(name, inner = {}) {
    const path = join(scratch, name);
    mkdirSync(path, { recursive: true });
    for (const [innerName, body] of Object.entries(inner)) {
        writeFileSync(join(path, innerName), body);
    }
    return path;
}

function body(path) {
    return existsSync(path) ? readFileSync(path, "utf8") : null;
}

/** Dispatches one asynchronous operation and waits for its completion event. */
async function runOp(bridge, events, method, params, { timeoutMs = 10000 } = {}) {
    const dispatch = await bridge.invoke(method, params);
    const operationId = dispatch?.operationId;
    await events.waitFor(
        (received) =>
            received.some((e) => e.name === "file:opComplete" && e.payload?.operationId === operationId),
        { timeoutMs, pollMs: 50 },
    );
    const own = events.received.filter((e) => e.payload?.operationId === operationId);
    const progress = own.filter((e) => e.name === "file:opProgress").map((e) => e.payload);
    const complete = own.find((e) => e.name === "file:opComplete")?.payload;
    const results = progress.flatMap((p) => p.results ?? []);
    return { dispatch, operationId, progress, complete, results };
}

function countsAddUp(complete) {
    return (
        complete &&
        complete.successCount + complete.skippedCount + complete.failureCount === complete.total
    );
}

function resultsInOrder(results, items, key) {
    return results.length === items.length && results.every((r, i) => r.source === items[i][key]);
}

async function runGetInfoCases(bridge, recorder) {
    const { invoke, invokeRaw } = bridge;

    const subject = file("info.txt", "twelve bytes");
    const stat = statSync(subject);
    const info = await invoke("file.getInfo", { path: subject });
    recorder.assertCase(
        "FO-01 getInfo describes a file the script wrote: size agrees with the disk, and modified is whole seconds",
        info?.success === true &&
            info.exists === true &&
            info.isFile === true &&
            info.isDirectory === false &&
            info.size === stat.size &&
            info.name === "info.txt" &&
            info.extension === ".txt" &&
            info.parent === scratch &&
            Number.isInteger(info.modified) &&
            info.modified % 1000 === 0 &&
            Math.abs(info.modified - stat.mtimeMs) < 2000,
        { info, disk: { size: stat.size, mtimeMs: stat.mtimeMs } },
        {
            size: stat.size,
            name: "info.txt",
            extension: ".txt",
            parent: scratch,
            modified: "ms since epoch, truncated to the second, within 2 s of the disk",
        },
    );

    const folder = dir("info-dir");
    const folderInfo = await invoke("file.getInfo", { path: folder });
    recorder.assertCase(
        "FO-02 getInfo on a directory reports size zero and an empty extension",
        folderInfo?.success === true &&
            folderInfo.exists === true &&
            folderInfo.isDirectory === true &&
            folderInfo.isFile === false &&
            folderInfo.size === 0 &&
            folderInfo.extension === "" &&
            folderInfo.name === "info-dir",
        { folderInfo },
        { isDirectory: true, isFile: false, size: 0, extension: "" },
    );

    const missing = await invoke("file.getInfo", { path: join(scratch, "absent.txt") });
    recorder.assertCase(
        "FO-03 getInfo on a missing path is a success with exists false and nothing else",
        missing?.success === true &&
            missing.exists === false &&
            Object.keys(missing).sort().join() === "exists,success",
        { missing },
        { success: true, exists: false, keys: ["exists", "success"] },
    );

    const [noKey, emptyPath] = await Promise.all([
        invokeRaw("file.getInfo", {}),
        invokeRaw("file.getInfo", { path: "" }),
    ]);
    recorder.assertCase(
        "FO-04 a missing path key is INVALID_PARAMS from the parameter reader, but an empty string is stopped earlier by the path tier",
        noKey?.value?.success === false &&
            noKey.value.code === "INVALID_PARAMS" &&
            emptyPath?.value?.success === false &&
            emptyPath.value.code === "PERMISSION_DENIED" &&
            /'path'/.test(emptyPath.value.error ?? ""),
        { noKey: noKey?.value, emptyPath: emptyPath?.value },
        {
            noKey: { code: "INVALID_PARAMS" },
            emptyPath: { code: "PERMISSION_DENIED", error: "names 'path'; wording belongs to the path tier" },
        },
    );

    const viaVariable = await invoke("file.getInfo", {
        path: `%TEMP%\\${basename(scratch)}\\info.txt`,
    });
    recorder.assertCase(
        "FO-05 a %TEMP% path is expanded, and the echoed parent keeps the doubled separator the expansion produced",
        viaVariable?.success === true &&
            viaVariable.exists === true &&
            viaVariable.name === "info.txt" &&
            typeof viaVariable.parent === "string" &&
            viaVariable.parent.includes("\\\\"),
        { parent: viaVariable?.parent, name: viaVariable?.name },
        {
            exists: true,
            parent: "TEMP's trailing separator plus the literal one, not normalised",
        },
    );
}

async function runRenameCases(bridge, recorder) {
    const { invoke, invokeRaw } = bridge;

    const subject = file("rename-me.txt", "r");
    const renamed = await invoke("file.rename", { path: subject, newName: "renamed.txt" });
    const renamedPath = join(scratch, "renamed.txt");
    recorder.assertCase(
        "FO-06 rename echoes the old path as given and the new path resolved beside it, and the disk agrees",
        renamed?.success === true &&
            renamed.oldPath === subject &&
            renamed.newPath === renamedPath &&
            !existsSync(subject) &&
            body(renamedPath) === "r",
        { renamed, disk: { oldExists: existsSync(subject), newBody: body(renamedPath) } },
        { oldPath: subject, newPath: renamedPath, disk: "old gone, new holds the same bytes" },
    );

    const folder = dir("rename-dir", { "inner.txt": "inner" });
    const renamedDir = await invoke("file.rename", { path: folder, newName: "renamed-dir" });
    recorder.assertCase(
        "FO-07 rename works on a directory the same way",
        renamedDir?.success === true &&
            renamedDir.newPath === join(scratch, "renamed-dir") &&
            !existsSync(folder) &&
            body(join(scratch, "renamed-dir", "inner.txt")) === "inner",
        { renamedDir, innerBody: body(join(scratch, "renamed-dir", "inner.txt")) },
        { newPath: join(scratch, "renamed-dir"), inner: "moved along" },
    );

    const taken = file("taken.txt", "t");
    const [noPath, noName, slash, backslash, absent, collide] = await Promise.all([
        invokeRaw("file.rename", { newName: "x.txt" }),
        invokeRaw("file.rename", { path: renamedPath }),
        invokeRaw("file.rename", { path: renamedPath, newName: "a/b.txt" }),
        invokeRaw("file.rename", { path: renamedPath, newName: "a\\b.txt" }),
        invokeRaw("file.rename", { path: join(scratch, "ghost.txt"), newName: "x.txt" }),
        invokeRaw("file.rename", { path: renamedPath, newName: "taken.txt" }),
    ]);
    recorder.assertCase(
        "FO-08 rename refuses missing arguments and either separator in newName as INVALID_PARAMS, a missing source as NOT_FOUND and a taken name as OPERATION_FAILED, and touches nothing",
        noPath?.value?.code === "INVALID_PARAMS" &&
            noName?.value?.code === "INVALID_PARAMS" &&
            slash?.value?.code === "INVALID_PARAMS" &&
            backslash?.value?.code === "INVALID_PARAMS" &&
            absent?.value?.code === "NOT_FOUND" &&
            collide?.value?.code === "OPERATION_FAILED" &&
            body(renamedPath) === "r" &&
            body(taken) === "t",
        {
            noPath: noPath?.value,
            noName: noName?.value,
            slash: slash?.value,
            backslash: backslash?.value,
            absent: absent?.value,
            collide: collide?.value,
            disk: { renamed: body(renamedPath), taken: body(taken) },
        },
        {
            noPath: "INVALID_PARAMS",
            noName: "INVALID_PARAMS",
            separators: "INVALID_PARAMS for / and for \\",
            absent: "NOT_FOUND",
            collide: "OPERATION_FAILED - not an already-exists code",
            disk: "both files unchanged",
        },
    );
}

async function runSyncMoveCases(bridge, recorder) {
    const { invoke, invokeRaw } = bridge;

    const subject = file("move-me.txt", "m");
    const parent = dir("moved");
    const target = join(parent, "moved.txt");
    const moved = await invoke("file.move", { source: subject, destination: target });
    recorder.assertCase(
        "FO-09 move renames a file into an existing directory under a new name and echoes both paths as given",
        moved?.success === true &&
            moved.source === subject &&
            moved.destination === target &&
            !existsSync(subject) &&
            body(target) === "m",
        { moved, disk: { sourceExists: existsSync(subject), targetBody: body(target) } },
        { source: subject, destination: target, disk: "source gone, target holds the bytes" },
    );

    const replacing = file("move-src.txt", "fresh");
    const existing = file("move-existing.txt", "stale");
    const replaced = await invoke("file.move", { source: replacing, destination: existing });
    recorder.assertCase(
        "FO-10 move onto an existing file replaces it silently - the synchronous endpoint has no overwrite flag to refuse with",
        replaced?.success === true && !existsSync(replacing) && body(existing) === "fresh",
        { replaced, targetBody: body(existing) },
        { success: true, targetBody: "fresh" },
    );

    const orphan = file("move-orphan.txt", "o");
    const intoMissingParent = await invokeRaw("file.move", {
        source: orphan,
        destination: join(scratch, "no-such-parent", "orphan.txt"),
    });
    const intoDirectory = await invokeRaw("file.move", { source: orphan, destination: parent });
    recorder.assertCase(
        "FO-11 move fails into a missing parent and onto an existing directory, carrying the Win32 error in details.value, and leaves the source alone",
        intoMissingParent?.value?.success === false &&
            intoMissingParent.value.code === "OPERATION_FAILED" &&
            intoMissingParent.value.details?.value === ERROR_PATH_NOT_FOUND &&
            intoDirectory?.value?.success === false &&
            intoDirectory.value.code === "OPERATION_FAILED" &&
            intoDirectory.value.details?.value === ERROR_ACCESS_DENIED &&
            body(orphan) === "o" &&
            !existsSync(join(parent, "move-orphan.txt")),
        {
            intoMissingParent: intoMissingParent?.value,
            intoDirectory: intoDirectory?.value,
            disk: { orphanBody: body(orphan), parentListing: readdirSync(parent) },
        },
        {
            intoMissingParent: { code: "OPERATION_FAILED", details: { value: ERROR_PATH_NOT_FOUND } },
            intoDirectory: {
                code: "OPERATION_FAILED",
                details: { value: ERROR_ACCESS_DENIED },
                note: "not moved inside the directory, unlike moveAsync",
            },
        },
    );

    const folder = dir("move-dir", { "inner.txt": "inner" });
    const folderTarget = join(scratch, "move-dir-moved");
    const [movedDir, noSource, noDestination, absent] = await Promise.all([
        invoke("file.move", { source: folder, destination: folderTarget }),
        invokeRaw("file.move", { destination: folderTarget }),
        invokeRaw("file.move", { source: folderTarget }),
        invokeRaw("file.move", { source: join(scratch, "ghost.txt"), destination: join(scratch, "g.txt") }),
    ]);
    recorder.assertCase(
        "FO-12 move handles a directory, and refuses a missing source or destination argument and a source that does not exist",
        movedDir?.success === true &&
            !existsSync(folder) &&
            body(join(folderTarget, "inner.txt")) === "inner" &&
            noSource?.value?.code === "INVALID_PARAMS" &&
            noDestination?.value?.code === "INVALID_PARAMS" &&
            absent?.value?.code === "NOT_FOUND",
        {
            movedDir,
            innerBody: body(join(folderTarget, "inner.txt")),
            noSource: noSource?.value,
            noDestination: noDestination?.value,
            absent: absent?.value,
        },
        { movedDir: "success, tree intact", noSource: "INVALID_PARAMS", noDestination: "INVALID_PARAMS", absent: "NOT_FOUND" },
    );
}

async function runCopyAsyncCases(bridge, recorder, events) {
    const { invokeRaw } = bridge;

    const plain = file("copy-a.txt", "copy a");
    const blocked = file("copy-b.txt", "copy b");
    const blockedTarget = file("copy-b-target.txt", "old");
    const tree = dir("copy-dir", { "inner.txt": "inner" });
    const items = [
        { source: plain, destination: join(scratch, "copy-a-target.txt") },
        { source: join(scratch, "ghost.txt"), destination: join(scratch, "ghost-target.txt") },
        { source: blocked, destination: blockedTarget },
        { source: tree, destination: join(scratch, "copy-dir-target") },
    ];
    const op = await runOp(bridge, events, "file.copyAsync", { items });
    recorder.assertCase(
        "FO-13 copyAsync answers with an operationId, reports one result per item in order, and its completion counts add up",
        op.dispatch?.success === true &&
            typeof op.operationId === "string" &&
            op.operationId.startsWith("fileop_") &&
            op.dispatch.totalCount === items.length &&
            resultsInOrder(op.results, items, "source") &&
            op.results[0].status === "ok" &&
            op.results[0].reason === undefined &&
            op.results[0].destination === items[0].destination &&
            op.results[1].status === "failed" &&
            op.results[1].reason === "not-found" &&
            op.results[2].status === "skipped" &&
            op.results[2].reason === "already-exists" &&
            op.results[3].status === "ok" &&
            op.complete?.op === "copy" &&
            op.complete.total === 4 &&
            op.complete.successCount === 2 &&
            op.complete.skippedCount === 1 &&
            op.complete.failureCount === 1 &&
            op.complete.cancelled === false &&
            countsAddUp(op.complete) &&
            body(items[0].destination) === "copy a" &&
            body(blockedTarget) === "old" &&
            body(join(scratch, "copy-dir-target", "inner.txt")) === "inner",
        {
            dispatch: op.dispatch,
            results: op.results,
            complete: op.complete,
            disk: {
                aTarget: body(items[0].destination),
                blockedTarget: body(blockedTarget),
                treeInner: body(join(scratch, "copy-dir-target", "inner.txt")),
            },
        },
        {
            statuses: ["ok", "failed/not-found", "skipped/already-exists", "ok"],
            complete: { successCount: 2, skippedCount: 1, failureCount: 1, cancelled: false },
            disk: "new target written, existing target untouched, tree copied",
        },
    );

    const intoDir = dir("copy-into-dir");
    const deep = join(scratch, "deep", "er", "copy-a.txt");
    const overwriteItems = [
        { source: blocked, destination: blockedTarget },
        { source: plain, destination: intoDir },
        { source: plain, destination: deep },
    ];
    const withOverwrite = await runOp(bridge, events, "file.copyAsync", {
        items: overwriteItems,
        overwrite: true,
    });
    recorder.assertCase(
        "FO-14 with overwrite an existing target is replaced; a directory destination receives the file; a missing parent is created",
        withOverwrite.complete?.successCount === 3 &&
            withOverwrite.results.every((r) => r.status === "ok") &&
            body(blockedTarget) === "copy b" &&
            body(join(intoDir, "copy-a.txt")) === "copy a" &&
            body(deep) === "copy a",
        {
            results: withOverwrite.results,
            complete: withOverwrite.complete,
            disk: {
                overwritten: body(blockedTarget),
                insideDir: body(join(intoDir, "copy-a.txt")),
                deep: body(deep),
            },
        },
        { every: "ok", disk: { overwritten: "copy b", insideDir: "copy a", deep: "copy a" } },
    );

    const completeBefore = events.received.filter((e) => e.name === "file:opComplete").length;
    const [noItems, emptyItems, noDestination, noSource, nonObject, notBoolean, emptyStrings] =
        await Promise.all([
            invokeRaw("file.copyAsync", {}),
            invokeRaw("file.copyAsync", { items: [] }),
            invokeRaw("file.copyAsync", { items: [{ source: plain }] }),
            invokeRaw("file.copyAsync", { items: [{ destination: plain }] }),
            invokeRaw("file.copyAsync", { items: ["x"] }),
            invokeRaw("file.copyAsync", {
                items: [{ source: plain, destination: join(scratch, "z.txt") }],
                overwrite: "yes",
            }),
            invokeRaw("file.copyAsync", { items: [{ source: "", destination: "" }] }),
        ]);
    await events.waitFor(() => false, { timeoutMs: 300 });
    const completeAfter = events.received.filter((e) => e.name === "file:opComplete").length;
    recorder.assertCase(
        "FO-15 copyAsync shape refusals are INVALID_PARAMS - missing or empty items from the handler, a missing source or destination key from the path tier naming items[0] - and none of them dispatches",
        noItems?.value?.code === "INVALID_PARAMS" &&
            emptyItems?.value?.code === "INVALID_PARAMS" &&
            noDestination?.value?.code === "INVALID_PARAMS" &&
            /items\[0\]/.test(noDestination.value.error ?? "") &&
            /destination/.test(noDestination.value.error ?? "") &&
            noSource?.value?.code === "INVALID_PARAMS" &&
            /items\[0\]/.test(noSource.value.error ?? "") &&
            /source/.test(noSource.value.error ?? "") &&
            nonObject?.value?.code === "INVALID_PARAMS" &&
            notBoolean?.value?.code === "INVALID_PARAMS" &&
            /overwrite/.test(notBoolean.value.error ?? "") &&
            emptyStrings?.value?.code === "PERMISSION_DENIED" &&
            /items\[0\]\.source/.test(emptyStrings.value.error ?? "") &&
            completeAfter === completeBefore,
        {
            noItems: noItems?.value,
            emptyItems: emptyItems?.value,
            noDestination: noDestination?.value,
            noSource: noSource?.value,
            nonObject: nonObject?.value,
            notBoolean: notBoolean?.value,
            emptyStrings: emptyStrings?.value,
            newCompletions: completeAfter - completeBefore,
        },
        {
            shape: "INVALID_PARAMS throughout",
            emptyStrings: "PERMISSION_DENIED naming items[0].source - the path tier sees the empty string first",
            newCompletions: 0,
        },
    );

    const [protectedSource, protectedDestination] = await Promise.all([
        invokeRaw("file.copyAsync", {
            items: [
                {
                    source: "C:\\Windows\\System32\\drivers\\etc\\hosts",
                    destination: join(scratch, "hosts.txt"),
                },
            ],
        }),
        invokeRaw("file.copyAsync", {
            items: [{ source: plain, destination: `C:\\Windows\\e2e-${runId}.txt` }],
        }),
    ]);
    recorder.assertCase(
        "FO-16 a protected system path is refused by the path tier whether it is the source or the destination, and the message names which",
        protectedSource?.value?.success === false &&
            protectedSource.value.code === "PERMISSION_DENIED" &&
            /items\[0\]\.source/.test(protectedSource.value.error ?? "") &&
            protectedDestination?.value?.success === false &&
            protectedDestination.value.code === "PERMISSION_DENIED" &&
            /items\[0\]\.destination/.test(protectedDestination.value.error ?? "") &&
            !existsSync(join(scratch, "hosts.txt")),
        { protectedSource: protectedSource?.value, protectedDestination: protectedDestination?.value },
        {
            both: { code: "PERMISSION_DENIED" },
            names: "items[0].source / items[0].destination; the wording belongs to the path tier and is not pinned",
        },
    );
}

async function runMoveAsyncCases(bridge, recorder, events) {
    const { invokeRaw } = bridge;

    const plain = file("movea.txt", "ma");
    const blocked = file("moveb.txt", "mb");
    const blockedTarget = file("moveb-target.txt", "old-b");
    const tree = dir("move-tree", { "inner.txt": "inner" });
    const items = [
        { source: plain, destination: join(scratch, "movea-target.txt") },
        { source: join(scratch, "ghost.txt"), destination: join(scratch, "ghost-target.txt") },
        { source: blocked, destination: blockedTarget },
        { source: tree, destination: join(scratch, "move-tree-target") },
    ];
    const op = await runOp(bridge, events, "file.moveAsync", { items });
    recorder.assertCase(
        "FO-17 moveAsync moves a file and a tree, reports a missing source as not-found and an existing target as already-exists without touching it",
        resultsInOrder(op.results, items, "source") &&
            op.results[0].status === "ok" &&
            op.results[0].reason === undefined &&
            op.results[1].status === "failed" &&
            op.results[1].reason === "not-found" &&
            op.results[2].status === "skipped" &&
            op.results[2].reason === "already-exists" &&
            op.results[3].status === "ok" &&
            op.complete?.op === "move" &&
            op.complete.successCount === 2 &&
            op.complete.skippedCount === 1 &&
            op.complete.failureCount === 1 &&
            countsAddUp(op.complete) &&
            !existsSync(plain) &&
            body(items[0].destination) === "ma" &&
            body(blocked) === "mb" &&
            body(blockedTarget) === "old-b" &&
            !existsSync(tree) &&
            body(join(scratch, "move-tree-target", "inner.txt")) === "inner",
        {
            results: op.results,
            complete: op.complete,
            disk: {
                sourceGone: !existsSync(plain),
                target: body(items[0].destination),
                blockedSource: body(blocked),
                blockedTarget: body(blockedTarget),
                treeInner: body(join(scratch, "move-tree-target", "inner.txt")),
            },
        },
        {
            statuses: ["ok", "failed/not-found", "skipped/already-exists", "ok"],
            disk: "blocked source and its target both untouched - unlike the synchronous move",
        },
    );

    const intoDir = dir("move-into-dir");
    const dirA = dir("dir-a", { "x.txt": "x" });
    const dirB = dir("dir-b");
    const second = await runOp(bridge, events, "file.moveAsync", {
        items: [
            { source: blocked, destination: blockedTarget },
            { source: join(scratch, "movea-target.txt"), destination: intoDir },
        ],
        overwrite: true,
    });
    const dirOntoDir = await runOp(bridge, events, "file.moveAsync", {
        items: [{ source: dirA, destination: dirB }],
    });
    recorder.assertCase(
        "FO-18 with overwrite moveAsync replaces the target; a directory destination receives the file; a directory onto an existing directory is skipped rather than merged",
        second.complete?.successCount === 2 &&
            body(blockedTarget) === "mb" &&
            !existsSync(blocked) &&
            body(join(intoDir, "movea-target.txt")) === "ma" &&
            dirOntoDir.results[0]?.status === "skipped" &&
            dirOntoDir.results[0].reason === "already-exists" &&
            existsSync(join(dirA, "x.txt")) &&
            readdirSync(dirB).length === 0,
        {
            second: { results: second.results, complete: second.complete },
            dirOntoDir: { results: dirOntoDir.results, complete: dirOntoDir.complete },
            disk: {
                overwritten: body(blockedTarget),
                insideDir: body(join(intoDir, "movea-target.txt")),
                dirAIntact: existsSync(join(dirA, "x.txt")),
                dirBListing: readdirSync(dirB),
            },
        },
        {
            overwritten: "mb",
            insideDir: "ma",
            dirOntoDir: "skipped/already-exists, source tree intact, target still empty",
        },
    );

    const [noItems, notBoolean] = await Promise.all([
        invokeRaw("file.moveAsync", {}),
        invokeRaw("file.moveAsync", {
            items: [{ source: blockedTarget, destination: join(scratch, "z.txt") }],
            overwrite: 1,
        }),
    ]);
    recorder.assertCase(
        "FO-19 moveAsync applies the same shape checks as copyAsync",
        noItems?.value?.code === "INVALID_PARAMS" &&
            notBoolean?.value?.code === "INVALID_PARAMS" &&
            /overwrite/.test(notBoolean.value.error ?? "") &&
            body(blockedTarget) === "mb",
        { noItems: noItems?.value, notBoolean: notBoolean?.value },
        { both: "INVALID_PARAMS", disk: "untouched" },
    );
}

async function runDeleteAsyncCases(bridge, recorder, events) {
    const { invokeRaw } = bridge;

    const plain = file("del-a.txt", "da");
    const tree = dir("del-tree", { "inner.txt": "inner" });
    const syncTree = dir("del-sync-tree", { "inner.txt": "inner" });
    const twice = file("del-twice.txt", "twice");

    const syncRefusal = await invokeRaw("file.delete", { path: syncTree, moveToTrash: false });
    const paths = [plain, join(scratch, "ghost.txt"), tree, twice, twice];
    const op = await runOp(bridge, events, "file.deleteAsync", { paths, moveToTrash: false });
    recorder.assertCase(
        "FO-20 permanent deleteAsync removes a file and a whole tree - where the synchronous delete refuses a non-empty directory - reports a missing path as not-found, and a path listed twice succeeds once",
        syncRefusal?.value?.success === false &&
            syncRefusal.value.code === "OPERATION_FAILED" &&
            syncRefusal.value.details?.value === ERROR_DIR_NOT_EMPTY &&
            existsSync(join(syncTree, "inner.txt")) &&
            op.results.length === paths.length &&
            op.results.every((r, i) => r.source === paths[i] && r.destination === undefined) &&
            op.results[0].status === "ok" &&
            op.results[1].status === "failed" &&
            op.results[1].reason === "not-found" &&
            op.results[2].status === "ok" &&
            op.results[3].status === "ok" &&
            op.results[4].status === "failed" &&
            op.results[4].reason === "not-found" &&
            op.complete?.op === "delete" &&
            op.complete.successCount === 3 &&
            op.complete.failureCount === 2 &&
            op.complete.skippedCount === 0 &&
            countsAddUp(op.complete) &&
            !existsSync(plain) &&
            !existsSync(tree) &&
            !existsSync(twice),
        {
            syncRefusal: syncRefusal?.value,
            results: op.results,
            complete: op.complete,
            disk: { plain: existsSync(plain), tree: existsSync(tree), syncTree: existsSync(syncTree) },
        },
        {
            syncRefusal: { code: "OPERATION_FAILED", details: { value: ERROR_DIR_NOT_EMPTY } },
            statuses: ["ok", "failed/not-found", "ok", "ok", "failed/not-found"],
            results: "no destination field on delete results",
            disk: "file and tree gone, the synchronous tree still there",
        },
    );

    const [noPaths, empty, nonString, emptyString, notBoolean, protectedPath] = await Promise.all([
        invokeRaw("file.deleteAsync", {}),
        invokeRaw("file.deleteAsync", { paths: [] }),
        invokeRaw("file.deleteAsync", { paths: [1] }),
        invokeRaw("file.deleteAsync", { paths: [""] }),
        invokeRaw("file.deleteAsync", { paths: [join(scratch, "x.txt")], moveToTrash: "no" }),
        invokeRaw("file.deleteAsync", { paths: ["C:\\Windows\\notepad.exe"], moveToTrash: false }),
    ]);
    recorder.assertCase(
        "FO-21 deleteAsync refusals: missing or empty paths and a non-boolean moveToTrash are INVALID_PARAMS, a non-string entry is INVALID_PARAMS naming paths[0], an empty string or a protected path is PERMISSION_DENIED naming paths[0]",
        noPaths?.value?.code === "INVALID_PARAMS" &&
            empty?.value?.code === "INVALID_PARAMS" &&
            nonString?.value?.code === "INVALID_PARAMS" &&
            /paths\[0\]/.test(nonString.value.error ?? "") &&
            emptyString?.value?.code === "PERMISSION_DENIED" &&
            /paths\[0\]/.test(emptyString.value.error ?? "") &&
            notBoolean?.value?.code === "INVALID_PARAMS" &&
            /moveToTrash/.test(notBoolean.value.error ?? "") &&
            protectedPath?.value?.code === "PERMISSION_DENIED" &&
            /paths\[0\]/.test(protectedPath.value.error ?? ""),
        {
            noPaths: noPaths?.value,
            empty: empty?.value,
            nonString: nonString?.value,
            emptyString: emptyString?.value,
            notBoolean: notBoolean?.value,
            protectedPath: protectedPath?.value,
        },
        {
            shape: "INVALID_PARAMS",
            pathTier: "PERMISSION_DENIED naming paths[0]; wording not pinned",
        },
    );

    recorder.addCase(
        "FO-22 the recycle-bin branch of deleteAsync and the eight-operation concurrency gate are out of reach here",
        true,
        {
            notExercised: ["deleteAsync with moveToTrash true (the default)", "too many concurrent file operations"],
            reason:
                "the recycle-bin branch would leave this script's files in the user's Recycle Bin, which the bridge cannot take back; the gate needs eight operations still in flight at once",
        },
        { recorded: "coverage boundary, not a wish" },
    );
}

async function runCancelCases(bridge, recorder, events) {
    const { invoke, invokeRaw } = bridge;

    const [unknown, missing, empty, nonString] = await Promise.all([
        invokeRaw("file.cancelOp", { operationId: `fileop_${runId}_never` }),
        invokeRaw("file.cancelOp", {}),
        invokeRaw("file.cancelOp", { operationId: "" }),
        invokeRaw("file.cancelOp", { operationId: 5 }),
    ]);
    recorder.assertCase(
        "FO-23 cancelOp on an unknown id is a success with cancelled false, and a missing, empty or non-string id is INVALID_PARAMS",
        unknown?.value?.success === true &&
            unknown.value.cancelled === false &&
            missing?.value?.code === "INVALID_PARAMS" &&
            empty?.value?.code === "INVALID_PARAMS" &&
            nonString?.value?.code === "INVALID_PARAMS",
        { unknown: unknown?.value, missing: missing?.value, empty: empty?.value, nonString: nonString?.value },
        { unknown: { success: true, cancelled: false }, others: "INVALID_PARAMS" },
    );

    // Enough work that a cancel issued right after dispatch usually lands
    // mid-flight: 400 files of 64 KiB, one result each.
    const bulkIn = dir("bulk");
    const bulkOut = dir("bulk-out");
    const chunk = Buffer.alloc(64 * 1024, 1);
    const items = [];
    for (let i = 0; i < 400; i++) {
        const name = `f${String(i).padStart(4, "0")}.bin`;
        writeFileSync(join(bulkIn, name), chunk);
        items.push({ source: join(bulkIn, name), destination: join(bulkOut, name) });
    }
    const dispatch = await invoke("file.copyAsync", { items });
    const operationId = dispatch?.operationId;
    const cancel = await invoke("file.cancelOp", { operationId });
    await events.waitFor(
        (received) =>
            received.some((e) => e.name === "file:opComplete" && e.payload?.operationId === operationId),
        { timeoutMs: 20000, pollMs: 50 },
    );
    const own = events.received.filter((e) => e.payload?.operationId === operationId);
    const progress = own.filter((e) => e.name === "file:opProgress").map((e) => e.payload);
    const complete = own.find((e) => e.name === "file:opComplete")?.payload;
    const results = progress.flatMap((p) => p.results);
    const statusTally = results.reduce((tally, r) => {
        const key = `${r.status}/${r.reason ?? "-"}`;
        tally[key] = (tally[key] ?? 0) + 1;
        return tally;
    }, {});
    const copied = readdirSync(bulkOut).length;
    const cancelAgain = await invoke("file.cancelOp", { operationId });

    const landed = cancel?.cancelled === true;
    recorder.assertCase(
        landed
            ? "FO-24 a cancel that lands mid-flight stops new copies: the rest are reported skipped/cancelled, the counts still add up to total, the files on disk equal successCount, and a second cancel finds nothing"
            : "FO-24 the batch finished before the cancel arrived, so the cancel reports false and the completion is uncancelled",
        landed
            ? complete?.cancelled === true &&
                  complete.successCount + complete.skippedCount + complete.failureCount === items.length &&
                  complete.skippedCount > 0 &&
                  complete.failureCount === 0 &&
                  results.length === items.length &&
                  results.every(
                      (r) =>
                          (r.status === "ok" && r.reason === undefined) ||
                          (r.status === "skipped" && r.reason === "cancelled"),
                  ) &&
                  copied === complete.successCount &&
                  cancelAgain?.cancelled === false
            : cancel?.cancelled === false &&
                  complete?.cancelled === false &&
                  complete.successCount === items.length &&
                  copied === items.length &&
                  cancelAgain?.cancelled === false,
        {
            cancel,
            complete,
            statusTally,
            copiedOnDisk: copied,
            cancelAgain,
            landed,
        },
        landed
            ? {
                  complete: { cancelled: true, failureCount: 0, sum: items.length },
                  results: "ok or skipped/cancelled only, one per item",
                  copiedOnDisk: "equals successCount",
                  cancelAgain: { cancelled: false },
              }
            : { skipped: "the race was lost; readings recorded", cancelAgain: { cancelled: false } },
    );

    const doneValues = progress.map((p) => p.done);
    recorder.assertCase(
        "FO-25 progress arrives in batches of at most 64 results, done climbs to total, and every batch carries the op name",
        progress.length > 1 &&
            progress.every((p) => p.results.length <= PROGRESS_BATCH_LIMIT && p.results.length > 0) &&
            progress.every((p) => p.total === items.length && p.op === "copy") &&
            doneValues.every((d, i) => i === 0 || d > doneValues[i - 1]) &&
            doneValues.at(-1) === items.length &&
            progress.reduce((sum, p) => sum + p.results.length, 0) === items.length,
        { batches: progress.length, sizes: progress.map((p) => p.results.length), done: doneValues },
        { batchSize: `1..${PROGRESS_BATCH_LIMIT}`, done: "strictly increasing, ending at total" },
    );
}

/**
 * Builds a path of exactly `total` characters ending in `name`, under the scratch directory with
 * junctions resolved: an atomic write replaces the target at its resolved location, so the
 * lengths only line up when the path handed over is already that location. Node writes through
 * `\\?\` and is not held to MAX_PATH, so it can create and inspect what the host cannot.
 */
function pathOfLength(total, name) {
    const base = realpathSync.native(scratch);
    const padding = total - base.length - 2 - name.length;
    if (padding < 1 || padding > 255) throw new Error(`cannot pad ${base} to ${total} characters for ${name}`);
    const folder = join(base, "p".repeat(padding));
    mkdirSync(folder, { recursive: true });
    return join(folder, name);
}

async function runPathLengthCases(bridge, recorder) {
    const { invoke, invokeRaw } = bridge;

    const tooLong = pathOfLength(MAX_PATH_CHARS + 1, "too-long.txt");
    const [writeTooLong, atomicTooLong] = await Promise.all([
        invokeRaw("file.write", { path: tooLong, content: "x" }),
        invokeRaw("file.write", { path: tooLong, content: "x", atomic: true }),
    ]);
    writeFileSync(tooLong, "kept");
    const [readTooLong, existsTooLong, infoTooLong] = await Promise.all([
        invokeRaw("file.read", { path: tooLong }),
        invokeRaw("file.exists", { path: tooLong }),
        invokeRaw("file.getInfo", { path: tooLong }),
    ]);
    const longFailure = (r) =>
        r?.value?.success === false &&
        r.value.code === "OPERATION_FAILED" &&
        r.value.details?.value === ERROR_FILENAME_EXCED_RANGE;
    recorder.assertCase(
        "FO-26 a path one character past 259 fails write, atomic write, read, exists and getInfo with OPERATION_FAILED and details.value 206, rather than reading an existing file as missing",
        [writeTooLong, atomicTooLong, readTooLong, existsTooLong, infoTooLong].every(longFailure) &&
            body(tooLong) === "kept",
        {
            length: tooLong.length,
            write: writeTooLong?.value,
            atomic: atomicTooLong?.value,
            read: readTooLong?.value,
            exists: existsTooLong?.value,
            getInfo: infoTooLong?.value,
            disk: body(tooLong),
        },
        { code: "OPERATION_FAILED", details: { value: ERROR_FILENAME_EXCED_RANGE }, disk: "kept, written by node" },
    );

    // The temporary name is .~<pid>-<seq>.tmp, at most 27 characters; a 40-character file name
    // leaves it room at the limit.
    const atLimit = pathOfLength(MAX_PATH_CHARS, `${"n".repeat(35)}.json`);
    const atomicAtLimit = await invoke("file.write", { path: atLimit, content: '{"v":2}', atomic: true });
    const leftovers = readdirSync(join(atLimit, "..")).filter((n) => n.endsWith(".tmp"));
    recorder.assertCase(
        "FO-27 an atomic write to a 259-character path succeeds when the file name is longer than the temporary name, and leaves no temporary file",
        atomicAtLimit?.success === true && body(atLimit) === '{"v":2}' && leftovers.length === 0,
        { length: atLimit.length, atomicAtLimit, disk: body(atLimit), leftovers },
        { success: true, disk: '{"v":2}', leftovers: [] },
    );

    // A short file name at the limit: the target fits, the temporary file next to it does not.
    const shortName = pathOfLength(MAX_PATH_CHARS, "a.json");
    writeFileSync(shortName, "old");
    const [plainShort, atomicShort] = await Promise.all([
        invokeRaw("file.write", { path: shortName, content: "plain" }),
        invokeRaw("file.write", { path: pathOfLength(MAX_PATH_CHARS, "b.json"), content: "x", atomic: true }),
    ]);
    const atomicOver = await invokeRaw("file.write", { path: shortName, content: "new", atomic: true });
    recorder.assertCase(
        "FO-28 at 259 characters with a short file name, a plain write succeeds but an atomic write fails with 206 because its temporary file is longer, and the target keeps its content",
        plainShort?.value?.success === true &&
            longFailure(atomicShort) &&
            longFailure(atomicOver) &&
            body(shortName) === "plain",
        {
            length: shortName.length,
            plain: plainShort?.value,
            atomicNewFile: atomicShort?.value,
            atomicExisting: atomicOver?.value,
            disk: body(shortName),
        },
        {
            plain: { success: true },
            atomic: { code: "OPERATION_FAILED", details: { value: ERROR_FILENAME_EXCED_RANGE } },
            disk: "plain",
        },
    );

    // CreateDirectoryW keeps 12 characters back for an 8.3 name, so a new folder stops at 247.
    const folderAt = (total) => {
        const parent = join(realpathSync.native(scratch), `mkdir-${total}`);
        mkdirSync(parent, { recursive: true });
        return join(parent, "d".repeat(total - parent.length - 1));
    };
    const [fits, over] = [folderAt(MAX_NEW_FOLDER_CHARS), folderAt(MAX_NEW_FOLDER_CHARS + 1)];
    const [mkdirFits, mkdirOver] = await Promise.all([
        invokeRaw("file.mkdir", { path: fits }),
        invokeRaw("file.mkdir", { path: over }),
    ]);
    recorder.assertCase(
        "FO-29 mkdir creates a 247-character folder and fails one character longer with OPERATION_FAILED and details.value 206",
        mkdirFits?.value?.created === true && existsSync(fits) && longFailure(mkdirOver) && !existsSync(over),
        { lengths: [fits.length, over.length], fits: mkdirFits?.value, over: mkdirOver?.value },
        {
            fits: { created: true },
            over: { code: "OPERATION_FAILED", details: { value: ERROR_FILENAME_EXCED_RANGE } },
        },
    );
}

async function runAsyncPathLengthCases(bridge, recorder, events) {
    const tooLong = pathOfLength(MAX_PATH_CHARS + 1, "async-too-long.txt");
    writeFileSync(tooLong, "kept");
    const shortSource = file("async-short.txt", "short");
    const longTarget = pathOfLength(MAX_PATH_CHARS + 1, "async-target.txt");

    // A folder copied under a destination of 240 characters puts its 30-character file at 271.
    const tree = dir("async-tree", { [`${"t".repeat(26)}.txt`]: "inside" });
    const treeDestination = join(realpathSync.native(scratch), "async-dest");
    const deepDestination = join(treeDestination, "q".repeat(240 - treeDestination.length - 1));

    const copied = await runOp(bridge, events, "file.copyAsync", {
        items: [
            { source: tooLong, destination: join(scratch, "copied-from-long.txt") },
            { source: shortSource, destination: longTarget },
            { source: tree, destination: deepDestination },
        ],
    });
    const moved = await runOp(bridge, events, "file.moveAsync", {
        items: [{ source: tooLong, destination: join(scratch, "moved-from-long.txt") }],
    });
    // Refused before the recycle bin is reached, so nothing lands there.
    const trashed = await runOp(bridge, events, "file.deleteAsync", { paths: [tooLong] });
    const deleted = await runOp(bridge, events, "file.deleteAsync", {
        paths: [tooLong],
        moveToTrash: false,
    });
    const tooLongResult = (r) => r?.status === "failed" && r.reason === "path-too-long";
    recorder.assertCase(
        "FO-30 the asynchronous family reports a path past 259 characters as failed with reason path-too-long - a long source, a long destination, a file inside a copied folder, and both delete branches - and writes no file",
        copied.results.length === 3 &&
            copied.results.every(tooLongResult) &&
            moved.results.length === 1 &&
            tooLongResult(moved.results[0]) &&
            tooLongResult(trashed.results[0]) &&
            tooLongResult(deleted.results[0]) &&
            [copied, moved, trashed, deleted].every((op) => countsAddUp(op.complete)) &&
            body(tooLong) === "kept" &&
            body(shortSource) === "short" &&
            !existsSync(longTarget) &&
            !existsSync(join(scratch, "copied-from-long.txt")) &&
            !existsSync(join(scratch, "moved-from-long.txt")),
        {
            copy: copied.results,
            move: moved.results,
            trash: trashed.results,
            delete: deleted.results,
            lengths: { tooLong: tooLong.length, longTarget: longTarget.length, deepDestination: deepDestination.length },
            disk: { tooLong: body(tooLong), shortSource: body(shortSource), longTarget: existsSync(longTarget) },
        },
        { every: { status: "failed", reason: "path-too-long" }, disk: "sources kept, no file written" },
    );
}

const recorder = createRecorder();
let client;
let bridge;
let events;
let blocked = false;
let fatalError;
let targets;

try {
    mkdirSync(scratch, { recursive: true });
    const connection = await connectBridgePage();
    client = connection.client;
    targets = connection.candidates;
    console.log(`Target: ${connection.page.title} ${connection.page.url}`);
    bridge = createBridge(client.Runtime, { invokeTimeoutMs });
    await requireResponsiveBridge(bridge, { probeId: runId });
    console.log(`Scratch: ${scratch}`);

    events = await createEventCollector(bridge, EVENT_NAMES, { collectorId: runId });

    await runGetInfoCases(bridge, recorder);
    await runRenameCases(bridge, recorder);
    await runSyncMoveCases(bridge, recorder);
    await runCopyAsyncCases(bridge, recorder, events);
    await runMoveAsyncCases(bridge, recorder, events);
    await runDeleteAsyncCases(bridge, recorder, events);
    await runCancelCases(bridge, recorder, events);
    await runPathLengthCases(bridge, recorder);
    await runAsyncPathLengthCases(bridge, recorder, events);
} catch (error) {
    fatalError = error;
    blocked = Boolean(error?.blocked);
    console.error(`${blocked ? "BLOCKED" : "ERROR"}: ${error?.message || error}`);
} finally {
    if (events) await events.stop();
    rmSync(scratch, { recursive: true, force: true });
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
            scratch,
            scratchRemoved: !existsSync(scratch),
            eventsSeen: [...new Set((events?.received ?? []).map((e) => e.name))],
            targets,
        },
    }),
);

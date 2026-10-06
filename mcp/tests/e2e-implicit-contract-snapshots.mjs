/**
 * Snapshots the implicit contracts of endpoints whose real behaviour differs
 * from what a page would reasonably expect: writes that succeed while storing
 * something else, lookups that report a miss as an empty success, responses
 * that carry no success field.
 *
 * Each case records the observed shape rather than a wish, so an intentional
 * change shows up as a failing snapshot to be updated, not as a silent drift.
 *
 * Fixtures are created under %TEMP% and removed afterwards. Whole-surface
 * reachability is checked separately by e2e-api-surface.mjs.
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

const invokeTimeoutMs = envInt("FB2K_SNAPSHOT_TIMEOUT_MS", 5000);

const snapshotId = new Date().toISOString().replace(/[-:.TZ]/g, "");
const root = `%TEMP%\\foo_ui_webview2_implicit_contract_${snapshotId}`;
const createdPaths = [];
const recorder = createRecorder();
const { assertCase } = recorder;
let bridgeResponsive = false;

async function removePath(bridge, path) {
    await bridge
        .invokeRaw("file.delete", { path, moveToTrash: false }, 2000)
        .catch(() => undefined);
}

async function runSnapshots(bridge) {
    await requireResponsiveBridge(bridge, { probeId: snapshotId });
    bridgeResponsive = true;

    const { invoke, invokeRaw } = bridge;

    await invoke("file.mkdir", { path: root });

    const payload = "iVBORw0KGgo=";
    const dataUrl = `data:image/png;base64,${payload}`;

    const exactPath = `${root}\\exact.png`;
    createdPaths.push(exactPath);
    const exactWrite = await invoke("file.write", {
        path: exactPath,
        encoding: "binary",
        content: `base64:${payload}`,
    });
    const exactRead = await invoke("file.read", {
        path: exactPath,
        encoding: "binary",
    });
    assertCase(
        "A-01 exact binary write",
        exactWrite?.success === true &&
            exactRead?.content === payload &&
            exactRead?.encoding === "base64",
        { exactWrite, exactRead },
        { success: true, content: payload, encoding: "base64" },
    );

    const rawPath = `${root}\\raw-base64.png`;
    createdPaths.push(rawPath);
    const rawWrite = await invoke("file.write", {
        path: rawPath,
        encoding: "binary",
        content: payload,
    });
    const rawRead = await invoke("file.read", { path: rawPath });
    assertCase(
        "A-02 binary plus raw Base64 writes text",
        rawWrite?.success === true && rawRead?.content === payload,
        { rawWrite, rawRead },
        { success: true, textContent: payload },
    );

    const roundTripPath = `${root}\\naive-roundtrip.png`;
    createdPaths.push(roundTripPath);
    const roundTripWrite = await invoke("file.write", {
        path: roundTripPath,
        encoding: exactRead.encoding,
        content: exactRead.content,
    });
    const roundTripRead = await invoke("file.read", { path: roundTripPath });
    assertCase(
        "A-03 binary read response cannot be written back unchanged",
        roundTripWrite?.success === true && roundTripRead?.content === payload,
        { roundTripWrite, roundTripRead },
        { success: true, textContent: payload },
    );

    const dataUrlPath = `${root}\\data-url.png`;
    createdPaths.push(dataUrlPath);
    const dataUrlWrite = await invoke("file.write", {
        path: dataUrlPath,
        encoding: "binary",
        content: dataUrl,
    });
    const dataUrlRead = await invoke("file.read", { path: dataUrlPath });
    assertCase(
        "A-04 binary plus Data URL writes the URL text",
        dataUrlWrite?.success === true && dataUrlRead?.content === dataUrl,
        { dataUrlWrite, dataUrlRead },
        { success: true, textContent: dataUrl },
    );

    const malformedPath = `${root}\\malformed-base64.png`;
    createdPaths.push(malformedPath);
    const malformedWrite = await invoke("file.write", {
        path: malformedPath,
        encoding: "binary",
        content: "base64:iVBORw0KGg!o=",
    });
    const malformedRead = await invoke("file.read", {
        path: malformedPath,
        encoding: "binary",
    });
    assertCase(
        "A-05 invalid Base64 characters are skipped",
        malformedWrite?.success === true && malformedRead?.content === payload,
        { malformedWrite, malformedRead },
        { success: true, decodedContent: payload },
    );

    const emptyPath = `${root}\\empty.bin`;
    createdPaths.push(emptyPath);
    const emptyWrite = await invoke("file.write", {
        path: emptyPath,
        encoding: "binary",
        content: "base64:",
    });
    const emptyRead = await invoke("file.read", {
        path: emptyPath,
        encoding: "binary",
    });
    assertCase(
        "A-06 empty prefixed payload succeeds with zero bytes",
        emptyWrite?.success === true && emptyRead?.size === 0 && emptyRead?.content === "",
        { emptyWrite, emptyRead },
        { success: true, size: 0, content: "" },
    );

    const appendPath = `${root}\\append.txt`;
    createdPaths.push(appendPath);
    await invoke("file.write", { path: appendPath, content: "abc" });
    const appendWrite = await invoke("file.write", {
        path: appendPath,
        content: "de",
        append: true,
    });
    const appendRead = await invoke("file.read", { path: appendPath });
    assertCase(
        "A-07 append bytesWritten is final file size",
        appendWrite?.success === true &&
            appendWrite?.bytesWritten === 5 &&
            appendRead?.content === "abcde",
        { appendWrite, appendRead },
        { bytesWritten: 5, content: "abcde" },
    );

    // atomic goes through a temporary file next to the target; neither a
    // finished nor a refused write may leave that file behind.
    const atomicDir = `${root}\\atomic`;
    const atomicPath = `${atomicDir}\\current.json`;
    const atomicContent = '{"version":"2.1.0"}';
    createdPaths.push(atomicDir, atomicPath);
    await invoke("file.write", { path: atomicPath, content: "old" });
    const atomicWrite = await invoke("file.write", {
        path: atomicPath,
        content: atomicContent,
        atomic: true,
    });
    const atomicRead = await invoke("file.read", { path: atomicPath });
    const atomicWithAppend = await invoke("file.write", {
        path: atomicPath,
        content: "x",
        atomic: true,
        append: true,
    });
    const afterRefusal = await invoke("file.read", { path: atomicPath });
    const atomicList = await invoke("file.list", { path: atomicDir });
    const atomicFiles = Array.isArray(atomicList?.files) ? atomicList.files : [];
    assertCase(
        "A-07b atomic write replaces the file and leaves no temporary file",
        atomicWrite?.success === true &&
            atomicWrite?.bytesWritten === atomicContent.length &&
            atomicRead?.content === atomicContent &&
            atomicFiles.length === 1 &&
            atomicFiles[0] === "current.json",
        { atomicWrite, atomicRead, atomicFiles },
        { bytesWritten: atomicContent.length, files: ["current.json"] },
    );
    assertCase(
        "A-07c atomic with append fails with INVALID_PARAMS and keeps the file",
        atomicWithAppend?.success === false &&
            atomicWithAppend?.code === "INVALID_PARAMS" &&
            afterRefusal?.content === atomicContent,
        { atomicWithAppend, afterRefusal },
        { success: false, code: "INVALID_PARAMS", content: atomicContent },
    );

    const copySource = `${root}\\copy-source.txt`;
    const copyTarget = `${root}\\copy-target.txt`;
    createdPaths.push(copySource, copyTarget);
    await invoke("file.write", { path: copySource, content: "new" });
    await invoke("file.write", { path: copyTarget, content: "old" });
    const copyResult = await invoke("file.copy", {
        source: copySource,
        destination: copyTarget,
        overwrite: false,
    });
    const copyRead = await invoke("file.read", { path: copyTarget });
    assertCase(
        "A-08 copy skip-existing is a zero-operation success",
        copyResult?.success === true && copyRead?.content === "old",
        { copyResult, copyRead },
        { success: true, destinationContent: "old" },
    );

    const listDir = `${root}\\list`;
    const coverPath = `${listDir}\\cover-a.jpg`;
    const notePath = `${listDir}\\note.txt`;
    createdPaths.push(listDir, coverPath, notePath);
    await invoke("file.mkdir", { path: listDir });
    await invoke("file.write", { path: coverPath, content: "cover" });
    await invoke("file.write", { path: notePath, content: "note" });
    const listResult = await invoke("file.list", {
        path: listDir,
        pattern: "cover*.jpg",
    });
    const listedFiles = Array.isArray(listResult?.files) ? listResult.files : [];
    assertCase(
        "A-09 unsupported file.list pattern matches all files",
        listResult?.success === true &&
            listedFiles.includes("cover-a.jpg") &&
            listedFiles.includes("note.txt"),
        listResult,
        { filesContain: ["cover-a.jpg", "note.txt"] },
    );

    // A session lookup miss is reported as success with an empty result, not as
    // an error, so a page cannot distinguish "expired" from "never existed".
    //
    // Whether that branch is reachable depends on the caller window having a
    // drag-drop registration, which this script cannot arrange, so the
    // precondition is read from a second endpoint behind the same gate instead
    // of assumed. The two must agree.
    const dndCaps = await invoke("dnd.getCapabilities", {});
    const registered = dndCaps?.success === true;
    const unknownSession = await invoke("dnd.getPathsAsync", {
        sessionId: `no-such-session-${snapshotId}`,
    });
    assertCase(
        "A-10 dnd.getPathsAsync reports an unknown session as empty success",
        registered
            ? unknownSession?.success === true &&
                  unknownSession?.sessionId === "" &&
                  Array.isArray(unknownSession?.paths) &&
                  unknownSession.paths.length === 0
            : unknownSession?.success === false && unknownSession?.code === "NOT_FOUND",
        { registered, unknownSession },
        registered
            ? { success: true, sessionId: "", paths: [] }
            : { success: false, code: "NOT_FOUND" },
    );

    // The host delivers a handler-returned error envelope through SendResponse,
    // so the promise resolves with the envelope instead of rejecting.
    const startDrag = await invokeRaw("dnd.startDrag", {});
    assertCase(
        "A-10b dnd.startDrag resolves a NOT_SUPPORTED envelope, not a fake success",
        startDrag?.kind === "result" &&
            startDrag.value?.success === false &&
            startDrag.value?.code === "NOT_SUPPORTED" &&
            /not implemented|IDropSource/i.test(startDrag.value?.error || ""),
        startDrag,
        { kind: "result", value: { success: false, code: "NOT_SUPPORTED" } },
    );

    // webview.getSource answers from the record the host made when it
    // submitted the navigation, not from where the page is now; only
    // activeTemplateName is read at call time. A folder source carries the
    // mapped directory, the others do not.
    const pageSource = await invoke("webview.getSource", {});
    const folderSources = ["panelTemplate", "activeTemplate", "componentDirectory", "defaultTemplate"];
    const isFolderSource = folderSources.includes(pageSource?.source);
    assertCase(
        "A-12 webview.getSource reports the host's record of the calling page",
        pageSource?.success === true &&
            ["devServer", "url", "builtInPage", ...folderSources].includes(pageSource.source) &&
            typeof pageSource.activeTemplateName === "string" &&
            typeof pageSource.templatesDirectory === "string" &&
            (isFolderSource
                ? typeof pageSource.directory === "string" && pageSource.url === undefined
                : pageSource.directory === undefined),
        pageSource,
        { success: true, source: "a declared source", directory: isFolderSource ? "string" : "absent" },
    );

    // dsp.getChain and output.getDevices used to answer bare; since their
    // declarations under src/api/schema they answer inside the success envelope.
    // Only output.getDevices declares a count next to its list, so the count is
    // checked only there.
    for (const [method, listKey, counted] of [
        ["dsp.getChain", "dsps", false],
        ["output.getDevices", "devices", true],
    ]) {
        const outcome = await invokeRaw(method, {});
        const value = outcome?.kind === "result" ? outcome.value : undefined;
        assertCase(
            `A-11 ${method} answers inside the success envelope`,
            outcome?.kind === "result" &&
                value?.success === true &&
                Array.isArray(value[listKey]) &&
                (!counted || value.count === value[listKey].length),
            outcome,
            {
                kind: "result",
                value: counted
                    ? { success: true, [listKey]: "an array", count: "its length" }
                    : { success: true, [listKey]: "an array" },
            },
        );
    }
}

let client;
let bridge;
let blocked = false;
let fatalError;
let targets;

try {
    const connection = await connectBridgePage();
    client = connection.client;
    targets = connection.candidates;
    console.log(`Target: ${connection.page.title} ${connection.page.url}`);
    bridge = createBridge(client.Runtime, { invokeTimeoutMs });
    await runSnapshots(bridge);
} catch (error) {
    fatalError = error;
    blocked = Boolean(error?.blocked);
    console.error(`${blocked ? "BLOCKED" : "ERROR"}: ${error?.message || error}`);
} finally {
    if (bridge && bridgeResponsive) {
        for (const path of [...createdPaths].reverse()) {
            await removePath(bridge, path);
        }
        await removePath(bridge, root);
    }
    await closeClient(client);
}

process.exit(
    report({
        recorder,
        blocked,
        fatalError,
        extra: { snapshotId, targetPort: resolvePort(), invokeTimeoutMs, targets },
    }),
);
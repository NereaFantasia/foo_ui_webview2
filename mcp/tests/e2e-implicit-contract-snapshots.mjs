import CDP from "chrome-remote-interface";

const port = Number.parseInt(process.env.FB2K_CDP_PORT || "9222", 10);
const invokeTimeoutMs = Number.parseInt(
    process.env.FB2K_SNAPSHOT_TIMEOUT_MS || "5000",
    10,
);

const snapshotId = new Date().toISOString().replace(/[-:.TZ]/g, "");
const root = `%TEMP%\\foo_ui_webview2_implicit_contract_${snapshotId}`;
const createdPaths = [];
const cases = [];
let bridgeResponsive = false;

function addCase(name, passed, observed, expected) {
    cases.push({ name, passed, observed, expected });
    const marker = passed ? "PASS" : "FAIL";
    console.log(`[${marker}] ${name}`);
    if (!passed) {
        console.log(`  expected: ${JSON.stringify(expected)}`);
        console.log(`  observed: ${JSON.stringify(observed)}`);
    }
}

function assertCase(name, condition, observed, expected) {
    addCase(name, Boolean(condition), observed, expected);
}

async function connectToBridgePage() {
    const targets = await CDP.List({ port });
    const pages = targets.filter(
        (target) => target.type === "page" && !target.url.startsWith("devtools://"),
    );

    for (const page of pages) {
        const client = await CDP({ port, target: page });
        const probe = await client.Runtime.evaluate({
            expression: "typeof window.fb2k === 'object' && typeof window.fb2k.invoke === 'function'",
            returnByValue: true,
        });
        if (probe.result.value === true) {
            return { client, page };
        }
        await client.close();
    }

    throw new Error("No WebView2 page exposes window.fb2k.invoke");
}

function invokeExpression(method, params, timeoutMs = invokeTimeoutMs) {
    const methodJson = JSON.stringify(method);
    const paramsJson = params === undefined ? "undefined" : JSON.stringify(params);
    return `Promise.race([
        window.fb2k.invoke(${methodJson}, ${paramsJson}).then(
            value => ({ kind: 'result', value }),
            error => ({ kind: 'error', error: String(error && error.message ? error.message : error) })
        ),
        new Promise(resolve => setTimeout(
            () => resolve({ kind: 'timeout', timeoutMs: ${timeoutMs} }),
            ${timeoutMs}
        ))
    ])`;
}

async function evaluateValue(
    Runtime,
    expression,
    awaitPromise = true,
    timeoutMs = invokeTimeoutMs + 1000,
) {
    let timerId;
    const evaluatePromise = Runtime.evaluate({
        expression,
        awaitPromise,
        returnByValue: true,
    });
    const timeoutPromise = new Promise((_, reject) => {
        timerId = setTimeout(() => {
            const error = new Error(`CDP Runtime.evaluate timed out after ${timeoutMs} ms`);
            error.code = "CDP_EVALUATE_TIMEOUT";
            reject(error);
        }, timeoutMs);
    });

    let response;
    try {
        response = await Promise.race([evaluatePromise, timeoutPromise]);
    } finally {
        clearTimeout(timerId);
    }
    if (response.exceptionDetails) {
        throw new Error(
            response.exceptionDetails.exception?.description ||
                response.exceptionDetails.text ||
                "Runtime.evaluate failed",
        );
    }
    return response.result.value;
}

async function invokeRaw(Runtime, method, params, timeoutMs = invokeTimeoutMs) {
    try {
        return await evaluateValue(
            Runtime,
            invokeExpression(method, params, timeoutMs),
            true,
            timeoutMs + 1000,
        );
    } catch (error) {
        if (error?.code === "CDP_EVALUATE_TIMEOUT") {
            return { kind: "cdp-timeout", timeoutMs: timeoutMs + 1000 };
        }
        throw error;
    }
}

async function invoke(Runtime, method, params) {
    const outcome = await invokeRaw(Runtime, method, params);
    if (outcome?.kind === "timeout") {
        throw new Error(`${method} timed out after ${outcome.timeoutMs} ms`);
    }
    if (outcome?.kind === "error") {
        throw new Error(`${method} rejected: ${outcome.error}`);
    }
    if (!outcome || outcome.kind !== "result") {
        throw new Error(`${method} returned an invalid probe envelope`);
    }
    return outcome.value;
}

async function removePath(Runtime, path) {
    await invokeRaw(Runtime, "file.delete", { path, moveToTrash: false }, 2000).catch(
        () => undefined,
    );
}

async function probeBridgeResponsiveness(Runtime) {
    const probeKey = `__implicitContractProbe_${snapshotId}`;
    const launchExpression = `(() => {
        const key = ${JSON.stringify(probeKey)};
        window[key] = { status: 'pending', startedAt: Date.now() };
        window.fb2k.invoke('file.exists', { path: '%TEMP%' }).then(
            value => { window[key] = { status: 'resolved', value, settledAt: Date.now() }; },
            error => { window[key] = { status: 'rejected', error: String(error && error.message ? error.message : error), settledAt: Date.now() }; }
        );
        return {
            launched: true,
            callId: window.fb2k._callId,
            callbacks: window.fb2k._callbacks?.size ?? null
        };
    })()`;

    let launch;
    try {
        launch = await evaluateValue(
            Runtime,
            launchExpression,
            false,
            invokeTimeoutMs + 1000,
        );
    } catch (error) {
        return {
            responsive: false,
            stage: "postMessage",
            error: String(error?.message || error),
        };
    }

    const deadline = Date.now() + invokeTimeoutMs;
    let state;
    do {
        await new Promise((resolve) => setTimeout(resolve, 100));
        state = await evaluateValue(
            Runtime,
            `(() => ({
                probe: window[${JSON.stringify(probeKey)}] || null,
                callId: window.fb2k?._callId ?? null,
                callbacks: window.fb2k?._callbacks?.size ?? null
            }))()`,
            false,
            1000,
        );
        if (state?.probe?.status !== "pending") break;
    } while (Date.now() < deadline);

    await evaluateValue(
        Runtime,
        `delete window[${JSON.stringify(probeKey)}]; true`,
        false,
        1000,
    ).catch(() => undefined);

    if (state?.probe?.status === "resolved") {
        return { responsive: true, launch, state };
    }
    return {
        responsive: false,
        stage: "response",
        launch,
        state,
    };
}

async function runSnapshots(Runtime) {
    const probe = await probeBridgeResponsiveness(Runtime);
    if (!probe.responsive) {
        const error = new Error(
            `file.exists preflight blocked at ${probe.stage}: ${JSON.stringify(probe)}`,
        );
        error.blocked = true;
        throw error;
    }
    bridgeResponsive = true;

    await invoke(Runtime, "file.mkdir", { path: root });

    const payload = "iVBORw0KGgo=";
    const dataUrl = `data:image/png;base64,${payload}`;

    const exactPath = `${root}\\exact.png`;
    createdPaths.push(exactPath);
    const exactWrite = await invoke(Runtime, "file.write", {
        path: exactPath,
        encoding: "binary",
        content: `base64:${payload}`,
    });
    const exactRead = await invoke(Runtime, "file.read", {
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
    const rawWrite = await invoke(Runtime, "file.write", {
        path: rawPath,
        encoding: "binary",
        content: payload,
    });
    const rawRead = await invoke(Runtime, "file.read", { path: rawPath });
    assertCase(
        "A-02 binary plus raw Base64 writes text",
        rawWrite?.success === true && rawRead?.content === payload,
        { rawWrite, rawRead },
        { success: true, textContent: payload },
    );

    const roundTripPath = `${root}\\naive-roundtrip.png`;
    createdPaths.push(roundTripPath);
    const roundTripWrite = await invoke(Runtime, "file.write", {
        path: roundTripPath,
        encoding: exactRead.encoding,
        content: exactRead.content,
    });
    const roundTripRead = await invoke(Runtime, "file.read", { path: roundTripPath });
    assertCase(
        "A-03 binary read response cannot be written back unchanged",
        roundTripWrite?.success === true && roundTripRead?.content === payload,
        { roundTripWrite, roundTripRead },
        { success: true, textContent: payload },
    );

    const dataUrlPath = `${root}\\data-url.png`;
    createdPaths.push(dataUrlPath);
    const dataUrlWrite = await invoke(Runtime, "file.write", {
        path: dataUrlPath,
        encoding: "binary",
        content: dataUrl,
    });
    const dataUrlRead = await invoke(Runtime, "file.read", { path: dataUrlPath });
    assertCase(
        "A-04 binary plus Data URL writes the URL text",
        dataUrlWrite?.success === true && dataUrlRead?.content === dataUrl,
        { dataUrlWrite, dataUrlRead },
        { success: true, textContent: dataUrl },
    );

    const malformedPath = `${root}\\malformed-base64.png`;
    createdPaths.push(malformedPath);
    const malformedWrite = await invoke(Runtime, "file.write", {
        path: malformedPath,
        encoding: "binary",
        content: "base64:iVBORw0KGg!o=",
    });
    const malformedRead = await invoke(Runtime, "file.read", {
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
    const emptyWrite = await invoke(Runtime, "file.write", {
        path: emptyPath,
        encoding: "binary",
        content: "base64:",
    });
    const emptyRead = await invoke(Runtime, "file.read", {
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
    await invoke(Runtime, "file.write", { path: appendPath, content: "abc" });
    const appendWrite = await invoke(Runtime, "file.write", {
        path: appendPath,
        content: "de",
        append: true,
    });
    const appendRead = await invoke(Runtime, "file.read", { path: appendPath });
    assertCase(
        "A-07 append bytesWritten is final file size",
        appendWrite?.success === true &&
            appendWrite?.bytesWritten === 5 &&
            appendRead?.content === "abcde",
        { appendWrite, appendRead },
        { bytesWritten: 5, content: "abcde" },
    );

    const copySource = `${root}\\copy-source.txt`;
    const copyTarget = `${root}\\copy-target.txt`;
    createdPaths.push(copySource, copyTarget);
    await invoke(Runtime, "file.write", { path: copySource, content: "new" });
    await invoke(Runtime, "file.write", { path: copyTarget, content: "old" });
    const copyResult = await invoke(Runtime, "file.copy", {
        source: copySource,
        destination: copyTarget,
        overwrite: false,
    });
    const copyRead = await invoke(Runtime, "file.read", { path: copyTarget });
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
    await invoke(Runtime, "file.mkdir", { path: listDir });
    await invoke(Runtime, "file.write", { path: coverPath, content: "cover" });
    await invoke(Runtime, "file.write", { path: notePath, content: "note" });
    const listResult = await invoke(Runtime, "file.list", {
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
    const unknownSession = await invoke(Runtime, "dnd.getPathsAsync", {
        sessionId: `no-such-session-${snapshotId}`,
    });
    assertCase(
        "A-10 dnd.getPathsAsync reports an unknown session as empty success",
        unknownSession?.success === true &&
            unknownSession?.sessionId === "" &&
            Array.isArray(unknownSession?.paths) &&
            unknownSession.paths.length === 0,
        unknownSession,
        { success: true, sessionId: "", paths: [] },
    );

    // The host delivers a handler-returned error envelope through SendResponse,
    // so the promise resolves with the envelope instead of rejecting.
    const startDrag = await invokeRaw(Runtime, "dnd.startDrag", {});
    assertCase(
        "A-10b dnd.startDrag resolves a NOT_SUPPORTED envelope, not a fake success",
        startDrag?.kind === "result" &&
            startDrag.value?.success === false &&
            startDrag.value?.code === "NOT_SUPPORTED" &&
            /not implemented|IDropSource/i.test(startDrag.value?.error || ""),
        startDrag,
        { kind: "result", value: { success: false, code: "NOT_SUPPORTED" } },
    );

    for (const method of ["dsp.getChain", "output.getDevices"]) {
        const outcome = await invokeRaw(Runtime, method, {});
        assertCase(
            `A-11 ${method} is not registered`,
            outcome?.kind === "error" && /method not found/i.test(outcome.error || ""),
            outcome,
            { kind: "error", errorContains: "Method not found" },
        );
    }
}

let client;
let blocked = false;
let fatalError;

try {
    const connection = await connectToBridgePage();
    client = connection.client;
    console.log(`Target: ${connection.page.title} ${connection.page.url}`);
    await runSnapshots(client.Runtime);
} catch (error) {
    fatalError = error;
    blocked = Boolean(error?.blocked);
    console.error(`${blocked ? "BLOCKED" : "ERROR"}: ${error?.message || error}`);
} finally {
    if (client) {
        if (bridgeResponsive) {
            for (const path of [...createdPaths].reverse()) {
                await removePath(client.Runtime, path);
            }
            await removePath(client.Runtime, root);
        }
        await Promise.race([
            client.close().catch(() => undefined),
            new Promise((resolve) => setTimeout(resolve, 1000)),
        ]);
    }
}

const failed = cases.filter((item) => !item.passed);
console.log(JSON.stringify({
    snapshotId,
    targetPort: port,
    invokeTimeoutMs,
    status: blocked ? "blocked" : fatalError ? "error" : failed.length ? "failed" : "passed",
    passed: cases.length - failed.length,
    failed: failed.length,
    cases,
    fatalError: fatalError ? String(fatalError.message || fatalError) : undefined,
}, null, 2));

const exitCode = blocked ? 2 : fatalError || failed.length > 0 ? 1 : 0;
process.exit(exitCode);
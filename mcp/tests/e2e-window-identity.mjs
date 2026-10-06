/**
 * Check caller identity and message delivery on the main page and two same-origin popups.
 * Only popups created by this suite are closed. The host configuration is unchanged.
 * Usage: node mcp/tests/e2e-window-identity.mjs
 */
import CDP from "chrome-remote-interface";
import {
    blockedError,
    closeClient,
    connectBridgePage,
    createBridge,
    createEventCollector,
    createRecorder,
    envInt,
    report,
    resolvePort,
} from "./lib/e2e-harness.mjs";

const invokeTimeoutMs = envInt("FB2K_E2E_TIMEOUT_MS", 8000);
const eventTimeoutMs = envInt("FB2K_EVENT_TIMEOUT_MS", 3000);
const popupReadyTimeoutMs = envInt("FB2K_POPUP_READY_TIMEOUT_MS", 20000);
const runId = `${Date.now()}-${process.pid}`;
const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
const recorder = createRecorder();
const sessions = [];
const createdPopups = [];
let mainBridge;
let blocked = false;
let fatalError;

async function attachPopup(port, windowId) {
    const deadline = Date.now() + popupReadyTimeoutMs;
    let session;
    while (Date.now() < deadline) {
        if (!session) {
            const pages = await CDP.List({ port });
            const page = pages.find((target) => {
                if (target.type !== "page") return false;
                try {
                    return new URL(target.url).searchParams.get("windowId") === windowId;
                } catch {
                    return false;
                }
            });
            if (page) {
                const client = await CDP({ port, target: page });
                session = { windowId, client, bridge: createBridge(client.Runtime, { invokeTimeoutMs }) };
                sessions.push(session);
            }
        }
        if (session) {
            // Readiness must not depend on a correct identity: that is the assertion below.
            try {
                const probe = await session.bridge.invokeRaw("window.getCurrentWindowId", {});
                if (probe?.kind === "result") return session;
            } catch {
                // A popup can replace its document after CDP first lists the page.
            }
        }
        await sleep(100);
    }
    throw new Error(`Popup bridge did not become ready: ${windowId}`);
}

async function checkIdentity(session) {
    const current = await session.bridge.invoke("window.getCurrentWindowId", {});
    const mode = await session.bridge.invoke("window.getMode", {});
    recorder.assertCase(
        "WI-01 caller reports consistent standalone identity",
        current?.windowId === session.windowId &&
            mode?.windowId === session.windowId &&
            mode?.mode === "standalone" && mode?.panelMode === false,
        { current, mode },
        { current: { windowId: session.windowId }, mode: { windowId: session.windowId, mode: "standalone", panelMode: false } },
    );
}

function matchingMessages(events, message) {
    return events.filter((event) => event.name === "window:message" &&
        event.payload?.message?.runId === runId && event.payload.message.token === message.token);
}

async function checkMessages() {
    for (const session of sessions) {
        session.events = await createEventCollector(session.bridge, ["window:message"], {
            collectorId: `${runId}-${session.windowId}`,
        });
    }
    for (let index = 0; index < sessions.length; index += 1) {
        const sender = sessions[index];
        const recipient = sessions[(index + 1) % sessions.length];
        const message = { runId, token: `direct-${index}` };
        await sender.bridge.invoke("window.sendMessage", { targetWindowId: recipient.windowId, message });
        const events = await recipient.events.waitFor(
            (items) => matchingMessages(items, message).length > 0, { timeoutMs: eventTimeoutMs },
        );
        const received = matchingMessages(events, message);
        recorder.assertCase(
            "WI-02 directed message carries its actual source id",
            received.length === 1 && received[0].payload.sourceWindowId === sender.windowId,
            received.map((event) => event.payload),
            [{ sourceWindowId: sender.windowId, message }],
        );
    }
    for (const sender of sessions) {
        const message = { runId, token: `broadcast-${sender.windowId}` };
        await sender.bridge.invoke("window.broadcast", { message });
        for (const recipient of sessions.filter((session) => session !== sender)) {
            const events = await recipient.events.waitFor(
                (items) => matchingMessages(items, message).length > 0, { timeoutMs: eventTimeoutMs },
            );
            const received = matchingMessages(events, message);
            recorder.assertCase(
                "WI-03 broadcast reaches the other pages with its actual source id",
                received.length === 1 && received[0].payload.sourceWindowId === sender.windowId,
                received.map((event) => event.payload),
                [{ sourceWindowId: sender.windowId, message }],
            );
        }
        const ownEvents = await sender.events.waitFor(
            (items) => matchingMessages(items, message).length > 0, { timeoutMs: 300 },
        );
        const ownMessages = matchingMessages(ownEvents, message);
        recorder.assertCase("WI-04 broadcast excludes its own page", ownMessages.length === 0,
            { windowId: sender.windowId, messages: ownMessages.map((event) => event.payload) },
            { windowId: sender.windowId, messages: [] });
    }
}

try {
    const connection = await connectBridgePage();
    mainBridge = createBridge(connection.client.Runtime, { invokeTimeoutMs });
    const main = { windowId: "main", client: connection.client, bridge: mainBridge };
    sessions.push(main);
    const popupUrl = new URL(connection.page.url);
    if (!['http:', 'https:'].includes(popupUrl.protocol) || popupUrl.searchParams.has("windowId")) {
        throw blockedError("Select the trusted main page before running this suite");
    }
    popupUrl.searchParams.set("identitySuite", runId);
    await checkIdentity(main);
    for (let index = 0; index < 2; index += 1) {
        const created = await mainBridge.invoke("window.createPopup", {
            url: popupUrl.href,
            title: `Window identity ${runId} ${index + 1}`,
            width: 240,
            height: 180,
            profile: "standard",
            behavior: { noActivate: true },
        });
        const distinctId = typeof created?.windowId === "string" && created.windowId !== "main" &&
            created.windowId.length > 0 && !createdPopups.includes(created.windowId);
        recorder.assertCase("WI-05 popup creation reports a distinct id", distinctId,
            { windowId: created?.windowId, previousIds: [...createdPopups] },
            { distinctPopupId: true });
        if (!distinctId) throw new Error("Popup creation returned no distinct popup id");
        createdPopups.push(created.windowId);
        await checkIdentity(await attachPopup(connection.port, created.windowId));
    }
    await checkMessages();
} catch (error) {
    fatalError = error;
    blocked = Boolean(error?.blocked);
    console.error(`${blocked ? "BLOCKED" : "ERROR"}: ${error?.message || error}`);
} finally {
    for (const session of sessions) {
        if (session.events) await session.events.stop();
    }
    for (const windowId of [...createdPopups].reverse()) {
        try {
            await mainBridge.invoke("window.closePopup", { windowId });
            const deadline = Date.now() + 6000;
            let present;
            do {
                const windows = await mainBridge.invoke("window.getAllWindows", {});
                present = windows.items.some((item) => item.windowId === windowId);
                if (present) await sleep(100);
            } while (present && Date.now() < deadline);
            recorder.assertCase("WI-06 suite popup is closed", !present, { windowId, present }, { windowId, present: false });
        } catch (error) {
            recorder.assertCase("WI-06 suite popup is closed", false,
                { windowId, error: String(error?.message || error) }, { windowId, present: false });
        }
    }
    for (const session of sessions) await closeClient(session.client);
}

process.exit(report({ recorder, blocked, fatalError, extra: { runId, targetPort: resolvePort(), createdPopups } }));

/**
 * Covers the live PCM stream: audio.subscribeStream and audio.unsubscribeStream
 * with the ring buffers they share, the audio:stream format-change event and
 * the `stream` part of audio.getPcmDebugState, against docs/audio-pcm/SPEC.md
 * (§4.5, §4.6, §4.10, D10, D11; acceptance A-D1–A-D7, A-D11, A-D12 in §10).
 *
 * The cases run in a popup of their own that is navigated to about:blank, which
 * the host still trusts and injects its bridge into, so no SDK is loaded there:
 * the SDK's shared-buffer receiver releases buffers of subscriptions it did not
 * make about 5 s after they arrive. The page reads the rings itself with the
 * seqlock protocol of D11.
 *
 * Two tiers. The contract tier never touches playback: parameter rejections,
 * registration bookkeeping through the debug state, the per-page limit, same-id
 * replacement, removal counts and cross-page ownership (a second popup, skipped
 * when none appears). The playback tier runs only with
 * FB2K_E2E_AUDIO_PCM_PLAYBACK=1. It mutes foobar2000 (the stream is taken before
 * the volume) and plays its fixtures from a scratch playlist; afterwards it
 * removes the playlist and restores what it found when the tier started: the
 * active playlist, the playing track and position, paused or stopped, and the
 * mute state. When playback is running at that point the tier is skipped unless
 * FB2K_E2E_INTERRUPT_PLAYBACK=1. A-D8 (panel removal) and A-D9 (exit while
 * streaming) change the layout or the process and are live-probe checks
 * recorded in the spec.
 *
 * Fixtures are 16-bit WAVs this suite writes into FB2K_E2E_PCM_FIXTURE_DIR
 * (default E:\FB2K\e2e-fixtures\audio-pcm): a 30 s 48 kHz stereo 1 kHz sine at
 * −20 dBFS and a 40 s 44.1 kHz stereo one for the format change and the cases
 * that follow it.
 *
 * Usage: node mcp/tests/e2e-audio-stream.mjs
 * Environment: FB2K_CDP_PORT, FB2K_E2E_TIMEOUT_MS, FB2K_E2E_PCM_FIXTURE_DIR,
 * FB2K_E2E_AUDIO_PCM_PLAYBACK, FB2K_E2E_INTERRUPT_PLAYBACK.
 */

import { existsSync, mkdirSync, statSync, writeFileSync } from "node:fs";
import { join } from "node:path";

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
    requireResponsiveBridge,
    resolvePort,
} from "./lib/e2e-harness.mjs";

const invokeTimeoutMs = envInt("FB2K_E2E_TIMEOUT_MS", 10000);
const FIXTURE_DIR = process.env.FB2K_E2E_PCM_FIXTURE_DIR || "E:\\FB2K\\e2e-fixtures\\audio-pcm";
const PLAYBACK_TIER = process.env.FB2K_E2E_AUDIO_PCM_PLAYBACK === "1";
const INTERRUPT_PLAYBACK = process.env.FB2K_E2E_INTERRUPT_PLAYBACK === "1";
const EVENTS = ["audio:stream"];
/** 100 µs at a 2 GHz TSC: the A-D7 budget per chunk. */
const CYCLES_PER_CHUNK_BUDGET = 200000;

const runId = Date.now().toString(36);
const storeKey = `__e2eStream_${runId}`;
const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
const within = (value, target, tolerance) => typeof value === "number" && Math.abs(value - target) <= tolerance;
const withinPercent = (value, target, percent) => within(value, target, (Math.abs(target) * percent) / 100);
const RMS_MINUS_20DBFS = Math.SQRT1_2 / 10;

// ---------------------------------------------------------------------------
// Fixtures

function writeWav(path, { sampleRate, seconds, channels, amplitude }) {
    const frames = sampleRate * seconds;
    const blockAlign = channels * 2;
    const dataBytes = frames * blockAlign;
    const buf = Buffer.alloc(44 + dataBytes);
    buf.write("RIFF", 0);
    buf.writeUInt32LE(36 + dataBytes, 4);
    buf.write("WAVE", 8);
    buf.write("fmt ", 12);
    buf.writeUInt32LE(16, 16);
    buf.writeUInt16LE(1, 20);
    buf.writeUInt16LE(channels, 22);
    buf.writeUInt32LE(sampleRate, 24);
    buf.writeUInt32LE(sampleRate * blockAlign, 28);
    buf.writeUInt16LE(blockAlign, 32);
    buf.writeUInt16LE(16, 34);
    buf.write("data", 36);
    buf.writeUInt32LE(dataBytes, 40);
    for (let i = 0; i < frames; i += 1) {
        const v = Math.round(amplitude * Math.sin((2 * Math.PI * 1000 * i) / sampleRate));
        for (let c = 0; c < channels; c += 1) buf.writeInt16LE(v, 44 + i * blockAlign + c * 2);
    }
    writeFileSync(path, buf);
}

function ensureFixtures() {
    try {
        mkdirSync(FIXTURE_DIR, { recursive: true });
    } catch (error) {
        throw blockedError(`fixture directory is not writable: ${FIXTURE_DIR}`, { error: String(error?.message ?? error) });
    }
    const amplitude = 32767 * Math.pow(10, -20 / 20);
    const specs = {
        s48: { file: "pcm_stream_sine_1k_-20dBFS_48k_stereo_30s.wav", sampleRate: 48000, seconds: 30, channels: 2, amplitude },
        s44: { file: "pcm_stream_sine_1k_-20dBFS_44k1_stereo_40s.wav", sampleRate: 44100, seconds: 40, channels: 2, amplitude },
    };
    const out = {};
    for (const [key, spec] of Object.entries(specs)) {
        const path = join(FIXTURE_DIR, spec.file);
        const expectedBytes = 44 + spec.sampleRate * spec.seconds * spec.channels * 2;
        const reused = existsSync(path) && statSync(path).size === expectedBytes;
        if (!reused) {
            try {
                writeWav(path, spec);
            } catch (error) {
                throw blockedError(`fixture is not writable: ${path}`, { error: String(error?.message ?? error) });
            }
        }
        out[key] = { path, sampleRate: spec.sampleRate, reused };
    }
    return out;
}

// ---------------------------------------------------------------------------
// Page-side store and ring reader (D11)

async function installStore(bridge) {
    const key = JSON.stringify(storeKey);
    await bridge.evaluateValue(
        `(() => {
            if (window[${key}]) return true;
            const store = window[${key}] = { buffers: [], readers: new Map() };
            window.chrome.webview.addEventListener('sharedbufferreceived', (e) => {
                const d = e.additionalData;
                if (!d || d.purpose !== 'audio.subscribeStream') return;
                store.buffers.push({ id: d.subscriptionId, epoch: d.epoch, data: d, buffer: e.getBuffer(), at: Date.now() });
            });
            store.header = (buffer) => {
                const u32 = new Uint32Array(buffer, 0, 16);
                const dv = new DataView(buffer, 0, 64);
                return { magic: u32[0], version: u32[1], headerBytes: u32[2], mode: u32[3], sampleRate: u32[4], channels: u32[5],
                    capacityFrames: u32[6], seq: Atomics.load(u32, 7), writeFrames: Atomics.load(u32, 8), flags: Atomics.load(u32, 9),
                    hostTimeMs: dv.getFloat64(40, true), epoch: u32[14], writeSlot: u32[15] };
            };
            store.makeReader = (buffer) => {
                const u32 = new Uint32Array(buffer, 0, 16);
                const channels = u32[5], capacity = u32[6];
                const samples = new Float32Array(buffer, 64, capacity * channels);
                const r = { channels, capacity, readFrames: 0, retries: 0, torn: 0, reads: 0, frames: 0, dropped: 0, maxFrames: 0, sumsq: new Array(channels).fill(0), samples: 0, tail: null };
                r.read = (keepTail) => {
                    let limit = Infinity;
                    for (let attempt = 0; attempt < 3; attempt += 1) {
                        const before = Atomics.load(u32, 7);
                        if (before & 1) { r.retries += 1; continue; }
                        const wf = u32[8], ws = u32[15];
                        const available = (wf - r.readFrames) >>> 0;
                        if (available === 0) return null;
                        const inRing = Math.min(available, capacity);
                        const frames = Math.max(1, Math.min(inRing, limit));
                        const planes = [];
                        for (let c = 0; c < channels; c += 1) planes.push(new Float32Array(frames));
                        let slot = (ws + capacity - frames) % capacity;
                        for (let i = 0; i < frames; i += 1) {
                            const base = slot * channels;
                            for (let c = 0; c < channels; c += 1) planes[c][i] = samples[base + c];
                            slot = slot + 1 === capacity ? 0 : slot + 1;
                        }
                        if (Atomics.load(u32, 7) !== before) { r.retries += 1; limit = Math.max(1, Math.floor(frames / 2)); continue; }
                        r.readFrames = wf;
                        r.reads += 1; r.frames += frames; r.dropped += available - frames; r.maxFrames = Math.max(r.maxFrames, frames);
                        for (let c = 0; c < channels; c += 1) { const p = planes[c]; for (let i = 0; i < p.length; i += 1) r.sumsq[c] += p[i] * p[i]; }
                        r.samples += frames;
                        if (keepTail) r.tail = planes;
                        return { frames, dropped: available - frames };
                    }
                    r.torn += 1;
                    return null;
                };
                r.stats = () => ({ reads: r.reads, frames: r.frames, dropped: r.dropped, maxFrames: r.maxFrames, retries: r.retries, torn: r.torn,
                    rms: r.sumsq.map((s) => Math.sqrt(s / Math.max(1, r.samples))), readFrames: r.readFrames, writeFrames: Atomics.load(u32, 8), ended: (Atomics.load(u32, 9) & 1) === 1 });
                r.resetStats = () => { r.reads = 0; r.frames = 0; r.dropped = 0; r.maxFrames = 0; r.retries = 0; r.torn = 0; r.sumsq.fill(0); r.samples = 0; };
                return r;
            };
            // Polls one reader at about 60 Hz for ms milliseconds and returns its stats.
            store.pump = (readerKey, ms) => new Promise((resolve) => {
                const r = store.readers.get(readerKey);
                const t0 = performance.now();
                const tick = () => { r.read(false); if (performance.now() - t0 < ms) setTimeout(tick, 16); else resolve(r.stats()); };
                tick();
            });
            return true;
        })()`,
        false,
        3000,
    );
}

const page = (bridge, body, timeoutMs = 5000) =>
    bridge.evaluateValue(`(() => { const store = window[${JSON.stringify(storeKey)}]; ${body} })()`, false, timeoutMs);
const pageAsync = (bridge, body, timeoutMs = 30000) =>
    bridge.evaluateValue(`(async () => { const store = window[${JSON.stringify(storeKey)}]; ${body} })()`, true, timeoutMs);

/** Buffers the page holds for one subscription, newest last, with their headers. */
const buffersOf = (bridge, id) =>
    page(bridge, `return store.buffers.filter((b) => b.id === ${JSON.stringify(id)}).map((b) => ({ epoch: b.epoch, data: b.data, header: store.header(b.buffer), byteLength: b.buffer.byteLength, at: b.at }));`);

async function waitForBuffer(bridge, id, epoch, timeoutMs = 5000) {
    const deadline = Date.now() + timeoutMs;
    for (;;) {
        const list = await buffersOf(bridge, id);
        const hit = list.find((b) => b.epoch === epoch);
        if (hit || Date.now() >= deadline) return hit ?? null;
        await sleep(50);
    }
}

/** Attaches a page-side reader to the newest buffer of one subscription. */
const attachReader = (bridge, id, epoch, readerKey) =>
    page(bridge, `const b = store.buffers.find((x) => x.id === ${JSON.stringify(id)} && x.epoch === ${epoch}); if (!b) return false; store.readers.set(${JSON.stringify(readerKey)}, store.makeReader(b.buffer)); return true;`);
const readOnce = (bridge, readerKey, keepTail = false) =>
    page(bridge, `const r = store.readers.get(${JSON.stringify(readerKey)}); const out = r.read(${keepTail}); return { out, stats: r.stats() };`);
const readerStats = (bridge, readerKey) => page(bridge, `return store.readers.get(${JSON.stringify(readerKey)}).stats();`);
const resetReader = (bridge, readerKey) => page(bridge, `store.readers.get(${JSON.stringify(readerKey)}).resetStats(); return true;`);
const pump = (bridge, readerKey, ms) => pageAsync(bridge, `return store.pump(${JSON.stringify(readerKey)}, ${ms});`, ms + 10000);

async function releaseAll(bridge) {
    await bridge
        .evaluateValue(
            `(() => { const store = window[${JSON.stringify(storeKey)}]; if (!store) return 0; let n = 0; for (const b of store.buffers) { try { window.chrome.webview.releaseBuffer(b.buffer); n += 1; } catch {} } store.buffers = []; store.readers.clear(); return n; })()`,
            false,
            5000,
        )
        .catch(() => undefined);
}

// ---------------------------------------------------------------------------
// Bridge helpers

const debugState = (bridge) => bridge.invoke("audio.getPcmDebugState", {});
const streamState = async (bridge) => (await debugState(bridge)).stream;
const raw = (bridge, method, params) => bridge.invokeRaw(method, params).then((r) => r.value ?? r);

async function waitForWriteFrames(bridge, id, predicate, timeoutMs = 5000) {
    const deadline = Date.now() + timeoutMs;
    let entry;
    for (;;) {
        entry = (await streamState(bridge)).subscriptions.find((s) => s.subscriptionId === id);
        if ((entry && predicate(entry)) || Date.now() >= deadline) return entry ?? null;
        await sleep(100);
    }
}

// ---------------------------------------------------------------------------
// Contract tier

async function runContractCases(bridge, recorder) {
    const idle = await streamState(bridge);
    recorder.assertCase(
        "PS-01 (§4.10) getPcmDebugState.stream reports no callback, no interval and no subscriptions while nobody subscribes",
        idle?.callbackRegistered === false &&
            idle.interval === undefined &&
            typeof idle.chunkCount === "number" &&
            typeof idle.chunkCycles === "number" &&
            Array.isArray(idle.subscriptions) &&
            idle.subscriptions.length === 0,
        idle,
        { callbackRegistered: false, subscriptions: [] },
    );

    const bad = {
        intervalHigh: await raw(bridge, "audio.subscribeStream", { interval: 0.5 }),
        intervalLow: await raw(bridge, "audio.subscribeStream", { interval: 0.001 }),
        bufferHigh: await raw(bridge, "audio.subscribeStream", { bufferSeconds: 11 }),
        bufferLow: await raw(bridge, "audio.subscribeStream", { bufferSeconds: 0.05 }),
        eventKey: await raw(bridge, "audio.subscribeStream", { event: "audio:stream" }),
        emptyId: await raw(bridge, "audio.subscribeStream", { subscriptionId: "" }),
        wrongType: await raw(bridge, "audio.subscribeStream", { bufferSeconds: "1" }),
    };
    const afterBad = await streamState(bridge);
    recorder.assertCase(
        "PS-02 (§4.5) out-of-range, mistyped, empty and undeclared parameters answer INVALID_PARAMS and register nothing",
        Object.values(bad).every((r) => r?.success === false && r.code === "INVALID_PARAMS") &&
            afterBad.callbackRegistered === false &&
            afterBad.subscriptions.length === 0,
        { bad: Object.fromEntries(Object.entries(bad).map(([k, v]) => [k, v?.code])), afterBad },
        { every: "INVALID_PARAMS", callbackRegistered: false },
    );

    const a = await bridge.invoke("audio.subscribeStream", { subscriptionId: `ps-a-${runId}`, interval: 0.1 });
    const stateA = await streamState(bridge);
    const b = await bridge.invoke("audio.subscribeStream", { subscriptionId: `ps-b-${runId}`, interval: 0.05, bufferSeconds: 2 });
    const stateAB = await streamState(bridge);
    const removedB = await bridge.invoke("audio.unsubscribeStream", { subscriptionId: `ps-b-${runId}` });
    const stateAfterB = await streamState(bridge);
    const entryA = stateA.subscriptions.find((s) => s.subscriptionId === a?.subscriptionId);
    recorder.assertCase(
        "PS-03 (§4.5, §4.10, D10) subscribing registers the callback with the shortest requested interval, the answer echoes the request, and an epoch of 0 marks a subscription that has not seen audio",
        a?.success === true &&
            a.subscriptionId === `ps-a-${runId}` &&
            a.interval === 0.1 &&
            a.bufferSeconds === 1 &&
            stateA.callbackRegistered === true &&
            stateA.interval === 0.1 &&
            entryA?.epoch === 0 &&
            entryA.capacityFrames === 0 &&
            entryA.writeFrames === 0 &&
            typeof entryA.windowId === "string" &&
            b?.success === true &&
            b.bufferSeconds === 2 &&
            stateAB.interval === 0.05 &&
            stateAB.subscriptions.length === 2 &&
            removedB?.success === true &&
            removedB.removed === 1 &&
            stateAfterB.interval === 0.1 &&
            stateAfterB.subscriptions.length === 1,
        { a, stateA, b, stateAB, removedB, stateAfterB },
        { a: { interval: 0.1, bufferSeconds: 1 }, intervals: [0.1, 0.05, 0.1], epoch: 0 },
    );

    const replaced = await bridge.invoke("audio.subscribeStream", { subscriptionId: `ps-a-${runId}`, bufferSeconds: 0.5 });
    const stateReplaced = await streamState(bridge);
    const ids = [];
    for (let i = 0; i < 8; i += 1) ids.push(await raw(bridge, "audio.subscribeStream", { subscriptionId: `ps-lim${i}-${runId}`, bufferSeconds: 0.1 }));
    const stateLimit = await streamState(bridge);
    const removedOne = await bridge.invoke("audio.unsubscribeStream", { subscriptionId: `ps-lim0-${runId}` });
    const removedUnknown = await bridge.invoke("audio.unsubscribeStream", { subscriptionId: `ps-nope-${runId}` });
    const removedAll = await bridge.invoke("audio.unsubscribeStream", {});
    const stateEmpty = await streamState(bridge);
    recorder.assertCase(
        "PS-04 (A-D12, §4.5, §4.6) the same id replaces instead of adding, a ninth subscription answers OPERATION_FAILED, and unsubscribing removes one by id, none for an unknown id, and all without an id, unregistering the callback",
        replaced?.success === true &&
            replaced.bufferSeconds === 0.5 &&
            stateReplaced.subscriptions.length === 1 &&
            stateReplaced.interval === undefined &&
            ids.slice(0, 7).every((r) => r?.success === true) &&
            ids[7]?.success === false &&
            ids[7].code === "OPERATION_FAILED" &&
            ids[7].details === "too many stream subscriptions" &&
            stateLimit.subscriptions.length === 8 &&
            removedOne?.removed === 1 &&
            removedUnknown?.removed === 0 &&
            removedAll?.removed === 7 &&
            stateEmpty.callbackRegistered === false &&
            stateEmpty.subscriptions.length === 0,
        { replaced, stateReplaced: stateReplaced.subscriptions.length, ninth: ids[7], stateLimit: stateLimit.subscriptions.length, removedOne, removedUnknown, removedAll, stateEmpty },
        { replaced: 1, ninth: "OPERATION_FAILED", limit: 8, removed: [1, 0, 7], callbackRegistered: false },
    );
}

/** Waits until the popup's page answers with its own window id (and, when given, sits at `href`). */
async function waitForPopupPage(popupBridge, popupId, deadline, href) {
    while (Date.now() < deadline) {
        try {
            if (!href || (await popupBridge.evaluateValue("location.href", false, 2000)) === href) {
                const id = await popupBridge.invokeRaw("window.getCurrentWindowId", {});
                if (id?.value?.windowId === popupId) return true;
            }
        } catch {
            // The page may still be navigating; retry until the deadline.
        }
        await sleep(100);
    }
    return false;
}

/**
 * Opens a small popup and attaches to its page; null when the page does not show up.
 * With `blank`, the popup is then navigated to about:blank, where the host's bridge
 * is still injected but the theme and its SDK are not loaded.
 */
async function openPopup(bridge, { blank = false } = {}) {
    const created = await bridge.invoke("window.createPopup", {
        width: 240,
        height: 180,
        maxWidth: 240,
        maxHeight: 180,
        resizable: false,
        title: `foo_ui_webview2 e2e stream ${runId}`,
        behavior: { noActivate: true },
    });
    const popupId = created?.windowId;
    if (typeof popupId !== "string") return null;
    const pattern = new RegExp(`[?&]windowId=${popupId}(?:[&#]|$)`);
    const deadline = Date.now() + 20000;
    while (Date.now() < deadline) {
        const targets = await CDP.List({ port: resolvePort() });
        const target = targets.find((t) => t.type === "page" && pattern.test(String(t.url || "")));
        if (target) {
            const client = await CDP({ port: resolvePort(), target });
            const popupBridge = createBridge(client.Runtime, { invokeTimeoutMs });
            if (await waitForPopupPage(popupBridge, popupId, deadline)) {
                if (!blank) return { popupId, client, bridge: popupBridge };
                await client.Page.navigate({ url: "about:blank" });
                if (await waitForPopupPage(popupBridge, popupId, deadline, "about:blank")) {
                    return { popupId, client, bridge: popupBridge };
                }
            }
            await closeClient(client);
            break;
        }
        await sleep(100);
    }
    await bridge.invokeRaw("window.closePopup", { windowId: popupId }).catch(() => undefined);
    return null;
}

async function runCrossPageCase(bridge, recorder, mainBridge) {
    const popup = await openPopup(mainBridge);
    if (!popup) {
        recorder.addCase("PS-05 (A-D5, §4.6) another page cannot unsubscribe this page's stream: skipped, not run", true,
            { skipped: "no popup page appeared on the DevTools port" }, { skipped: "needs window.createPopup and a CDP target for the popup" });
        return;
    }
    try {
        const id = `ps-own-${runId}`;
        const mine = await bridge.invoke("audio.subscribeStream", { subscriptionId: id });
        const foreignById = await popup.bridge.invoke("audio.unsubscribeStream", { subscriptionId: id });
        const foreignAll = await popup.bridge.invoke("audio.unsubscribeStream", {});
        const still = (await streamState(bridge)).subscriptions.some((s) => s.subscriptionId === id);
        const own = await bridge.invoke("audio.unsubscribeStream", { subscriptionId: id });
        recorder.assertCase(
            "PS-05 (A-D5, §4.6) another page cannot unsubscribe this page's stream, by id or wholesale: removed 0 and the subscription stays until its owner removes it",
            mine?.success === true && foreignById?.removed === 0 && foreignAll?.removed === 0 && still === true && own?.removed === 1,
            { mine, foreignById, foreignAll, still, own },
            { foreign: { removed: 0 }, still: true, own: { removed: 1 } },
        );
    } finally {
        await closeClient(popup.client).catch(() => undefined);
        await mainBridge.invokeRaw("window.closePopup", { windowId: popup.popupId }).catch(() => undefined);
    }
}

// ---------------------------------------------------------------------------
// Playback tier

/** What the playback tier restores: taken when the tier starts, not when the suite does. */
async function snapshotPlayback(bridge) {
    const state = (await bridge.invoke("playback.getState")).state;
    const volume = await bridge.invoke("playback.getVolume");
    const active = await bridge.invoke("playlist.getActive", {});
    const playing = await bridge.invoke("playback.getCurrentTrackIndex", {});
    const position = await bridge.invoke("playback.getPosition");
    return {
        state,
        muted: volume?.muted === true,
        activePlaylist: typeof active?.index === "number" ? active.index : null,
        playing: playing?.found === true ? { playlist: playing.playlist, index: playing.index } : null,
        position: typeof position?.position === "number" ? position.position : 0,
    };
}

/** Mutes and makes a scratch playlist active, so playback.playPath adds the fixtures there. */
async function preparePlayback(bridge, cleanup) {
    await bridge.invoke("playback.mute", { muted: true });
    const created = await bridge.invoke("playlist.create", { name: `foo_ui_webview2 e2e stream ${runId}` });
    cleanup.scratchIndex = typeof created?.index === "number" ? created.index : null;
    if (cleanup.scratchIndex === null) throw blockedError("playlist.create returned no index", { created });
    await bridge.invoke("playlist.setActive", { playlist: cleanup.scratchIndex });
}

/**
 * Removes the scratch playlist (created last, so no other index moves) and puts back
 * the active playlist, the playing track and position, the paused state and the mute state.
 */
async function restorePlayback(bridge, snapshot, cleanup) {
    const quiet = (method, params) => bridge.invokeRaw(method, params).catch(() => undefined);
    await quiet("playback.stop", {});
    if (cleanup.scratchIndex !== null) await quiet("playlist.remove", { playlist: cleanup.scratchIndex });
    cleanup.scratchIndex = null;
    if (!snapshot) return;
    if (snapshot.activePlaylist !== null) await quiet("playlist.setActive", { playlist: snapshot.activePlaylist });
    if (snapshot.playing && snapshot.state !== "stopped") {
        await quiet("playlist.playTrack", { playlist: snapshot.playing.playlist, index: snapshot.playing.index });
        // setPosition needs the track open and seekable.
        const deadline = Date.now() + 5000;
        while (Date.now() < deadline) {
            const now = await bridge.invoke("playback.getState").catch(() => null);
            if (now?.state === "playing" && now.canSeek === true) break;
            await sleep(50);
        }
        if (snapshot.position > 0) await quiet("playback.setPosition", { position: snapshot.position });
        if (snapshot.state === "paused") await quiet("playback.pause", {});
    }
    await quiet("playback.mute", { muted: snapshot.muted });
}

async function runPlaybackCases(bridge, recorder, collector, fixtures) {
    const id = `ps-live-${runId}`;
    // The core's default interval depends on the setup (about one chunk per second on a clean
    // foobar2000 2.25.8 install, five on another), so the cases ask for 200 ms explicitly.
    const sub = await bridge.invoke("audio.subscribeStream", { subscriptionId: id, bufferSeconds: 1, interval: 0.2 });
    const chunkStart = (await streamState(bridge)).chunkCount;
    await bridge.invoke("playback.playPath", { path: fixtures.s48.path });
    const first = await waitForBuffer(bridge, id, 1, 3000);
    await attachReader(bridge, id, 1, "live");
    const live = await pump(bridge, "live", 5000);
    recorder.assertCase(
        "PS-10 (A-D1) a 1 s subscription gets its buffer within 2 s of playback starting (48 kHz, 48000 frames, epoch 1) and 5 s of 60 Hz reads yield about 5 s of −20 dBFS audio with no drops and few retries",
        sub?.success === true &&
            first?.data?.sampleRate === 48000 &&
            first.data.channels === 2 &&
            first.data.capacityFrames === 48000 &&
            first.data.epoch === 1 &&
            first.header.magic === 0x4d435046 &&
            first.header.mode === 2 &&
            first.header.epoch === 1 &&
            live?.frames >= 48000 * 4.5 &&
            live.frames <= 48000 * 5.6 &&
            live.dropped === 0 &&
            live.torn === 0 &&
            live.retries <= Math.max(2, live.reads * 0.01) &&
            withinPercent(live.rms[0], RMS_MINUS_20DBFS, 2) &&
            withinPercent(live.rms[1], RMS_MINUS_20DBFS, 2),
        { sub, first: first && { data: first.data, header: first.header }, live },
        { format: "48000/2, capacity 48000, epoch 1", frames: "216000..268800", dropped: 0, rms: "0.0707 ± 2%" },
    );

    await sleep(3000);
    const late = await readOnce(bridge, "live");
    await resetReader(bridge, "live");
    const after = await pump(bridge, "live", 1500);
    recorder.assertCase(
        "PS-11 (A-D2) a reader that stops for 3 s finds about 2 s of frames dropped on its next read, then no more drops",
        within(late?.out?.dropped, 96000, 9600) && late.out.frames === 48000 && after?.dropped === 0 && after.frames > 48000,
        { late: late?.out, after },
        { dropped: "96000 ± 9600", framesInRing: 48000, afterDropped: 0 },
    );

    await bridge.invoke("playback.pause");
    await sleep(600);
    const pausedA = await waitForWriteFrames(bridge, id, () => true, 200);
    await sleep(1000);
    const pausedB = await waitForWriteFrames(bridge, id, () => true, 200);
    const pausedHeader = (await buffersOf(bridge, id)).find((b) => b.epoch === 1)?.header;
    await bridge.invoke("playback.play");
    const resumed = await waitForWriteFrames(bridge, id, (s) => s.writeFrames !== pausedB?.writeFrames, 3000);
    await bridge.invoke("playback.stop");
    await sleep(600);
    const stoppedA = await waitForWriteFrames(bridge, id, () => true, 200);
    await sleep(1000);
    const stoppedB = await waitForWriteFrames(bridge, id, () => true, 200);
    recorder.assertCase(
        "PS-12 (A-D3) writeFrames stops growing within a second of pausing and of stopping, the ended flag stays clear, and resuming makes it grow again",
        pausedA?.writeFrames === pausedB?.writeFrames &&
            (pausedHeader?.flags & 1) === 0 &&
            resumed?.writeFrames !== pausedB?.writeFrames &&
            stoppedA?.writeFrames === stoppedB?.writeFrames,
        { paused: [pausedA?.writeFrames, pausedB?.writeFrames], flags: pausedHeader?.flags, resumed: resumed?.writeFrames, stopped: [stoppedA?.writeFrames, stoppedB?.writeFrames] },
        { pausedEqual: true, ended: false, resumedDifferent: true, stoppedEqual: true },
    );

    await bridge.invoke("playback.playPath", { path: fixtures.s44.path });
    const second = await waitForBuffer(bridge, id, 2, 3000);
    await collector.waitFor((all) => all.some((e) => e.payload?.subscriptionId === id && e.payload.epoch === 1), { timeoutMs: 2000 });
    const formatEvents = collector.received.filter((e) => e.payload?.subscriptionId === id);
    const oldHeader = (await buffersOf(bridge, id)).find((b) => b.epoch === 1)?.header;
    const entry2 = (await streamState(bridge)).subscriptions.find((s) => s.subscriptionId === id);
    recorder.assertCase(
        "PS-13 (A-D4) switching to a 44.1 kHz track posts a second buffer (epoch 2, 44100 Hz) and exactly one audio:stream { type: 'ended', epoch: 1, reason: 'format-change' }; the old buffer's ended flag is set and the debug state moves to epoch 2",
        second?.data?.sampleRate === 44100 &&
            second.data.epoch === 2 &&
            second.header.epoch === 2 &&
            second.header.capacityFrames === 44100 &&
            formatEvents.length === 1 &&
            formatEvents[0].payload.type === "ended" &&
            formatEvents[0].payload.epoch === 1 &&
            formatEvents[0].payload.reason === "format-change" &&
            (oldHeader?.flags & 1) === 1 &&
            entry2?.epoch === 2 &&
            entry2.capacityFrames === 44100,
        { second: second && { data: second.data, header: second.header }, formatEvents: formatEvents.map((e) => e.payload), oldFlags: oldHeader?.flags, entry2 },
        { second: "44100/2, epoch 2", events: 1, oldEnded: true },
    );

    // A-D6: two more subscriptions of different lengths see the same samples.
    const shortId = `ps-short-${runId}`;
    const longId = `ps-long-${runId}`;
    await bridge.invoke("audio.subscribeStream", { subscriptionId: shortId, bufferSeconds: 0.5 });
    await bridge.invoke("audio.subscribeStream", { subscriptionId: longId, bufferSeconds: 2 });
    const shortBuf = await waitForBuffer(bridge, shortId, 1, 3000);
    const longBuf = await waitForBuffer(bridge, longId, 1, 3000);
    await sleep(1500);
    await bridge.invoke("playback.pause");
    await sleep(500);
    const aligned = await page(
        bridge,
        `const pick = (id) => store.buffers.filter((b) => b.id === id).pop();
         const s = pick(${JSON.stringify(shortId)}), l = pick(${JSON.stringify(longId)});
         if (!s || !l) return null;
         const hs = store.header(s.buffer), hl = store.header(l.buffer);
         const n = 4000;
         const tail = (buf, h) => { const ch = h.channels, cap = h.capacityFrames; const f = new Float32Array(buf, 64, cap * ch); const out = new Float32Array(n * ch); let slot = (h.writeSlot + cap - n) % cap; for (let i = 0; i < n; i += 1) { for (let c = 0; c < ch; c += 1) out[i * ch + c] = f[slot * ch + c]; slot = slot + 1 === cap ? 0 : slot + 1; } return out; };
         const ts = tail(s.buffer, hs), tl = tail(l.buffer, hl);
         let same = ts.length === tl.length; for (let i = 0; same && i < ts.length; i += 1) if (ts[i] !== tl[i]) same = false;
         return { sameWriteFrames: hs.writeFrames === hl.writeFrames, writeFrames: [hs.writeFrames, hl.writeFrames], capacities: [hs.capacityFrames, hl.capacityFrames], identicalTail: same, compared: n };`,
        20000,
    );
    await bridge.invoke("playback.play");
    recorder.assertCase(
        "PS-14 (A-D6) two subscriptions of 0.5 s and 2 s on the same 44.1 kHz audio get rings of 22050 and 88200 frames and, once paused, hold bit-identical newest frames",
        shortBuf?.data?.capacityFrames === 22050 &&
            longBuf?.data?.capacityFrames === 88200 &&
            aligned?.identicalTail === true,
        { short: shortBuf?.data, long: longBuf?.data, aligned },
        { capacities: [22050, 88200], identicalTail: true },
    );

    // A-D7: main-thread cost per chunk with the three live subscriptions plus one more.
    const costId = `ps-cost-${runId}`;
    await bridge.invoke("audio.subscribeStream", { subscriptionId: costId, bufferSeconds: 1 });
    const c0 = await streamState(bridge);
    await sleep(4000);
    const c1 = await streamState(bridge);
    const chunks = c1.chunkCount - c0.chunkCount;
    const cyclesPerChunk = chunks > 0 ? (c1.chunkCycles - c0.chunkCycles) / chunks : null;
    recorder.assertCase(
        "PS-15 (A-D7) with four subscriptions the capture callback spends at most 200000 cycles (100 µs at 2 GHz) per chunk on average",
        c1.subscriptions.length === 4 && chunks >= 10 && cyclesPerChunk !== null && cyclesPerChunk <= CYCLES_PER_CHUNK_BUDGET,
        { subscriptions: c1.subscriptions.length, chunks, cyclesPerChunk: cyclesPerChunk && Math.round(cyclesPerChunk) },
        { subscriptions: 4, cyclesPerChunk: `<= ${CYCLES_PER_CHUNK_BUDGET}` },
    );
    const removedExtra = await bridge.invoke("audio.unsubscribeStream", { subscriptionId: costId });
    await bridge.invoke("audio.unsubscribeStream", { subscriptionId: shortId });
    await bridge.invoke("audio.unsubscribeStream", { subscriptionId: longId });

    // A-D11: a chunk longer than the ring.
    const tinyId = `ps-tiny-${runId}`;
    const tiny = await bridge.invoke("audio.subscribeStream", { subscriptionId: tinyId, interval: 0.2, bufferSeconds: 0.1 });
    const tinyBuf = await waitForBuffer(bridge, tinyId, 1, 3000);
    await attachReader(bridge, tinyId, 1, "tiny");
    const tinyStart = await readerStats(bridge, "tiny");
    const tinyRun = await pump(bridge, "tiny", 5000);
    const tinyEntry = (await streamState(bridge)).subscriptions.find((s) => s.subscriptionId === tinyId);
    const accounted = tinyRun.frames + tinyRun.dropped;
    const written = (tinyRun.writeFrames - tinyStart.readFrames) >>> 0;
    recorder.assertCase(
        "PS-16 (A-D11) a 0.1 s ring under 200 ms chunks keeps the host alive, never hands out more frames than its capacity per read, and frames read plus frames dropped account for everything written to within one chunk",
        tiny?.success === true &&
            tinyBuf?.data?.capacityFrames === 4410 &&
            tinyRun.maxFrames <= 4410 &&
            tinyRun.reads > 0 &&
            Math.abs(accounted - written) <= 44100 * 0.25 &&
            tinyEntry?.epoch === 1,
        { tiny, capacity: tinyBuf?.data?.capacityFrames, tinyRun, written, accounted },
        { capacity: 4410, maxFramesPerRead: "<= 4410", accountedWithin: "one chunk" },
    );
    await bridge.invoke("audio.unsubscribeStream", { subscriptionId: tinyId });

    // A-D5: unsubscribe sets the flag, empties the debug state and stops the callback.
    const removedLive = await bridge.invoke("audio.unsubscribeStream", { subscriptionId: id });
    const afterUnsub = await streamState(bridge);
    const liveHeader = (await buffersOf(bridge, id)).find((b) => b.epoch === 2)?.header;
    const chunkAtUnsub = afterUnsub.chunkCount;
    await sleep(1500);
    const chunkLater = (await streamState(bridge)).chunkCount;
    recorder.assertCase(
        "PS-17 (A-D5) unsubscribing the last subscription answers removed 1, sets the ended flag on its buffer, empties the debug state and unregisters the callback so chunkCount stops growing",
        removedLive?.removed === 1 &&
            removedExtra?.removed === 1 &&
            (liveHeader?.flags & 1) === 1 &&
            afterUnsub.subscriptions.length === 0 &&
            afterUnsub.callbackRegistered === false &&
            chunkLater === chunkAtUnsub &&
            chunkAtUnsub > chunkStart,
        { removedLive, flags: liveHeader?.flags, afterUnsub, chunkAtUnsub, chunkLater, chunkStart },
        { removed: 1, ended: true, subscriptions: 0, callbackRegistered: false, chunkCountFrozen: true },
    );
}

// ---------------------------------------------------------------------------
// Entry

const recorder = createRecorder();
let client;
let mainBridge;
let streamPage = null;
let bridge;
let blocked = false;
let fatalError;
let snapshot = null;
let touchedPlayback = false;
const cleanup = { scratchIndex: null };
const entry = { playbackTier: PLAYBACK_TIER };

try {
    const connection = await connectBridgePage();
    client = connection.client;
    console.log(`Target: ${connection.page.title} ${connection.page.url}`);
    mainBridge = createBridge(client.Runtime, { invokeTimeoutMs });
    await requireResponsiveBridge(mainBridge, { probeId: "audio-stream" });

    streamPage = await openPopup(mainBridge, { blank: true });
    if (!streamPage) {
        throw blockedError("no popup page appeared on the DevTools port; the suite runs its cases in an about:blank popup", {});
    }
    bridge = streamPage.bridge;
    entry.streamPage = streamPage.popupId;

    const state = await debugState(bridge);
    if (!state?.runtime?.environment12 || !state.runtime.webview17) {
        throw blockedError("this WebView2 runtime cannot share buffers with pages", { runtime: state?.runtime });
    }
    if (state.stream?.subscriptions?.length) {
        throw blockedError("another caller holds stream subscriptions; the bookkeeping cases need none", { stream: state.stream });
    }

    await installStore(bridge);
    const collector = await createEventCollector(bridge, EVENTS, { collectorId: `stream-${runId}` });
    try {
        await runContractCases(bridge, recorder);
        await runCrossPageCase(bridge, recorder, mainBridge);
        if (PLAYBACK_TIER) {
            snapshot = await snapshotPlayback(bridge);
            entry.playbackOnEntry = snapshot;
            if (snapshot.state === "playing" && !INTERRUPT_PLAYBACK) {
                recorder.addCase(
                    "PS-10 to PS-17 (A-D1–A-D7, A-D11) playback tier: skipped, not run",
                    true,
                    { skipped: "playback was running when the tier started" },
                    { skipped: "set FB2K_E2E_INTERRUPT_PLAYBACK=1 to interrupt it; the tier restores the track and position afterwards" },
                );
            } else {
                const fixtures = ensureFixtures();
                entry.fixtures = Object.fromEntries(Object.entries(fixtures).map(([k, v]) => [k, { path: v.path, reused: v.reused }]));
                touchedPlayback = true;
                try {
                    await preparePlayback(bridge, cleanup);
                    await runPlaybackCases(bridge, recorder, collector, fixtures);
                } finally {
                    await bridge.invokeRaw("audio.unsubscribeStream", {}).catch(() => undefined);
                    await restorePlayback(bridge, snapshot, cleanup);
                    touchedPlayback = false;
                }
            }
        } else {
            recorder.addCase(
                "PS-10 to PS-17 (A-D1–A-D7, A-D11) playback tier: skipped, not run",
                true,
                { skipped: "FB2K_E2E_AUDIO_PCM_PLAYBACK is not 1" },
                { skipped: "mutes foobar2000 and plays fixtures from a scratch playlist; set FB2K_E2E_AUDIO_PCM_PLAYBACK=1 to run" },
            );
        }
    } finally {
        await collector.stop();
        await releaseAll(bridge);
    }
} catch (error) {
    fatalError = error;
    blocked = Boolean(error?.blocked);
    if (bridge && touchedPlayback) await restorePlayback(bridge, snapshot, cleanup);
} finally {
    if (streamPage) {
        await closeClient(streamPage.client).catch(() => undefined);
        await mainBridge.invokeRaw("window.closePopup", { windowId: streamPage.popupId }).catch(() => undefined);
    }
    if (client) await closeClient(client);
}

process.exitCode = report({ recorder, blocked, fatalError, extra: entry });

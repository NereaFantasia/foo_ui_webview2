/**
 * Covers offline PCM decoding: audio.decodePcm, audio.cancelDecodePcm and
 * audio.getPcmDebugState with the audio:pcmReady / audio:pcmFailed events and
 * the shared buffers that carry the samples, against docs/audio-pcm/SPEC.md
 * (§4.1–§4.4, §4.10; acceptance A-C1–A-C6 and A-C10 in §10).
 *
 * Decoding never touches playback, so the whole suite is one tier. The page
 * side keeps every `sharedbufferreceived` buffer of purpose audio.decodePcm,
 * because a fast task's buffer can arrive before its taskId is known here. It
 * reads the header and samples of this run's tasks in the page and releases
 * only those: every listener of one event shares the same ArrayBuffer, so
 * releasing a buffer of someone else's task would break its owner. The
 * cross-page cases open a small popup of their own and close it again; they
 * are skipped when no popup page shows up.
 *
 * A-C7 (reload drops the result) and A-C8 (exit while decoding) change the
 * page document and the process, which would break the suites after this one,
 * so they are live-probe checks recorded in the spec rather than cases here.
 *
 * Fixtures are 16-bit WAVs this suite writes into FB2K_E2E_PCM_FIXTURE_DIR
 * (default E:\FB2K\e2e-fixtures\audio-pcm), outside the monitored library
 * folders: a 20 s mono 1 kHz sine at 0 dBFS, a 20 s stereo one with the tone
 * on the left only, and a 200 s 8 kHz stereo sine whose conversion to 48 kHz
 * takes long enough to cancel.
 *
 * Usage: node mcp/tests/e2e-audio-pcm.mjs
 * Environment: FB2K_CDP_PORT, FB2K_E2E_TIMEOUT_MS, FB2K_E2E_PCM_FIXTURE_DIR.
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
const EVENTS = ["audio:pcmReady", "audio:pcmFailed"];
/** Longest wait for one task's terminal event; the 200 s conversion takes about two seconds. */
const TERMINAL_MS = 30000;
/** How long a case keeps listening after a terminal event to prove nothing else arrives. */
const QUIET_MS = 1500;
/** 64-bit foobar2000's single-buffer limit (§4.2, D9). */
const LIMIT_BYTES = 256 * 1024 * 1024;

const runId = Date.now().toString(36);
const storeKey = `__e2ePcm_${runId}`;
const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
const within = (value, target, tolerance) => typeof value === "number" && Math.abs(value - target) <= tolerance;
const withinPercent = (value, target, percent) => within(value, target, (Math.abs(target) * percent) / 100);

// ---------------------------------------------------------------------------
// Fixtures

function writeWav(path, { sampleRate, seconds, channels, sample }) {
    const frames = sampleRate * seconds;
    const blockAlign = channels * 2;
    const dataBytes = frames * blockAlign;
    const buf = Buffer.alloc(44 + dataBytes);
    buf.write("RIFF", 0);
    buf.writeUInt32LE(36 + dataBytes, 4);
    buf.write("WAVE", 8);
    buf.write("fmt ", 12);
    buf.writeUInt32LE(16, 16);
    buf.writeUInt16LE(1, 20); // PCM
    buf.writeUInt16LE(channels, 22);
    buf.writeUInt32LE(sampleRate, 24);
    buf.writeUInt32LE(sampleRate * blockAlign, 28);
    buf.writeUInt16LE(blockAlign, 32);
    buf.writeUInt16LE(16, 34);
    buf.write("data", 36);
    buf.writeUInt32LE(dataBytes, 40);
    for (let i = 0; i < frames; i += 1) {
        for (let c = 0; c < channels; c += 1) {
            const v = Math.round(sample(i, c));
            buf.writeInt16LE(Math.max(-32768, Math.min(32767, v)), 44 + i * blockAlign + c * 2);
        }
    }
    writeFileSync(path, buf);
}

/** Writes the fixtures unless files of the expected size already exist. */
function ensureFixtures() {
    try {
        mkdirSync(FIXTURE_DIR, { recursive: true });
    } catch (error) {
        throw blockedError(`fixture directory is not writable: ${FIXTURE_DIR}`, { error: String(error?.message ?? error) });
    }
    const tone = (sampleRate) => (i) => 32767 * Math.sin((2 * Math.PI * 1000 * i) / sampleRate);
    const specs = {
        mono: { file: "pcm_sine_1k_0dBFS_48k_mono_20s.wav", sampleRate: 48000, seconds: 20, channels: 1, sample: tone(48000) },
        left: {
            file: "pcm_sine_1k_left_0dBFS_48k_stereo_20s.wav",
            sampleRate: 48000,
            seconds: 20,
            channels: 2,
            sample: (i, c) => (c === 0 ? tone(48000)(i) : 0),
        },
        long: { file: "pcm_sine_1k_8k_stereo_200s.wav", sampleRate: 8000, seconds: 200, channels: 2, sample: tone(8000) },
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
        out[key] = { path, sampleRate: spec.sampleRate, seconds: spec.seconds, channels: spec.channels, reused };
    }
    return out;
}

// ---------------------------------------------------------------------------
// Page-side buffer store

/** Installs the page listener that keeps every decode buffer until a case reads it. */
async function installBufferStore(bridge) {
    const key = JSON.stringify(storeKey);
    await bridge.evaluateValue(
        `(() => {
            if (window[${key}]) return true;
            const store = window[${key}] = { claimed: new Set(), entries: [] };
            window.chrome.webview.addEventListener('sharedbufferreceived', (e) => {
                const d = e.additionalData;
                if (!d || d.purpose !== 'audio.decodePcm') return;
                store.entries.push({ taskId: d.taskId, data: d, buffer: e.getBuffer() });
            });
            return true;
        })()`,
        false,
        3000,
    );
}

/** Marks task ids as this run's, so the final cleanup may release their buffers. */
const claim = (bridge, taskIds) =>
    bridge.evaluateValue(`(() => { for (const id of ${JSON.stringify(taskIds)}) window[${JSON.stringify(storeKey)}].claimed.add(id); return true; })()`, false, 3000);

/** Header, per-channel RMS and optional FFT peak of every stored buffer of one task, then releases them. */
async function readAndRelease(bridge, taskId, { fft = false } = {}) {
    return bridge.evaluateValue(
        `(() => {
            const store = window[${JSON.stringify(storeKey)}];
            const mine = store.entries.filter((e) => e.taskId === ${JSON.stringify(taskId)});
            store.entries = store.entries.filter((e) => e.taskId !== ${JSON.stringify(taskId)});
            const views = mine.map((entry) => {
                const buf = entry.buffer;
                const u32 = new Uint32Array(buf, 0, 16);
                const dv = new DataView(buf, 0, 64);
                const header = {
                    magic: u32[0], version: u32[1], headerBytes: u32[2], mode: u32[3], sampleRate: u32[4], channels: u32[5],
                    capacityFrames: u32[6], seq: Atomics.load(u32, 7), writeFrames: u32[8], flags: u32[9],
                    hostTimeMs: dv.getFloat64(40, true), startSeconds: dv.getFloat64(48, true), epoch: u32[14], writeSlot: u32[15],
                };
                const channels = [];
                for (let c = 0; c < header.channels; c += 1) {
                    const view = new Float32Array(buf, 64 + c * header.capacityFrames * 4, header.writeFrames);
                    let sum = 0;
                    for (let i = 0; i < view.length; i += 1) sum += view[i] * view[i];
                    channels.push({ length: view.length, rms: Math.sqrt(sum / view.length) });
                }
                let peak = null;
                if (${fft} && header.writeFrames >= 4096) {
                    const n = 2048, view = new Float32Array(buf, 64, header.writeFrames), off = Math.floor(view.length / 2) - n / 2;
                    let best = 0, bestK = 0;
                    for (let k = 1; k < n / 2; k += 1) {
                        let re = 0, im = 0;
                        for (let j = 0; j < n; j += 1) { const a = (2 * Math.PI * k * j) / n; re += view[off + j] * Math.cos(a); im -= view[off + j] * Math.sin(a); }
                        const m = re * re + im * im;
                        if (m > best) { best = m; bestK = k; }
                    }
                    peak = { bin: bestK, binHz: header.sampleRate / n, hz: (bestK * header.sampleRate) / n };
                }
                return { byteLength: buf.byteLength, data: entry.data, header, channels, peak };
            });
            for (const entry of mine) { try { window.chrome.webview.releaseBuffer(entry.buffer); } catch {} }
            return views;
        })()`,
        false,
        20000,
    );
}

async function waitForBuffers(bridge, taskId, count, timeoutMs = 5000) {
    const deadline = Date.now() + timeoutMs;
    for (;;) {
        const n = await bridge.evaluateValue(
            `window[${JSON.stringify(storeKey)}].entries.filter((e) => e.taskId === ${JSON.stringify(taskId)}).length`,
            false,
            3000,
        );
        if (n >= count || Date.now() >= deadline) return n;
        await sleep(30);
    }
}

// ---------------------------------------------------------------------------
// Bridge helpers

const debugState = (bridge) => bridge.invoke("audio.getPcmDebugState", {});

/** Waits until nothing decodes, queues or holds a host-side buffer. */
async function waitIdle(bridge, timeoutMs = TERMINAL_MS) {
    const deadline = Date.now() + timeoutMs;
    let state;
    for (;;) {
        state = await debugState(bridge);
        const d = state?.decode;
        if ((d?.active === 0 && d.queued === 0 && d.openBufferBytes === 0) || Date.now() >= deadline) return state;
        await sleep(50);
    }
}

const eventsOf = (collector, taskId) => collector.received.filter((e) => e.payload?.taskId === taskId);

async function waitTerminal(collector, taskId, timeoutMs = TERMINAL_MS) {
    await collector.waitFor((all) => all.some((e) => e.payload?.taskId === taskId), { timeoutMs, pollMs: 50 });
    return eventsOf(collector, taskId);
}

/**
 * Starts one decode, waits for its terminal event and buffer, and returns the
 * answer, the events and what the page read from the buffer.
 */
async function decode(bridge, collector, params, { fft = false } = {}) {
    const res = await bridge.invokeRaw("audio.decodePcm", params).then((r) => r.value ?? r);
    if (res?.success !== true) return { res, events: [], views: [] };
    await claim(bridge, [res.taskId]);
    const events = await waitTerminal(collector, res.taskId);
    if (events[0]?.name === "audio:pcmReady") await waitForBuffers(bridge, res.taskId, 1);
    const views = await readAndRelease(bridge, res.taskId, { fft });
    return { res, events, views };
}

// ---------------------------------------------------------------------------
// Cases

async function runDebugStateCases(bridge, recorder) {
    const state = await debugState(bridge);
    recorder.assertCase(
        "PC-01 (A-C10, §4.10) getPcmDebugState reports the installed runtime version, shared-buffer support of this page and an idle decode queue",
        state?.success === true &&
            typeof state.runtime?.version === "string" &&
            state.runtime.version.length > 0 &&
            state.runtime.environment12 === true &&
            state.runtime.webview17 === true &&
            state.decode?.active === 0 &&
            state.decode.queued === 0 &&
            state.decode.openBufferBytes === 0,
        state,
        { runtime: { version: "non-empty", environment12: true, webview17: true }, decode: { active: 0, queued: 0, openBufferBytes: 0 } },
    );
    return state;
}

async function runDecodeCases(bridge, recorder, collector, fixtures) {
    const whole = await decode(bridge, collector, { path: fixtures.mono.path });
    const ready = whole.events[0]?.payload;
    const view = whole.views[0];
    recorder.assertCase(
        "PC-02 (A-C1) a 20 s 48 kHz mono sine decodes to one ready event and one buffer: 960000 +/- 480 frames, not truncated, a view of exactly `frames` samples with RMS 0.7071 +/- 1%",
        whole.res?.status === "pending" &&
            /^pcm_\d+$/.test(whole.res.taskId) &&
            whole.events.length === 1 &&
            whole.events[0].name === "audio:pcmReady" &&
            ready?.sampleRate === 48000 &&
            ready.channels === 1 &&
            within(ready.frames, 960000, 480) &&
            ready.truncated === false &&
            ready.resampled === false &&
            whole.views.length === 1 &&
            view.channels[0].length === ready.frames &&
            withinPercent(view.channels[0].rms, 0.7071, 1),
        { res: whole.res, ready, views: whole.views.map((v) => ({ byteLength: v.byteLength, channels: v.channels })) },
        { frames: "959520..960480", rms: "0.7000..0.7142", views: 1 },
    );
    recorder.assertCase(
        "PC-03 (§4.1, §4.3) the buffer describes itself: FPCM magic, version 1, one-shot mode, seq 2, ended set, writeFrames equal to the event's frames, and additionalData matching the event",
        view?.header.magic === 0x4d435046 &&
            view.header.version === 1 &&
            view.header.headerBytes === 64 &&
            view.header.mode === 1 &&
            view.header.seq === 2 &&
            (view.header.flags & 1) === 1 &&
            view.header.writeFrames === ready?.frames &&
            view.header.sampleRate === ready.sampleRate &&
            view.header.epoch === 1 &&
            view.data?.taskId === whole.res?.taskId &&
            view.data.frames === ready.frames &&
            view.data.sampleRate === ready.sampleRate &&
            view.data.channels === ready.channels &&
            view.data.headerBytes === 64,
        { header: view?.header, additionalData: view?.data },
        { magic: 0x4d435046, version: 1, mode: 1, seq: 2, ended: true },
    );

    const range = await decode(bridge, collector, { path: fixtures.mono.path, start: 5, end: 7 });
    const beyond = await decode(bridge, collector, { path: fixtures.mono.path, start: 25 });
    const clamped = await decode(bridge, collector, { path: fixtures.mono.path, end: 100 });
    const r = range.events[0]?.payload;
    const c = clamped.events[0]?.payload;
    recorder.assertCase(
        "PC-04 (A-C2) start 5 end 7 gives 96000 +/- 480 frames starting at 5 s; start past the end fails with INVALID_PARAMS; end 100 is cut to the 20 s track",
        range.events[0]?.name === "audio:pcmReady" &&
            within(r?.frames, 96000, 480) &&
            r.start === 5 &&
            within(r.end, 7, 0.01) &&
            range.views[0]?.header.startSeconds === 5 &&
            beyond.events.length === 1 &&
            beyond.events[0].name === "audio:pcmFailed" &&
            beyond.events[0].payload.code === "INVALID_PARAMS" &&
            clamped.events[0]?.name === "audio:pcmReady" &&
            within(c?.end, 20, 0.01) &&
            within(c.frames, 960000, 480),
        { range: r, beyond: beyond.events, clamped: c },
        { range: { frames: "95520..96480", start: 5 }, beyond: "audio:pcmFailed INVALID_PARAMS", clamped: { end: 20 } },
    );

    const down = await decode(bridge, collector, { path: fixtures.mono.path, sampleRate: 8000 }, { fft: true });
    const mono = await decode(bridge, collector, { path: fixtures.left.path, mono: true });
    const stereo = await decode(bridge, collector, { path: fixtures.left.path });
    const d = down.events[0]?.payload;
    const noResampler = down.events[0]?.name === "audio:pcmFailed" && down.events[0].payload.code === "NOT_SUPPORTED";
    recorder.assertCase(
        "PC-05 (A-C3) sampleRate 8000 gives 160000 +/- 80 frames with its 2048-point FFT peak within one bin of 1 kHz (or NOT_SUPPORTED when no resampler handles it); mono averages the left-only sine to RMS 0.354 +/- 1% while the plain decode keeps two channels",
        (noResampler ||
            (d?.sampleRate === 8000 &&
                d.resampled === true &&
                within(d.frames, 160000, 80) &&
                down.views[0]?.peak !== null &&
                Math.abs(down.views[0].peak.hz - 1000) <= down.views[0].peak.binHz)) &&
            mono.events[0]?.payload?.channels === 1 &&
            mono.views[0]?.channels.length === 1 &&
            withinPercent(mono.views[0].channels[0].rms, 0.3536, 1) &&
            stereo.events[0]?.payload?.channels === 2 &&
            stereo.views[0]?.channels.length === 2 &&
            stereo.views[0].channels[1].rms < 1e-6,
        { down: d ?? down.events, peak: down.views[0]?.peak, mono: mono.views[0]?.channels, stereo: stereo.views[0]?.channels },
        { down: { frames: "159920..160080", peakHz: "1000 +/- one bin" }, monoRms: "0.3500..0.3571", stereoChannels: 2 },
    );
}

async function runRefusalCases(bridge, recorder, fixtures) {
    const before = (await debugState(bridge))?.decode;
    const call = (params) => bridge.invokeRaw("audio.decodePcm", params).then((r) => r.value ?? r);
    const refused = {
        negativeStart: await call({ path: fixtures.mono.path, start: -1 }),
        endEqualsStart: await call({ path: fixtures.mono.path, start: 3, end: 3 }),
        rateTooLow: await call({ path: fixtures.mono.path, sampleRate: 7999 }),
        fractionalRate: await call({ path: fixtures.mono.path, sampleRate: 44100.5 }),
        monoString: await call({ path: fixtures.mono.path, mono: "yes" }),
        unknownKey: await call({ path: fixtures.mono.path, foo: 1 }),
        noPath: await call({}),
    };
    const missing = await call({ path: join(FIXTURE_DIR, `missing-${runId}.wav`) });
    const oversize = await call({ path: fixtures.long.path, sampleRate: 192000 });
    const after = (await debugState(bridge))?.decode;
    const allInvalid = Object.values(refused).every((res) => res?.success === false && res.code === "INVALID_PARAMS");
    recorder.assertCase(
        "PC-06 (A-C4) out-of-range, mistyped and undeclared parameters answer INVALID_PARAMS synchronously, a missing file INVALID_PATH, and none of them queues a task",
        allInvalid &&
            missing?.success === false &&
            missing.code === "INVALID_PATH" &&
            JSON.stringify(before) === JSON.stringify(after),
        { refused, missing, before, after },
        { refused: "INVALID_PARAMS each", missing: "INVALID_PATH", queue: "unchanged" },
    );
    recorder.assertCase(
        "PC-07 (A-C4, D9) a range whose estimate exceeds the single-buffer limit (200 s stereo at 192 kHz) answers INVALID_PARAMS with the estimate and the 256 MiB limit in details",
        oversize?.success === false &&
            oversize.code === "INVALID_PARAMS" &&
            oversize.details?.limitBytes === LIMIT_BYTES &&
            oversize.details.estimatedBytes > LIMIT_BYTES &&
            typeof oversize.details.suggestion === "string",
        oversize,
        { code: "INVALID_PARAMS", details: { limitBytes: LIMIT_BYTES, estimatedBytes: `> ${LIMIT_BYTES}` } },
    );
}

async function runCancelCases(bridge, recorder, collector, fixtures) {
    const res = await bridge.invoke("audio.decodePcm", { path: fixtures.long.path, sampleRate: 48000 });
    await claim(bridge, [res.taskId]);
    await sleep(30);
    const busy = (await debugState(bridge))?.decode;
    const cancelled = await bridge.invoke("audio.cancelDecodePcm", { taskId: res.taskId });
    const again = await bridge.invoke("audio.cancelDecodePcm", { taskId: res.taskId });
    const unknown = await bridge.invoke("audio.cancelDecodePcm", { taskId: `pcm_${runId}` });
    const empty = await bridge.invokeRaw("audio.cancelDecodePcm", { taskId: "" }).then((r) => r.value ?? r);
    await waitTerminal(collector, res.taskId);
    await sleep(QUIET_MS);
    await collector.drain();
    const events = eventsOf(collector, res.taskId);
    const idle = (await waitIdle(bridge))?.decode;
    const views = await readAndRelease(bridge, res.taskId);
    recorder.assertCase(
        "PC-08 (A-C5, §4.4) cancelling a decoding task answers cancelled true, sends exactly one CANCELLED audio:pcmFailed and no buffer, and the host returns to idle with no open buffer; a second cancel and an unknown taskId answer cancelled false, an empty one INVALID_PARAMS",
        busy?.active === 1 &&
            cancelled?.success === true &&
            cancelled.cancelled === true &&
            again?.cancelled === false &&
            unknown?.cancelled === false &&
            empty?.success === false &&
            empty.code === "INVALID_PARAMS" &&
            events.length === 1 &&
            events[0].name === "audio:pcmFailed" &&
            events[0].payload.code === "CANCELLED" &&
            views.length === 0 &&
            idle?.active === 0 &&
            idle.openBufferBytes === 0,
        { busy, cancelled, again, unknown, empty, events, views: views.length, idle },
        { events: "one CANCELLED", idle: { active: 0, openBufferBytes: 0 } },
    );
}

async function runQueueCases(bridge, recorder, collector, fixtures) {
    // Fired from the page in one go, so all three arrive while the first decodes.
    const params = { path: fixtures.long.path, sampleRate: 48000 };
    const answers = await bridge.evaluateValue(
        `Promise.all([0, 1, 2].map(() => window.fb2k.invoke("audio.decodePcm", ${JSON.stringify(params)})))`,
        true,
        invokeTimeoutMs,
    );
    const ids = answers.map((a) => a?.taskId);
    await claim(bridge, ids);
    const merged = (await debugState(bridge))?.decode;
    for (const id of ids) await waitTerminal(collector, id);
    for (const id of ids) await waitForBuffers(bridge, id, 1);
    const views = [];
    for (const id of ids) views.push(...(await readAndRelease(bridge, id)));
    recorder.assertCase(
        "PC-09 (A-C6, D7) three identical requests in flight get three task ids but share one decode, and each gets its own ready event and its own view of the same-sized buffer",
        new Set(ids).size === 3 &&
            ids.every((id) => /^pcm_\d+$/.test(id)) &&
            merged?.active === 1 &&
            merged.queued === 0 &&
            ids.every((id) => eventsOf(collector, id).length === 1 && eventsOf(collector, id)[0].name === "audio:pcmReady") &&
            views.length === 3 &&
            views.every((v) => v.byteLength === views[0].byteLength && v.byteLength > 0),
        { ids, merged, events: ids.map((id) => eventsOf(collector, id).map((e) => e.name)), byteLengths: views.map((v) => v.byteLength) },
        { merged: { active: 1, queued: 0 }, readyEach: 1, views: 3 },
    );

    const first = await bridge.invoke("audio.decodePcm", { path: fixtures.long.path, sampleRate: 22050 });
    const second = await bridge.invoke("audio.decodePcm", { path: fixtures.mono.path });
    await claim(bridge, [first.taskId, second.taskId]);
    const serial = (await debugState(bridge))?.decode;
    const firstEvents = await waitTerminal(collector, first.taskId);
    const secondEvents = await waitTerminal(collector, second.taskId);
    await readAndRelease(bridge, first.taskId);
    await readAndRelease(bridge, second.taskId);
    recorder.assertCase(
        "PC-10 (A-C6, D7) two different requests decode one after the other: the second queues while the first decodes and its ready event does not come first",
        serial?.active === 1 &&
            serial.queued === 1 &&
            firstEvents[0]?.name === "audio:pcmReady" &&
            secondEvents[0]?.name === "audio:pcmReady" &&
            firstEvents[0].at <= secondEvents[0].at,
        { serial, firstAt: firstEvents[0]?.at, secondAt: secondEvents[0]?.at },
        { serial: { active: 1, queued: 1 }, order: "first before second" },
    );
    await waitIdle(bridge);
}

/** Opens a small popup and attaches to its page; null when the page does not show up. */
async function openPopup(bridge) {
    const created = await bridge.invoke("window.createPopup", {
        width: 240,
        height: 180,
        maxWidth: 240,
        maxHeight: 180,
        resizable: false,
        title: `foo_ui_webview2 e2e pcm ${runId}`,
        behavior: { noActivate: true },
    });
    const popupId = created?.windowId;
    if (typeof popupId !== "string") return null;
    const pattern = new RegExp(`[?&]windowId=${popupId}(?:[&#]|$)`);
    const deadline = Date.now() + 20000;
    while (Date.now() < deadline) {
        const targets = await CDP.List({ port: resolvePort() });
        const page = targets.find((t) => t.type === "page" && pattern.test(String(t.url || "")));
        if (page) {
            const client = await CDP({ port: resolvePort(), target: page });
            const popupBridge = createBridge(client.Runtime, { invokeTimeoutMs });
            while (Date.now() < deadline) {
                try {
                    const id = await popupBridge.invokeRaw("window.getCurrentWindowId", {});
                    if (id?.value?.windowId === popupId) return { popupId, client, bridge: popupBridge };
                } catch {
                    // The page may still be navigating; retry until the deadline.
                }
                await sleep(100);
            }
            await closeClient(client);
            break;
        }
        await sleep(100);
    }
    await bridge.invokeRaw("window.closePopup", { windowId: popupId }).catch(() => undefined);
    return null;
}

async function runCrossPageCases(bridge, recorder, collector, fixtures) {
    const popup = await openPopup(bridge);
    if (!popup) {
        recorder.addCase(
            "PC-11, PC-12 (A-C5, D15) cross-page cancel and popup close: skipped, not run",
            true,
            { skipped: "no popup page appeared on the DevTools port" },
            { skipped: "needs window.createPopup and a CDP target for the popup" },
        );
        return;
    }
    let popupClosed = false;
    try {
        const popupEvents = await createEventCollector(popup.bridge, EVENTS, { collectorId: `pcm-popup-${runId}` });
        const mine = await bridge.invoke("audio.decodePcm", { path: fixtures.long.path, sampleRate: 48000 });
        await claim(bridge, [mine.taskId]);
        const foreign = await popup.bridge.invoke("audio.cancelDecodePcm", { taskId: mine.taskId });
        const ownEvents = await waitTerminal(collector, mine.taskId);
        await waitForBuffers(bridge, mine.taskId, 1);
        const views = await readAndRelease(bridge, mine.taskId);
        await popupEvents.waitFor(null, { timeoutMs: 300 });
        const leaked = popupEvents.received.filter((e) => e.payload?.taskId === mine.taskId);
        recorder.assertCase(
            "PC-11 (A-C5, §4.4) another page cannot cancel this page's task: it answers cancelled false, the task still completes here with its buffer, and the other page gets none of its events",
            foreign?.success === true &&
                foreign.cancelled === false &&
                ownEvents.length === 1 &&
                ownEvents[0].name === "audio:pcmReady" &&
                views.length === 1 &&
                leaked.length === 0,
            { foreign, ownEvents: ownEvents.map((e) => e.name), views: views.length, leaked },
            { foreign: { cancelled: false }, own: "audio:pcmReady with one buffer", leaked: 0 },
        );

        const theirs = await popup.bridge.invoke("audio.decodePcm", { path: fixtures.long.path, sampleRate: 44100 });
        await sleep(50);
        const busy = (await debugState(bridge))?.decode;
        await popupEvents.stop();
        await closeClient(popup.client);
        const closed = await bridge.invoke("window.closePopup", { windowId: popup.popupId });
        popupClosed = true;
        const idle = (await waitIdle(bridge))?.decode;
        await collector.waitFor(null, { timeoutMs: QUIET_MS });
        const strays = eventsOf(collector, theirs?.taskId);
        recorder.assertCase(
            "PC-12 (D15, §4.3) closing a popup drops the task it started: the decode is aborted without any event, and the host returns to idle with no open buffer",
            theirs?.status === "pending" &&
                busy?.active === 1 &&
                closed?.success === true &&
                idle?.active === 0 &&
                idle.openBufferBytes === 0 &&
                strays.length === 0,
            { theirs, busy, closed, idle, strays },
            { busy: { active: 1 }, idle: { active: 0, openBufferBytes: 0 }, events: 0 },
        );
    } finally {
        if (!popupClosed) {
            await closeClient(popup.client).catch(() => undefined);
            await bridge.invokeRaw("window.closePopup", { windowId: popup.popupId }).catch(() => undefined);
        }
    }
}

// ---------------------------------------------------------------------------
// Entry

const recorder = createRecorder();
let client;
let bridge;
let blocked = false;
let fatalError;
let targets;
const entry = {};

try {
    const connection = await connectBridgePage();
    client = connection.client;
    targets = connection.candidates;
    console.log(`Target: ${connection.page.title} ${connection.page.url}`);
    bridge = createBridge(client.Runtime, { invokeTimeoutMs });
    await requireResponsiveBridge(bridge, { probeId: "audio-pcm" });

    const fixtures = ensureFixtures();
    entry.fixtures = Object.fromEntries(Object.entries(fixtures).map(([k, v]) => [k, { path: v.path, reused: v.reused }]));
    const state = await runDebugStateCases(bridge, recorder);
    if (!state?.runtime?.environment12 || !state.runtime.webview17) {
        throw blockedError("this WebView2 runtime cannot share buffers with pages", { runtime: state?.runtime });
    }
    const idle = await waitIdle(bridge, 5000);
    if (idle?.decode?.active !== 0 || idle.decode.queued !== 0) {
        throw blockedError("another caller is decoding; the queue cases need an idle host", { decode: idle?.decode });
    }

    await installBufferStore(bridge);
    const collector = await createEventCollector(bridge, EVENTS, { collectorId: `pcm-${runId}` });
    try {
        await runDecodeCases(bridge, recorder, collector, fixtures);
        await runRefusalCases(bridge, recorder, fixtures);
        await runCancelCases(bridge, recorder, collector, fixtures);
        await runQueueCases(bridge, recorder, collector, fixtures);
        await runCrossPageCases(bridge, recorder, collector, fixtures);
    } finally {
        await collector.stop();
    }
} catch (error) {
    fatalError = error;
    blocked = Boolean(error?.blocked);
    console.error(`${blocked ? "BLOCKED" : "ERROR"}: ${error?.message || error}`);
} finally {
    if (bridge) {
        // Releases what a failed case left of this run's buffers; other callers' are only dropped from the store.
        await bridge
            .evaluateValue(
                `(() => {
                    const store = window[${JSON.stringify(storeKey)}];
                    if (!store) return 0;
                    const mine = store.entries.filter((entry) => store.claimed.has(entry.taskId));
                    for (const entry of mine) { try { window.chrome.webview.releaseBuffer(entry.buffer); } catch {} }
                    store.entries = [];
                    store.claimed.clear();
                    return mine.length;
                })()`,
                false,
                3000,
            )
            .catch(() => undefined);
    }
    await closeClient(client);
}

process.exit(
    report({
        recorder,
        blocked,
        fatalError,
        extra: {
            targetPort: resolvePort(),
            invokeTimeoutMs,
            entryState: entry,
            targets,
        },
    }),
);

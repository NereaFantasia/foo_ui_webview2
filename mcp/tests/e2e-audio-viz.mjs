/**
 * Covers the spectrum side of audio.*: subscribeSpectrum, unsubscribeSpectrum,
 * getSpectrum and getSpectrumDebugState, against the host contract in
 * docs/audio-visualization/SPEC.md (§4.1–§4.4; acceptance A1.1–A1.5, A1.7,
 * A1.9, A3.1, A3.1b, A3.2, A6.5, A6.6 in §10), full-track waveforms: the cache of
 * generateFullWaveform, cancelFullWaveform and the decode queue (§4.5, §4.6;
 * A2.1, A2.2, A2.4-A2.6), and the live getWaveform (§4.9; A4.1, A4.2, A4.4).
 * The playback tier also checks where the spectrum and getWaveform windows sit
 * in time against docs/audio-timing/SPEC.md (T8; acceptance A-V1, A-V1b, A-V3,
 * A-V6 in §10).
 *
 * The suite has two tiers.
 *
 * The contract tier always runs and never changes playback: parameter
 * refusals, clamping, replacement of a same-id subscription, the debug state,
 * getSpectrum's failure envelopes, and the frame or silence-frame semantics in
 * whatever playback state the instance is in when the suite attaches (the
 * assertion is chosen per entry state, the same way selection-context reads its
 * oracle at the moment of the call). Full-track waveforms decode the fixture
 * files without playing them, so they belong here too; they write the fixtures
 * on first use, and an unwritable fixture directory skips those cases rather
 * than blocking the tier. The queue cases decode ten local FLAC tracks picked
 * from the media library, long enough that a decode outlasts a cancel, and are
 * skipped when the library has fewer.
 *
 * The playback tier runs only with FB2K_E2E_AUDIO_VIZ_PLAYBACK=1, because it
 * stops whatever is playing, plays generated sine fixtures (muted - the
 * visualisation stream sits before the volume control, so readings are not
 * affected) and leaves the instance stopped. It owns a scratch playlist so the
 * user's lists are untouched, restores the active playlist and the mute state,
 * and unsubscribes only by id: an id-less unsubscribe would also remove the
 * theme's own subscription.
 *
 * Fixtures are 48 kHz 16-bit WAVs written by this suite into
 * FB2K_E2E_FIXTURE_DIR (default E:\FB2K\e2e-fixtures\audio-viz): mono 1 kHz
 * sines at 0 dBFS and -20 dBFS, and a stereo one with a 0 dBFS 1 kHz sine on
 * the left and silence on the right for the getWaveform stereo case. The timing
 * cases play a stepped fixture at 44.1 kHz and at 48 kHz whose tone alternates
 * between 500 Hz and 2000 Hz every two seconds, so a boundary between the two
 * tones marks a known track time. They live outside the monitored library
 * folders so they are not auto-added to the media library.
 *
 * Every frame case listens on a suite-owned event name, so frames of the
 * theme's own subscription never reach these assertions and the theme never
 * sees ours.
 *
 * Usage: node mcp/tests/e2e-audio-viz.mjs
 * Environment: FB2K_CDP_PORT, FB2K_E2E_TIMEOUT_MS, FB2K_E2E_AUDIO_VIZ_PLAYBACK,
 * FB2K_E2E_FIXTURE_DIR.
 */

import { existsSync, mkdirSync, statSync, writeFileSync } from "node:fs";
import { join } from "node:path";

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
const PLAYBACK_TIER = process.env.FB2K_E2E_AUDIO_VIZ_PLAYBACK === "1";
const FIXTURE_DIR = process.env.FB2K_E2E_FIXTURE_DIR || "E:\\FB2K\\e2e-fixtures\\audio-viz";
const SCRATCH_PLAYLIST = "e2e-audio-viz";

/** Silence values per scale, as the host fills a paused / stopped frame. */
const SILENCE = { weighted: 0, db: -160 };
/** The visualisation stream takes about 0.7 s to produce its first frame. */
const FIRST_FRAME_MS = 2000;
/** Frames the host must not send after a silence frame; one second per A1.7. */
const QUIET_MS = 1000;

const runId = Date.now().toString(36);
const subId = (name) => `e2e-av-${name}-${runId}`;
const eventName = (name) => `audio:e2eViz${name}`;

const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
const isSilence = (spectrum, scale) =>
    Array.isArray(spectrum) && spectrum.every((v) => v === SILENCE[scale]);
const arraysEqual = (a, b) =>
    Array.isArray(a) && Array.isArray(b) && a.length === b.length && a.every((v, i) => v === b[i]);
// Rules the handler checks itself (a power-of-two FFT size, key combinations, a
// maxFrequency above minFrequency, a duration above 0) name the key in details.
const isInvalidParams = (res, param) =>
    res?.success === false &&
    res.code === "INVALID_PARAMS" &&
    res.details?.param === param &&
    typeof res.error === "string";
// The generated parameter reader refuses a wrong type, a missing key, a value out of
// the declared range or outside the declared set with a message that starts with the
// key, and attaches no details.
const isRefusedByReader = (res, param) =>
    res?.success === false &&
    res.code === "INVALID_PARAMS" &&
    typeof res.error === "string" &&
    res.error.startsWith(`${param} `);

function median(values) {
    const sorted = [...values].sort((a, b) => a - b);
    const mid = Math.floor(sorted.length / 2);
    return sorted.length % 2 ? sorted[mid] : (sorted[mid - 1] + sorted[mid]) / 2;
}

/** Log-spaced band edges over [minHz, maxHz], as the host divides a frequency range. */
function rangeBandEdges(minHz, maxHz, bands) {
    const logMin = Math.log10(minHz);
    const logSpan = Math.log10(maxHz) - logMin;
    return Array.from({ length: bands + 1 }, (_, b) => 10 ** (logMin + (logSpan * b) / bands));
}

/** Band edges of the default range, 20 Hz to sampleRate / 2. */
const bandEdges = (sampleRate, bands) => rangeBandEdges(20, sampleRate / 2, bands);

function bandOf(edges, hz) {
    let b = 0;
    while (b + 1 < edges.length - 1 && hz >= edges[b + 1]) b += 1;
    return b;
}

/** Sum of dB readings in the power domain, back in dB. */
function sumDb(values) {
    return 10 * Math.log10(values.reduce((acc, v) => acc + 10 ** (v / 10), 0));
}

function indexOfMax(values) {
    let best = 0;
    for (let i = 1; i < values.length; i += 1) if (values[i] > values[best]) best = i;
    return best;
}

/** Frames per second over the window that starts at the first frame. */
function rateFrom(frames, windowMs) {
    if (frames.length === 0) return { rate: 0, counted: 0 };
    const start = frames[0].at;
    const counted = frames.filter((f) => f.at >= start && f.at < start + windowMs).length;
    return { rate: (counted * 1000) / windowMs, counted };
}

/** Nearest-rank percentile; null for an empty list. */
function percentile(values, p) {
    if (values.length === 0) return null;
    const sorted = [...values].sort((a, b) => a - b);
    return sorted[Math.min(sorted.length - 1, Math.max(0, Math.ceil((p / 100) * sorted.length) - 1))];
}

// ---------------------------------------------------------------------------
// Fixtures

function writeSineWav(path, { sampleRate, seconds, hz, amplitude }) {
    const frames = sampleRate * seconds;
    const dataBytes = frames * 2;
    const buf = Buffer.alloc(44 + dataBytes);
    buf.write("RIFF", 0);
    buf.writeUInt32LE(36 + dataBytes, 4);
    buf.write("WAVE", 8);
    buf.write("fmt ", 12);
    buf.writeUInt32LE(16, 16);
    buf.writeUInt16LE(1, 20); // PCM
    buf.writeUInt16LE(1, 22); // mono
    buf.writeUInt32LE(sampleRate, 24);
    buf.writeUInt32LE(sampleRate * 2, 28);
    buf.writeUInt16LE(2, 32);
    buf.writeUInt16LE(16, 34);
    buf.write("data", 36);
    buf.writeUInt32LE(dataBytes, 40);
    for (let i = 0; i < frames; i += 1) {
        const v = Math.round(amplitude * Math.sin((2 * Math.PI * hz * i) / sampleRate));
        buf.writeInt16LE(Math.max(-32768, Math.min(32767, v)), 44 + i * 2);
    }
    writeFileSync(path, buf);
    return buf.length;
}

/** A stereo sine with the tone on the left channel and silence on the right. */
function writeStereoSineWav(path, { sampleRate, seconds, hz, amplitude }) {
    const frames = sampleRate * seconds;
    const dataBytes = frames * 4; // two 16-bit channels
    const buf = Buffer.alloc(44 + dataBytes);
    buf.write("RIFF", 0);
    buf.writeUInt32LE(36 + dataBytes, 4);
    buf.write("WAVE", 8);
    buf.write("fmt ", 12);
    buf.writeUInt32LE(16, 16);
    buf.writeUInt16LE(1, 20); // PCM
    buf.writeUInt16LE(2, 22); // stereo
    buf.writeUInt32LE(sampleRate, 24);
    buf.writeUInt32LE(sampleRate * 4, 28);
    buf.writeUInt16LE(4, 32);
    buf.writeUInt16LE(16, 34);
    buf.write("data", 36);
    buf.writeUInt32LE(dataBytes, 40);
    for (let i = 0; i < frames; i += 1) {
        const v = Math.round(amplitude * Math.sin((2 * Math.PI * hz * i) / sampleRate));
        buf.writeInt16LE(Math.max(-32768, Math.min(32767, v)), 44 + i * 4);
        buf.writeInt16LE(0, 44 + i * 4 + 2);
    }
    writeFileSync(path, buf);
    return buf.length;
}

/** Tones of the stepped fixture: it starts on the first and alternates every STEP_SECONDS. */
const STEP_HZ = [500, 2000];
const STEP_SECONDS = 2;
const STEPPED_SECONDS = 32;
/** -6 dBFS, so neither tone clips. */
const STEP_AMPLITUDE = 16384;

/**
 * A mono tone that alternates between STEP_HZ every STEP_SECONDS. Each step
 * restarts the phase at zero; both tones complete whole cycles in a step, so
 * the signal stays continuous and every boundary is a rising zero crossing.
 */
function writeSteppedSineWav(path, { sampleRate, seconds }) {
    const frames = sampleRate * seconds;
    const stepFrames = sampleRate * STEP_SECONDS;
    const dataBytes = frames * 2;
    const buf = Buffer.alloc(44 + dataBytes);
    buf.write("RIFF", 0);
    buf.writeUInt32LE(36 + dataBytes, 4);
    buf.write("WAVE", 8);
    buf.write("fmt ", 12);
    buf.writeUInt32LE(16, 16);
    buf.writeUInt16LE(1, 20); // PCM
    buf.writeUInt16LE(1, 22); // mono
    buf.writeUInt32LE(sampleRate, 24);
    buf.writeUInt32LE(sampleRate * 2, 28);
    buf.writeUInt16LE(2, 32);
    buf.writeUInt16LE(16, 34);
    buf.write("data", 36);
    buf.writeUInt32LE(dataBytes, 40);
    for (let i = 0; i < frames; i += 1) {
        const step = Math.floor(i / stepFrames);
        const hz = STEP_HZ[step % 2];
        const v = Math.round(STEP_AMPLITUDE * Math.sin((2 * Math.PI * hz * (i - step * stepFrames)) / sampleRate));
        buf.writeInt16LE(v, 44 + i * 2);
    }
    writeFileSync(path, buf);
    return buf.length;
}

/**
 * Writes the fixtures unless files of the expected size already exist. The
 * sines run twenty seconds: the longest timed window on them is ten seconds
 * and every playback phase restarts its fixture, so a track never ends inside
 * a window. The stepped fixtures run 32 seconds for the 30 s window of the
 * long-window case, which seeks to 20 s partway through.
 */
function ensureFixtures() {
    const sampleRate = 48000;
    const seconds = 20;
    try {
        mkdirSync(FIXTURE_DIR, { recursive: true });
    } catch (error) {
        throw blockedError(`fixture directory is not writable: ${FIXTURE_DIR}`, { error: String(error?.message ?? error) });
    }
    const specs = {
        full: { file: "av_sine_1k_0dBFS_48k.wav", hz: 1000, amplitude: 32767, dbfs: 0, channels: 1 },
        minus20: { file: "av_sine_1k_-20dBFS_48k.wav", hz: 1000, amplitude: 3277, dbfs: -20, channels: 1 },
        stereo: { file: "av_sine_1k_left_0dBFS_48k_stereo.wav", hz: 1000, amplitude: 32767, dbfs: 0, channels: 2 },
        stepped44: { file: "av_step_500_2000_44k1.wav", sampleRate: 44100, seconds: STEPPED_SECONDS, dbfs: -6, channels: 1, write: writeSteppedSineWav },
        stepped48: { file: "av_step_500_2000_48k.wav", sampleRate: 48000, seconds: STEPPED_SECONDS, dbfs: -6, channels: 1, write: writeSteppedSineWav },
    };
    const out = {};
    for (const [key, spec] of Object.entries(specs)) {
        const path = join(FIXTURE_DIR, spec.file);
        const rate = spec.sampleRate ?? sampleRate;
        const length = spec.seconds ?? seconds;
        const expectedBytes = 44 + rate * length * 2 * spec.channels;
        const reused = existsSync(path) && statSync(path).size === expectedBytes;
        if (!reused) {
            try {
                const write = spec.write ?? (spec.channels === 2 ? writeStereoSineWav : writeSineWav);
                write(path, { sampleRate: rate, seconds: length, hz: spec.hz, amplitude: spec.amplitude });
            } catch (error) {
                throw blockedError(`fixture is not writable: ${path}`, { error: String(error?.message ?? error) });
            }
        }
        out[key] = { path, hz: spec.hz, dbfs: spec.dbfs, sampleRate: rate, seconds: length, channels: spec.channels, reused };
    }
    return out;
}

// ---------------------------------------------------------------------------
// Bridge helpers

/** Every subscription the suite creates, so teardown can remove exactly those. */
const ownedSubscriptions = new Set();

async function subscribe(bridge, name, params) {
    const subscriptionId = subId(name);
    ownedSubscriptions.add(subscriptionId);
    const res = await bridge.invoke("audio.subscribeSpectrum", {
        subscriptionId,
        event: eventName(name),
        ...params,
    });
    return { subscriptionId, event: eventName(name), res };
}

async function unsubscribe(bridge, subscriptionId) {
    const res = await bridge.invoke("audio.unsubscribeSpectrum", { subscriptionId });
    ownedSubscriptions.delete(subscriptionId);
    return res;
}

const debugState = (bridge) => bridge.invoke("audio.getSpectrumDebugState", {});
const playbackState = async (bridge) => (await bridge.invoke("playback.getState", {})).state;

/** Frames of one event, in arrival order. */
const framesOf = (received, event) => received.filter((e) => e.name === event);

// ---------------------------------------------------------------------------
// Contract tier

async function runRefusalCases(bridge, recorder) {
    const { invoke } = bridge;
    const before = await debugState(bridge);

    const fresh = (extra) => ({ subscriptionId: subId("refused"), event: eventName("Refused"), ...extra });
    const [fft1000, fft131072, scaleFoo, scale42, throttleNo] = await Promise.all([
        invoke("audio.subscribeSpectrum", fresh({ fftSize: 1000 })),
        invoke("audio.subscribeSpectrum", fresh({ fftSize: 131072 })),
        invoke("audio.subscribeSpectrum", fresh({ scale: "foo" })),
        invoke("audio.subscribeSpectrum", fresh({ scale: 42 })),
        invoke("audio.subscribeSpectrum", fresh({ backgroundThrottle: "no" })),
    ]);
    const after = await debugState(bridge);

    recorder.assertCase(
        "AV-01 (A1.1) fftSize 1000 and 131072, scale 'foo' and 42, backgroundThrottle 'no' each answer INVALID_PARAMS naming the parameter, and none of them registered a subscription",
        isInvalidParams(fft1000, "fftSize") &&
            fft1000.details.value === 1000 &&
            isRefusedByReader(fft131072, "fftSize") &&
            isRefusedByReader(scaleFoo, "scale") &&
            scaleFoo.error.includes("'foo'") &&
            isRefusedByReader(scale42, "scale") &&
            isRefusedByReader(throttleNo, "backgroundThrottle") &&
            after.subscriptionCount === before.subscriptionCount,
        {
            fft1000,
            fft131072,
            scaleFoo,
            scale42,
            throttleNo,
            subscriptionCount: { before: before.subscriptionCount, after: after.subscriptionCount },
        },
        {
            fft1000: { success: false, code: "INVALID_PARAMS", details: { param: "fftSize", value: 1000 } },
            others: { success: false, code: "INVALID_PARAMS", error: "starts with the offending key" },
            subscriptionCount: "unchanged",
        },
    );
    return before;
}

async function runRangeRefusalCases(bridge, recorder) {
    const { invoke } = bridge;
    const before = await debugState(bridge);

    const fresh = (extra) => ({ subscriptionId: subId("rangeRefused"), event: eventName("RangeRefused"), ...extra });
    const [min0, minNeg, minStr, maxAtMin, maxEqual, maxStr] = await Promise.all([
        invoke("audio.subscribeSpectrum", fresh({ minFrequency: 0 })),
        invoke("audio.subscribeSpectrum", fresh({ minFrequency: -1 })),
        invoke("audio.subscribeSpectrum", fresh({ minFrequency: "20" })),
        // Not above the default minFrequency of 20.
        invoke("audio.subscribeSpectrum", fresh({ maxFrequency: 20 })),
        invoke("audio.subscribeSpectrum", fresh({ minFrequency: 500, maxFrequency: 500 })),
        invoke("audio.subscribeSpectrum", fresh({ maxFrequency: "x" })),
    ]);
    // Without a subscriptionId, getSpectrum validates the range the same way,
    // and does so before it looks for subscriptions.
    const [pollMin0, pollMaxStr, pollInverted] = await Promise.all([
        invoke("audio.getSpectrum", { minFrequency: 0 }),
        invoke("audio.getSpectrum", { maxFrequency: "x" }),
        invoke("audio.getSpectrum", { minFrequency: 500, maxFrequency: 400 }),
    ]);
    const after = await debugState(bridge);

    recorder.assertCase(
        "AV-25 (A6.6) minFrequency 0, -1 and '20', maxFrequency 20 (not above the default minFrequency), 500 with minFrequency 500 and 'x' each answer INVALID_PARAMS naming the parameter and register nothing; getSpectrum without a subscriptionId refuses minFrequency 0, maxFrequency 'x' and a maxFrequency below minFrequency the same way",
        isRefusedByReader(min0, "minFrequency") &&
            isRefusedByReader(minNeg, "minFrequency") &&
            isRefusedByReader(minStr, "minFrequency") &&
            isInvalidParams(maxAtMin, "maxFrequency") &&
            maxAtMin.details.value === 20 &&
            isInvalidParams(maxEqual, "maxFrequency") &&
            isRefusedByReader(maxStr, "maxFrequency") &&
            isRefusedByReader(pollMin0, "minFrequency") &&
            isRefusedByReader(pollMaxStr, "maxFrequency") &&
            isInvalidParams(pollInverted, "maxFrequency") &&
            pollInverted.details.value === 400 &&
            after.subscriptionCount === before.subscriptionCount,
        {
            subscribe: { min0, minNeg, minStr, maxAtMin, maxEqual, maxStr },
            getSpectrum: { pollMin0, pollMaxStr, pollInverted },
            subscriptionCount: { before: before.subscriptionCount, after: after.subscriptionCount },
        },
        {
            notAboveMin: { success: false, code: "INVALID_PARAMS", details: { param: "maxFrequency", value: "as sent" } },
            others: { success: false, code: "INVALID_PARAMS", error: "starts with minFrequency or maxFrequency" },
            subscriptionCount: "unchanged",
        },
    );
}

async function runRangeRegistrationCases(bridge, recorder, entryPlayback) {
    const narrow = await subscribe(bridge, "RangeNarrow", {
        fftSize: 2048,
        bands: 32,
        fps: 10,
        scale: "db",
        minFrequency: 500,
        maxFrequency: 2000,
    });
    const plain = await subscribe(bridge, "RangePlain", { fftSize: 1024, bands: 16, fps: 10 });
    const debug = await debugState(bridge);
    const entryNarrow = debug.subscriptions.find((s) => s.token === narrow.subscriptionId);
    const entryPlain = debug.subscriptions.find((s) => s.token === plain.subscriptionId);

    // The frame reports the range actually used: the subscription's
    // minFrequency, and its maxFrequency capped at sampleRate / 2. A silence
    // frame of a subscription that has never produced a frame knows no sample
    // rate, so its upper edge reads 0.
    const playing = entryPlayback === "playing";
    const frame = await pollFrame(bridge, { subscriptionId: narrow.subscriptionId }, playing ? FIRST_FRAME_MS : 0);
    const frameOk =
        frame?.success === true &&
        frame.subscriptionId === narrow.subscriptionId &&
        frame.minFrequency === 500 &&
        (playing ? frame.maxFrequency === 2000 && frame.sampleRate >= 4000 : frame.maxFrequency === 0 && frame.sampleRate === 0);

    recorder.assertCase(
        "AV-26 (§4.1, §4.2, §4.4) a registration echoes the requested minFrequency and maxFrequency and answers maxFrequency null when none was given, getSpectrumDebugState lists both on each subscription, and a frame of the ranged subscription reports minFrequency 500 with maxFrequency 2000 while playing or 0 while its sample rate is unknown",
        narrow.res?.success === true &&
            narrow.res.minFrequency === 500 &&
            narrow.res.maxFrequency === 2000 &&
            plain.res?.success === true &&
            plain.res.minFrequency === 20 &&
            plain.res.maxFrequency === null &&
            entryNarrow?.minFrequency === 500 &&
            entryNarrow.maxFrequency === 2000 &&
            entryPlain?.minFrequency === 20 &&
            entryPlain.maxFrequency === null &&
            frameOk,
        {
            entryPlayback,
            narrow: { response: narrow.res, debug: entryNarrow },
            plain: { response: plain.res, debug: entryPlain },
            frame: frame && { ...frame, spectrum: `${frame.spectrum?.length} values` },
        },
        {
            narrow: { minFrequency: 500, maxFrequency: 2000 },
            plain: { minFrequency: 20, maxFrequency: null },
            frame: { minFrequency: 500, maxFrequency: playing ? 2000 : 0 },
        },
    );
}

// Bin output (docs/audio-visualization/SPECTRUM_BINS_SPEC.md A-B6): parameter
// refusals, the registration echo, the debug state and the shape of a frame.
async function runBinsContractCases(bridge, recorder, entryPlayback) {
    const { invoke } = bridge;
    const before = await debugState(bridge);

    // A host without bin output ignores the new keys and registers these; they
    // are removed right after the assertion so the count checks after it hold.
    const refusedId = subId("binsRefused");
    ownedSubscriptions.add(refusedId);
    const fresh = (extra) => ({ subscriptionId: refusedId, event: eventName("BinsRefused"), ...extra });
    const [outputFoo, output42, channelsFoo, stereoBands, binsWeighted] = await Promise.all([
        invoke("audio.subscribeSpectrum", fresh({ output: "foo" })),
        invoke("audio.subscribeSpectrum", fresh({ output: 42 })),
        invoke("audio.subscribeSpectrum", fresh({ output: "bins", channels: "foo" })),
        invoke("audio.subscribeSpectrum", fresh({ channels: "stereo" })),
        invoke("audio.subscribeSpectrum", fresh({ output: "bins", scale: "weighted" })),
    ]);
    // Without a subscriptionId, getSpectrum takes its FFT size from the other
    // subscriptions, so it accepts band output only.
    const [pollBins, pollStereo] = await Promise.all([
        invoke("audio.getSpectrum", { output: "bins" }),
        invoke("audio.getSpectrum", { channels: "stereo" }),
    ]);
    const after = await debugState(bridge);

    recorder.assertCase(
        "AV-39 (spectrum bins A-B6) output 'foo' and 42, channels 'foo', channels 'stereo' with band output and scale 'weighted' with bin output each answer INVALID_PARAMS naming the parameter and register nothing; getSpectrum without a subscriptionId refuses output 'bins' and channels 'stereo' the same way",
        isRefusedByReader(outputFoo, "output") &&
            outputFoo.error.includes("'foo'") &&
            isRefusedByReader(output42, "output") &&
            isRefusedByReader(channelsFoo, "channels") &&
            channelsFoo.error.includes("'foo'") &&
            isInvalidParams(stereoBands, "channels") &&
            stereoBands.details.value === "stereo" &&
            isInvalidParams(binsWeighted, "scale") &&
            binsWeighted.details.value === "weighted" &&
            isInvalidParams(pollBins, "output") &&
            isInvalidParams(pollStereo, "channels") &&
            after.subscriptionCount === before.subscriptionCount,
        {
            subscribe: { outputFoo, output42, channelsFoo, stereoBands, binsWeighted },
            getSpectrum: { pollBins, pollStereo },
            subscriptionCount: { before: before.subscriptionCount, after: after.subscriptionCount },
        },
        {
            combinations: { success: false, code: "INVALID_PARAMS", details: { param: "channels, scale or output", value: "as sent" } },
            unknownValues: { success: false, code: "INVALID_PARAMS", error: "starts with output or channels" },
            subscriptionCount: "unchanged",
        },
    );
    await unsubscribe(bridge, refusedId);

    const bins = await subscribe(bridge, "BinsShape", { output: "bins", channels: "stereo", fftSize: 4096, bands: 1024, fps: 10 });
    const plain = await subscribe(bridge, "BinsPlain", { fftSize: 1024, bands: 16, fps: 10 });
    try {
        const debug = await debugState(bridge);
        const entryBins = debug.subscriptions.find((sub) => sub.token === bins.subscriptionId);
        const entryPlain = debug.subscriptions.find((sub) => sub.token === plain.subscriptionId);

        // While playing the frame is computed; otherwise it is a silence frame of
        // a subscription that has never produced a frame, so its arrays are empty.
        const playing = entryPlayback === "playing";
        const frame = await pollFrame(bridge, { subscriptionId: bins.subscriptionId }, playing ? FIRST_FRAME_MS : 0);
        const frameOk =
            frame?.success === true &&
            frame.output === "bins" &&
            frame.channels === "stereo" &&
            frame.scale === "db" &&
            frame.fftSize === 4096 &&
            !("bands" in frame) &&
            !("spectrum" in frame) &&
            Array.isArray(frame.left) &&
            Array.isArray(frame.right) &&
            frame.left.length === frame.right.length &&
            (playing
                ? frame.firstBin >= 1 && frame.left.length > 0 && frame.channelCount >= 1
                : frame.firstBin === 0 && frame.left.length === 0 && frame.channelCount === 0);

        recorder.assertCase(
            "AV-40 (spectrum bins A-B6) a bin registration echoes output 'bins', channels 'stereo' and scale 'db', a default one echoes 'bands' and 'mix', getSpectrumDebugState lists both keys on each subscription, and a frame of the bin subscription carries left, right, firstBin, channelCount and the requested fftSize 4096 without bands or spectrum: filled while playing, empty while its sample rate is unknown",
            bins.res?.success === true &&
                bins.res.output === "bins" &&
                bins.res.channels === "stereo" &&
                bins.res.scale === "db" &&
                plain.res?.success === true &&
                plain.res.output === "bands" &&
                plain.res.channels === "mix" &&
                entryBins?.output === "bins" &&
                entryBins.channels === "stereo" &&
                entryPlain?.output === "bands" &&
                entryPlain.channels === "mix" &&
                frameOk,
            {
                entryPlayback,
                bins: { response: bins.res, debug: entryBins },
                plain: { response: plain.res, debug: entryPlain },
                frame: frame && { ...frame, left: `${frame.left?.length} values`, right: `${frame.right?.length} values` },
            },
            {
                bins: { output: "bins", channels: "stereo", scale: "db" },
                plain: { output: "bands", channels: "mix" },
                frame: { output: "bins", fftSize: 4096, bands: "absent", spectrum: "absent", left: playing ? "filled" : "empty" },
            },
        );
    } finally {
        await unsubscribe(bridge, bins.subscriptionId);
        await unsubscribe(bridge, plain.subscriptionId);
    }
}

async function runRegistrationCases(bridge, recorder, entryDebug) {
    const { invoke } = bridge;

    const clamp = await subscribe(bridge, "Clamp", { fftSize: 256, bands: 100000, fps: 999 });
    const low = await subscribe(bridge, "Low", { fftSize: 1024, bands: 4, fps: 0 });
    recorder.assertCase(
        "AV-02 (A1.2, §4.1) a registration answers success true with a boolean streamReady, echoes the requested fftSize, clamps bands to 8..fftSize/2 and fps to 1..60, and defaults scale to weighted and backgroundThrottle to true",
        clamp.res?.success === true &&
            typeof clamp.res.streamReady === "boolean" &&
            clamp.res.subscriptionId === clamp.subscriptionId &&
            clamp.res.event === clamp.event &&
            clamp.res.fftSize === 256 &&
            clamp.res.bands === 128 &&
            clamp.res.fps === 60 &&
            clamp.res.scale === "weighted" &&
            clamp.res.backgroundThrottle === true &&
            low.res?.success === true &&
            low.res.bands === 8 &&
            low.res.fps === 1 &&
            !("error" in clamp.res) &&
            !("code" in clamp.res),
        { clamp: clamp.res, low: low.res },
        {
            clamp: { success: true, fftSize: 256, bands: 128, fps: 60, scale: "weighted", backgroundThrottle: true, streamReady: "boolean" },
            low: { bands: 8, fps: 1 },
        },
    );

    // Same id again: replaced, not duplicated. Debug state shows the new values.
    const replaced = await invoke("audio.subscribeSpectrum", {
        subscriptionId: clamp.subscriptionId,
        event: clamp.event,
        fftSize: 2048,
        bands: 32,
        fps: 10,
        scale: "db",
        backgroundThrottle: false,
    });
    const debug = await debugState(bridge);
    const entry = debug.subscriptions.find((s) => s.token === clamp.subscriptionId);
    recorder.assertCase(
        "AV-03 (§4.1, §4.4) subscribing again with the same id replaces that subscription, and getSpectrumDebugState lists it once with its event, scale and backgroundThrottle, reports the largest requested values as effective* and a numeric framesComputed",
        replaced?.success === true &&
            replaced.scale === "db" &&
            replaced.backgroundThrottle === false &&
            debug.subscriptionCount === entryDebug.subscriptionCount + 2 &&
            debug.subscriptions.filter((s) => s.token === clamp.subscriptionId).length === 1 &&
            entry?.event === clamp.event &&
            entry.fftSize === 2048 &&
            entry.bands === 32 &&
            entry.fps === 10 &&
            entry.scale === "db" &&
            entry.backgroundThrottle === false &&
            debug.effectiveFftSize >= 2048 &&
            debug.effectiveBands >= 32 &&
            debug.effectiveFps >= 10 &&
            Number.isInteger(debug.framesComputed) &&
            debug.framesComputed >= 0 &&
            debug.streamReady === true,
        {
            replaced,
            subscriptionCount: { entry: entryDebug.subscriptionCount, now: debug.subscriptionCount },
            entry,
            effective: { fftSize: debug.effectiveFftSize, bands: debug.effectiveBands, fps: debug.effectiveFps },
            framesComputed: debug.framesComputed,
            streamReady: debug.streamReady,
        },
        {
            subscriptionCount: "entry + 2",
            entry: { event: clamp.event, fftSize: 2048, bands: 32, fps: 10, scale: "db", backgroundThrottle: false },
            effective: "at least this subscription's values",
            streamReady: true,
        },
    );

    return { clamp, low };
}

async function runGetSpectrumRefusalCases(bridge, recorder) {
    const { invoke } = bridge;
    const [unknown, badScale, badId] = await Promise.all([
        invoke("audio.getSpectrum", { subscriptionId: subId("nobody") }),
        invoke("audio.getSpectrum", { scale: "loud" }),
        invoke("audio.getSpectrum", { subscriptionId: 7 }),
    ]);
    recorder.assertCase(
        "AV-04 (§4.3) getSpectrum answers NOT_FOUND for an unknown subscriptionId, INVALID_PARAMS for a scale other than weighted / db and for a non-string subscriptionId",
        unknown?.success === false &&
            unknown.code === "NOT_FOUND" &&
            unknown.details?.param === "subscriptionId" &&
            isRefusedByReader(badScale, "scale") &&
            badScale.error.includes("'loud'") &&
            isRefusedByReader(badId, "subscriptionId"),
        { unknown, badScale, badId },
        {
            unknown: { success: false, code: "NOT_FOUND" },
            badScale: { success: false, code: "INVALID_PARAMS", error: "starts with scale" },
            badId: { success: false, code: "INVALID_PARAMS", error: "starts with subscriptionId" },
        },
    );
}

/** Polls getSpectrum until it succeeds or the deadline passes (stream warm-up). */
async function pollFrame(bridge, params, timeoutMs) {
    const deadline = Date.now() + timeoutMs;
    let last;
    for (;;) {
        last = await bridge.invoke("audio.getSpectrum", params);
        if (last?.success === true || Date.now() >= deadline) return last;
        await sleep(100);
    }
}

async function runFrameShapeCases(bridge, recorder, entryPlayback) {
    const weighted = await subscribe(bridge, "ShapeW", { fftSize: 1024, bands: 16, fps: 30 });
    const db = await subscribe(bridge, "ShapeD", { fftSize: 1024, bands: 16, fps: 30, scale: "db" });
    const playing = entryPlayback === "playing";

    const [fw, fd] = await Promise.all([
        pollFrame(bridge, { subscriptionId: weighted.subscriptionId }, playing ? FIRST_FRAME_MS : 0),
        pollFrame(bridge, { subscriptionId: db.subscriptionId }, playing ? FIRST_FRAME_MS : 0),
    ]);
    const shapeOk = (frame, scale) =>
        frame?.success === true &&
        frame.subscriptionId === (scale === "db" ? db : weighted).subscriptionId &&
        frame.bands === 16 &&
        Array.isArray(frame.spectrum) &&
        frame.spectrum.length === 16 &&
        frame.fftSize === 1024 &&
        frame.scale === scale &&
        frame.minFrequency === 20 &&
        frame.state === entryPlayback &&
        typeof frame.streamTime === "number" &&
        typeof frame.hostTime === "number" &&
        typeof frame.sampleRate === "number";
    const valuesOk = playing
        ? fw?.spectrum?.every((v) => v >= 0 && v <= 1) &&
          fd?.spectrum?.every((v) => v >= -160 && v <= 10) &&
          fw?.sampleRate > 0 &&
          fw.maxFrequency === Math.floor(fw.sampleRate / 2)
        : isSilence(fw?.spectrum, "weighted") && isSilence(fd?.spectrum, "db");
    recorder.assertCase(
        playing
            ? "AV-05 (§4.2, §4.3) getSpectrum with a subscriptionId answers that subscription's frame - its id, 16 bands, the effective fftSize, its scale, state playing, values in range for each scale and maxFrequency of sampleRate / 2"
            : "AV-05 (§4.2, §4.3) getSpectrum with a subscriptionId while not playing answers a silence frame for that subscription - its id, 16 bands, the effective fftSize, its scale, the current state, all 0 for weighted and all -160 for db",
        shapeOk(fw, "weighted") && shapeOk(fd, "db") && valuesOk,
        { entryPlayback, weighted: fw, db: fd },
        {
            frame: { bands: 16, spectrumLength: 16, fftSize: 1024, minFrequency: 20, state: entryPlayback },
            values: playing ? "weighted in [0, 1], db in [-160, 10]" : "silence per scale",
        },
    );

    // Without a subscriptionId the frame carries no such field; bands 0 takes
    // the largest band count among all subscriptions (ours are 16 and 32 here,
    // the theme's may be larger), an explicit bands is honoured.
    const [aggregate, twelve] = await Promise.all([
        pollFrame(bridge, { bands: 0 }, playing ? FIRST_FRAME_MS : 0),
        pollFrame(bridge, { bands: 12, scale: "db" }, playing ? FIRST_FRAME_MS : 0),
    ]);
    recorder.assertCase(
        "AV-06 (§4.3) getSpectrum without a subscriptionId omits that field, reports bands equal to the spectrum length - at least the largest of ours for bands 0 - and honours an explicit bands and scale",
        aggregate?.success === true &&
            !("subscriptionId" in aggregate) &&
            aggregate.bands === aggregate.spectrum?.length &&
            aggregate.bands >= 32 &&
            aggregate.state === entryPlayback &&
            twelve?.success === true &&
            !("subscriptionId" in twelve) &&
            twelve.bands === 12 &&
            twelve.spectrum?.length === 12 &&
            twelve.scale === "db" &&
            (playing || isSilence(twelve.spectrum, "db")),
        {
            aggregate: { ...aggregate, spectrum: `${aggregate?.spectrum?.length} values` },
            twelve: { ...twelve, spectrum: `${twelve?.spectrum?.length} values` },
        },
        {
            aggregate: { subscriptionIdPresent: false, bands: ">= 32 and equal to the spectrum length" },
            twelve: { bands: 12, scale: "db" },
        },
    );

    return { weighted, db };
}

async function runFrameDeliveryCases(bridge, recorder, entryPlayback, pageVisible, shape) {
    const playing = entryPlayback === "playing";
    if (!pageVisible) {
        recorder.addCase(
            "AV-07 (D5, §4.2) frame delivery cases",
            true,
            { skipped: "the bridge page is hidden, so the host drops every delivery" },
            { skipped: "needs a visible page" },
        );
        return;
    }

    const collector = await createEventCollector(bridge, [shape.weighted.event, eventName("Late")], {
        collectorId: `av-delivery-${runId}`,
    });
    try {
        // A subscription created while paused or stopped: the state at
        // registration is the initial state, not a transition, so no silence
        // frame is owed. While playing it must produce tagged frames instead.
        const late = await subscribe(bridge, "Late", { fftSize: 1024, bands: 16, fps: 30 });
        const received = await collector.waitFor(
            (r) => framesOf(r, late.event).length > 0,
            { timeoutMs: playing ? FIRST_FRAME_MS : 700 },
        );
        const lateFrames = framesOf(received, late.event);
        recorder.assertCase(
            playing
                ? "AV-07 (D5, §4.2) a subscription created while playing receives frames tagged with its own id and state playing within two seconds"
                : "AV-07 (D5) a subscription created while paused or stopped receives no silence frame: the state at registration is not a transition",
            playing
                ? lateFrames.length > 0 &&
                  lateFrames.every(
                      (f) =>
                          f.payload?.subscriptionId === late.subscriptionId &&
                          f.payload.state === "playing" &&
                          f.payload.spectrum?.length === 16,
                  )
                : lateFrames.length === 0,
            { entryPlayback, frames: lateFrames.length, first: lateFrames[0]?.payload && { ...lateFrames[0].payload, spectrum: "16 values" } },
            playing ? { frames: "> 0, all tagged with the new id" } : { frames: 0 },
        );

        // Two subscriptions on one event name: each frame names its owner and
        // both owners appear. Only observable while frames flow.
        if (playing) {
            const twin = await bridge.invoke("audio.subscribeSpectrum", {
                subscriptionId: subId("ShapeTwin"),
                event: shape.weighted.event,
                fftSize: 1024,
                bands: 8,
                fps: 30,
            });
            ownedSubscriptions.add(subId("ShapeTwin"));
            const all = await collector.waitFor(
                (r) => {
                    const ids = new Set(framesOf(r, shape.weighted.event).map((f) => f.payload?.subscriptionId));
                    return ids.has(shape.weighted.subscriptionId) && ids.has(subId("ShapeTwin"));
                },
                { timeoutMs: FIRST_FRAME_MS },
            );
            const shared = framesOf(all, shape.weighted.event);
            const ids = new Set(shared.map((f) => f.payload?.subscriptionId));
            const lengthsMatch = shared.every(
                (f) => f.payload?.spectrum?.length === (f.payload?.subscriptionId === subId("ShapeTwin") ? 8 : 16),
            );
            recorder.assertCase(
                "AV-08 (D1, §5.4) two subscriptions on the same event name each receive their own frames: every frame names its owner, both owners appear, and each frame has its owner's band count",
                twin?.success === true &&
                    ids.has(shape.weighted.subscriptionId) &&
                    ids.has(subId("ShapeTwin")) &&
                    ids.size === 2 &&
                    lengthsMatch,
                { frames: shared.length, owners: [...ids], lengthsMatch },
                { owners: "exactly the two ids", lengthsMatch: true },
            );
        } else {
            recorder.addCase(
                "AV-08 (D1, §5.4) per-subscription frames on a shared event name",
                true,
                { skipped: `playback is ${entryPlayback}; frames only flow while playing (covered by the playback tier)` },
                { skipped: "needs playing audio" },
            );
        }
    } finally {
        await collector.stop();
    }
}

async function runUnsubscribeCases(bridge, recorder, entryDebug) {
    const ids = [...ownedSubscriptions];
    const results = [];
    for (const id of ids) results.push(await unsubscribe(bridge, id));
    const again = await bridge.invoke("audio.unsubscribeSpectrum", { subscriptionId: ids[0] });
    const debug = await debugState(bridge);
    recorder.assertCase(
        "AV-09 (§4.4) unsubscribing by id removes exactly that subscription, a second call for the same id removes nothing, and the count is back to what the suite found on entry",
        results.every((r) => r?.success === true && r.removed === 1) &&
            again?.success === true &&
            again.removed === 0 &&
            debug.subscriptionCount === entryDebug.subscriptionCount,
        {
            removed: results.map((r) => r?.removed),
            again,
            subscriptionCount: { entry: entryDebug.subscriptionCount, now: debug.subscriptionCount },
        },
        { removed: "1 each", again: { removed: 0 }, subscriptionCount: "equal to entry" },
    );
}

/** Beat length the host reports for a 60 fps beat, 1000 / 60 ms. */
const BEAT_60_MS = { min: 16.6, max: 16.7 };

async function runBeatStateCases(bridge, recorder) {
    const beat = await subscribe(bridge, "Beat", { fftSize: 1024, bands: 16, fps: 60 });
    const running = await debugState(bridge);
    await unsubscribe(bridge, beat.subscriptionId);
    const after = await debugState(bridge);

    // Whatever else is subscribed (the theme, another window) keeps the beat
    // thread alive at its own rate once ours is gone.
    const othersLeft = after.subscriptionCount > 0;
    const expectedAfterMs = othersLeft ? 1000 / after.effectiveFps : null;
    recorder.assertCase(
        "AV-31 (§4.4, D13) with a 60 fps subscription registered the beat thread runs: timerRunning true, timerHwnd 0, beatSource high-resolution or standard, beatIntervalMs 16.6..16.7 and an integer beatsCoalesced; once it is removed the beat follows the remaining subscriptions at 1000 / effectiveFps, or with none left timerRunning is false and beatSource and beatIntervalMs are null",
        beat.res?.success === true &&
            running.timerRunning === true &&
            running.timerHwnd === 0 &&
            ["high-resolution", "standard"].includes(running.beatSource) &&
            running.beatIntervalMs >= BEAT_60_MS.min &&
            running.beatIntervalMs <= BEAT_60_MS.max &&
            Number.isInteger(running.beatsCoalesced) &&
            running.beatsCoalesced >= 0 &&
            after.timerHwnd === 0 &&
            (othersLeft
                ? after.timerRunning === true && Math.abs(after.beatIntervalMs - expectedAfterMs) < 1e-3
                : after.timerRunning === false && after.beatSource === null && after.beatIntervalMs === null),
        {
            running: {
                timerRunning: running.timerRunning,
                timerHwnd: running.timerHwnd,
                beatSource: running.beatSource,
                beatIntervalMs: running.beatIntervalMs,
                beatsCoalesced: running.beatsCoalesced,
                effectiveFps: running.effectiveFps,
            },
            after: {
                subscriptionCount: after.subscriptionCount,
                effectiveFps: after.effectiveFps,
                timerRunning: after.timerRunning,
                beatSource: after.beatSource,
                beatIntervalMs: after.beatIntervalMs,
            },
        },
        {
            running: { timerRunning: true, timerHwnd: 0, beatSource: "high-resolution | standard", beatIntervalMs: "16.6..16.7" },
            after: othersLeft ? { timerRunning: true, beatIntervalMs: expectedAfterMs } : { timerRunning: false, beatSource: null, beatIntervalMs: null },
        },
    );
}

// ---------------------------------------------------------------------------
// Full-track waveform (contract tier: decoding a file does not touch playback)

const WAVEFORM_RESOLUTION = 256;
const WAVEFORM_READY_MS = 15000;
const WAVEFORM_EVENTS = ["audio:fullWaveformReady", "audio:fullWaveformFailed"];

/**
 * Requests a full-track waveform. A cache hit answers synchronously; otherwise
 * the answer is pending and the result arrives as an event carrying the taskId.
 * The collector is shared across requests, so events are matched by taskId.
 */
async function requestWaveform(bridge, collector, path, params) {
    const res = await bridge.invoke("audio.generateFullWaveform", {
        path,
        resolution: WAVEFORM_RESOLUTION,
        ...params,
    });
    if (res?.status !== "pending" || typeof res.taskId !== "string") return { res, event: null };
    const received = await collector.waitFor(
        (r) => r.some((e) => e.payload?.taskId === res.taskId),
        { timeoutMs: WAVEFORM_READY_MS },
    );
    const event = received.find((e) => e.payload?.taskId === res.taskId) ?? null;
    return { res, event };
}

const withinPercent = (value, expected, percent) =>
    typeof value === "number" && Math.abs(value - expected) <= (expected * percent) / 100;

async function runFullWaveformCases(bridge, recorder, fixtures) {
    const collector = await createEventCollector(bridge, WAVEFORM_EVENTS, { collectorId: `av-wave-${runId}` });
    try {
        // preferCache false forces a decode whatever an earlier run left in
        // the cache; the entry it writes then serves every later request shape.
        const rms = await requestWaveform(bridge, collector, fixtures.minus20.path, { method: "rms", preferCache: false });
        const peak = await requestWaveform(bridge, collector, fixtures.minus20.path, { method: "peak" });
        const signedHit = await requestWaveform(bridge, collector, fixtures.minus20.path, { method: "rms", signed: true });
        const readyEvent = rms.event?.name === "audio:fullWaveformReady" ? rms.event.payload : null;
        const isHit = (r, method, signedOutput) =>
            r.event === null &&
            r.res?.success === true &&
            r.res.status === "ready" &&
            r.res.cached === true &&
            r.res.method === method &&
            r.res.signed === signedOutput &&
            typeof r.res.maxAmplitude === "number" &&
            Array.isArray(r.res.waveform) &&
            r.res.waveform.length === WAVEFORM_RESOLUTION;
        recorder.assertCase(
            "AV-17 (A2.1, D6, §4.5) one decode serves every request shape: rms with preferCache false goes pending and its ready event carries maxAmplitude, then peak and signed on the same track are synchronous cache hits that echo their own method and signed and carry maxAmplitude",
            rms.res?.status === "pending" &&
                readyEvent !== null &&
                typeof readyEvent.maxAmplitude === "number" &&
                readyEvent.signed === false &&
                isHit(peak, "peak", false) &&
                isHit(signedHit, "rms", true),
            {
                rms: { status: rms.res?.status, event: rms.event?.name, maxAmplitude: readyEvent?.maxAmplitude, signed: readyEvent?.signed },
                peak: { status: peak.res?.status, cached: peak.res?.cached, method: peak.res?.method, signed: peak.res?.signed, maxAmplitude: peak.res?.maxAmplitude, pendingEvent: peak.event?.name },
                signed: { status: signedHit.res?.status, cached: signedHit.res?.cached, signed: signedHit.res?.signed, maxAmplitude: signedHit.res?.maxAmplitude, pendingEvent: signedHit.event?.name },
            },
            { rms: "pending, then ready with maxAmplitude", peak: "cached hit, method peak, signed false", signed: "cached hit, signed true" },
        );

        // A 1 kHz sine at -20 dBFS peaks at 3277 / 32768 = 0.1 and has an RMS
        // of 0.1 / sqrt(2); a 256-point window of the 20 s fixture spans 78
        // cycles, enough for the window RMS to sit within 1%.
        recorder.assertCase(
            "AV-18 (A2.2) on the -20 dBFS sine at resolution 256, maxAmplitude is 0.1 +/- 1% for peak and 0.0707 +/- 1% for rms",
            withinPercent(peak.res?.maxAmplitude, 0.1, 1) && withinPercent(readyEvent?.maxAmplitude, 0.0707, 1),
            { peak: peak.res?.maxAmplitude, rms: readyEvent?.maxAmplitude },
            { peak: "0.099..0.101", rms: "0.0700..0.0714" },
        );

        const signedFirst = await requestWaveform(bridge, collector, fixtures.full.path, { method: "rms", signed: true, preferCache: false });
        const plainAfter = await requestWaveform(bridge, collector, fixtures.full.path, { method: "rms" });
        const signedWave = signedFirst.event?.name === "audio:fullWaveformReady" ? signedFirst.event.payload.waveform : null;
        const plainWave = plainAfter.res?.waveform;
        const hasNegative = Array.isArray(signedWave) && signedWave.some((v) => v < 0);
        const plainMin = Array.isArray(plainWave) ? Math.min(...plainWave) : null;
        recorder.assertCase(
            "AV-19 (A2.4, N4) a signed request followed by a non-signed one on the same track: the signed result has negative values, and the non-signed answer is a cache hit with signed false and no negative value",
            hasNegative && plainAfter.res?.cached === true && plainAfter.res.signed === false && plainMin !== null && plainMin >= 0,
            { signedHasNegative: hasNegative, plain: { cached: plainAfter.res?.cached, signed: plainAfter.res?.signed, min: plainMin } },
            { signedHasNegative: true, plain: { cached: true, signed: false, min: ">= 0" } },
        );
    } finally {
        await collector.stop();
    }
}

// ---------------------------------------------------------------------------
// Full-track waveform queue (contract tier: decodes library files, plays nothing)

/** Library query for the queue cases: local FLAC files long enough that a decode outlasts a cancel. */
const QUEUE_TRACK_QUERY = "%codec% IS FLAC AND %length_seconds% GREATER 280 AND %length_seconds% LESS 420";
const QUEUE_TRACKS = 10;

const isCancelled = (e) => e.name === "audio:fullWaveformFailed" && e.payload?.code === "CANCELLED";
const eventsFor = (received, taskId) => received.filter((e) => e.payload?.taskId === taskId);

/** Distinct single-track files on a local drive; subsongs and relative paths are left out. */
async function pickQueueTracks(bridge) {
    const res = await bridge.invoke("library.query", { query: QUEUE_TRACK_QUERY, limit: 40 });
    const seen = new Set();
    const tracks = [];
    for (const t of res?.tracks ?? []) {
        const path = t.absolutePath ?? "";
        if (t.subsong !== 0 || path[1] !== ":" || seen.has(path)) continue;
        seen.add(path);
        tracks.push(path);
        if (tracks.length === QUEUE_TRACKS) break;
    }
    return tracks;
}

/**
 * Requests a waveform with preferCache false and waits for its terminal event.
 * The latency is measured to the page-side receipt time of that event, so the
 * collector's polling interval does not inflate it.
 */
async function timedDecode(bridge, collector, path, params = {}) {
    const startedAt = Date.now();
    const res = await bridge.invoke("audio.generateFullWaveform", {
        path,
        resolution: WAVEFORM_RESOLUTION,
        preferCache: false,
        ...params,
    });
    const received = await collector.waitFor((r) => eventsFor(r, res?.taskId).length > 0, {
        timeoutMs: WAVEFORM_READY_MS,
    });
    const event = eventsFor(received, res?.taskId)[0] ?? null;
    return { res, event, latencyMs: event ? event.at - startedAt : null };
}

async function runWaveformCancelRefusalCases(bridge, recorder) {
    const missing = await bridge.invoke("audio.cancelFullWaveform", {});
    const wrongType = await bridge.invoke("audio.cancelFullWaveform", { taskId: 42 });
    const empty = await bridge.invoke("audio.cancelFullWaveform", { taskId: "" });
    const unknown = await bridge.invoke("audio.cancelFullWaveform", { taskId: `waveform_e2e_${runId}` });
    recorder.assertCase(
        "AV-20 (§4.6) cancelFullWaveform answers INVALID_PARAMS without taskId and for a non-string or empty one, and cancelled false for a taskId that does not exist",
        isRefusedByReader(missing, "taskId") &&
            isRefusedByReader(wrongType, "taskId") &&
            isRefusedByReader(empty, "taskId") &&
            unknown?.success === true &&
            unknown.cancelled === false,
        { missing, wrongType, empty, unknown },
        { missing: "INVALID_PARAMS", wrongType: "INVALID_PARAMS", empty: "INVALID_PARAMS", unknown: { success: true, cancelled: false } },
    );
}

async function runWaveformParamRefusalCases(bridge, recorder) {
    const { invoke } = bridge;
    // getWaveform validates its parameters before it looks for stream data, so
    // these refusals hold whatever the instance is playing; each request sends
    // exactly one bad parameter so the answer names it.
    const [chFoo, ch42, p1, pBig, pFrac, pStr, d0, dNeg, dBig, dStr] = await Promise.all([
        invoke("audio.getWaveform", { channels: "foo" }),
        invoke("audio.getWaveform", { channels: 42 }),
        invoke("audio.getWaveform", { points: 1 }),
        invoke("audio.getWaveform", { points: 65537 }),
        invoke("audio.getWaveform", { points: 2.5 }),
        invoke("audio.getWaveform", { points: "8" }),
        invoke("audio.getWaveform", { duration: 0 }),
        invoke("audio.getWaveform", { duration: -1 }),
        invoke("audio.getWaveform", { duration: 1.5 }),
        invoke("audio.getWaveform", { duration: "0.05" }),
    ]);
    recorder.assertCase(
        "AV-28 (A4.2) getWaveform refuses channels 'foo' and 42, points 1 / 65537 / 2.5 / '8', and duration 0 / -1 / 1.5 / '0.05', each INVALID_PARAMS naming the offending parameter",
        isRefusedByReader(chFoo, "channels") &&
            isRefusedByReader(ch42, "channels") &&
            isRefusedByReader(p1, "points") &&
            isRefusedByReader(pBig, "points") &&
            isRefusedByReader(pFrac, "points") &&
            isRefusedByReader(pStr, "points") &&
            isInvalidParams(d0, "duration") &&
            d0.details.value === 0 &&
            isInvalidParams(dNeg, "duration") &&
            isRefusedByReader(dBig, "duration") &&
            isRefusedByReader(dStr, "duration"),
        {
            channels: { chFoo, ch42 },
            points: { p1, pBig, pFrac, pStr },
            duration: { d0, dNeg, dBig, dStr },
        },
        {
            notAboveZero: { success: false, code: "INVALID_PARAMS", details: { param: "duration", value: "as sent" } },
            others: { success: false, code: "INVALID_PARAMS", error: "starts with channels / points / duration" },
        },
    );
}

async function runWaveformQueueCases(bridge, recorder, tracks) {
    const collector = await createEventCollector(bridge, WAVEFORM_EVENTS, { collectorId: `av-queue-${runId}` });
    try {
        // Read every track once first, so the solo timings below compare warm
        // reads with warm reads instead of a cold first read with a cached one.
        for (const path of tracks) await timedDecode(bridge, collector, path);

        // Cancelling one's own pending request: exactly one CANCELLED failure and
        // nothing else for that taskId; a second cancel finds nothing.
        const own = await bridge.invoke("audio.generateFullWaveform", {
            path: tracks[0],
            resolution: WAVEFORM_RESOLUTION,
            preferCache: false,
        });
        const first = await bridge.invoke("audio.cancelFullWaveform", { taskId: own?.taskId });
        const second = await bridge.invoke("audio.cancelFullWaveform", { taskId: own?.taskId });
        await sleep(1000);
        const ownEvents = eventsFor(await collector.waitFor(null), own?.taskId);
        recorder.assertCase(
            "AV-21 (§4.6) cancelling one's own pending request answers cancelled true, sends exactly one audio:fullWaveformFailed with code CANCELLED and nothing else for that taskId, and a second cancel answers cancelled false",
            own?.status === "pending" &&
                first?.success === true &&
                first.cancelled === true &&
                second?.success === true &&
                second.cancelled === false &&
                ownEvents.length === 1 &&
                isCancelled(ownEvents[0]),
            { status: own?.status, first, second, events: ownEvents.map((e) => [e.name, e.payload?.code]) },
            { first: { cancelled: true }, second: { cancelled: false }, events: [["audio:fullWaveformFailed", "CANCELLED"]] },
        );

        // Two request shapes on one decode key while it is in flight: each
        // waiter is answered in its own shape. Merging is not observable from
        // the page; answers that arrive together are what one decode answering
        // both waiters looks like, and the queue unit tests pin the merge itself.
        const rms = await bridge.invoke("audio.generateFullWaveform", {
            path: tracks[1],
            resolution: WAVEFORM_RESOLUTION,
            preferCache: false,
        });
        const peakSentAt = Date.now();
        const peak = await bridge.invoke("audio.generateFullWaveform", {
            path: tracks[1],
            resolution: WAVEFORM_RESOLUTION,
            method: "peak",
            preferCache: false,
        });
        const merged = await collector.waitFor(
            (r) => eventsFor(r, rms?.taskId).length > 0 && eventsFor(r, peak?.taskId).length > 0,
            { timeoutMs: WAVEFORM_READY_MS },
        );
        const rmsEvent = eventsFor(merged, rms?.taskId)[0];
        const peakEvent = eventsFor(merged, peak?.taskId)[0];
        const rmsReady = rmsEvent?.payload;
        const peakReady = peakEvent?.payload;
        const apartMs = rmsEvent && peakEvent ? Math.abs(peakEvent.at - rmsEvent.at) : null;
        recorder.assertCase(
            "AV-22 (D7, §4.5) a peak request sent while the rms decode of the same track is still running: both get their own ready event in their own shape, the peak maxAmplitude is at least the rms one, and the two answers arrive within 5 ms of each other",
            rms?.status === "pending" &&
                peak?.status === "pending" &&
                rms.taskId !== peak.taskId &&
                rmsEvent.at > peakSentAt &&
                rmsReady?.method === "rms" &&
                peakReady?.method === "peak" &&
                typeof rmsReady.maxAmplitude === "number" &&
                peakReady.maxAmplitude >= rmsReady.maxAmplitude &&
                apartMs <= 5,
            { rms: { taskId: rms?.taskId, method: rmsReady?.method, maxAmplitude: rmsReady?.maxAmplitude }, peak: { taskId: peak?.taskId, method: peakReady?.method, maxAmplitude: peakReady?.maxAmplitude }, rmsAfterPeakSentMs: rmsEvent ? rmsEvent.at - peakSentAt : null, apartMs },
            { rms: "own ready, method rms", peak: "own ready, method peak, maxAmplitude >= rms", rmsAfterPeakSentMs: "> 0", apartMs: "<= 5" },
        );

        // A2.5: ten tracks requested back to back, each cancelling the one before.
        const solo = await timedDecode(bridge, collector, tracks[QUEUE_TRACKS - 1]);
        const requests = [];
        const cancels = [];
        for (let i = 0; i < QUEUE_TRACKS; i += 1) {
            const startedAt = Date.now();
            const res = await bridge.invoke("audio.generateFullWaveform", {
                path: tracks[i],
                resolution: WAVEFORM_RESOLUTION,
                preferCache: false,
            });
            requests.push({ taskId: res?.taskId, startedAt });
            if (i > 0) cancels.push(await bridge.invoke("audio.cancelFullWaveform", { taskId: requests[i - 1].taskId }));
        }
        const last = requests[QUEUE_TRACKS - 1];
        await collector.waitFor((r) => eventsFor(r, last.taskId).length > 0, { timeoutMs: WAVEFORM_READY_MS });
        await sleep(1000);
        const settled = await collector.waitFor(null);
        const cancelledOk = requests.slice(0, -1).map((q) => {
            const events = eventsFor(settled, q.taskId);
            return events.length === 1 && isCancelled(events[0]);
        });
        const lastEvents = eventsFor(settled, last.taskId);
        const lastLatency = lastEvents[0] ? lastEvents[0].at - last.startedAt : null;
        recorder.assertCase(
            "AV-23 (A2.5) ten tracks requested back to back, each cancelling the previous one: every cancel answers true, each of the nine cancelled taskIds gets exactly one CANCELLED failure, and the last track is ready within 1.5 times its solo decode time",
            solo.event?.name === "audio:fullWaveformReady" &&
                cancels.every((c) => c?.success === true && c.cancelled === true) &&
                cancelledOk.every(Boolean) &&
                lastEvents.length === 1 &&
                lastEvents[0].name === "audio:fullWaveformReady" &&
                lastLatency <= 1.5 * solo.latencyMs,
            { soloMs: solo.latencyMs, lastMs: lastLatency, cancels: cancels.map((c) => c?.cancelled), cancelledOk, lastEvents: lastEvents.map((e) => e.name) },
            { lastMs: `<= ${solo.latencyMs === null ? "?" : Math.round(1.5 * solo.latencyMs)}`, cancels: "all true", cancelledOk: "all true" },
        );

        // A2.6: six tracks without cancelling. With two decodes at a time the
        // batch takes about half the summed solo times, with three about a third;
        // 40% of the sum separates the two. The first result can only come from
        // one of the first two requests. Start order itself is pinned by the
        // queue unit tests.
        const six = tracks.slice(0, 6);
        const soloTimes = [];
        for (const path of six) soloTimes.push((await timedDecode(bridge, collector, path)).latencyMs);
        const batchStart = Date.now();
        const batch = [];
        for (const path of six) {
            const res = await bridge.invoke("audio.generateFullWaveform", {
                path,
                resolution: WAVEFORM_RESOLUTION,
                preferCache: false,
            });
            batch.push(res?.taskId);
        }
        const done = await collector.waitFor((r) => batch.every((id) => eventsFor(r, id).length > 0), {
            timeoutMs: WAVEFORM_READY_MS,
        });
        const finishes = batch.map((id) => eventsFor(done, id)[0]).filter(Boolean);
        const order = [...finishes].sort((a, b) => a.at - b.at).map((e) => batch.indexOf(e.payload.taskId));
        const wallMs = finishes.length ? Math.max(...finishes.map((e) => e.at)) - batchStart : null;
        const soloSum = soloTimes.reduce((acc, v) => acc + (v ?? 0), 0);
        recorder.assertCase(
            "AV-24 (A2.6) six tracks requested back to back without cancelling all finish ready, the first result belongs to one of the first two requests, and the batch takes at least 40% of the summed warm solo decode times, which three or more decodes at a time would undercut",
            soloTimes.every((v) => v !== null) &&
                finishes.length === 6 &&
                finishes.every((e) => e.name === "audio:fullWaveformReady") &&
                (order[0] === 0 || order[0] === 1) &&
                wallMs >= 0.4 * soloSum,
            { soloTimes, wallMs, completionOrder: order },
            { completionOrder: "starts with 0 or 1", wallMs: `>= ${Math.round(0.4 * soloSum)}` },
        );
    } finally {
        await collector.stop();
    }
}

// ---------------------------------------------------------------------------
// Playback tier

async function runFirstFrameCases(bridge, recorder, fixtures) {
    await bridge.invoke("playback.stop", {});
    await sleep(300);
    const stopped = await playbackState(bridge);

    const main = await subscribe(bridge, "Main", { fftSize: 1024, bands: 16, fps: 30, backgroundThrottle: false });
    const mainDb = await subscribe(bridge, "MainDb", { fftSize: 1024, bands: 16, fps: 30, scale: "db", backgroundThrottle: false });
    const debug = await debugState(bridge);
    const collector = await createEventCollector(bridge, [main.event, mainDb.event], {
        collectorId: `av-first-${runId}`,
    });

    const startedAt = Date.now();
    const played = await bridge.invoke("playback.playPath", { path: fixtures.full.path });
    const received = await collector.waitFor((r) => framesOf(r, main.event).some((f) => f.payload?.state === "playing"), {
        timeoutMs: FIRST_FRAME_MS + 1000,
    });
    const first = framesOf(received, main.event).find((f) => f.payload?.state === "playing");
    recorder.assertCase(
        "AV-10 (A1.2) subscribing while stopped answers success true and streamReady true, the debug state agrees, and the first playing frame arrives within two seconds of starting the fixture",
        stopped === "stopped" &&
            main.res?.success === true &&
            main.res.streamReady === true &&
            debug.streamReady === true &&
            played?.success === true &&
            first !== undefined &&
            first.at - startedAt <= FIRST_FRAME_MS,
        {
            stopped,
            subscribe: main.res,
            debugStreamReady: debug.streamReady,
            played,
            firstFrameAfterMs: first ? first.at - startedAt : null,
        },
        { stopped: "stopped", streamReady: true, firstFrameAfterMs: `<= ${FIRST_FRAME_MS}` },
    );

    // A1.4 reads the frames of the same subscription over the next two seconds.
    await sleep(2000);
    const frames = framesOf(await collector.drain(), main.event).filter((f) => f.payload?.state === "playing");
    const facts = frames.map((f) => f.payload);
    const positive = facts.filter((p) => p.streamTime > 0);
    // The clock may step back once by a few ms within about 200 ms of a start (spec §4.2): allowed
    // up to 5 ms in the first 0.5 s, recorded either way.
    let increasing = positive.length > 1;
    const stepsBack = [];
    for (let i = 1; i < positive.length; i += 1) {
        if (positive[i].streamTime > positive[i - 1].streamTime) continue;
        const backMs = (positive[i - 1].streamTime - positive[i].streamTime) * 1000;
        stepsBack.push({ at: positive[i].streamTime, backMs });
        if (!(positive[i].streamTime < 0.5 && backMs <= 5)) increasing = false;
    }
    const deltas = frames.map((f) => f.at - f.payload.hostTime);
    recorder.assertCase(
        "AV-11 (A1.4) frames of the 48 kHz fixture report sampleRate 48000, minFrequency 20 and maxFrequency 24000, streamTime increases once it has left zero, apart from steps back of at most 5 ms in the first 0.5 s, and the page receives each frame between -5 ms and 200 ms after its hostTime",
        facts.length >= 10 &&
            facts.every((p) => p.sampleRate === 48000 && p.minFrequency === 20 && p.maxFrequency === 24000) &&
            facts.every((p) => p.subscriptionId === main.subscriptionId && p.fftSize === 1024 && p.bands === 16) &&
            increasing &&
            deltas.every((d) => d >= -5 && d <= 200),
        {
            frames: facts.length,
            sampleRates: [...new Set(facts.map((p) => p.sampleRate))],
            maxFrequency: [...new Set(facts.map((p) => p.maxFrequency))],
            streamTime: { first: facts[0]?.streamTime, last: facts.at(-1)?.streamTime, positiveFrames: positive.length, increasing, stepsBack },
            hostTimeDeltaMs: { min: Math.min(...deltas), max: Math.max(...deltas) },
        },
        { sampleRate: 48000, maxFrequency: 24000, minFrequency: 20, increasing: true, hostTimeDeltaMs: "-5..200" },
    );

    await collector.stop();
    return { main, mainDb };
}

async function runIndependenceCases(bridge, recorder, fixtures) {
    const a = await subscribe(bridge, "IndA", { bands: 64, fftSize: 2048, fps: 30, backgroundThrottle: false });
    const b = await subscribe(bridge, "IndB", { bands: 256, fftSize: 16384, fps: 60, backgroundThrottle: false });
    const collector = await createEventCollector(bridge, [a.event, b.event], { collectorId: `av-ind-${runId}` });
    const windowMs = 5000;
    try {
        await bridge.invoke("playback.playPath", { path: fixtures.full.path });
        await collector.waitFor((r) => framesOf(r, a.event).length > 0 && framesOf(r, b.event).length > 0, {
            timeoutMs: FIRST_FRAME_MS + 1000,
        });
        await sleep(windowMs + 200);
        const received = await collector.drain();
        const fa = framesOf(received, a.event).filter((f) => f.payload?.state === "playing");
        const fb = framesOf(received, b.event).filter((f) => f.payload?.state === "playing");
        const rateA = rateFrom(fa, windowMs);
        const rateB = rateFrom(fb, windowMs);
        // B is due on every timer tick, so its rate is the tick rate the timer
        // actually achieves; A may not exceed its own 30 fps on top of that.
        const floorA = 0.9 * Math.min(rateB.rate, 30);
        recorder.assertCase(
            "AV-12 (A1.3) with a 64-band / 2048 and a 256-band / 16384 subscription side by side, each frame carries its owner's band count and fftSize (8192 after the automatic raise, 16384) and its owner's id, and the 30 fps one stays at or under 31 frames per second while reaching 90% of the lesser of the tick rate and 30",
            fa.length > 0 &&
                fb.length > 0 &&
                fa.every((f) => f.payload.subscriptionId === a.subscriptionId && f.payload.spectrum.length === 64 && f.payload.fftSize === 8192 && f.payload.bands === 64) &&
                fb.every((f) => f.payload.subscriptionId === b.subscriptionId && f.payload.spectrum.length === 256 && f.payload.fftSize === 16384 && f.payload.bands === 256) &&
                rateA.rate <= 31 &&
                rateA.rate >= floorA,
            { a: { frames: fa.length, ...rateA }, b: { frames: fb.length, ...rateB }, floorA },
            { a: { spectrumLength: 64, fftSize: 8192, rate: `<= 31 and >= ${floorA.toFixed(1)}` }, b: { spectrumLength: 256, fftSize: 16384 } },
        );
    } finally {
        await collector.stop();
        await unsubscribe(bridge, a.subscriptionId);
        await unsubscribe(bridge, b.subscriptionId);
    }
}

/**
 * Median peak reading of a db subscription inside a steady window of a fixture:
 * frames received between settleMs and settleMs + sampleMs after the fixture
 * was started. The first frames of a track can still carry the crossfade from
 * the previous one (about 1.5 s), which is what the settle time skips.
 */
function steadyPeak(received, event, startedAt, { settleMs, sampleMs }) {
    const frames = framesOf(received, event).filter((f) => f.payload?.state === "playing");
    const steady = frames
        .filter((f) => f.at >= startedAt + settleMs && f.at < startedAt + settleMs + sampleMs)
        .map((f) => f.payload.spectrum);
    if (steady.length === 0) return { frames: frames.length, steady: 0 };
    const peak = median(steady.map(indexOfMax));
    const reading = median(steady.map((s) => s[peak]));
    const neighbourhood = median(steady.map((s) => sumDb(s.slice(Math.max(0, peak - 1), peak + 2))));
    return { frames: frames.length, steady: steady.length, peak, reading, neighbourhood };
}

/** Steady-window timing shared by the readings below: skip the crossfade, then sample. */
const STEADY_TIMING = { settleMs: 2500, sampleMs: 1500 };

/** Starts a fixture and returns the frames the collector saw over the steady window. */
async function playAndSample(bridge, collector, path, timing = STEADY_TIMING) {
    await collector.drain();
    const startedAt = Date.now();
    await bridge.invoke("playback.playPath", { path });
    await sleep(timing.settleMs + timing.sampleMs + 300);
    return { startedAt, received: await collector.drain() };
}

async function runCalibrationCases(bridge, recorder, fixtures) {
    const d48 = await subscribe(bridge, "Db48", { bands: 48, fftSize: 8192, fps: 10, scale: "db", backgroundThrottle: false });
    const d256 = await subscribe(bridge, "Db256", { bands: 256, fftSize: 8192, fps: 10, scale: "db", backgroundThrottle: false });
    const collector = await createEventCollector(bridge, [d48.event, d256.event], { collectorId: `av-cal-${runId}` });
    const timing = STEADY_TIMING;
    const band1k = bandOf(bandEdges(48000, 48), 1000);

    try {
        const full = await playAndSample(bridge, collector, fixtures.full.path);
        const full48 = steadyPeak(full.received, d48.event, full.startedAt, timing);
        const full256 = steadyPeak(full.received, d256.event, full.startedAt, timing);
        const quiet = await playAndSample(bridge, collector, fixtures.minus20.path);
        const quiet48 = steadyPeak(quiet.received, d48.event, quiet.startedAt, timing);

        recorder.assertCase(
            "AV-13 (A1.5) on the 'db' scale a 0 dBFS 1 kHz sine reads 0 +/- 1 dB in the 48-band band that holds 1 kHz, a -20 dBFS one reads -20 +/- 1 dB there, and at 256 bands the peak band with its two neighbours sums to within 1 dB of the 48-band reading",
            full48.peak === band1k &&
                Math.abs(full48.reading - 0) <= 1 &&
                quiet48.peak === band1k &&
                Math.abs(quiet48.reading - -20) <= 1 &&
                Math.abs(full256.neighbourhood - full48.reading) <= 1,
            { band1k, full48, full256, quiet48 },
            {
                full48: { peak: band1k, reading: "0 +/- 1" },
                quiet48: { peak: band1k, reading: "-20 +/- 1" },
                full256: { neighbourhood: "within 1 dB of full48.reading" },
            },
        );
    } finally {
        await collector.stop();
        await unsubscribe(bridge, d48.subscriptionId);
        await unsubscribe(bridge, d256.subscriptionId);
    }
}

async function runRangeCases(bridge, recorder, fixtures) {
    const rangeParams = { bands: 32, fftSize: 8192, fps: 10, scale: "db", backgroundThrottle: false };
    const narrow = await subscribe(bridge, "Range", { ...rangeParams, minFrequency: 500, maxFrequency: 2000 });
    const plain = await subscribe(bridge, "RangeDefault", rangeParams);
    const high = await subscribe(bridge, "RangeHigh", { ...rangeParams, maxFrequency: 30000 });
    const events = [narrow.event, plain.event, high.event];
    const collector = await createEventCollector(bridge, events, { collectorId: `av-range-${runId}` });
    const timing = STEADY_TIMING;

    // 1 kHz is the geometric centre of 500..2000 Hz, so with an even band count
    // it sits exactly on a band edge and the sine's main lobe is shared by the
    // two bands meeting there. The peak is one of those two, and the power of
    // the peak band with its neighbours is the whole sine, 0 dBFS.
    const edges = rangeBandEdges(500, 2000, 32);
    const edgeAt1k = edges.findIndex((e) => Math.abs(e - 1000) < 0.5);
    const bandsAt1k = [edgeAt1k - 1, edgeAt1k];

    try {
        const full = await playAndSample(bridge, collector, fixtures.full.path);
        const facts = (event) =>
            framesOf(full.received, event)
                .filter((f) => f.at >= full.startedAt + timing.settleMs && f.payload?.state === "playing")
                .map((f) => f.payload);
        const range = (list) => ({
            frames: list.length,
            minFrequency: [...new Set(list.map((p) => p.minFrequency))],
            maxFrequency: [...new Set(list.map((p) => p.maxFrequency))],
        });
        const narrowFacts = range(facts(narrow.event));
        const plainFacts = range(facts(plain.event));
        const highFacts = range(facts(high.event));
        const peak = steadyPeak(full.received, narrow.event, full.startedAt, timing);
        const reports = (r, min, max) =>
            r.frames > 0 && r.minFrequency.length === 1 && r.minFrequency[0] === min && r.maxFrequency.length === 1 && r.maxFrequency[0] === max;

        recorder.assertCase(
            "AV-27 (A6.5) on the 48 kHz 0 dBFS 1 kHz fixture, a db subscription with minFrequency 500 and maxFrequency 2000 reports exactly that range in its frames, its peak is one of the two 32-band bands meeting at 1 kHz and that band with its neighbours sums to 0 +/- 1 dB; a default subscription alongside reports 20 and 24000, and one asking for 30000 echoes 30000 in the registration answer but reports 24000 in its frames",
            edgeAt1k > 0 &&
                reports(narrowFacts, 500, 2000) &&
                bandsAt1k.includes(peak.peak) &&
                Math.abs(peak.neighbourhood - 0) <= 1 &&
                reports(plainFacts, 20, 24000) &&
                high.res?.maxFrequency === 30000 &&
                reports(highFacts, 20, 24000),
            { edgeAt1k, narrow: { ...narrowFacts, peak }, plain: plainFacts, high: { registered: high.res?.maxFrequency, ...highFacts } },
            {
                narrow: { minFrequency: 500, maxFrequency: 2000, peak: bandsAt1k, neighbourhood: "0 +/- 1 dB" },
                plain: { minFrequency: 20, maxFrequency: 24000 },
                high: { registered: 30000, maxFrequency: 24000 },
            },
        );
    } finally {
        await collector.stop();
        for (const s of [narrow, plain, high]) await unsubscribe(bridge, s.subscriptionId);
    }
}

async function runTransitionCases(bridge, recorder, subs) {
    const events = [subs.main.event, subs.mainDb.event];
    const collector = await createEventCollector(bridge, events, { collectorId: `av-trans-${runId}` });
    try {
        // Make sure frames are flowing before the transition.
        await collector.waitFor((r) => framesOf(r, subs.main.event).some((f) => f.payload?.state === "playing"), {
            timeoutMs: FIRST_FRAME_MS + 1000,
        });
        await collector.drain();

        const pausedAt = Date.now();
        await bridge.invoke("playback.pause", {});
        await sleep(500);
        const afterPause = await collector.drain();
        const quietFrom = Date.now();
        const computedAfterPause = (await debugState(bridge)).framesComputed;
        await sleep(QUIET_MS);
        const stillQuiet = (await collector.drain()).filter((f) => f.at >= quietFrom);
        const computedLater = (await debugState(bridge)).framesComputed;

        const pauseFramesOf = (event, scale) => {
            const frames = framesOf(afterPause, event).filter((f) => f.at >= pausedAt);
            const paused = frames.filter((f) => f.payload?.state === "paused");
            const afterSilence = frames.slice(frames.indexOf(paused[0]) + 1);
            return {
                paused: paused.length,
                silence: paused.every((f) => isSilence(f.payload.spectrum, scale) && f.payload.spectrum.length === 16),
                trailing: afterSilence.length,
            };
        };
        const pw = pauseFramesOf(subs.main.event, "weighted");
        const pd = pauseFramesOf(subs.mainDb.event, "db");
        const quietFrames = stillQuiet.filter((f) => events.includes(f.name)).length;
        recorder.assertCase(
            "AV-14 (A1.7) pausing sends each subscription exactly one frame with state paused whose spectrum is that scale's silence value, nothing follows it for a second, and framesComputed does not move while paused",
            pw.paused === 1 &&
                pw.silence &&
                pw.trailing === 0 &&
                pd.paused === 1 &&
                pd.silence &&
                pd.trailing === 0 &&
                quietFrames === 0 &&
                computedLater === computedAfterPause,
            { weighted: pw, db: pd, quietFrames, framesComputed: { afterPause: computedAfterPause, oneSecondLater: computedLater } },
            { each: { paused: 1, silence: true, trailing: 0 }, quietFrames: 0, framesComputed: "unchanged" },
        );

        const stoppedAt = Date.now();
        await bridge.invoke("playback.stop", {});
        await sleep(500);
        const afterStop = (await collector.drain()).filter((f) => f.at >= stoppedAt && events.includes(f.name));
        const sw = afterStop.filter((f) => f.name === subs.main.event);
        const sd = afterStop.filter((f) => f.name === subs.mainDb.event);

        const resumedAt = Date.now();
        await bridge.invoke("playback.play", {});
        const resumed = await collector.waitFor(
            (r) => r.some((f) => f.at >= resumedAt && f.name === subs.main.event && f.payload?.state === "playing"),
            { timeoutMs: FIRST_FRAME_MS + 500 },
        );
        const firstPlaying = resumed.find((f) => f.at >= resumedAt && f.name === subs.main.event && f.payload?.state === "playing");
        recorder.assertCase(
            "AV-15 (A1.7) stopping while paused sends each subscription exactly one frame with state stopped, and resuming playback brings a playing frame within two seconds",
            sw.length === 1 &&
                sw[0].payload?.state === "stopped" &&
                isSilence(sw[0].payload.spectrum, "weighted") &&
                sd.length === 1 &&
                sd[0].payload?.state === "stopped" &&
                isSilence(sd[0].payload.spectrum, "db") &&
                firstPlaying !== undefined &&
                firstPlaying.at - resumedAt <= FIRST_FRAME_MS,
            {
                stopped: { weighted: sw.map((f) => f.payload?.state), db: sd.map((f) => f.payload?.state) },
                resumedAfterMs: firstPlaying ? firstPlaying.at - resumedAt : null,
            },
            { stopped: { weighted: ["stopped"], db: ["stopped"] }, resumedAfterMs: `<= ${FIRST_FRAME_MS}` },
        );
    } finally {
        await collector.stop();
    }
}

async function runThrottleCases(bridge, recorder, fixtures) {
    const debug = await debugState(bridge);
    if (debug.foregroundIsExternal !== true) {
        recorder.addCase(
            "AV-16 (A1.9) external-foreground throttle",
            true,
            { skipped: "foobar2000 itself is in the foreground, so the throttle does not apply", foregroundTitle: debug.foregroundTitle },
            { skipped: "needs another application in the foreground (the terminal running this suite usually is)" },
        );
        return;
    }

    const t1 = await subscribe(bridge, "Thr1", { fftSize: 1024, bands: 16, fps: 30 });
    const t2 = await subscribe(bridge, "Thr2", { fftSize: 1024, bands: 16, fps: 30 });
    const free = await subscribe(bridge, "Free", { fftSize: 1024, bands: 16, fps: 30, backgroundThrottle: false });
    // Due on every tick: measures the tick rate the timer achieves.
    const tick = await subscribe(bridge, "Tick", { fftSize: 1024, bands: 16, fps: 60, backgroundThrottle: false });
    const events = [t1.event, t2.event, free.event, tick.event];
    const collector = await createEventCollector(bridge, events, { collectorId: `av-thr-${runId}` });
    const windowMs = 10000;
    try {
        await bridge.invoke("playback.playPath", { path: fixtures.full.path });
        await collector.waitFor((r) => events.every((e) => framesOf(r, e).length > 0), { timeoutMs: FIRST_FRAME_MS + 1000 });
        await sleep(windowMs + 200);
        const received = await collector.drain();
        const playing = (event) => framesOf(received, event).filter((f) => f.payload?.state === "playing");
        const r1 = rateFrom(playing(t1.event), windowMs);
        const r2 = rateFrom(playing(t2.event), windowMs);
        const rf = rateFrom(playing(free.event), windowMs);
        const rt = rateFrom(playing(tick.event), windowMs);
        const floorFree = 0.9 * Math.min(rt.rate, 30);
        const still = await debugState(bridge);
        recorder.assertCase(
            "AV-16 (A1.9) with another application in the foreground, two throttled 30 fps subscriptions each run at 8..12 frames per second over ten seconds, and an unthrottled one alongside them reaches 90% of the lesser of the tick rate and 30",
            still.foregroundIsExternal === true &&
                r1.rate >= 8 &&
                r1.rate <= 12 &&
                r2.rate >= 8 &&
                r2.rate <= 12 &&
                rf.rate >= floorFree,
            { throttled: [r1, r2], free: rf, tick: rt, floorFree, foregroundIsExternal: still.foregroundIsExternal, foregroundTitle: still.foregroundTitle },
            { throttled: "8..12 each", free: `>= ${floorFree.toFixed(1)}`, foregroundIsExternal: true },
        );
    } finally {
        await collector.stop();
        for (const s of [t1, t2, free, tick]) await unsubscribe(bridge, s.subscriptionId);
    }
}

async function runPushBeatCases(bridge, recorder, fixtures) {
    const push = await subscribe(bridge, "Push60", { fftSize: 1024, bands: 16, fps: 60, backgroundThrottle: false });
    const collector = await createEventCollector(bridge, [push.event], { collectorId: `av-push-${runId}` });
    const windowMs = 10000;
    try {
        await bridge.invoke("playback.playPath", { path: fixtures.full.path });
        // Counting starts past the silence frames a fresh start sends at
        // streamTime zero for about 200 ms: they share one streamTime and would
        // otherwise read as missing frames.
        await collector.waitFor(
            (r) => framesOf(r, push.event).some((f) => f.payload?.state === "playing" && f.payload.streamTime > 0),
            { timeoutMs: FIRST_FRAME_MS + 1000 },
        );
        await sleep(windowMs + 300);
        const debug = await debugState(bridge);
        const frames = framesOf(await collector.drain(), push.event).filter((f) => f.payload?.state === "playing");
        const startIndex = frames.findIndex((f) => f.payload.streamTime > 0);
        const start = startIndex >= 0 ? frames[startIndex].payload.hostTime : 0;
        const counted = startIndex >= 0 ? frames.slice(startIndex).filter((f) => f.payload.hostTime < start + windowMs) : [];
        const newFrames = counted.filter((f, i) => i === 0 || f.payload.streamTime !== counted[i - 1].payload.streamTime).length;
        // hostTime is stamped when the host builds the frame, so the gaps
        // measure the beat, not the page's event loop.
        const gaps = counted.slice(1).map((f, i) => f.payload.hostTime - counted[i].payload.hostTime);
        const rate = (newFrames * 1000) / windowMs;
        const p95 = percentile(gaps, 95);
        recorder.assertCase(
            "AV-32 (A3.1) a single 60 fps push subscription delivers at least 57 new frames per second over the ten seconds from its first frame past streamTime zero, consecutive hostTime gaps have p95 <= 20 ms, and the debug state reports a high-resolution beat of 16.6..16.7 ms",
            startIndex >= 0 &&
                rate >= 57 &&
                p95 !== null &&
                p95 <= 20 &&
                debug.beatSource === "high-resolution" &&
                debug.beatIntervalMs >= BEAT_60_MS.min &&
                debug.beatIntervalMs <= BEAT_60_MS.max,
            {
                frames: counted.length,
                newFrames,
                rate,
                hostTimeGapMs: { p50: percentile(gaps, 50), p95, p99: percentile(gaps, 99), max: gaps.length ? Math.max(...gaps) : null },
                beatSource: debug.beatSource,
                beatIntervalMs: debug.beatIntervalMs,
                beatsCoalesced: debug.beatsCoalesced,
            },
            { rate: ">= 57", hostTimeGapMs: { p95: "<= 20" }, beatSource: "high-resolution", beatIntervalMs: "16.6..16.7" },
        );
    } finally {
        await collector.stop();
        await unsubscribe(bridge, push.subscriptionId);
    }
}

async function runBeatChangeCases(bridge, recorder, fixtures) {
    const keep = await subscribe(bridge, "Keep1", { fftSize: 1024, bands: 16, fps: 1, backgroundThrottle: false });
    const fastId = subId("Fast60");
    const fastEvent = eventName("Fast60");
    const collector = await createEventCollector(bridge, [keep.event, fastEvent], { collectorId: `av-beat-${runId}` });
    try {
        await bridge.invoke("playback.playPath", { path: fixtures.full.path });
        await collector.waitFor((r) => framesOf(r, keep.event).some((f) => f.payload?.state === "playing"), {
            timeoutMs: FIRST_FRAME_MS + 1500,
        });
        const before = await debugState(bridge);
        if (before.effectiveFps !== 1) {
            recorder.addCase(
                "AV-33 (A3.1b) beat-length change",
                true,
                { skipped: `another subscription keeps the beat at ${before.effectiveFps} fps, so a new 60 fps one proves nothing`, subscriptionCount: before.subscriptionCount },
                { skipped: "needs every other subscription at fps 1" },
            );
            return;
        }

        // Register a third of the way into the 1 s beat: a beat thread that
        // stored the new length without waking up would wait out the rest.
        await sleep(300);
        ownedSubscriptions.add(fastId);
        const params = { subscriptionId: fastId, event: fastEvent, fftSize: 1024, bands: 16, fps: 60, backgroundThrottle: false };
        // Both timestamps come from the page clock: the answer here, the frame in the collector.
        const registered = await bridge.evaluateValue(
            `window.fb2k.invoke("audio.subscribeSpectrum", ${JSON.stringify(params)}).then((res) => ({ res, answeredAt: Date.now() }))`,
            true,
            invokeTimeoutMs + 1000,
        );
        const received = await collector.waitFor((r) => framesOf(r, fastEvent).length > 0, { timeoutMs: 2000 });
        const first = framesOf(received, fastEvent)[0];
        const latency = first && registered ? first.at - registered.answeredAt : null;
        recorder.assertCase(
            "AV-33 (A3.1b) while playing with every subscription at 1 fps, registering a 60 fps one brings its first frame within 50 ms of the registration answer: the new beat length takes effect at once instead of after the rest of the 1 s beat",
            registered?.res?.success === true && first !== undefined && latency <= 50,
            { effectiveFpsBefore: before.effectiveFps, beatIntervalMsBefore: before.beatIntervalMs, registered: registered?.res?.success, firstFrameAfterMs: latency },
            { effectiveFpsBefore: 1, firstFrameAfterMs: "<= 50" },
        );
    } finally {
        await collector.stop();
        await unsubscribe(bridge, fastId);
        await unsubscribe(bridge, keep.subscriptionId);
    }
}

/**
 * Runs in the page: for windowMs, calls getSpectrum from requestAnimationFrame
 * at no more than 60 Hz with at most one call in flight, the way a page is
 * meant to pull. A timeout settles it too, because a hidden or minimised page
 * gets no animation frames. Must stay self-contained: it is sent as source.
 */
function pullProbe(subscriptionId, windowMs) {
    return new Promise((resolve) => {
        const interval = 1000 / 60;
        const trips = [];
        let newFrames = 0;
        let failures = 0;
        let rafs = 0;
        let lastStreamTime = null;
        let inFlight = false;
        let done = false;
        const startedAt = performance.now();
        let nextAt = startedAt;
        const finish = () => {
            if (done) return;
            done = true;
            resolve({ trips, newFrames, failures, rafs, elapsedMs: performance.now() - startedAt });
        };
        const step = (now) => {
            if (done) return;
            rafs += 1;
            if (now - startedAt >= windowMs) {
                finish();
                return;
            }
            // Deadline-based limiter with 2 ms slack: on a 165 Hz display a
            // plain "16.7 ms since the last call" test would settle at 55 Hz.
            if (!inFlight && now >= nextAt - 2) {
                nextAt = now - nextAt > interval ? now + interval : nextAt + interval;
                inFlight = true;
                const sent = performance.now();
                window.fb2k
                    .invoke("audio.getSpectrum", { subscriptionId })
                    .then(
                        (res) => {
                            trips.push(performance.now() - sent);
                            if (res?.success === true && res.state === "playing") {
                                if (res.streamTime !== lastStreamTime) newFrames += 1;
                                lastStreamTime = res.streamTime;
                            } else {
                                failures += 1;
                            }
                        },
                        () => {
                            failures += 1;
                        },
                    )
                    .finally(() => {
                        inFlight = false;
                    });
            }
            requestAnimationFrame(step);
        };
        requestAnimationFrame(step);
        setTimeout(finish, windowMs + 1000);
    });
}

async function runPullCases(bridge, recorder, fixtures) {
    const keep = await subscribe(bridge, "Pull", { fftSize: 1024, bands: 16, fps: 1, backgroundThrottle: false });
    const windowMs = 5000;
    try {
        await bridge.invoke("playback.playPath", { path: fixtures.full.path });
        await pollFrame(bridge, { subscriptionId: keep.subscriptionId }, FIRST_FRAME_MS);
        await sleep(500);
        const stats = await bridge.evaluateValue(
            `(${pullProbe.toString()})(${JSON.stringify(keep.subscriptionId)}, ${windowMs})`,
            true,
            windowMs + invokeTimeoutMs,
        );
        // The same call back to back outside animation frames: the host's share
        // of the round trip, reported next to the rAF-driven figure.
        const outsideFrames = await bridge.evaluateValue(
            `(async () => {
                const trips = [];
                for (let i = 0; i < 100; i += 1) {
                    const sent = performance.now();
                    await window.fb2k.invoke("audio.getSpectrum", { subscriptionId: ${JSON.stringify(keep.subscriptionId)} });
                    trips.push(performance.now() - sent);
                }
                return trips;
            })()`,
            true,
            invokeTimeoutMs + 1000,
        );
        const rafRate = (stats.rafs * 1000) / windowMs;
        if (rafRate < 55) {
            recorder.addCase(
                "AV-34 (A3.2) pull regression",
                true,
                { skipped: "blocked: the page got fewer than 55 animation frames per second (window minimised or covered?)", rafRate },
                { skipped: "needs a visible bridge page drawing at least 55 frames per second" },
            );
            return;
        }
        const newRate = (stats.newFrames * 1000) / windowMs;
        // The round-trip line is judged outside animation frames: a call made
        // inside a rAF callback waits on the page's own scheduling (about one
        // frame on the component's built-in page), which is not the host's cost.
        // The rAF-driven round trip is recorded only.
        const hostP95 = percentile(outsideFrames, 95);
        recorder.assertCase(
            "AV-34 (A3.2) a page pulling getSpectrum for a 1 fps subscription from requestAnimationFrame, limited to 60 Hz with one call in flight, gets at least 55 new frames per second over five seconds, and the same call made back to back outside animation frames has a round-trip p95 <= 8 ms; the rAF-driven round trip is recorded only",
            newRate >= 55 && hostP95 !== null && hostP95 <= 8,
            {
                newRate,
                calls: stats.trips.length,
                failures: stats.failures,
                rafRate,
                outsideFramesRoundTripMs: { p50: percentile(outsideFrames, 50), p95: hostP95 },
                rafRoundTripMs: { p50: percentile(stats.trips, 50), p95: percentile(stats.trips, 95), max: stats.trips.length ? Math.max(...stats.trips) : null },
                page: await bridge.evaluateValue("location.href", false, 2000),
            },
            { newRate: ">= 55", outsideFramesRoundTripMs: { p95: "<= 8" }, rafRoundTripMs: "recorded only" },
        );
    } finally {
        await unsubscribe(bridge, keep.subscriptionId);
    }
}

async function runLiveWaveformCases(bridge, recorder, fixtures) {
    // getWaveform reads the visualisation stream, so a subscription must be
    // active and audio must be playing; fps 1 keeps the stream alive cheaply.
    const keep = await subscribe(bridge, "WavePipe", { fftSize: 1024, bands: 16, fps: 1, backgroundThrottle: false });
    const getWaveform = (params) => bridge.invoke("audio.getWaveform", params);
    try {
        await bridge.invoke("playback.playPath", { path: fixtures.stereo.path });
        await sleep(FIRST_FRAME_MS);
        const stereo = await getWaveform({ channels: "stereo", signed: true });
        const stereoPoints = await getWaveform({ channels: "stereo", signed: true, points: 256 });

        await bridge.invoke("playback.playPath", { path: fixtures.full.path });
        await sleep(FIRST_FRAME_MS);
        const mono = await getWaveform({ channels: "stereo", signed: true });

        const leftHasSignal = Array.isArray(stereo?.left) && stereo.left.some((v) => Math.abs(v) > 0.01);
        const rightSilent = Array.isArray(stereo?.right) && stereo.right.every((v) => Math.abs(v) < 1e-3);
        recorder.assertCase(
            "AV-29 (A4.1) on the stereo fixture getWaveform channels stereo signed returns equal-length left / right with signal on the left, the right below 1e-3 and channelCount 2; points 256 makes both arrays 256 long; on the mono fixture left equals right elementwise and channelCount is 1",
            stereo?.success === true &&
                stereo.channels === "stereo" &&
                stereo.channelCount === 2 &&
                Array.isArray(stereo.left) &&
                stereo.left.length === stereo.right.length &&
                leftHasSignal &&
                rightSilent &&
                stereoPoints?.success === true &&
                stereoPoints.left.length === 256 &&
                stereoPoints.right.length === 256 &&
                mono?.success === true &&
                mono.channelCount === 1 &&
                arraysEqual(mono.left, mono.right),
            {
                stereo: { success: stereo?.success, channelCount: stereo?.channelCount, lengths: [stereo?.left?.length, stereo?.right?.length], leftHasSignal, rightSilent },
                stereoPoints: { lengths: [stereoPoints?.left?.length, stereoPoints?.right?.length] },
                mono: { success: mono?.success, channelCount: mono?.channelCount, leftEqualsRight: arraysEqual(mono?.left, mono?.right) },
            },
            {
                stereo: { channelCount: 2, leftHasSignal: true, rightSilent: true },
                stereoPoints: { lengths: [256, 256] },
                mono: { channelCount: 1, leftEqualsRight: true },
            },
        );

        // The host asks the stream to keep at least two seconds of history, so
        // every duration up to the 1 s cap is available two seconds into the
        // fixture; a window reaching back before the start would be zero-padded
        // rather than refused.
        await bridge.invoke("playback.playPath", { path: fixtures.full.path });
        await sleep(FIRST_FRAME_MS);
        const records = [];
        for (const duration of [0.05, 0.2, 0.5, 1.0]) {
            const res = await getWaveform({ duration });
            records.push({
                duration,
                success: res?.success === true,
                samples: Array.isArray(res?.waveform) ? res.waveform.length : null,
                sampleRate: res?.sampleRate,
                error: res?.success === true ? undefined : res?.error,
            });
        }
        recorder.assertCase(
            "AV-30 (A4.4, audio-timing A-V6) two seconds into the fixture, getWaveform at durations 0.05, 0.2, 0.5 and 1.0 s each succeeds with round(duration x sampleRate) samples",
            records.every((r) => r.success && r.samples === Math.round(r.duration * r.sampleRate)),
            { records },
            { records: "every success true; samples = round(duration x sampleRate)" },
        );
    } finally {
        await unsubscribe(bridge, keep.subscriptionId);
    }
}

// ---------------------------------------------------------------------------
// Visualisation timing (docs/audio-timing/SPEC.md T8)

/** FFT sizes the timing criteria cover. */
const TIMING_FFT_SIZES = [4096, 8192, 16384, 32768, 65536];
/** Largest error allowed between a measured and an expected moment, in ms. */
const TIMING_TOLERANCE_MS = 30;
/** A 30 fps subscription has missed a frame when two frames are further apart than this, in ms. */
const MISSING_FRAME_GAP_MS = 2 * (1000 / 30);

/**
 * Finds the boundary between the two tones of the stepped fixture in a signed
 * mono window: the zero crossing where the spacing of crossings switches from
 * one tone's half period to the other's. Returns its fractional sample index
 * and whether the tone rises from lowHz to highHz, or null when the window
 * holds no boundary. Zeros count as positive, so the zero padding in front of
 * a window adds no crossings. Must stay self-contained: it is also sent to the
 * page as source.
 */
function findStepBoundary(samples, sampleRate, lowHz, highHz) {
    const crossings = [];
    for (let i = 1; i < samples.length; i += 1) {
        const a = samples[i - 1];
        const b = samples[i];
        if ((a < 0) !== (b < 0)) crossings.push(i - 1 + a / (a - b));
    }
    // Half periods are sampleRate / (2 * hz); the geometric mean separates the two tones.
    const threshold = sampleRate / (2 * Math.sqrt(lowHz * highHz));
    const isLong = (c) => crossings[c] - crossings[c - 1] > threshold;
    for (let c = 2; c + 2 < crossings.length; c += 1) {
        const longBefore = isLong(c) && isLong(c - 1);
        const shortBefore = !isLong(c) && !isLong(c - 1);
        const longAfter = isLong(c + 1) && isLong(c + 2);
        const shortAfter = !isLong(c + 1) && !isLong(c + 2);
        if ((longBefore && shortAfter) || (shortBefore && longAfter)) {
            return { index: crossings[c], rising: longBefore };
        }
    }
    return null;
}

/** Track time in seconds of the boundary nearest `estimate` whose direction matches. */
function boundaryTime(rising, estimate) {
    // Boundary k sits at k * STEP_SECONDS; the tone rises into odd steps and falls into even ones.
    const period = 2 * STEP_SECONDS;
    const offset = rising ? STEP_SECONDS : 0;
    return offset + period * Math.round((estimate - offset) / period);
}

/**
 * Moments in streamTime at which the stepped fixture's newer tone overtakes
 * the older one in a db subscription's frames, interpolated linearly between
 * the two frames either side. Silence frames at streamTime zero are skipped.
 */
function toneSwitches(frames) {
    const playing = frames
        .map((f) => f.payload)
        .filter((p) => p?.state === "playing" && p.streamTime > 0)
        .sort((a, b) => a.streamTime - b.streamTime);
    if (playing.length === 0) return [];
    const { minFrequency, maxFrequency, bands } = playing[0];
    const edges = rangeBandEdges(minFrequency, maxFrequency, bands);
    const low = bandOf(edges, STEP_HZ[0]);
    const high = bandOf(edges, STEP_HZ[1]);
    const switches = [];
    for (let i = 1; i < playing.length; i += 1) {
        const before = playing[i - 1].spectrum[high] - playing[i - 1].spectrum[low];
        const after = playing[i].spectrum[high] - playing[i].spectrum[low];
        if ((before < 0) === (after < 0)) continue;
        const span = playing[i].streamTime - playing[i - 1].streamTime;
        switches.push({ at: playing[i - 1].streamTime + span * (before / (before - after)), rising: before < 0 });
    }
    return switches;
}

async function runSpectrumWindowCases(bridge, recorder, fixtures) {
    const subs = [];
    for (const fftSize of TIMING_FFT_SIZES) {
        subs.push(await subscribe(bridge, `Win${fftSize}`, { fftSize, bands: 16, fps: 30, scale: "db", backgroundThrottle: false }));
    }
    const collector = await createEventCollector(bridge, subs.map((s) => s.event), { collectorId: `av-win-${runId}` });
    try {
        await bridge.invoke("playback.playPath", { path: fixtures.stepped48.path });
        // Boundaries fall at 2, 4, 6, 8 and 10 s; even the 65536-point window
        // (1.37 s) is half past the last one well before 11.5 s.
        await sleep(11500);
        const received = await collector.drain();
        const sizes = subs.map((s) => {
            const frames = framesOf(received, s.event);
            const sample = frames.find((f) => f.payload?.state === "playing")?.payload;
            // A window of N samples ending at streamTime holds more of the newer
            // tone than the older one once it is N / 2 samples past the boundary.
            const lag = sample ? sample.fftSize / (2 * sample.sampleRate) : 0;
            const errorsMs = toneSwitches(frames).map((sw) => (sw.at - boundaryTime(sw.rising, sw.at - lag) - lag) * 1000);
            return { fftSize: sample?.fftSize, sampleRate: sample?.sampleRate, lagMs: lag * 1000, errorsMs };
        });
        recorder.assertCase(
            "AV-35 (audio-timing A-V1b) playing the stepped 48 kHz fixture from the start, each db subscription from 4096 to 65536 points sees the dominant band switch N / (2 x sampleRate) after a tone boundary, within +/- 30 ms, at no fewer than four of the five boundaries",
            sizes.every((s) => s.errorsMs.length >= 4 && s.errorsMs.every((e) => Math.abs(e) <= TIMING_TOLERANCE_MS)),
            { sizes },
            { errorsMs: "each within +/- 30, at least 4 per size" },
        );
    } finally {
        await collector.stop();
        for (const s of subs) await unsubscribe(bridge, s.subscriptionId);
    }
}

/** Readings further than this from the moment being judged are left out of the position envelope, in ms. */
const ENVELOPE_SPAN_MS = 500;
/** How long the window-end probe runs per FFT size, in ms. */
const WINDOW_END_MS = 6000;

/**
 * Upper envelope of position readings at page time t: the largest reading
 * carried forward or back to t at one second per second. The position moves
 * in steps of 10 to 30 ms and lags between them, so the envelope follows the
 * tops of the steps.
 */
function positionEnvelope(readings, t) {
    let best = -Infinity;
    for (const r of readings) {
        if (Math.abs(r.t - t) <= ENVELOPE_SPAN_MS) best = Math.max(best, r.p + (t - r.t) / 1000);
    }
    return best;
}

/**
 * Runs in the page for windowMs: reads playback.getPosition back to back and,
 * whenever the latest position puts a tone boundary 0.1 to 0.4 s behind the
 * present, fetches a 0.5 s signed getWaveform window and locates the boundary
 * in it. Times are the page's performance.now(). Must stay self-contained: it
 * is sent as source, with findStepBoundary passed in.
 */
async function windowEndProbe(findBoundary, windowMs, stepSeconds, lowHz, highHz) {
    const readings = [];
    const windows = [];
    const startedAt = performance.now();
    let lastWindowAt = -Infinity;
    while (performance.now() - startedAt < windowMs) {
        const sent = performance.now();
        const pos = await window.fb2k.invoke("playback.getPosition", {});
        const got = performance.now();
        if (typeof pos?.position !== "number") continue;
        readings.push({ t: (sent + got) / 2, p: pos.position, trip: got - sent });
        const phase = pos.position % stepSeconds;
        if (phase < 0.1 || phase > 0.4 || got - lastWindowAt < 60) continue;
        lastWindowAt = got;
        const windowSent = performance.now();
        const res = await window.fb2k.invoke("audio.getWaveform", { duration: 0.5, signed: true });
        if (res?.success !== true || !Array.isArray(res.waveform)) {
            windows.push({ sent: windowSent, error: res?.error ?? "no waveform" });
            continue;
        }
        windows.push({
            sent: windowSent,
            samples: res.waveform.length,
            sampleRate: res.sampleRate,
            boundary: findBoundary(res.waveform, res.sampleRate, lowHz, highHz),
        });
    }
    return { readings, windows };
}

async function runWindowEndCases(bridge, recorder, fixtures) {
    const sizes = [];
    for (const fftSize of TIMING_FFT_SIZES) {
        const sub = await subscribe(bridge, `End${fftSize}`, { fftSize, bands: 16, fps: 30, backgroundThrottle: false });
        try {
            await bridge.invoke("playback.playPath", { path: fixtures.stepped48.path });
            await sleep(1500);
            const probe = await bridge.evaluateValue(
                `(${windowEndProbe.toString()})(${findStepBoundary.toString()}, ${WINDOW_END_MS}, ${STEP_SECONDS}, ${STEP_HZ[0]}, ${STEP_HZ[1]})`,
                true,
                WINDOW_END_MS + invokeTimeoutMs,
            );
            // The host reads its clock when a call arrives, about half a small
            // round trip after the page sent it.
            const halfTrip = median(probe.readings.map((r) => r.trip)) / 2;
            const diffsMs = [];
            for (const w of probe.windows) {
                if (!w.boundary) continue;
                const heard = positionEnvelope(probe.readings, w.sent + halfTrip);
                const tail = (w.samples - w.boundary.index) / w.sampleRate;
                diffsMs.push((boundaryTime(w.boundary.rising, heard - tail) + tail - heard) * 1000);
            }
            sizes.push({
                fftSize,
                windows: probe.windows.length,
                failed: probe.windows.filter((w) => w.error).length,
                withoutBoundary: probe.windows.filter((w) => !w.error && !w.boundary).length,
                measured: diffsMs.length,
                medianMs: diffsMs.length ? median(diffsMs) : null,
                p5Ms: percentile(diffsMs, 5),
                p95Ms: percentile(diffsMs, 95),
            });
        } finally {
            await unsubscribe(bridge, sub.subscriptionId);
        }
    }
    recorder.assertCase(
        "AV-36 (audio-timing A-V1) with one 30 fps subscription of each size from 4096 to 65536 points in turn, the end of a getWaveform window on the stepped fixture, located by the tone boundary inside it, minus the heard position (upper envelope of getPosition) has a median of -20..+5 ms and a p5-p95 span of at most 30 ms over at least 10 windows, and no call fails",
        sizes.every((s) => s.measured >= 10 && s.failed === 0 && s.medianMs >= -20 && s.medianMs <= 5 && s.p95Ms - s.p5Ms <= 30),
        { sizes },
        { medianMs: "-20..5", p95MinusP5Ms: "<= 30", measured: ">= 10", failed: 0 },
    );
}

/** Seek target of the reset case: 0.3 s before the tone boundary at 4 s. */
const RESET_SEEK_SECONDS = 3.7;

/**
 * Playing frames that arrived after actedAt, and the index of the first one
 * past the reset: where streamTime drops below the frame before it, or the
 * first frame when it is already near zero.
 */
function framesAfterReset(frames, actedAt) {
    const after = frames.filter((f) => f.at >= actedAt && f.payload?.state === "playing");
    const reset = after.findIndex(
        (f, i) => f.payload.streamTime < 0.3 && (i === 0 || f.payload.streamTime < after[i - 1].payload.streamTime),
    );
    return { after, reset };
}

/**
 * Checks a 1 s signed window taken shortly after seeking to RESET_SEEK_SECONDS:
 * the window ends at the seek target plus the time played since, which the
 * zeros in front give as the non-zero length, and the tone boundary at 4 s
 * inside it gives independently.
 */
function seekWindowFacts(res) {
    if (res?.success !== true || !Array.isArray(res.waveform)) return { success: false, error: res?.error };
    const samples = res.waveform;
    let zeros = 0;
    while (zeros < samples.length && samples[zeros] === 0) zeros += 1;
    const endByZeros = RESET_SEEK_SECONDS + (samples.length - zeros) / res.sampleRate;
    const found = findStepBoundary(samples, res.sampleRate, STEP_HZ[0], STEP_HZ[1]);
    if (!found) return { success: true, samples: samples.length, zeros, boundary: null };
    const tail = (samples.length - found.index) / res.sampleRate;
    const boundary = boundaryTime(found.rising, endByZeros - tail);
    return { success: true, samples: samples.length, sampleRate: res.sampleRate, zeros, boundary, diffMs: (boundary + tail - endByZeros) * 1000 };
}

async function runResetCases(bridge, recorder, fixtures) {
    const sub = await subscribe(bridge, "Reset", { fftSize: 16384, bands: 16, fps: 30, scale: "db", backgroundThrottle: false });
    const collector = await createEventCollector(bridge, [sub.event], { collectorId: `av-reset-${runId}` });
    const phases = [];
    let seekWindow = null;
    try {
        const actions = [
            ["start", () => bridge.invoke("playback.playPath", { path: fixtures.stepped48.path })],
            ["seek", () => bridge.invoke("playback.setPosition", { position: RESET_SEEK_SECONDS })],
            ["restart", () => bridge.invoke("playback.playPath", { path: fixtures.stepped48.path })],
        ];
        for (const [name, act] of actions) {
            const actedAt = Date.now();
            await act();
            const ran = (r) => {
                const { after, reset } = framesAfterReset(framesOf(r, sub.event), actedAt);
                return reset >= 0 && after.slice(reset).some((f) => f.payload.streamTime >= 0.4);
            };
            await collector.waitFor(ran, { timeoutMs: 4000 });
            if (name === "seek") seekWindow = seekWindowFacts(await bridge.invoke("audio.getWaveform", { duration: 1.0, signed: true }));
            await sleep(700);
            const { after, reset } = framesAfterReset(framesOf(await collector.drain(), sub.event), actedAt);
            const until = reset >= 0 ? after[reset].at + 1000 : Infinity;
            const judged = after.filter((f) => f.at <= until);
            const gaps = judged.slice(1).map((f, i) => f.payload.hostTime - judged[i].payload.hostTime);
            const zeroFrames = reset >= 0 ? after.slice(reset).filter((f) => f.payload.streamTime === 0) : [];
            phases.push({
                name,
                frames: after.length,
                resetFound: reset >= 0,
                maxGapMs: gaps.length ? Math.max(...gaps) : null,
                zeroFrames: zeroFrames.length,
                zeroFramesSilent: zeroFrames.every((f) => isSilence(f.payload.spectrum, "db")),
            });
        }
        recorder.assertCase(
            "AV-37 (audio-timing A-V6) starting the stepped fixture, seeking in it and restarting it, a 16384-point 30 fps db subscription gets frames with no gap over two frame intervals through the first second after each reset, its frames at streamTime zero are silence, and a 1 s getWaveform taken 0.4 s after the seek succeeds with zeros in front and ends where the tone boundary inside it says the seek target plus the time played is, within 30 ms",
            phases.every((p) => p.resetFound && p.maxGapMs !== null && p.maxGapMs <= MISSING_FRAME_GAP_MS && p.zeroFramesSilent) &&
                seekWindow?.success === true &&
                seekWindow.zeros > 0 &&
                seekWindow.boundary === 2 * STEP_SECONDS &&
                Math.abs(seekWindow.diffMs) <= TIMING_TOLERANCE_MS,
            { phases, seekWindow },
            { maxGapMs: `<= ${MISSING_FRAME_GAP_MS.toFixed(1)}`, zeroFramesSilent: true, seekWindow: { zeros: "> 0", boundary: 2 * STEP_SECONDS, diffMs: "within +/- 30" } },
        );
    } finally {
        await collector.stop();
        await unsubscribe(bridge, sub.subscriptionId);
    }
}

async function runLongWindowCases(bridge, recorder, fixtures) {
    const rates = [];
    for (const stepped of [fixtures.stepped44, fixtures.stepped48]) {
        const sub = await subscribe(bridge, `Long${stepped.sampleRate}`, { fftSize: 65536, bands: 16, fps: 30, backgroundThrottle: false });
        const collector = await createEventCollector(bridge, [sub.event], { collectorId: `av-long-${stepped.sampleRate}-${runId}` });
        try {
            const resets = [];
            const steps = [
                () => bridge.invoke("playback.playPath", { path: stepped.path }),
                () => bridge.invoke("playback.setPosition", { position: 20 }),
                () => bridge.invoke("playback.playPath", { path: stepped.path }),
            ];
            for (const step of steps) {
                resets.push(Date.now());
                await step();
                await sleep(10000);
            }
            // Frames of whatever played before the first reset belong to another track.
            const { after, reset } = framesAfterReset(framesOf(await collector.drain(), sub.event), resets[0]);
            const frames = reset >= 0 ? after.slice(reset) : [];
            // The criterion leaves out the first N / sampleRate seconds after each reset.
            const graceMs = (65536 / stepped.sampleRate) * 1000;
            const gaps = [];
            for (let i = 1; i < frames.length; i += 1) {
                const gapMs = frames[i].payload.hostTime - frames[i - 1].payload.hostTime;
                if (gapMs <= MISSING_FRAME_GAP_MS) continue;
                const from = frames[i - 1].at;
                const to = frames[i].at;
                gaps.push({ atMs: from - resets[0], gapMs, inGrace: resets.some((r) => from <= r + graceMs && to >= r) });
            }
            rates.push({
                sampleRate: stepped.sampleRate,
                frameRates: [...new Set(frames.map((f) => f.payload.sampleRate))],
                frames: frames.length,
                gaps,
                missedOutsideGrace: gaps.filter((g) => !g.inGrace).length,
            });
        } finally {
            await collector.stop();
            await unsubscribe(bridge, sub.subscriptionId);
        }
    }
    recorder.assertCase(
        "AV-38 (audio-timing A-V3) a 65536-point 30 fps subscription on the stepped fixture at 44.1 kHz and at 48 kHz, played 10 s, sought to 20 s and played 10 s, then restarted and played 10 s, reports the fixture's sample rate and has no gap over two frame intervals outside the first N / sampleRate seconds after each reset",
        rates.every((r) => r.frames > 0 && r.frameRates.length === 1 && r.frameRates[0] === r.sampleRate && r.missedOutsideGrace === 0),
        { rates },
        { frameRates: "the fixture's sample rate only", missedOutsideGrace: 0 },
    );
}

// Bin output on the playback tier (SPECTRUM_BINS_SPEC.md A-B7, E-B2). Runs last:
// it pauses playback, which the cases before it do not expect.
// playback:trackChanged sends the shared Track: the same keys and handle as the track row of
// playback.getCurrentTrack, and none of the old payload's id and fullPath.
async function runTrackEventCases(bridge, recorder, fixtures) {
    await bridge.invoke("playback.stop", {});
    await sleep(300);
    const collector = await createEventCollector(bridge, ["playback:trackChanged"], { collectorId: `av-track-${runId}` });
    try {
        await bridge.invoke("playback.playPath", { path: fixtures.full.path });
        const received = await collector.waitFor((r) => r.some((e) => e.name === "playback:trackChanged"), {
            timeoutMs: FIRST_FRAME_MS + 1000,
        });
        const event = received.find((e) => e.name === "playback:trackChanged")?.payload ?? null;
        const current = await bridge.invoke("playback.getCurrentTrack", {});
        const row = current?.track ?? null;
        const keys = (o) => Object.keys(o ?? {}).sort().join(",");
        recorder.assertCase(
            "AV-43 playback:trackChanged sends the track as playback.getCurrentTrack answers it: the same keys and handle, with no id or fullPath",
            event !== null &&
                row !== null &&
                keys(event) === keys(row) &&
                typeof event.handle === "string" &&
                event.handle === row.handle &&
                Array.isArray(event.artists) &&
                !("id" in event) &&
                !("fullPath" in event),
            { eventKeys: keys(event), rowKeys: keys(row), eventHandle: event?.handle, rowHandle: row?.handle },
            { eventKeys: "the keys of getCurrentTrack().track", handle: "equal", absent: ["id", "fullPath"] },
        );
    } finally {
        await collector.stop();
    }
}

async function runBinsCases(bridge, recorder, fixtures) {
    const base = { output: "bins", fps: 10, backgroundThrottle: false };
    const mix = await subscribe(bridge, "BinsMix", { ...base, fftSize: 8192 });
    const small = await subscribe(bridge, "BinsSmall", { ...base, fftSize: 4096, bands: 1024 });
    const large = await subscribe(bridge, "BinsLarge", { ...base, fftSize: 65536 });
    const ranged = await subscribe(bridge, "BinsRange", { ...base, fftSize: 8192, minFrequency: 500, maxFrequency: 2000 });
    const stereo = await subscribe(bridge, "BinsStereo", { ...base, fftSize: 8192, channels: "stereo" });
    const all = [mix, small, large, ranged, stereo];
    const collector = await createEventCollector(
        bridge,
        all.map((sub) => sub.event),
        { collectorId: `av-bins-${runId}` },
    );
    const timing = STEADY_TIMING;
    const steady = (sample, event) =>
        framesOf(sample.received, event)
            .filter(
                (f) =>
                    f.payload?.state === "playing" &&
                    f.at >= sample.startedAt + timing.settleMs &&
                    f.at < sample.startedAt + timing.settleMs + timing.sampleMs,
            )
            .map((f) => f.payload);
    const distinct = (list, key) => [...new Set(list.map(key))];
    const only = (values, expected) => values.length === 1 && values[0] === expected;

    try {
        // 48 kHz, 8192 points: bins are 5.859375 Hz wide, 1 kHz sits between bins 170 and 171.
        const full = await playAndSample(bridge, collector, fixtures.full.path);
        const mixFrames = steady(full, mix.event);
        const peakBin = median(mixFrames.map((p) => p.firstBin + indexOfMax(p.spectrum)));
        const total = median(mixFrames.map((p) => sumDb(p.spectrum)));
        // Recorded for the documentation: the peak bin's own reading and how many
        // bins around it stay within 40 dB of it. A clean sine gives the same pair on
        // every frame, so the per-frame lists show which frames a discontinuity in the
        // analysed audio reached.
        const peakReadings = mixFrames.map((p) => p.spectrum[indexOfMax(p.spectrum)]);
        const lobeWidths = mixFrames.map((p) => {
            const top = indexOfMax(p.spectrum);
            let lo = top;
            let hi = top;
            while (lo > 0 && p.spectrum[lo - 1] > p.spectrum[top] - 40) lo -= 1;
            while (hi < p.spectrum.length - 1 && p.spectrum[hi + 1] > p.spectrum[top] - 40) hi += 1;
            return hi - lo + 1;
        });
        const peakReading = median(peakReadings);
        const lobeWidth = median(lobeWidths);
        const shapeOf = (event) => {
            const list = steady(full, event);
            return {
                frames: list.length,
                fftSize: distinct(list, (p) => p.fftSize),
                firstBin: distinct(list, (p) => p.firstBin),
                length: distinct(list, (p) => p.spectrum?.length),
            };
        };
        const smallShape = shapeOf(small.event);
        const largeShape = shapeOf(large.event);
        const rangedShape = shapeOf(ranged.event);

        recorder.assertCase(
            "AV-41 (spectrum bins A-B7, E-B2) on the 48 kHz 0 dBFS 1 kHz fixture an 8192-point bin subscription peaks at bin 171 +/- 1 and its bins sum to 0 +/- 0.5 dB as powers; a 4096-point one asking for 1024 bands keeps fftSize 4096 and sends bins 2 to 2047, a 65536-point one sends 32740 bins from bin 28, and one limited to 500-2000 Hz sends 256 bins from bin 86",
            mixFrames.length > 0 &&
                Math.abs(peakBin - 171) <= 1 &&
                Math.abs(total) <= 0.5 &&
                smallShape.frames > 0 &&
                only(smallShape.fftSize, 4096) &&
                only(smallShape.firstBin, 2) &&
                only(smallShape.length, 2046) &&
                largeShape.frames > 0 &&
                only(largeShape.fftSize, 65536) &&
                only(largeShape.firstBin, 28) &&
                only(largeShape.length, 32740) &&
                rangedShape.frames > 0 &&
                only(rangedShape.firstBin, 86) &&
                only(rangedShape.length, 256),
            {
                mix: {
                    frames: mixFrames.length,
                    peakBin,
                    total,
                    peakReading,
                    lobeWidth,
                    perFrame: mixFrames.map((p, i) => ({ streamTime: p.streamTime, peak: peakReadings[i], lobe: lobeWidths[i] })),
                },
                small: smallShape,
                large: largeShape,
                ranged: rangedShape,
            },
            {
                mix: { peakBin: "171 +/- 1", total: "0 +/- 0.5 dB" },
                small: { fftSize: 4096, firstBin: 2, length: 2046 },
                large: { fftSize: 65536, firstBin: 28, length: 32740 },
                ranged: { firstBin: 86, length: 256 },
            },
        );

        // Left channel 0 dBFS 1 kHz, right channel digital silence. A host without bin
        // output sends band frames, which carry neither array: the case has to fail on
        // them rather than throw and end the run.
        const hasStereoBins = (p) => Array.isArray(p?.left) && Array.isArray(p?.right);
        const split = await playAndSample(bridge, collector, fixtures.stereo.path);
        const stereoFrames = steady(split, stereo.event);
        const stereoShaped = stereoFrames.length > 0 && stereoFrames.every(hasStereoBins);
        const leftTotal = stereoShaped ? median(stereoFrames.map((p) => sumDb(p.left))) : NaN;
        const rightMax = stereoShaped ? Math.max(...stereoFrames.map((p) => Math.max(...p.right))) : NaN;
        const channelCounts = distinct(stereoFrames, (p) => p.channelCount);
        const last = stereoFrames.at(-1);

        await collector.drain();
        const pausedAt = Date.now();
        await bridge.invoke("playback.pause", {});
        await sleep(500 + QUIET_MS);
        const afterPause = framesOf(await collector.drain(), stereo.event).filter((f) => f.at >= pausedAt);
        const paused = afterPause.filter((f) => f.payload?.state === "paused");
        const silence = paused[0]?.payload;
        const silenceOk =
            paused.length === 1 &&
            afterPause.length === 1 &&
            hasStereoBins(silence) &&
            silence.firstBin === last?.firstBin &&
            silence.left.length === last?.left.length &&
            silence.right.length === last?.right.length &&
            isSilence(silence.left, "db") &&
            isSilence(silence.right, "db");

        recorder.assertCase(
            "AV-42 (spectrum bins A-B7) a stereo bin subscription on the left-only fixture reads 0 +/- 0.5 dB summed over the left bins, no right bin above -100 dB and channelCount 2; pausing sends exactly one paused frame that keeps the previous firstBin and lengths with every value at -160, and nothing after it",
            stereoShaped &&
                Math.abs(leftTotal) <= 0.5 &&
                rightMax <= -100 &&
                only(channelCounts, 2) &&
                silenceOk,
            {
                frames: stereoFrames.length,
                withBothArrays: stereoFrames.filter(hasStereoBins).length,
                leftTotal,
                rightMax,
                channelCounts,
                pause: {
                    frames: afterPause.length,
                    paused: paused.length,
                    firstBin: silence?.firstBin,
                    lengths: [silence?.left?.length, silence?.right?.length],
                    previous: { firstBin: last?.firstBin, lengths: [last?.left?.length, last?.right?.length] },
                },
            },
            {
                leftTotal: "0 +/- 0.5 dB",
                rightMax: "<= -100 dB",
                channelCounts: [2],
                pause: { frames: 1, paused: 1, firstBin: "as before", lengths: "as before", values: -160 },
            },
        );
    } finally {
        await collector.stop();
        for (const sub of all) await unsubscribe(bridge, sub.subscriptionId);
    }
}

function recordPlaybackTierDisabled(recorder) {
    recorder.addCase(
        "AV-10 (A1.2–A1.5, A1.7, A1.9, A3.1, A3.1b, A3.2, A4.1, A4.4, A6.5; audio-timing A-V1, A-V1b, A-V3, A-V6; spectrum bins A-B7) playback tier",
        true,
        { skipped: "FB2K_E2E_AUDIO_VIZ_PLAYBACK is not 1" },
        { skipped: "set FB2K_E2E_AUDIO_VIZ_PLAYBACK=1 with the user's consent: it stops playback, plays sine fixtures muted and leaves the instance stopped" },
    );
}

// ---------------------------------------------------------------------------

const recorder = createRecorder();
let client;
let bridge;
let blocked = false;
let fatalError;
let targets;
const entry = {};
const cleanup = { scratchIndex: null, muted: null, activePlaylist: null };

try {
    const connection = await connectBridgePage();
    client = connection.client;
    targets = connection.candidates;
    console.log(`Target: ${connection.page.title} ${connection.page.url}`);
    bridge = createBridge(client.Runtime, { invokeTimeoutMs });
    await requireResponsiveBridge(bridge, { probeId: "audio-viz" });

    entry.playback = await playbackState(bridge);
    entry.pageVisible = (await bridge.evaluateValue("document.visibilityState", false, 2000)) === "visible";
    const entryDebug = await debugState(bridge);
    entry.subscriptionCount = entryDebug.subscriptionCount;
    entry.framesComputed = entryDebug.framesComputed;
    entry.foregroundIsExternal = entryDebug.foregroundIsExternal;

    await runRefusalCases(bridge, recorder);
    await runRangeRefusalCases(bridge, recorder);
    await runRegistrationCases(bridge, recorder, entryDebug);
    await runGetSpectrumRefusalCases(bridge, recorder);
    const shape = await runFrameShapeCases(bridge, recorder, entry.playback);
    await runRangeRegistrationCases(bridge, recorder, entry.playback);
    await runBinsContractCases(bridge, recorder, entry.playback);
    await runFrameDeliveryCases(bridge, recorder, entry.playback, entry.pageVisible, shape);
    await runUnsubscribeCases(bridge, recorder, entryDebug);
    await runBeatStateCases(bridge, recorder);
    let waveformFixtures = null;
    try {
        waveformFixtures = ensureFixtures();
    } catch (error) {
        if (!error?.blocked) throw error;
        recorder.addCase(
            "AV-17 (A2.1, A2.2, A2.4) full-track waveform cases",
            true,
            { skipped: error.message },
            { skipped: "needs a writable FB2K_E2E_FIXTURE_DIR" },
        );
    }
    if (waveformFixtures) await runFullWaveformCases(bridge, recorder, waveformFixtures);
    await runWaveformCancelRefusalCases(bridge, recorder);
    await runWaveformParamRefusalCases(bridge, recorder);
    const queueTracks = await pickQueueTracks(bridge);
    if (queueTracks.length === QUEUE_TRACKS) {
        await runWaveformQueueCases(bridge, recorder, queueTracks);
    } else {
        recorder.addCase(
            "AV-21–AV-24 (A2.5, A2.6, D7, §4.6) full-track waveform queue cases: skipped, not run",
            true,
            { skipped: `the media library has ${queueTracks.length} matching local FLAC tracks` },
            { skipped: `needs ${QUEUE_TRACKS} local FLAC tracks of 280-420 s in the media library` },
        );
    }

    if (!PLAYBACK_TIER) {
        recordPlaybackTierDisabled(recorder);
    } else if (!entry.pageVisible) {
        throw blockedError("the bridge page is hidden; the playback tier needs delivered frames", { visibilityState: "hidden" });
    } else {
        const fixtures = ensureFixtures();
        entry.fixtures = Object.fromEntries(Object.entries(fixtures).map(([k, v]) => [k, { path: v.path, reused: v.reused }]));

        const volume = await bridge.invoke("playback.getVolume", {});
        cleanup.muted = volume?.muted === true;
        const active = await bridge.invoke("playlist.getActive", {});
        cleanup.activePlaylist = typeof active?.index === "number" ? active.index : null;
        const created = await bridge.invoke("playlist.create", { name: SCRATCH_PLAYLIST });
        cleanup.scratchIndex = created?.index ?? null;
        await bridge.invoke("playlist.setActive", { playlist: cleanup.scratchIndex });
        await bridge.invoke("playback.mute", { muted: true });

        const subs = await runFirstFrameCases(bridge, recorder, fixtures);
        await runIndependenceCases(bridge, recorder, fixtures);
        await runCalibrationCases(bridge, recorder, fixtures);
        await runRangeCases(bridge, recorder, fixtures);
        await runTransitionCases(bridge, recorder, subs);
        for (const s of [subs.main, subs.mainDb]) await unsubscribe(bridge, s.subscriptionId);
        await runThrottleCases(bridge, recorder, fixtures);
        await runPushBeatCases(bridge, recorder, fixtures);
        await runBeatChangeCases(bridge, recorder, fixtures);
        await runPullCases(bridge, recorder, fixtures);
        await runLiveWaveformCases(bridge, recorder, fixtures);
        await runSpectrumWindowCases(bridge, recorder, fixtures);
        await runWindowEndCases(bridge, recorder, fixtures);
        await runResetCases(bridge, recorder, fixtures);
        await runLongWindowCases(bridge, recorder, fixtures);
        await runBinsCases(bridge, recorder, fixtures);
        await runTrackEventCases(bridge, recorder, fixtures);
    }
} catch (error) {
    fatalError = error;
    blocked = Boolean(error?.blocked);
    console.error(`${blocked ? "BLOCKED" : "ERROR"}: ${error?.message || error}`);
} finally {
    if (bridge) {
        const quiet = (method, params) => bridge.invokeRaw(method, params).catch(() => undefined);
        for (const id of [...ownedSubscriptions]) await quiet("audio.unsubscribeSpectrum", { subscriptionId: id });
        if (PLAYBACK_TIER) {
            await quiet("playback.stop", {});
            if (cleanup.scratchIndex !== null) await quiet("playlist.remove", { playlist: cleanup.scratchIndex });
            if (cleanup.activePlaylist !== null) await quiet("playlist.setActive", { playlist: cleanup.activePlaylist });
            if (cleanup.muted !== null) await quiet("playback.mute", { muted: cleanup.muted });
        }
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
            playbackTier: PLAYBACK_TIER,
            entryState: entry,
            targets,
        },
    }),
);

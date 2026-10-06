/**
 * Covers replaygain.* - the global ReplayGain settings and the three endpoints
 * that read or strip the gain stored in the files themselves.
 *
 * The settings half is a read-then-write-back round trip over the live
 * configuration. It is worth testing against the config namespace at the same
 * time, because config.getReplaygainMode / setReplaygainMode reach the same
 * setting through a different spelling - names here, numbers there - and the two
 * answer a call with no argument differently, one as a no-op and one as a
 * refusal, though neither changes the setting (RG-06).
 *
 * The per-file half runs against copies in the library fixture area, never
 * against the user's own music: clear rewrites tags. The copies are built by
 * editing the VORBIS_COMMENT block out of band, so the ReplayGain values a case
 * asserts were never produced by the endpoint under test.
 *
 * scan is driven through its refusals only (RG-17). Its success path hands the
 * files to foobar2000's own scanner through the context menu, and whether that
 * scanner's progress window blocks the message pump has not been established -
 * it is the open question M-43 on the manual checklist. Both refusals return
 * before the menu is built, so they cost nothing.
 *
 * The refusal in RG-16 comes from the path-security layer rather than from this
 * namespace. The case asserts the error code and the parameter it names;
 * the full wording belongs to that shared layer and is only recorded.
 *
 * Usage: node mcp/tests/e2e-replaygain.mjs
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
import { resolveFixtureArea } from "./lib/fixture-area.mjs";

const invokeTimeoutMs = envInt("FB2K_E2E_TIMEOUT_MS", 20000);
const runId = new Date().toISOString().replace(/[-:.TZ]/g, "");

const OUTSIDE_PATH = `Z:\\no-such-directory-${runId}\\absent.flac`;
/** t_replaygain_config::source_mode_*, in declaration order. */
const MODE_NUMBERS = { none: 0, track: 1, album: 2, auto: 3 };
/** What setMode accepts, paired with what getMode answers. */
const SOURCE_MODES = [
    ["none", "none"],
    ["track", "track"],
    ["album", "album"],
    ["byPlaybackOrder", "auto"],
];
const PREAMP_LIMIT = 24;

const TAGGED_ENTRIES = [
    ["ARTIST", "ReplayGain Fixture"],
    ["TITLE", "tagged"],
    ["ALBUM", `RG ${runId}`],
    ["REPLAYGAIN_TRACK_GAIN", "-7.25 dB"],
    ["REPLAYGAIN_TRACK_PEAK", "0.812500"],
    ["REPLAYGAIN_ALBUM_GAIN", "-6.50 dB"],
    ["REPLAYGAIN_ALBUM_PEAK", "0.937500"],
];
const BARE_ENTRIES = [
    ["ARTIST", "ReplayGain Fixture"],
    ["TITLE", "bare"],
];
/** Every gain key replaygain.get adds only when the file carries the value. */
const GAIN_KEYS = [
    "trackGain",
    "trackGainRaw",
    "trackPeak",
    "trackPeakRaw",
    "albumGain",
    "albumGainRaw",
    "albumPeak",
    "albumPeakRaw",
];

const resultFor = (response, path) =>
    (response?.results ?? []).find((entry) => entry.path === path);

async function runSettingsReadCases(bridge, recorder) {
    const { invoke } = bridge;

    const [settings, mode, preamp] = await Promise.all([
        invoke("replaygain.getSettings", {}),
        invoke("replaygain.getMode", {}),
        invoke("replaygain.getPreamp", {}),
    ]);

    recorder.assertCase(
        "RG-01 the three read endpoints are three windows onto one configuration, each inside the success envelope",
        settings?.sourceMode === mode?.sourceMode &&
            settings.processingMode === mode.processingMode &&
            settings.preampWithRg === preamp?.withRg &&
            settings.preampWithoutRg === preamp.withoutRg &&
            typeof settings.active === "boolean" &&
            [settings, mode, preamp].every((response) => response?.success === true),
        {
            settings,
            mode,
            preamp,
            success: [settings, mode, preamp].map((r) => r?.success),
        },
        {
            agreement: "getSettings repeats what getMode and getPreamp report",
            success: [true, true, true],
        },
    );

    return { settings, mode, preamp };
}

async function runModeCases(bridge, recorder, entryMode) {
    const { invoke } = bridge;

    // Each mode is set, then read back through both spellings at once.
    const observed = [];
    for (const [sent, expected] of SOURCE_MODES) {
        const applied = await invoke("replaygain.setMode", { sourceMode: sent });
        const [readBack, asNumber, asSettings] = await Promise.all([
            invoke("replaygain.getMode", {}),
            invoke("config.getReplaygainMode", {}),
            invoke("replaygain.getSettings", {}),
        ]);
        observed.push({
            sent,
            expected,
            applied: { success: applied?.success, sourceMode: applied?.sourceMode },
            readBack: readBack?.sourceMode,
            asNumber: asNumber?.mode,
            active: asSettings?.active,
        });
    }

    recorder.assertCase(
        "RG-03 setMode round trips every source mode, and byPlaybackOrder is read back under the different name auto",
        observed.every(
            (row) =>
                row.applied.success === true &&
                row.applied.sourceMode === row.expected &&
                row.readBack === row.expected,
        ),
        { observed },
        { each: "setMode echoes and getMode reports the canonical name for what was sent" },
    );

    recorder.assertCase(
        "RG-04 the name this namespace uses and the number the config namespace uses denote the same mode",
        observed.every((row) => row.asNumber === MODE_NUMBERS[row.expected]),
        {
            pairs: observed.map((row) => ({ name: row.readBack, number: row.asNumber })),
            table: MODE_NUMBERS,
        },
        { pairs: "each name maps to its declaration-order number" },
    );

    // is_active() is computed, and it does not simply mean "a mode is set":
    // byPlaybackOrder is a real selection that still reads inactive.
    const activeByMode = Object.fromEntries(observed.map((row) => [row.expected, row.active]));
    await invoke("replaygain.setMode", { sourceMode: "album", processingMode: "none" });
    const processingNone = await invoke("replaygain.getSettings", {});
    recorder.assertCase(
        "RG-02 active is derived rather than stored: a processing mode of none turns it off, and so does byPlaybackOrder despite being a real source mode",
        activeByMode.none === false &&
            activeByMode.track === true &&
            activeByMode.album === true &&
            activeByMode.auto === false &&
            processingNone?.active === false &&
            processingNone.sourceMode === "album",
        { activeByMode, withProcessingNone: processingNone },
        {
            activeByMode: { none: false, track: true, album: true, auto: false },
            withProcessingNone: { active: false, sourceMode: "album" },
        },
    );

    await invoke("replaygain.setMode", {
        sourceMode: "album",
        processingMode: entryMode.processingMode,
    });
    // The declaration lists the accepted names, so the generated parser refuses anything else
    // before the handler runs, and the setting stays where the previous call put it.
    const unknown = await invoke("replaygain.setMode", { sourceMode: `nonsense-${runId}` });
    const afterUnknown = await invoke("replaygain.getMode", {});
    recorder.assertCase(
        "RG-05 an unrecognised mode name is refused with INVALID_PARAMS and leaves the mode alone",
        unknown?.success === false &&
            unknown.code === "INVALID_PARAMS" &&
            /sourceMode has an unsupported value/.test(unknown.error ?? "") &&
            afterUnknown?.sourceMode === "album",
        { unknown, afterUnknown },
        {
            unknown: { success: false, code: "INVALID_PARAMS", error: "names sourceMode and the value" },
            afterUnknown: { sourceMode: "album" },
        },
    );

    // Both endpoints write the same setting and neither treats an omitted argument
    // as an instruction: setMode answers it as a no-op, setReplaygainMode refuses
    // it because it needs a mode by number or by name.
    await invoke("replaygain.setMode", { sourceMode: "album" });
    const setModeNoArgs = await invoke("replaygain.setMode", {});
    const afterSetMode = await invoke("replaygain.getMode", {});
    const configNoArgs = await invoke("config.setReplaygainMode", {});
    const afterConfig = await invoke("replaygain.getMode", {});
    recorder.assertCase(
        "RG-06 an omitted argument leaves this one setting alone on both endpoints: setMode answers it as a no-op, setReplaygainMode refuses it with INVALID_PARAMS",
        setModeNoArgs?.success === true &&
            setModeNoArgs.changed === false &&
            afterSetMode?.sourceMode === "album" &&
            configNoArgs?.success === false &&
            configNoArgs.code === "INVALID_PARAMS" &&
            configNoArgs.error === "mode or sourceMode is required" &&
            afterConfig?.sourceMode === "album",
        {
            "replaygain.setMode": { response: setModeNoArgs, modeAfter: afterSetMode?.sourceMode },
            "config.setReplaygainMode": { response: configNoArgs, modeAfter: afterConfig?.sourceMode },
        },
        {
            "replaygain.setMode": { changed: false, modeAfter: "album" },
            "config.setReplaygainMode": {
                success: false,
                code: "INVALID_PARAMS",
                error: "mode or sourceMode is required",
                modeAfter: "album",
            },
        },
    );
}

async function runPreampCases(bridge, recorder) {
    const { invoke } = bridge;

    // The declaration limits both keys to plus or minus 24 dB; the generated parser refuses a
    // value outside before the handler runs, so nothing changes.
    const before = await invoke("replaygain.getPreamp", {});
    const high = await invoke("replaygain.setPreamp", { withRg: 99 });
    const low = await invoke("replaygain.setPreamp", { withoutRg: -99 });
    const afterRefusal = await invoke("replaygain.getPreamp", {});
    recorder.assertCase(
        "RG-07 setPreamp refuses a value outside plus or minus 24 dB with INVALID_PARAMS and leaves both keys alone",
        high?.success === false &&
            high.code === "INVALID_PARAMS" &&
            /withRg is out of range/.test(high.error ?? "") &&
            low?.success === false &&
            low.code === "INVALID_PARAMS" &&
            /withoutRg is out of range/.test(low.error ?? "") &&
            afterRefusal?.withRg === before?.withRg &&
            afterRefusal.withoutRg === before.withoutRg,
        { before, high, low, afterRefusal },
        {
            each: { success: false, code: "INVALID_PARAMS", error: "names the key and says out of range" },
            afterRefusal: "equals the reading before the two calls",
        },
    );

    // Each key can be written on its own, and the limit itself is accepted.
    const edgeHigh = await invoke("replaygain.setPreamp", { withRg: PREAMP_LIMIT });
    const edgeLow = await invoke("replaygain.setPreamp", { withoutRg: -PREAMP_LIMIT });
    const exact = await invoke("replaygain.setPreamp", { withRg: 3.5, withoutRg: -2.25 });
    const afterExact = await invoke("replaygain.getPreamp", {});
    const noArgs = await invoke("replaygain.setPreamp", {});
    recorder.assertCase(
        "RG-07b the limit itself is stored, and a call with one key leaves the other where it was",
        edgeHigh?.success === true &&
            edgeHigh.withRg === PREAMP_LIMIT &&
            edgeLow?.success === true &&
            edgeLow.withoutRg === -PREAMP_LIMIT &&
            edgeLow.withRg === PREAMP_LIMIT,
        { edgeHigh, edgeLow },
        { edgeHigh: { withRg: PREAMP_LIMIT }, edgeLow: { withoutRg: -PREAMP_LIMIT, withRg: PREAMP_LIMIT } },
    );
    recorder.assertCase(
        "RG-08 a value inside the range is stored as given, and a call with no argument succeeds as a no-op that echoes what is already set",
        exact?.success === true &&
            exact.changed === true &&
            afterExact?.withRg === 3.5 &&
            afterExact.withoutRg === -2.25 &&
            noArgs?.success === true &&
            noArgs.changed === false &&
            noArgs.withRg === 3.5 &&
            noArgs.withoutRg === -2.25,
        { exact, afterExact, noArgs },
        {
            exact: { changed: true, stored: { withRg: 3.5, withoutRg: -2.25 } },
            noArgs: { success: true, changed: false, echo: "the current values" },
        },
    );
}

async function runFileReadCases(bridge, recorder, tagged, bare) {
    const { invoke, invokeRaw } = bridge;

    const taggedRead = await invoke("replaygain.get", { paths: [tagged] });
    const row = resultFor(taggedRead, tagged);
    recorder.assertCase(
        "RG-09 get reports each stored value twice - a display string and the raw number - and flags the file as carrying ReplayGain",
        taggedRead?.success === true &&
            taggedRead.count === 1 &&
            row?.success === true &&
            row.hasReplayGain === true &&
            row.trackGain === "-7.25 dB" &&
            row.trackGainRaw === -7.25 &&
            row.albumGain === "-6.50 dB" &&
            row.albumGainRaw === -6.5 &&
            row.trackPeak === "0.812500" &&
            row.trackPeakRaw === 0.8125 &&
            row.albumPeak === "0.937500" &&
            row.albumPeakRaw === 0.9375,
        { row },
        {
            gains: "two decimals with a dB suffix beside the float",
            peaks: "six decimals beside the float",
            hasReplayGain: true,
        },
    );

    const bareRead = await invoke("replaygain.get", { paths: [bare] });
    const bareRow = resultFor(bareRead, bare);
    recorder.assertCase(
        "RG-10 a file with no ReplayGain is a success carrying the flag false, with every gain key absent rather than null or zero",
        bareRead?.success === true &&
            bareRow?.success === true &&
            bareRow.hasReplayGain === false &&
            GAIN_KEYS.every((key) => !(key in bareRow)),
        { bareRow, keys: Object.keys(bareRow ?? {}).sort() },
        { hasReplayGain: false, keys: ["hasReplayGain", "path", "success"] },
    );

    const batch = await invoke("replaygain.get", { paths: [tagged, bare, OUTSIDE_PATH] });
    const outsideRow = resultFor(batch, OUTSIDE_PATH);
    recorder.assertCase(
        "RG-11 a batch answers one row per path in the order it was given, and the envelope succeeds however the individual rows turned out",
        batch?.success === true &&
            batch.count === 3 &&
            batch.results.map((entry) => entry.path).join("|") ===
                [tagged, bare, OUTSIDE_PATH].join("|") &&
            batch.results.filter((entry) => entry.success === true).length === 2,
        {
            count: batch?.count,
            order: batch?.results?.map((entry) => entry.path),
            perRowSuccess: batch?.results?.map((entry) => entry.success),
        },
        { count: 3, order: "as sent", perRowSuccess: [true, true, false] },
    );

    recorder.assertCase(
        "RG-12 a path the host cannot read is a per-path failure, not an envelope failure - the read tier does not refuse it the way the write tier does",
        outsideRow?.success === false &&
            /failed to get track info/i.test(outsideRow.error ?? "") &&
            !("code" in outsideRow),
        { outsideRow },
        {
            success: false,
            error: "Failed to get track info",
            codePresent: false,
        },
    );

    const notArray = await invokeRaw("replaygain.get", { paths: "nope" });
    const nonString = await invokeRaw("replaygain.get", { paths: [42] });
    recorder.assertCase(
        "RG-13 the bridge's own parameter check refuses a non-array and a non-string element before the handler sees them, naming the offending position",
        notArray?.value?.success === false &&
            notArray.value.code === "INVALID_PARAMS" &&
            /must be an array/i.test(notArray.value.error ?? "") &&
            nonString?.value?.success === false &&
            nonString.value.code === "INVALID_PARAMS" &&
            /paths\[0\] must be a string/i.test(nonString.value.error ?? ""),
        { notArray: notArray?.value, nonString: nonString?.value },
        { each: { success: false, code: "INVALID_PARAMS" } },
    );

    const [getEmpty, clearEmpty, scanEmpty, getMissing, clearMissing, scanMissing] =
        await Promise.all([
            invoke("replaygain.get", { paths: [] }),
            invoke("replaygain.clear", { paths: [] }),
            invoke("replaygain.scan", { paths: [] }),
            invoke("replaygain.get", {}),
            invoke("replaygain.clear", {}),
            invoke("replaygain.scan", {}),
        ]);
    // paths is required everywhere; get accepts an empty list (nothing to read), clear and scan
    // declare at least one item. All three refusals come from the generated parser.
    recorder.assertCase(
        "RG-14 a missing paths array is INVALID_PARAMS on all three endpoints; an empty one reads as nothing for get and is refused for clear and scan",
        getEmpty?.success === true &&
            getEmpty.count === 0 &&
            clearEmpty?.success === false &&
            clearEmpty.code === "INVALID_PARAMS" &&
            /paths must have at least 1 item/i.test(clearEmpty.error ?? "") &&
            scanEmpty?.success === false &&
            scanEmpty.code === "INVALID_PARAMS" &&
            /paths must have at least 1 item/i.test(scanEmpty.error ?? "") &&
            [getMissing, clearMissing, scanMissing].every(
                (res) => res?.success === false && res.code === "INVALID_PARAMS" && /paths is required/i.test(res.error ?? ""),
            ),
        {
            empty: { get: getEmpty, clear: clearEmpty, scan: scanEmpty },
            missing: { get: getMissing, clear: clearMissing, scan: scanMissing },
        },
        {
            empty: { get: "success with count 0", clearAndScan: "INVALID_PARAMS: paths must have at least 1 item(s)" },
            missing: { all: "INVALID_PARAMS: paths is required" },
        },
    );
}

async function runClearCases(bridge, recorder, tagged) {
    const { invoke } = bridge;

    const cleared = await invoke("replaygain.clear", { paths: [tagged] });

    // update_info_async is asynchronous and emits no bridge event, so the only
    // signal is the file reading back without the values.
    const deadline = Date.now() + 20000;
    let after;
    for (;;) {
        after = await invoke("replaygain.get", { paths: [tagged] });
        if (resultFor(after, tagged)?.hasReplayGain === false) break;
        if (Date.now() >= deadline) break;
        await new Promise((resolve) => setTimeout(resolve, 250));
    }
    const row = resultFor(after, tagged);
    recorder.assertCase(
        "RG-15 clear reports how many files it queued, and the values are gone from the file when it is read again",
        cleared?.success === true &&
            cleared.clearedCount === 1 &&
            row?.success === true &&
            row.hasReplayGain === false &&
            GAIN_KEYS.every((key) => !(key in row)),
        { cleared, afterRead: row },
        {
            cleared: { success: true, clearedCount: 1 },
            afterRead: { hasReplayGain: false, gainKeys: "all absent" },
        },
    );

    // The refusal here belongs to the path-security tier, not to this namespace,
    // and that tier is under active change - the code and the parameter it names
    // are pinned, the wording is only recorded.
    const outside = await invoke("replaygain.clear", { paths: [OUTSIDE_PATH] });
    recorder.assertCase(
        "RG-16 clearing a path outside the trusted media context is refused by the write tier before the handler runs, unlike the same path under get",
        outside?.success === false &&
            outside.code === "PERMISSION_DENIED" &&
            /paths\[0\]/.test(outside.error ?? ""),
        { outside },
        {
            success: false,
            code: "PERMISSION_DENIED",
            error: "names paths[0]; the wording belongs to the path-security tier and is not pinned here",
        },
    );
}

function recordScanBoundary(recorder) {
    recorder.addCase(
        "RG-17 scan is driven through its refusals only, because its success path hands the files to foobar2000's own scanner",
        true,
        {
            covered: "the missing-array and empty-array refusals, both of which return before the context menu is built",
            notCovered:
                "scanCmd->execute(), which starts the bundled ReplayGain scanner",
            reason:
                "whether that scanner's progress window blocks the message pump is still open - manual checklist M-43",
        },
        { recorded: "an environment question, not an API limitation" },
    );
}

const recorder = createRecorder();
let client;
let bridge;
let blocked = false;
let fatalError;
let targets;
let area;
let entry;

try {
    const connection = await connectBridgePage();
    client = connection.client;
    targets = connection.candidates;
    console.log(`Target: ${connection.page.title} ${connection.page.url}`);
    bridge = createBridge(client.Runtime, { invokeTimeoutMs });
    await requireResponsiveBridge(bridge, { probeId: runId });

    const resolved = await resolveFixtureArea(bridge, { runId });
    area = resolved.area;
    const tagged = area.taggedAudio(`rg-tagged-${runId}.flac`, TAGGED_ENTRIES).targetPath;
    const bare = area.taggedAudio(`rg-bare-${runId}.flac`, BARE_ENTRIES).targetPath;

    const reads = await runSettingsReadCases(bridge, recorder);
    entry = {
        mode: reads.mode,
        preamp: reads.preamp,
        fixtureDir: area.dir,
        source: resolved.source.absolutePath,
    };

    await runModeCases(bridge, recorder, reads.mode);
    await runPreampCases(bridge, recorder);
    await runFileReadCases(bridge, recorder, tagged, bare);
    await runClearCases(bridge, recorder, tagged);
    recordScanBoundary(recorder);

    const [finalMode, finalPreamp] = await Promise.all([
        bridge.invoke("replaygain.setMode", {
            sourceMode: entry.mode.sourceMode,
            processingMode: entry.mode.processingMode,
        }),
        bridge.invoke("replaygain.setPreamp", {
            withRg: entry.preamp.withRg,
            withoutRg: entry.preamp.withoutRg,
        }),
    ]);
    const settlement = await bridge.invoke("replaygain.getSettings", {});
    recorder.assertCase(
        "RG-18 the settings the suite found on entry are the ones it leaves behind",
        finalMode?.success === true &&
            finalPreamp?.success === true &&
            settlement?.sourceMode === entry.mode.sourceMode &&
            settlement.processingMode === entry.mode.processingMode &&
            settlement.preampWithRg === entry.preamp.withRg &&
            settlement.preampWithoutRg === entry.preamp.withoutRg,
        { entry: { mode: entry.mode, preamp: entry.preamp }, settlement },
        { settlement: "equals the entry reading" },
    );
} catch (error) {
    fatalError = error;
    blocked = Boolean(error?.blocked);
    console.error(`${blocked ? "BLOCKED" : "ERROR"}: ${error?.message || error}`);
} finally {
    if (bridge && entry) {
        const quiet = (method, params) => bridge.invokeRaw(method, params).catch(() => undefined);
        await quiet("replaygain.setMode", {
            sourceMode: entry.mode.sourceMode,
            processingMode: entry.mode.processingMode,
        });
        await quiet("replaygain.setPreamp", {
            withRg: entry.preamp.withRg,
            withoutRg: entry.preamp.withoutRg,
        });
    }
    if (area) area.remove();
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
            entryState: entry,
            targets,
        },
    }),
);

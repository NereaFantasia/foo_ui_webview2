/**
 * Covers the DSP chain (dsp.*), the output module and device listings
 * (output.*), and the config endpoints that read or write the same two things
 * (config.getOutputConfig, getOutputDevices, setOutputDevice, setOutputBuffer,
 * getDspPresets, getActiveDspPreset, setActiveDspPreset).
 *
 * They belong together because they are three views of one subject - what the
 * audio passes through on its way out - and the suite's main job is to pin where
 * the views disagree. The same DSP presets come with a selected index and a
 * per-entry active flag under dsp and with neither under config; the same output
 * devices carry entry/entryGuid under output.getDevices and
 * outputId/deviceId/isCurrent under config.getOutputDevices.
 *
 * Three success paths are deliberately not exercised, because the bridge has no
 * way back from them (AC-21):
 *   - dsp.setChain rebuilds every entry from its default preset, and getChain
 *     reports only guid and name, so a chain whose processors were configured
 *     cannot be snapshotted and put back;
 *   - dsp.applyPreset and config.setActiveDspPreset select a preset, and neither
 *     accepts the "nothing selected" reading the instance starts from.
 * All three refuse before they touch anything, so this suite drives them through
 * their refusals only and leaves the success paths to the manual checklist.
 *
 * What is exercised for real is restorable by construction: addDsp, moveDsp and
 * removeDsp each operate on a copy of the live chain, so the processors already
 * in it keep their own configuration, and an add followed by a remove of the same
 * entry returns the chain to what it was. setOutputBuffer is read first and
 * written back.
 *
 * Enumeration order can differ between calls. Listings are compared as
 * multisets so a reordered result does not fail when its entries and their
 * multiplicities agree.
 *
 * Usage: node mcp/tests/e2e-audio-chain.mjs
 * Environment: FB2K_CDP_PORT, FB2K_E2E_TIMEOUT_MS.
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

const invokeTimeoutMs = envInt("FB2K_E2E_TIMEOUT_MS", 20000);

/** Well-formed, and deliberately not the GUID of any installed DSP. */
const UNKNOWN_GUID = "{DEADBEEF-0000-4000-8000-000000000000}";
const MALFORMED_GUID = "not-a-guid";
/** The bounds SetOutputBuffer enforces, in seconds. */
const BUFFER_MIN = 0.05;
const BUFFER_MAX = 2.0;

const guidsOf = (chain) => (chain?.dsps ?? []).map((entry) => entry.guid);
const sorted = (values) => [...values].sort();
const sameMultiset = (a, b) =>
    a.length === b.length && sorted(a).every((value, i) => value === sorted(b)[i]);

/** Whether a response is the failure envelope with exactly this code and message. */
function refusedWith(response, code, error) {
    return response?.success === false && response.code === code && response.error === error;
}

async function runPresetReadCases(bridge, recorder) {
    const { invoke } = bridge;

    const [available, presets, cfgPresets, chain, cfgActive] = await Promise.all([
        invoke("dsp.getAvailable", {}),
        invoke("dsp.getPresets", {}),
        invoke("config.getDspPresets", {}),
        invoke("dsp.getChain", {}),
        invoke("config.getActiveDspPreset", {}),
    ]);

    recorder.assertCase(
        "AC-01 getAvailable lists every installed processor with a GUID and whether it has a config popup, inside the success envelope",
        available?.success === true &&
            Array.isArray(available.dsps) &&
            available.dsps.length > 0 &&
            available.count === available.dsps.length &&
            available.dsps.every(
                (entry) =>
                    /^\{[0-9A-F-]{36}\}$/i.test(entry.guid) &&
                    typeof entry.name === "string" &&
                    entry.name.length > 0 &&
                    typeof entry.hasConfig === "boolean",
            ),
        {
            success: available?.success,
            count: available?.count,
            keys: Object.keys(available?.dsps?.[0] ?? {}).sort(),
        },
        {
            success: true,
            count: "equals the list length",
            keys: ["guid", "hasConfig", "name"],
        },
    );

    // Same presets, two shapes: both endpoints wrap the list with a count, but
    // only dsp.getPresets adds a selected index and a per-entry active flag.
    recorder.assertCase(
        "AC-02 the two preset listings describe the same presets inside their envelopes, and only the dsp one carries a selection and per-entry active flags",
        cfgPresets?.success === true &&
            Array.isArray(cfgPresets.presets) &&
            cfgPresets.count === cfgPresets.presets.length &&
            !("selectedIndex" in cfgPresets) &&
            Array.isArray(presets?.presets) &&
            presets.count === presets.presets.length &&
            sameMultiset(
                cfgPresets.presets.map((entry) => entry.name),
                presets.presets.map((entry) => entry.name),
            ) &&
            presets.presets.every((entry) => typeof entry.active === "boolean") &&
            cfgPresets.presets.every((entry) => !("active" in entry)),
        {
            config: { success: cfgPresets?.success, count: cfgPresets?.count, presets: cfgPresets?.presets },
            dsp: { count: presets?.count, selectedIndex: presets?.selectedIndex },
            dspKeys: Object.keys(presets?.presets?.[0] ?? {}).sort(),
            configKeys: Object.keys(cfgPresets?.presets?.[0] ?? {}).sort(),
        },
        {
            names: "the same set under both endpoints",
            config: { success: true, count: "equals the list length", selectedIndex: "absent" },
            dspKeys: ["active", "index", "name"],
            configKeys: ["index", "name"],
        },
    );

    // Three endpoints spell "no preset is selected" three different ways.
    const nothingSelected = presets?.selectedIndex === -1;
    recorder.assertCase(
        "AC-03 an unselected preset reads as index minus one, a null active preset and an inactive flag, depending on which endpoint is asked",
        nothingSelected
            ? chain?.activePresetIndex === -1 &&
              chain.activePreset === null &&
              cfgActive?.isActive === false &&
              cfgActive.index === null &&
              cfgActive.name === null
            : chain?.activePresetIndex === presets?.selectedIndex &&
              cfgActive?.isActive === true &&
              cfgActive.index === presets?.selectedIndex,
        {
            getPresets: { selectedIndex: presets?.selectedIndex },
            getChain: {
                activePreset: chain?.activePreset,
                activePresetIndex: chain?.activePresetIndex,
            },
            getActiveDspPreset: cfgActive,
        },
        nothingSelected
            ? {
                  selectedIndex: -1,
                  activePreset: null,
                  activePresetIndex: -1,
                  isActive: false,
              }
            : { allThree: "agree on the selected index" },
    );

    return { available, entryChain: chain };
}

async function runPresetRefusalCases(bridge, recorder) {
    const { invoke } = bridge;

    const [neither, unknownName, badIndex] = await Promise.all([
        invoke("dsp.applyPreset", {}),
        invoke("dsp.applyPreset", { name: "no-such-preset" }),
        invoke("dsp.applyPreset", { index: 9999 }),
    ]);
    recorder.assertCase(
        "AC-04 applyPreset refuses with neither name nor index, with a name no preset has, and with an index past the end - each before anything is selected",
        neither?.success === false &&
            neither.code === "INVALID_PARAMS" &&
            /name or index/i.test(neither.error ?? "") &&
            unknownName?.success === false &&
            unknownName.code === "NOT_FOUND" &&
            /not found/i.test(unknownName.error ?? "") &&
            unknownName.error.includes("no-such-preset") &&
            badIndex?.success === false &&
            badIndex.code === "INVALID_INDEX" &&
            /invalid preset index/i.test(badIndex.error ?? ""),
        { neither, unknownName, badIndex },
        {
            neither: { code: "INVALID_PARAMS", error: "names both accepted keys" },
            unknownName: { code: "NOT_FOUND", error: "echoes the name it could not find" },
            badIndex: { code: "INVALID_INDEX", error: "Invalid preset index" },
        },
    );

    // The generated parser refuses what the declaration rules out - a missing, a
    // negative and a non-integer index - before the handler runs; only an index
    // past the end of the list reaches the handler, which refuses it as INVALID_INDEX.
    const [noIndex, outOfRange, negative, notANumber] = await Promise.all([
        invoke("config.setActiveDspPreset", {}),
        invoke("config.setActiveDspPreset", { index: 9999 }),
        invoke("config.setActiveDspPreset", { index: -1 }),
        invoke("config.setActiveDspPreset", { index: "one" }),
    ]);
    recorder.assertCase(
        "AC-05 setActiveDspPreset refuses a missing, a past-the-end, a negative and a non-integer index with a failure envelope that names the rule each one broke",
        refusedWith(noIndex, "INVALID_PARAMS", "index is required") &&
            refusedWith(outOfRange, "INVALID_INDEX", "Invalid preset index") &&
            refusedWith(negative, "INVALID_PARAMS", "index is out of range") &&
            refusedWith(notANumber, "INVALID_PARAMS", "index must be an integer"),
        { noIndex, outOfRange, negative, notANumber },
        {
            noIndex: { code: "INVALID_PARAMS", error: "index is required" },
            outOfRange: { code: "INVALID_INDEX", error: "Invalid preset index" },
            negative: { code: "INVALID_PARAMS", error: "index is out of range" },
            notANumber: { code: "INVALID_PARAMS", error: "index must be an integer" },
        },
    );

    const stillInactive = await invoke("config.getActiveDspPreset", {});
    recorder.assertCase(
        "AC-06 none of those refusals selected a preset",
        stillInactive?.isActive === false &&
            stillInactive.index === null &&
            stillInactive.name === null,
        { stillInactive },
        { isActive: false, index: null, name: null },
    );
}

async function runChainRefusalCases(bridge, recorder, entryChain) {
    const { invoke } = bridge;

    const [noGuid, malformed, unknown] = await Promise.all([
        invoke("dsp.addDsp", {}),
        invoke("dsp.addDsp", { guid: MALFORMED_GUID }),
        invoke("dsp.addDsp", { guid: UNKNOWN_GUID }),
    ]);
    recorder.assertCase(
        "AC-07 addDsp separates a missing GUID from a malformed one and from a well-formed GUID no installed processor owns",
        noGuid?.success === false &&
            noGuid.code === "INVALID_PARAMS" &&
            /guid is required/i.test(noGuid.error ?? "") &&
            malformed?.success === false &&
            malformed.code === "INVALID_PARAMS" &&
            /invalid guid format/i.test(malformed.error ?? "") &&
            unknown?.success === false &&
            unknown.code === "NOT_FOUND" &&
            /not found or no default preset/i.test(unknown.error ?? ""),
        { noGuid, malformed, unknown },
        {
            noGuid: { code: "INVALID_PARAMS", error: "guid is required" },
            malformed: { code: "INVALID_PARAMS", error: "Invalid GUID format" },
            unknown: { code: "NOT_FOUND", error: "DSP not found or no default preset" },
        },
    );

    const [noIndex, badIndex, noFrom, noTo, badRange, same] = await Promise.all([
        invoke("dsp.removeDsp", {}),
        invoke("dsp.removeDsp", { index: 9999 }),
        invoke("dsp.moveDsp", { to: 0 }),
        invoke("dsp.moveDsp", { from: 0 }),
        invoke("dsp.moveDsp", { from: 0, to: 9999 }),
        invoke("dsp.moveDsp", { from: 0, to: 0 }),
    ]);
    recorder.assertCase(
        "AC-08 removeDsp and moveDsp each name the index key they are missing, refuse one past the end, and treat a move onto itself as a success that says it did nothing",
        noIndex?.success === false &&
            noIndex.code === "INVALID_PARAMS" &&
            /^index is required/i.test(noIndex.error ?? "") &&
            badIndex?.success === false &&
            badIndex.code === "INVALID_INDEX" &&
            /out of range/i.test(badIndex.error ?? "") &&
            noFrom?.success === false &&
            noFrom.code === "INVALID_PARAMS" &&
            /^from is required/i.test(noFrom.error ?? "") &&
            noTo?.success === false &&
            noTo.code === "INVALID_PARAMS" &&
            /^to is required/i.test(noTo.error ?? "") &&
            badRange?.success === false &&
            badRange.code === "INVALID_INDEX" &&
            /out of range/i.test(badRange.error ?? "") &&
            same?.success === true &&
            same.message === "No change needed" &&
            same.from === 0 &&
            same.to === 0 &&
            !("movedDsp" in same),
        { noIndex, badIndex, noFrom, noTo, badRange, same },
        {
            missingKeys: "the generated parser names from, to or index with INVALID_PARAMS",
            outOfRange: { error: "Index out of range", code: "INVALID_INDEX" },
            selfMove: { success: true, message: "No change needed", from: 0, to: 0, movedDspPresent: false },
        },
    );

    const [noDsps, notArray, nonObject, entryNoGuid, emptyGuid, entryBadGuid, entryUnknown] =
        await Promise.all([
            invoke("dsp.setChain", {}),
            invoke("dsp.setChain", { dsps: "nope" }),
            invoke("dsp.setChain", { dsps: [42] }),
            invoke("dsp.setChain", { dsps: [{}] }),
            invoke("dsp.setChain", { dsps: [{ guid: "" }] }),
            invoke("dsp.setChain", { dsps: [{ guid: MALFORMED_GUID }] }),
            invoke("dsp.setChain", { dsps: [{ guid: UNKNOWN_GUID }] }),
        ]);
    recorder.assertCase(
        "AC-09 setChain rejects the whole call on the first unusable entry and tags the message with that entry's position, so a partial chain is never applied",
        noDsps?.success === false &&
            noDsps.code === "INVALID_PARAMS" &&
            /^dsps is required/i.test(noDsps.error ?? "") &&
            notArray?.success === false &&
            notArray.code === "INVALID_PARAMS" &&
            /^dsps must be an array/i.test(notArray.error ?? "") &&
            [nonObject, entryNoGuid, emptyGuid, entryBadGuid, entryUnknown].every(
                (res) => res?.success === false && res.error?.startsWith("dsps[0]"),
            ) &&
            /must be an object/i.test(nonObject.error ?? "") &&
            /guid is required/i.test(entryNoGuid.error ?? "") &&
            /guid is required/i.test(emptyGuid.error ?? "") &&
            entryBadGuid.code === "INVALID_PARAMS" &&
            entryBadGuid.error.includes(MALFORMED_GUID) &&
            entryUnknown.code === "NOT_FOUND" &&
            entryUnknown.error.includes(UNKNOWN_GUID),
        { noDsps, notArray, nonObject, entryNoGuid, emptyGuid, entryBadGuid, entryUnknown },
        {
            missingOrNotArray: { code: "INVALID_PARAMS", error: "dsps is required / dsps must be an array" },
            perEntry: "prefixed dsps[0] and echoing the offending GUID",
            emptyGuid: "indistinguishable from a missing one",
        },
    );

    const afterRefusals = await invoke("dsp.getChain", {});
    recorder.assertCase(
        "AC-10 the chain is byte-for-byte what it was before any of those refusals",
        sameMultiset(guidsOf(afterRefusals), guidsOf(entryChain)) &&
            guidsOf(afterRefusals).join() === guidsOf(entryChain).join(),
        { before: guidsOf(entryChain), after: guidsOf(afterRefusals) },
        { after: "the same GUIDs in the same order" },
    );
}

async function runChainRoundTripCases(bridge, recorder, entryChain, candidate, cleanup) {
    const { invoke } = bridge;

    if (!candidate) {
        recorder.addCase(
            "AC-11 addDsp appends at the tail and reports the position it landed at",
            true,
            { skipped: "every installed processor is already in the chain" },
            { skipped: "needs one installed processor the chain does not hold" },
        );
        return;
    }

    const entryLength = guidsOf(entryChain).length;
    // Recorded before the call, not after it: if anything below throws, the
    // teardown still knows there is one entry to take back out.
    cleanup.addedGuid = candidate.guid;
    const added = await invoke("dsp.addDsp", { guid: candidate.guid });
    const afterAdd = await invoke("dsp.getChain", {});
    const tailIndex = guidsOf(afterAdd).length - 1;
    recorder.assertCase(
        "AC-11 addDsp with no position appends at the tail, names the processor it added and reports the index it landed at",
        added?.success === true &&
            added.addedDsp === candidate.name &&
            added.position === entryLength &&
            guidsOf(afterAdd).length === entryLength + 1 &&
            guidsOf(afterAdd)[tailIndex] === candidate.guid &&
            guidsOf(afterAdd).slice(0, entryLength).join() === guidsOf(entryChain).join(),
        { added, chainAfterAdd: guidsOf(afterAdd), entryChain: guidsOf(entryChain) },
        {
            added: { success: true, position: entryLength },
            chain: "the entry chain unchanged, with the new processor after it",
        },
    );

    const toHead = await invoke("dsp.moveDsp", { from: tailIndex, to: 0 });
    const afterMove = await invoke("dsp.getChain", {});
    const backToTail = await invoke("dsp.moveDsp", { from: 0, to: tailIndex });
    const afterMoveBack = await invoke("dsp.getChain", {});
    recorder.assertCase(
        "AC-12 moveDsp relocates one processor and shifts the rest along, and moving it back reproduces the previous order exactly",
        toHead?.success === true &&
            toHead.from === tailIndex &&
            toHead.to === 0 &&
            guidsOf(afterMove)[0] === candidate.guid &&
            guidsOf(afterMove).slice(1).join() === guidsOf(entryChain).join() &&
            backToTail?.success === true &&
            guidsOf(afterMoveBack).join() === guidsOf(afterAdd).join(),
        {
            toHead,
            afterMove: guidsOf(afterMove),
            backToTail,
            afterMoveBack: guidsOf(afterMoveBack),
        },
        {
            afterMove: "the moved processor first, the others in their original order",
            afterMoveBack: "identical to the chain before the move",
        },
    );

    const removed = await invoke("dsp.removeDsp", { index: tailIndex });
    const afterRemove = await invoke("dsp.getChain", {});
    recorder.assertCase(
        "AC-13 removeDsp drops the named index, reports what it dropped, and leaves the chain the suite started with",
        removed?.success === true &&
            removed.removedDsp === candidate.name &&
            removed.removedIndex === tailIndex &&
            guidsOf(afterRemove).join() === guidsOf(entryChain).join(),
        { removed, afterRemove: guidsOf(afterRemove), entryChain: guidsOf(entryChain) },
        {
            removed: { success: true, removedIndex: tailIndex },
            afterRemove: "the entry chain, restored",
        },
    );

    if (guidsOf(afterRemove).join() === guidsOf(entryChain).join()) {
        cleanup.addedGuid = null;
    }
}

async function runOutputReadCases(bridge, recorder) {
    const { invoke } = bridge;

    const [entries, settings, devices, cfgConfig, cfgDevices] = await Promise.all([
        invoke("output.getEntries", {}),
        invoke("output.getSettings", {}),
        invoke("output.getDevices", {}),
        invoke("config.getOutputConfig", {}),
        invoke("config.getOutputDevices", {}),
    ]);

    recorder.assertCase(
        "AC-14 getEntries pairs every output module with a GUID and its five capability flags, inside the success envelope",
        entries?.success === true &&
            Array.isArray(entries.entries) &&
            entries.entries.length > 0 &&
            entries.count === entries.entries.length &&
            entries.entries.every(
                (entry) =>
                    /^\{[0-9A-F-]{36}\}$/i.test(entry.guid) &&
                    typeof entry.name === "string" &&
                    ["needsBitdepthConfig", "needsDitherConfig", "supportsMultipleStreams", "isHighLatency", "isLowLatency"].every(
                        (flag) => typeof entry[flag] === "boolean",
                    ),
            ),
        {
            success: entries?.success,
            count: entries?.count,
            keys: Object.keys(entries?.entries?.[0] ?? {}).sort(),
        },
        {
            success: true,
            keys: [
                "guid",
                "isHighLatency",
                "isLowLatency",
                "name",
                "needsBitdepthConfig",
                "needsDitherConfig",
                "supportsMultipleStreams",
            ],
        },
    );

    // getSettings carries no settings: it is the same module list reduced to
    // display names, which is why two modules can both read "default" and one
    // can read empty. Compared as a multiset because the enumeration order
    // differs between calls.
    recorder.assertCase(
        "AC-15 getSettings reports no actual setting - it is the getEntries list flattened to names, duplicates and blanks included",
        settings?.success === true &&
            Array.isArray(settings.availableOutputs) &&
            typeof settings.note === "string" &&
            sameMultiset(
                settings.availableOutputs,
                entries.entries.map((entry) => entry.name),
            ),
        {
            success: settings?.success,
            availableOutputs: settings?.availableOutputs,
            entryNames: entries.entries.map((entry) => entry.name),
            duplicateNames:
                settings?.availableOutputs?.length !==
                new Set(settings?.availableOutputs).size,
        },
        {
            success: true,
            availableOutputs: "the same multiset of names getEntries reports",
            note: "points at getEntries for name and GUID pairs",
        },
    );

    // The same devices under two shapes: output.getDevices groups them by the
    // module that owns them, config.getOutputDevices flattens and marks one.
    const cfgDeviceList = cfgDevices?.devices ?? [];
    const current = cfgDeviceList.filter((device) => device.isCurrent);
    recorder.assertCase(
        "AC-16 both device listings enumerate the same devices inside their envelopes, and only the config one says which is in effect - agreeing with getOutputConfig",
        cfgDevices?.success === true &&
            Array.isArray(cfgDevices.devices) &&
            cfgDevices.count === cfgDeviceList.length &&
            devices?.count === cfgDeviceList.length &&
            sameMultiset(
                devices.devices.map((device) => device.guid),
                cfgDeviceList.map((device) => device.deviceId),
            ) &&
            devices.devices.every((device) => !("isCurrent" in device)) &&
            current.length === 1 &&
            current[0].outputId === cfgConfig?.outputId &&
            current[0].deviceId === cfgConfig?.deviceId,
        {
            outputCount: devices?.count,
            configCount: cfgDevices?.count,
            configListed: cfgDeviceList.length,
            outputKeys: Object.keys(devices?.devices?.[0] ?? {}).sort(),
            configKeys: Object.keys(cfgDeviceList[0] ?? {}).sort(),
            current,
            outputConfig: { outputId: cfgConfig?.outputId, deviceId: cfgConfig?.deviceId },
        },
        {
            counts: "equal, and the config count equals its list length",
            outputKeys: ["entry", "entryGuid", "guid", "name"],
            configKeys: ["deviceId", "id", "isCurrent", "name", "outputId"],
            current: "exactly one, matching getOutputConfig",
        },
    );

    return cfgConfig;
}

async function runOutputWriteCases(bridge, recorder, entryConfig) {
    const { invoke, invokeRaw } = bridge;

    const inSeconds = await invokeRaw("config.setOutputBuffer", { bufferLength: 0.75 });
    const afterSeconds = await invoke("config.getOutputConfig", {});
    const inMs = await invokeRaw("config.setOutputBuffer", { milliseconds: 900 });
    const afterMs = await invoke("config.getOutputConfig", {});
    const both = await invokeRaw("config.setOutputBuffer", {
        milliseconds: 300,
        bufferLength: 1.5,
    });
    const afterBoth = await invoke("config.getOutputConfig", {});
    recorder.assertCase(
        "AC-17 setOutputBuffer takes seconds or milliseconds, and milliseconds wins silently when a caller passes both",
        inSeconds?.value?.success === true &&
            afterSeconds?.bufferLength === 0.75 &&
            inMs?.value?.success === true &&
            Math.abs(afterMs?.bufferLength - 0.9) < 1e-9 &&
            both?.value?.success === true &&
            Math.abs(afterBoth?.bufferLength - 0.3) < 1e-9,
        {
            seconds: { sent: 0.75, read: afterSeconds?.bufferLength },
            milliseconds: { sent: 900, read: afterMs?.bufferLength },
            both: { sent: { milliseconds: 300, bufferLength: 1.5 }, read: afterBoth?.bufferLength },
        },
        {
            seconds: 0.75,
            milliseconds: 0.9,
            both: { read: 0.3, note: "the seconds key is ignored, without a warning" },
        },
    );

    // The declaration puts the range on each key, so the generated parser names the
    // key that broke it; a call with neither key is refused by the handler.
    const [tooSmall, tooLarge, missing, msTooSmall] = await Promise.all([
        invoke("config.setOutputBuffer", { bufferLength: 0.01 }),
        invoke("config.setOutputBuffer", { bufferLength: 5 }),
        invoke("config.setOutputBuffer", {}),
        invoke("config.setOutputBuffer", { milliseconds: 10 }),
    ]);
    const atMin = await invokeRaw("config.setOutputBuffer", { bufferLength: BUFFER_MIN });
    const afterMin = await invoke("config.getOutputConfig", {});
    recorder.assertCase(
        "AC-18 the bounds are refused rather than clamped, under the name of the key that broke them, a call with neither key is refused as such, and the lower bound itself is accepted",
        refusedWith(tooSmall, "INVALID_PARAMS", "bufferLength is out of range") &&
            refusedWith(tooLarge, "INVALID_PARAMS", "bufferLength is out of range") &&
            refusedWith(missing, "INVALID_PARAMS", "bufferLength or milliseconds is required") &&
            refusedWith(msTooSmall, "INVALID_PARAMS", "milliseconds is out of range") &&
            atMin?.value?.success === true &&
            afterMin?.bufferLength === BUFFER_MIN,
        { tooSmall, tooLarge, missing, msTooSmall, atMin, readAtMin: afterMin?.bufferLength },
        {
            outOfRange: { code: "INVALID_PARAMS", error: "bufferLength / milliseconds is out of range" },
            noArgument: { code: "INVALID_PARAMS", error: "bufferLength or milliseconds is required" },
            lowerBound: { accepted: true, read: BUFFER_MIN },
            upperBound: BUFFER_MAX,
        },
    );

    const restore = await invokeRaw("config.setOutputBuffer", {
        bufferLength: entryConfig.bufferLength,
    });
    const afterRestore = await invoke("config.getOutputConfig", {});
    recorder.assertCase(
        "AC-19 the buffer length the suite found on entry is the one it leaves behind",
        restore?.value?.success === true &&
            afterRestore?.bufferLength === entryConfig.bufferLength,
        { entry: entryConfig.bufferLength, afterRestore: afterRestore?.bufferLength },
        { afterRestore: entryConfig.bufferLength },
    );

    // Only the device already in effect is written: switching to another one can
    // land on a backend that is exclusive-mode or absent, and the refusal path
    // for that is not reachable on demand.
    const [noIds, onlyOutput, badGuid] = await Promise.all([
        invoke("config.setOutputDevice", {}),
        invoke("config.setOutputDevice", { outputId: entryConfig.outputId }),
        invoke("config.setOutputDevice", {
            outputId: MALFORMED_GUID,
            deviceId: MALFORMED_GUID,
        }),
    ]);
    const same = await invokeRaw("config.setOutputDevice", {
        outputId: entryConfig.outputId,
        deviceId: entryConfig.deviceId,
    });
    const afterSame = await invoke("config.getOutputConfig", {});
    recorder.assertCase(
        "AC-20 setOutputDevice needs both ids and names the one that is missing, refuses a malformed GUID, and accepts writing back the device already in effect without changing anything",
        refusedWith(noIds, "INVALID_PARAMS", "outputId is required") &&
            refusedWith(onlyOutput, "INVALID_PARAMS", "deviceId is required") &&
            refusedWith(badGuid, "INVALID_PARAMS", "Invalid GUID format") &&
            same?.value?.success === true &&
            afterSame?.outputId === entryConfig.outputId &&
            afterSame?.deviceId === entryConfig.deviceId,
        { noIds, onlyOutput, badGuid, same, afterSame: { outputId: afterSame?.outputId, deviceId: afterSame?.deviceId } },
        {
            noIds: { code: "INVALID_PARAMS", error: "outputId is required" },
            onlyOutput: { code: "INVALID_PARAMS", error: "deviceId is required" },
            badGuid: { code: "INVALID_PARAMS", error: "Invalid GUID format" },
            same: { success: true, config: "unchanged" },
        },
    );
}

function recordUnreachablePaths(recorder) {
    recorder.addCase(
        "AC-21 three success paths are out of reach from an automated run, and are recorded rather than attempted",
        true,
        {
            "dsp.setChain":
                "rebuilds each entry from its default preset while getChain reports only guid and name, so a configured chain cannot be snapshotted and restored",
            "dsp.applyPreset":
                "select_preset has no argument for the unselected state the instance starts from",
            "config.setActiveDspPreset":
                "same one-way selection, reached through a different endpoint",
        },
        { recorded: "an API limitation, covered by the manual checklist" },
    );
}

const recorder = createRecorder();
let client;
let bridge;
let blocked = false;
let fatalError;
let targets;
let entry;
let entryChain;
let entryConfig;
const cleanup = { addedGuid: null };

try {
    const connection = await connectBridgePage();
    client = connection.client;
    targets = connection.candidates;
    console.log(`Target: ${connection.page.title} ${connection.page.url}`);
    bridge = createBridge(client.Runtime, { invokeTimeoutMs });
    await requireResponsiveBridge(bridge, { probeId: "audio-chain" });

    const reads = await runPresetReadCases(bridge, recorder);
    entryChain = reads.entryChain;
    const inChain = new Set(guidsOf(entryChain));
    const candidate = (reads.available?.dsps ?? []).find((dsp) => !inChain.has(dsp.guid));
    entry = {
        chain: guidsOf(entryChain),
        installedProcessors: reads.available?.count,
        candidate: candidate?.name,
    };

    await runPresetRefusalCases(bridge, recorder);
    await runChainRefusalCases(bridge, recorder, entryChain);
    await runChainRoundTripCases(bridge, recorder, entryChain, candidate, cleanup);

    entryConfig = await runOutputReadCases(bridge, recorder);
    await runOutputWriteCases(bridge, recorder, entryConfig);
    recordUnreachablePaths(recorder);
} catch (error) {
    fatalError = error;
    blocked = Boolean(error?.blocked);
    console.error(`${blocked ? "BLOCKED" : "ERROR"}: ${error?.message || error}`);
} finally {
    if (bridge) {
        const quiet = (method, params) => bridge.invokeRaw(method, params).catch(() => undefined);
        // setChain is not used to put the chain back: it would rebuild the
        // processors that were already there from their defaults. The one entry
        // this suite adds is removed by its current index instead, which leaves
        // every other entry's configuration untouched.
        if (cleanup.addedGuid) {
            const chain = await bridge.invoke("dsp.getChain", {}).catch(() => undefined);
            const index = guidsOf(chain).indexOf(cleanup.addedGuid);
            if (index >= 0) await quiet("dsp.removeDsp", { index });
        }
        if (entryConfig) {
            await quiet("config.setOutputBuffer", { bufferLength: entryConfig.bufferLength });
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
            entryState: entry,
            targets,
        },
    }),
);

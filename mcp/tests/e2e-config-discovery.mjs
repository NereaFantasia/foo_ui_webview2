/**
 * Covers the settings surface and the host-introspection surface: the plugin's
 * own key-value store, the foobar2000 settings it proxies, and the discovery
 * endpoints that enumerate what the host has loaded.
 *
 * Nothing here is verified against another call to the same endpoint. The
 * key-value cases assert against values this script chose; the enumeration cases
 * cross-check endpoints that answer the same question from different code paths
 * (config.getComponents against discovery.getComponents, the aggregate count
 * against the individual listings) or against filesystem facts node can see for
 * itself.
 *
 * Every setting written here is a real user preference that survives a restart:
 * the two follow-cursor toggles, the ReplayGain source mode, this component's
 * dev-server URL advanced entry, and any key written through config.set, which
 * persists via fb2k's configStore. Each is recorded on entry and written back in
 * the finally block, and the scratch key is removed.
 * The context-menu case additionally selects one track, because the endpoint
 * builds its tree against a selection; that selection is also restored.
 *
 * Left out: config.export beyond its shape (it mirrors the same store), the
 * setters for output device and buffer (switching a device mid-playback is
 * audible and can fail against exclusive-mode hardware), DSP preset activation
 * (it rewrites the user's chain), the showXxx endpoints and misc.exit/restart
 * (they open windows or end the process), advanced-config writes to any entry
 * but the dev-server URL (they change host-wide settings with no reliable undo),
 * and executeMainMenuCommand/executeContextMenuCommand (they run arbitrary host
 * commands).
 *
 * Usage: node mcp/tests/e2e-config-discovery.mjs
 * Environment: FB2K_CDP_PORT, FB2K_E2E_TIMEOUT_MS.
 */

import { existsSync, statSync } from "node:fs";
import { join } from "node:path";

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

const invokeTimeoutMs = envInt("FB2K_E2E_TIMEOUT_MS", 25000);
const runId = new Date().toISOString().replace(/[-:.TZ]/g, "");
const SCRATCH_KEY = `e2e.probe.${runId}`;

// t_replaygain_config::source_mode_*, in SDK declaration order.
const RG_NONE = 0;
const RG_TRACK = 1;
const RG_ALBUM = 2;
const RG_BY_PLAYBACK_ORDER = 3;

/** Well-formed, and deliberately not the GUID of any registered config entry. */
const ABSENT_GUID = "{00000000-0000-0000-0000-000000000000}";
/** This component's own dev-server URL, and the checkbox that decides whether anything reads it. */
const DEV_SERVER_URL_GUID = "{A1B2C3D9-E5F6-7890-1234-56789ABCDEF5}";
const DEV_SERVER_TOGGLE_GUID = "{A1B2C3DA-E5F6-7890-1234-56789ABCDEF6}";

/** Flattens the advanced-config tree into one list of every entry, branches included. */
function flattenAdvanced(nodes, into = []) {
    for (const node of nodes ?? []) {
        if (node?.guid && node?.type) into.push(node);
        if (Array.isArray(node?.children)) flattenAdvanced(node.children, into);
    }
    return into;
}

/** Whether a response is the failure envelope with exactly this code and message. */
function refusedWith(response, code, error) {
    return response?.success === false && response.code === code && response.error === error;
}

async function runStoreCases(bridge, recorder) {
    const { invoke } = bridge;

    const missing = await invoke("config.get", { key: `${SCRATCH_KEY}.never.written` });
    recorder.assertCase(
        "CD-01 reading an absent key is a success with found false, not an error",
        missing?.success === true && missing.found === false && missing.value === null,
        missing,
        { success: true, found: false, value: null },
    );

    // The store keeps JSON, so a page can put a structure in a single key rather
    // than flattening it into several.
    const values = [
        ["a string", "hello"],
        ["a number", 42],
        ["a boolean", true],
        ["an object", { a: 1, b: [2, 3], c: { d: null } }],
        ["an array", [1, "two", false]],
    ];
    const roundTrips = [];
    for (const [label, value] of values) {
        const set = await invoke("config.set", { key: SCRATCH_KEY, value });
        const got = await invoke("config.get", { key: SCRATCH_KEY });
        roundTrips.push({
            label,
            setOk: set?.success === true && set.key === SCRATCH_KEY,
            found: got?.found,
            match: JSON.stringify(got?.value) === JSON.stringify(value),
        });
    }
    recorder.assertCase(
        "CD-02 config.set stores each JSON type and config.get returns it unchanged",
        roundTrips.every((entry) => entry.setOk && entry.found === true && entry.match),
        roundTrips,
        { each: { setOk: true, found: true, match: true } },
    );

    const all = await invoke("config.getAll", {});
    recorder.assertCase(
        "CD-03 getAll lists the stored key under both of its field names",
        all?.success === true &&
            all.count >= 1 &&
            JSON.stringify(all.configs) === JSON.stringify(all.items) &&
            Object.prototype.hasOwnProperty.call(all.configs, SCRATCH_KEY),
        { count: all?.count, keys: Object.keys(all?.configs ?? {}), configsEqualItems: JSON.stringify(all?.configs) === JSON.stringify(all?.items) },
        { count: ">= 1", configsEqualItems: true, contains: SCRATCH_KEY },
    );

    const exported = await invoke("config.export", {});
    let exportedParsed;
    try {
        exportedParsed = JSON.parse(exported?.json ?? "");
    } catch {
        exportedParsed = undefined;
    }
    recorder.assertCase(
        "CD-04 export carries the same store as data and as a parseable JSON string",
        exported?.success === true &&
            exported.count === all.count &&
            JSON.stringify(exported.data) === JSON.stringify(all.configs) &&
            JSON.stringify(exportedParsed) === JSON.stringify(all.configs),
        { count: exported?.count, jsonParses: exportedParsed !== undefined },
        { count: all.count, data: "equals getAll", json: "parses to the same object" },
    );

    const removed = await invoke("config.remove", { key: SCRATCH_KEY });
    const afterRemove = await invoke("config.get", { key: SCRATCH_KEY });
    const removedAgain = await invoke("config.remove", { key: SCRATCH_KEY });
    recorder.assertCase(
        "CD-05 remove reports whether the key existed, and is safe to repeat",
        removed?.success === true &&
            removed.existed === true &&
            afterRemove?.found === false &&
            removedAgain?.success === true &&
            removedAgain.existed === false,
        { first: removed, afterRemove: { found: afterRemove?.found }, second: removedAgain },
        { first: { existed: true }, second: { existed: false } },
    );

    const noKey = await Promise.all([
        bridge.invoke("config.set", { value: 1 }),
        bridge.invoke("config.get", {}),
    ]);
    recorder.assertCase(
        "CD-06 set and get both require a key rather than defaulting to one",
        noKey.every((response) => refusedWith(response, "INVALID_PARAMS", "key is required")),
        noKey,
        { each: { success: false, code: "INVALID_PARAMS", error: "key is required" } },
    );
}

async function runSettingsCases(bridge, recorder, entry) {
    const { invoke } = bridge;

    // Two independent toggles with similar names and identical response shapes.
    // Flipping one must not move the other, which is the mistake their names
    // invite.
    const toggles = [
        ["cursorFollowPlayback", "config.getCursorFollowPlayback", "config.setCursorFollowPlayback"],
        ["playbackFollowCursor", "config.getPlaybackFollowCursor", "config.setPlaybackFollowCursor"],
    ];
    const toggleResults = [];
    for (const [label, getter, setter] of toggles) {
        const other = toggles.find((pair) => pair[0] !== label);
        const otherBefore = await invoke(other[1], {});
        const before = await invoke(getter, {});
        const set = await invoke(setter, { enabled: !before.enabled });
        const after = await invoke(getter, {});
        const otherAfter = await invoke(other[1], {});
        await invoke(setter, { enabled: before.enabled });
        const restored = await invoke(getter, {});
        toggleResults.push({
            label,
            aliasAgrees: before.enabled === before.value && after.enabled === after.value,
            setEchoed: set?.success === true && set.enabled === !before.enabled,
            flipped: after.enabled === !before.enabled,
            otherUnchanged: otherBefore.enabled === otherAfter.enabled,
            restored: restored.enabled === before.enabled,
        });
    }
    recorder.assertCase(
        "CD-07 each follow-cursor toggle round-trips without disturbing the other",
        toggleResults.every(
            (entry) =>
                entry.aliasAgrees &&
                entry.setEchoed &&
                entry.flipped &&
                entry.otherUnchanged &&
                entry.restored,
        ),
        toggleResults,
        { each: { aliasAgrees: true, flipped: true, otherUnchanged: true, restored: true } },
    );

    const rgEntry = entry.replaygain.mode;
    const rgOther = rgEntry === RG_TRACK ? RG_ALBUM : RG_TRACK;
    const byNumber = await invoke("config.setReplaygainMode", { mode: rgOther });
    const afterNumber = await invoke("config.getReplaygainMode", {});
    recorder.assertCase(
        "CD-08 the ReplayGain source mode round-trips by number under both field names",
        byNumber?.success === true &&
            byNumber.mode === rgOther &&
            byNumber.value === rgOther &&
            afterNumber?.mode === rgOther &&
            afterNumber.value === afterNumber.mode,
        { requested: rgOther, response: byNumber, read: afterNumber },
        { mode: rgOther, value: "equals mode" },
    );

    // The named form maps onto the same four modes. "auto" and "byPlaybackOrder"
    // are two spellings of one mode, which is worth pinning because a page that
    // round-trips a mode through its name has to pick one.
    const named = {
        none: RG_NONE,
        track: RG_TRACK,
        album: RG_ALBUM,
        byPlaybackOrder: RG_BY_PLAYBACK_ORDER,
        auto: RG_BY_PLAYBACK_ORDER,
    };
    const namedResults = [];
    for (const [name, expected] of Object.entries(named)) {
        const set = await invoke("config.setReplaygainMode", { sourceMode: name });
        const read = await invoke("config.getReplaygainMode", {});
        namedResults.push({ name, expected, responseMode: set?.mode, readMode: read?.mode });
    }
    recorder.assertCase(
        "CD-09 every documented mode name selects its mode, with auto an alias of byPlaybackOrder",
        namedResults.every((entry) => entry.responseMode === entry.expected && entry.readMode === entry.expected),
        namedResults,
        { each: "responseMode and readMode equal the mode the name stands for" },
    );

    // A wrong name is refused by the generated parser, and a call that names no
    // mode at all is refused by the handler, so a caller that forgets the key no
    // longer turns ReplayGain off. Neither refusal touches the setting.
    await invoke("config.setReplaygainMode", { mode: rgOther });
    const badName = await invoke("config.setReplaygainMode", { sourceMode: "nonsense" });
    const afterBadName = await invoke("config.getReplaygainMode", {});
    const noArgs = await invoke("config.setReplaygainMode", {});
    const afterNoArgs = await invoke("config.getReplaygainMode", {});
    recorder.assertCase(
        "CD-10 an unknown mode name and a call with no mode at all are both refused with INVALID_PARAMS, and neither moves the mode",
        refusedWith(badName, "INVALID_PARAMS", "sourceMode has an unsupported value 'nonsense'") &&
            afterBadName?.mode === rgOther &&
            refusedWith(noArgs, "INVALID_PARAMS", "mode or sourceMode is required") &&
            afterNoArgs?.mode === rgOther,
        {
            unknownName: { response: badName, modeAfter: afterBadName?.mode },
            noArgument: { response: noArgs, modeAfter: afterNoArgs?.mode },
        },
        {
            unknownName: {
                code: "INVALID_PARAMS",
                error: "sourceMode has an unsupported value 'nonsense'",
                modeAfter: rgOther,
            },
            noArgument: { code: "INVALID_PARAMS", error: "mode or sourceMode is required", modeAfter: rgOther },
        },
    );

    await invoke("config.setReplaygainMode", { mode: rgEntry });
}

async function runHostFactCases(bridge, recorder) {
    const { invoke } = bridge;

    const [version, libraryStatus, foobarPath, profilePath, componentPath, theme, dpi, locale] =
        await Promise.all(
            [
                "config.getVersionInfo",
                "config.getLibraryStatus",
                "misc.getFoobarPath",
                "misc.getProfilePath",
                "misc.getComponentPath",
                "system.getTheme",
                "system.getDPI",
                "system.getLocale",
            ].map((method) => invoke(method, {})),
        );

    // The paths are the one part of this surface node can check independently:
    // they have to exist on disk, and the component directory has to be the one
    // holding the loaded DLL.
    const pathsExist = {
        foobar: existsSync(foobarPath?.path ?? ""),
        profile: existsSync(profilePath?.path ?? ""),
        component: existsSync(componentPath?.path ?? ""),
    };
    const dllPath = join(componentPath?.path ?? "", "foo_ui_webview2.dll");
    recorder.assertCase(
        "CD-11 the reported paths exist on disk and the component directory holds the DLL",
        pathsExist.foobar &&
            pathsExist.profile &&
            pathsExist.component &&
            existsSync(dllPath) &&
            statSync(dllPath).size > 0 &&
            componentPath.path.startsWith(foobarPath.path),
        {
            paths: { foobar: foobarPath?.path, profile: profilePath?.path, component: componentPath?.path },
            exist: pathsExist,
            dllBytes: existsSync(dllPath) ? statSync(dllPath).size : 0,
        },
        { allExist: true, dll: "present and non-empty", componentUnderFoobarPath: true },
    );

    recorder.assertCase(
        "CD-12 each path endpoint reports the same value under path and value",
        foobarPath?.path === foobarPath?.value &&
            profilePath?.path === profilePath?.value &&
            componentPath?.path === componentPath?.value &&
            profilePath.path === version?.profilePath,
        {
            aliasesAgree: true,
            profileFromMisc: profilePath?.path,
            profileFromVersionInfo: version?.profilePath,
        },
        { pathEqualsValue: true, profilePathAgreesWithVersionInfo: true },
    );

    recorder.assertCase(
        "CD-13 getVersionInfo reports the host build and this plugin's own version",
        typeof version?.foobar2000 === "string" &&
            version.foobar2000.includes("foobar2000") &&
            version.version === version.foobar2000 &&
            typeof version?.is64bit === "boolean" &&
            typeof version?.isPortable === "boolean" &&
            version?.plugin?.name === "foo_ui_webview2" &&
            /^\d+\.\d+\.\d+/.test(version.plugin.version ?? ""),
        {
            foobar2000: version?.foobar2000,
            is64bit: version?.is64bit,
            isPortable: version?.isPortable,
            plugin: version?.plugin,
        },
        { foobar2000: "a version string", plugin: { name: "foo_ui_webview2", version: "x.y.z" } },
    );

    // The library count is the same number library.getStats reports, from a
    // different code path, so the two are a real cross-check.
    const stats = await invoke("library.getStats", {});
    recorder.assertCase(
        "CD-14 the library item count agrees with what the library namespace reports",
        libraryStatus?.enabled === true &&
            libraryStatus.initialized === true &&
            libraryStatus.itemCount === stats?.totalTracks,
        { configLibraryStatus: libraryStatus, libraryTotalTracks: stats?.totalTracks },
        { itemCount: "equals library.getStats totalTracks" },
    );

    recorder.assertCase(
        "CD-15 theme, DPI and locale report usable values under both spellings",
        typeof theme?.darkMode === "boolean" &&
            theme.darkMode === theme.isDark &&
            /^#[0-9A-Fa-f]{6}$/.test(theme?.accentColor ?? "") &&
            dpi?.dpi > 0 &&
            Math.abs(dpi.scale - dpi.dpi / 96) < 0.001 &&
            /^[a-z]{2}-[A-Z]{2}$/.test(locale?.locale ?? ""),
        { theme, dpi, locale },
        { darkMode: "equals isDark", accentColor: "#RRGGBB", scale: "dpi / 96", locale: "xx-YY" },
    );
}

async function runIntrospectionCases(bridge, recorder) {
    const { invoke } = bridge;

    const lyricsApis = await invoke("system.getApisByNamespace", { namespace: "lyrics" });
    const emptyNamespace = await invoke("system.getApisByNamespace", { namespace: "nosuchnamespace" });
    const searched = await invoke("system.searchApis", { query: "volume" });
    recorder.assertCase(
        "CD-16 the API index can be listed per namespace and searched by substring",
        lyricsApis?.success === true &&
            Array.isArray(lyricsApis.apis) &&
            lyricsApis.apis.length === 3 &&
            lyricsApis.apis.every((api) => api.namespace === "lyrics" && api.fullName.startsWith("lyrics.")) &&
            emptyNamespace?.success === true &&
            Array.isArray(emptyNamespace.apis) &&
            emptyNamespace.apis.length === 0 &&
            searched?.success === true &&
            Array.isArray(searched.apis) &&
            searched.apis.length > 0 &&
            searched.apis.every((api) => api.fullName.toLowerCase().includes("volume")),
        {
            lyrics: lyricsApis?.apis?.map((api) => api.fullName),
            unknownNamespace: emptyNamespace?.apis,
            searchHits: searched?.apis?.map((api) => api.fullName),
        },
        { lyrics: 3, unknownNamespace: [], searchHits: "all contain the query" },
    );

    // The declared parser answers a missing key with an INVALID_PARAMS envelope.
    const namespaceRequired = await invoke("system.getApisByNamespace", {});
    recorder.assertCase(
        "CD-17 listing by namespace requires the namespace rather than returning everything",
        namespaceRequired?.success === false &&
            namespaceRequired.code === "INVALID_PARAMS" &&
            /namespace/i.test(namespaceRequired.error ?? ""),
        namespaceRequired,
        { success: false, code: "INVALID_PARAMS", error: "mentions the missing namespace" },
    );

    // Two endpoints enumerate loaded components through different services. They
    // have to agree on the set, or one of them is looking at a stale list.
    const [configComponents, discoveryComponents] = await Promise.all([
        invoke("config.getComponents", {}),
        invoke("discovery.getComponents", {}),
    ]);
    const configList = configComponents?.components ?? [];
    const configNames = configList.map((entry) => entry.filename).sort();
    const discoveryNames = (discoveryComponents.components ?? []).map((entry) => entry.filename).sort();
    recorder.assertCase(
        "CD-18 both component listings report the same components, each inside its envelope",
        configComponents?.success === true &&
            configComponents.count === configList.length &&
            configNames.length > 0 &&
            JSON.stringify(configNames) === JSON.stringify(discoveryNames) &&
            configList.every((entry) => entry.filename === entry.fileName),
        {
            success: configComponents?.success,
            count: configComponents?.count,
            listed: configNames.length,
            sameSet: JSON.stringify(configNames) === JSON.stringify(discoveryNames),
        },
        { success: true, count: "equals the list length", sameSet: true, filenameAliasAgrees: true },
    );

    const services = await invoke("discovery.getAllServices", {});
    const [inputs, dspEntries, devices, uiElements, groups, prefPages, contextCommands, mainCommands] =
        await Promise.all([
            invoke("discovery.getInputFormats", {}),
            invoke("discovery.getDspEntries", {}),
            invoke("discovery.getOutputDevices", {}),
            invoke("discovery.getUIElements", {}),
            invoke("discovery.getMainMenuGroups", {}),
            invoke("discovery.getPreferencePages", {}),
            invoke("discovery.getContextMenuCommands", {}),
            invoke("discovery.getMainMenuCommands", {}),
        ]);
    const perEndpoint = {
        inputFormats: inputs.count,
        dspEntries: dspEntries.count,
        outputDevices: devices.count,
        uiElements: uiElements.count,
        mainMenuGroups: groups.count,
        preferencePages: prefPages.count,
        contextMenuCommands: contextCommands.count,
        mainMenuCommands: mainCommands.count,
        components: discoveryNames.length,
        mainMenuDynamicCommands: mainCommands.dynamicCount,
    };
    const aggregateMismatches = Object.entries(perEndpoint).filter(
        ([key, value]) => services.services?.[key] !== value,
    );
    recorder.assertCase(
        "CD-19 the aggregate service counts match what each listing endpoint reports",
        services?.success === true && aggregateMismatches.length === 0,
        { aggregate: services?.services, perEndpoint, mismatches: aggregateMismatches },
        { mismatches: [] },
    );

    // The total is not the sum of the listed counts: dynamic main-menu commands
    // are reported alongside the others but are already inside mainMenuCommands,
    // so adding every field double-counts them.
    const sumOfAll = Object.values(services.services ?? {}).reduce((total, value) => total + value, 0);
    recorder.assertCase(
        "CD-20 totalServices excludes the dynamic commands already counted in the main menu",
        services.totalServices === sumOfAll - services.services.mainMenuDynamicCommands,
        {
            totalServices: services.totalServices,
            sumOfEveryField: sumOfAll,
            mainMenuDynamicCommands: services.services?.mainMenuDynamicCommands,
        },
        { totalServices: "sum of every field minus mainMenuDynamicCommands" },
    );

    // Both filters are off by default and both widen the listing, so a page that
    // renders the default list is not seeing everything the host knows.
    const [contextWithHidden, mainWithHidden, mainCollapsed] = await Promise.all([
        invoke("discovery.getContextMenuCommands", { includeHidden: true }),
        invoke("discovery.getMainMenuCommands", { includeHidden: true }),
        invoke("discovery.getMainMenuCommands", { expandDynamic: false }),
    ]);
    recorder.assertCase(
        "CD-21 includeHidden and expandDynamic each change the listing and are off by default",
        contextCommands.includeHidden === false &&
            contextCommands.hiddenFiltered > 0 &&
            contextWithHidden.count === contextCommands.count + contextCommands.hiddenFiltered &&
            contextWithHidden.hiddenFiltered === 0 &&
            mainWithHidden.count > mainCommands.count &&
            mainCommands.expandDynamic === true &&
            mainCollapsed.expandDynamic === false &&
            mainCollapsed.dynamicCount === 0 &&
            mainCommands.count === mainCollapsed.count + mainCommands.dynamicCount,
        {
            context: {
                visible: contextCommands.count,
                filtered: contextCommands.hiddenFiltered,
                withHidden: contextWithHidden.count,
            },
            main: {
                expanded: mainCommands.count,
                dynamic: mainCommands.dynamicCount,
                collapsed: mainCollapsed.count,
                withHidden: mainWithHidden.count,
            },
        },
        {
            context: { withHidden: "visible plus filtered" },
            main: { expanded: "collapsed plus dynamic" },
        },
    );

    const commandsShape = (mainCommands.commands ?? [])[0];
    recorder.assertCase(
        "CD-22 a listed command carries the identity and state a menu needs to render it",
        typeof commandsShape?.guid === "string" &&
            /^\{[0-9A-Fa-f-]{36}\}$/.test(commandsShape.guid) &&
            typeof commandsShape?.name === "string" &&
            commandsShape.name.length > 0 &&
            typeof commandsShape?.enabled === "boolean" &&
            typeof commandsShape?.executable === "boolean" &&
            typeof commandsShape?.stateKnown === "boolean" &&
            typeof commandsShape?.source === "string",
        { keys: Object.keys(commandsShape ?? {}).sort(), sample: { guid: commandsShape?.guid, name: commandsShape?.name, source: commandsShape?.source } },
        { has: ["guid", "name", "enabled", "executable", "stateKnown", "source"] },
    );

    // searchCommands matches the localized display names, which is what the user
    // sees; a scope narrows which family is searched.
    //
    // The match is asserted by guid, not by name equality. A static parent such
    // as "switch to playlist" shares its guid with one dynamic child per
    // playlist, and each child is named after its playlist - so searching the
    // parent's name returns the children and no result's name equals the query.
    // Which command this picks depends on how many playlists exist, so a name
    // comparison here passes or fails on session state.
    const anyCommand = (mainCommands.commands ?? []).find((command) => command.name?.length > 1);
    const anyCommandName = anyCommand?.name;
    const [searchHit, searchScoped, searchMiss, searchNoQuery] = await Promise.all([
        invoke("discovery.searchCommands", { query: anyCommandName }),
        invoke("discovery.searchCommands", { query: anyCommandName, scope: "mainmenu" }),
        invoke("discovery.searchCommands", { query: "zzzz-not-a-command-zzzz" }),
        invoke("discovery.searchCommands", {}),
    ]);
    recorder.assertCase(
        "CD-23 command search matches display names, honours a scope, and requires a query",
        searchHit?.success === true &&
            searchHit.count > 0 &&
            searchHit.results.some((result) => result.guid === anyCommand?.guid) &&
            searchHit.mainMenuHits + searchHit.contextMenuHits === searchHit.count &&
            searchScoped?.scope === "mainmenu" &&
            searchScoped.contextMenuHits === 0 &&
            searchMiss?.success === true &&
            searchMiss.count === 0 &&
            searchNoQuery?.success === false,
        {
            query: anyCommandName,
            queryGuid: anyCommand?.guid,
            hit: {
                count: searchHit?.count,
                mainMenuHits: searchHit?.mainMenuHits,
                contextMenuHits: searchHit?.contextMenuHits,
                guidMatched: searchHit?.results?.some((result) => result.guid === anyCommand?.guid),
                names: (searchHit?.results ?? []).slice(0, 8).map((result) => result.name),
            },
            scoped: { scope: searchScoped?.scope, count: searchScoped?.count, contextMenuHits: searchScoped?.contextMenuHits },
            miss: searchMiss?.count,
            noQuery: searchNoQuery?.error,
        },
        {
            hit: { count: "> 0, split across the two families", guidMatched: true },
            scoped: { contextMenuHits: 0 },
            miss: 0,
            noQuery: "refused",
        },
    );

    // The tree is built against a real selection, so it needs one: with nothing
    // playing and nothing selected the endpoint refuses. Borrowing whatever the
    // host happened to have selected makes the case pass or fail on session
    // state, so one track is selected here and the entry selection restored.
    const tracksInView = await invoke("playlist.getTrackCount", {});
    const selectionOnEntry = await invoke("playlist.getSelection", {});
    let tree;
    try {
        if (tracksInView.count > 0) {
            await invoke("playlist.setSelection", { indices: [0], clearOthers: true });
        }
        tree = await invoke("discovery.getContextMenuTree", {});
    } finally {
        if (selectionOnEntry.count > 0) {
            await invoke("playlist.setSelection", {
                indices: selectionOnEntry.items,
                clearOthers: true,
            });
        } else {
            await invoke("playlist.deselectAll", {});
        }
    }
    recorder.assertCase(
        "CD-24 the context menu tree reports its own truncation limits and stays inside them",
        tracksInView.count > 0 &&
            tree?.success === true &&
            tree.depthExceeded === false &&
            tree.childrenExceeded === false &&
            tree.maxDepth > 0 &&
            tree.maxChildrenPerNode > 0 &&
            Array.isArray(tree.tree?.children) &&
            tree.tree.children.length === tree.tree.childCount &&
            tree.tree.children.every((child) => child.depth === 1 && typeof child.type === "string"),
        {
            selectedForTree: tracksInView.count > 0 ? [0] : "none: the active playlist is empty",
            limits: { maxDepth: tree?.maxDepth, maxChildrenPerNode: tree?.maxChildrenPerNode },
            exceeded: { depth: tree?.depthExceeded, children: tree?.childrenExceeded },
            rootChildCount: tree?.tree?.childCount,
            childrenLength: tree?.tree?.children?.length,
            error: tree?.error,
        },
        {
            activePlaylist: "has at least one track to select",
            exceeded: { depth: false, children: false },
            childrenLength: "equals childCount",
        },
    );

    // Two preference-page listings that do not agree: the config one includes
    // pages the discovery one filters out, among them an entry named for an
    // invalid component. A page picking either endpoint gets a different menu.
    const configPages = await invoke("config.getPreferencesPages", {});
    const configPageList = configPages?.pages ?? [];
    const discoveryGuids = new Set((prefPages.pages ?? []).map((page) => page.guid));
    const onlyInConfig = configPageList.filter((page) => !discoveryGuids.has(page.guid));
    const configGuids = new Set(configPageList.map((page) => page.guid));
    const onlyInDiscovery = (prefPages.pages ?? []).filter((page) => !configGuids.has(page.guid));
    recorder.assertCase(
        "CD-25 the config preference-page listing is a superset of the discovery one",
        configPages?.success === true &&
            configPages.count === configPageList.length &&
            configPageList.length > 0 &&
            onlyInDiscovery.length === 0 &&
            onlyInConfig.length > 0 &&
            configPageList.every((page) => typeof page.guid === "string" && typeof page.name === "string"),
        {
            configCount: configPages?.count,
            configListed: configPageList.length,
            discoveryCount: prefPages.count,
            onlyInConfig: onlyInConfig.map((page) => page.name),
            onlyInDiscovery: onlyInDiscovery.map((page) => page.name),
        },
        { configCount: "equals the list length", onlyInDiscovery: [], onlyInConfig: "a non-empty set" },
    );

    const standardGuids = await invoke("config.getPreferencesStandardGuids", {});
    // The GUID keys sit beside the envelope's own success flag, which is not one of them.
    const standardEntries = Object.entries(standardGuids ?? {}).filter(([name]) => name !== "success");
    const standardValues = standardEntries.map(([, guid]) => guid);
    const knownPages = new Set([...configGuids]);
    recorder.assertCase(
        "CD-26 the standard preference GUIDs are well-formed and name real pages",
        standardGuids?.success === true &&
            standardValues.length > 5 &&
            standardValues.every((guid) => /^\{[0-9A-Fa-f-]{36}\}$/.test(guid)) &&
            standardValues.filter((guid) => knownPages.has(guid)).length > 0,
        {
            success: standardGuids?.success,
            count: standardValues.length,
            names: standardEntries.map(([name]) => name),
            matchingListedPages: standardValues.filter((guid) => knownPages.has(guid)).length,
        },
        { success: true, count: "> 5", each: "a braced GUID", matchingListedPages: "> 0" },
    );

    // The main-menu tree walks static slots and the children of dynamic slots
    // through one recursion, so a leaf's `source` has to come from its own
    // `subGuid`, not from the value the walk was started with. Which tier
    // answered decides what to expect: on the v2 tree a leaf with `subGuid`
    // must say `mainmenu_dynamic` and one without `mainmenu_static`; on the v1
    // HMENU fallback (what a localized host answers) every leaf says
    // `hmenu_fallback` even when a `subGuid` was back-filled for it. The
    // dynamic branch is therefore only exercised on a host that answers with
    // the v2 tree and has at least one dynamic child (a "switch to playlist"
    // entry, an output device); `dynamicLeaves` in the observation shows
    // whether this run reached it.
    const mainMenu = await invoke("menu.getMainMenu", {});
    const leaves = [];
    const collectLeaves = (nodes) => {
        for (const node of nodes ?? []) {
            if (node?.type === "command") leaves.push(node);
            else if (node?.type === "submenu") collectLeaves(node.children);
        }
    };
    collectLeaves(mainMenu.items);
    const tier = mainMenu.fallback ? "flat" : mainMenu.source === "v1-hmenu" ? "v1-hmenu" : "v2-tree";
    const dynamicLeaves = leaves.filter((leaf) => typeof leaf.subGuid === "string");
    const staticLeaves = leaves.filter((leaf) => leaf.subGuid === undefined);
    const expectedDynamic = tier === "v1-hmenu" ? "hmenu_fallback" : "mainmenu_dynamic";
    const expectedStatic = tier === "v1-hmenu" ? "hmenu_fallback" : "mainmenu_static";
    recorder.assertCase(
        "CD-35 a main-menu leaf's source follows its own address: subGuid marks it dynamic",
        mainMenu.success === true &&
            leaves.length > 0 &&
            dynamicLeaves.every((leaf) => leaf.source === expectedDynamic) &&
            staticLeaves.every((leaf) => leaf.source === expectedStatic),
        {
            tier,
            leaves: leaves.length,
            dynamicLeaves: dynamicLeaves.length,
            dynamicSources: [...new Set(dynamicLeaves.map((leaf) => leaf.source))],
            staticSources: [...new Set(staticLeaves.map((leaf) => leaf.source))],
        },
        {
            dynamicSources: [expectedDynamic],
            staticSources: [expectedStatic],
            dynamicLeaves: "> 0 only on a v2-tree host with dynamic children",
        },
    );
}

async function runAdvancedConfigCases(bridge, recorder) {
    const { invoke } = bridge;

    const tree = await invoke("config.getAdvancedConfig", {});
    const leaves = flattenAdvanced(tree?.entries);
    const checkbox = leaves.find((leaf) => leaf.type === "checkbox");
    recorder.assertCase(
        "CD-27 the advanced config tree yields typed leaves with a value and a default",
        tree?.success === true &&
            Array.isArray(tree.entries) &&
            tree.count === tree.entries.length &&
            leaves.length > 50 &&
            checkbox !== undefined &&
            typeof checkbox.value === "boolean" &&
            typeof checkbox.defaultValue === "boolean" &&
            leaves.every((leaf) => /^\{[0-9A-Fa-f-]{36}\}$/.test(leaf.guid)),
        {
            success: tree?.success,
            topLevel: tree?.count,
            leafCount: leaves.length,
            types: [...new Set(leaves.map((leaf) => leaf.type))].sort(),
            sampleCheckbox: { name: checkbox?.name, value: checkbox?.value, defaultValue: checkbox?.defaultValue },
        },
        {
            success: true,
            topLevel: "equals the entries length",
            leafCount: "> 50",
            everyGuid: "braced",
            checkbox: "has boolean value and defaultValue",
        },
    );

    // Reading one entry by GUID has to agree with the same entry inside the tree,
    // since they are two ways of asking one question.
    const single = await invoke("config.getAdvancedConfigValue", { guid: checkbox.guid });
    recorder.assertCase(
        "CD-28 reading one advanced entry by GUID agrees with its leaf in the tree",
        single?.guid === checkbox.guid &&
            single.name === checkbox.name &&
            single.type === checkbox.type &&
            single.value === checkbox.value,
        { fromTree: { name: checkbox.name, type: checkbox.type, value: checkbox.value }, byGuid: single },
        { byGuid: "equals the tree leaf" },
    );

    const unknown = await invoke("config.getAdvancedConfigValue", {
        guid: "{00000000-0000-0000-0000-000000000000}",
    });
    const missingGuid = await invoke("config.getAdvancedConfigValue", {});
    recorder.assertCase(
        "CD-29 an unknown GUID and a missing GUID are both refused with a failure envelope, not answered with a default",
        refusedWith(unknown, "NOT_FOUND", "Config entry not found") &&
            refusedWith(missingGuid, "INVALID_PARAMS", "guid is required"),
        { unknownGuid: unknown, missingGuid },
        {
            unknownGuid: { success: false, code: "NOT_FOUND", error: "Config entry not found" },
            missingGuid: { success: false, code: "INVALID_PARAMS", error: "guid is required" },
        },
    );

    return { leaves, checkbox };
}

/**
 * The write side of advanced config, aimed at this component's own dev-server
 * URL rather than at whichever entry happens to come first.
 *
 * Picking an entry generically is not safe: the tree holds 153 leaves belonging
 * to the host and to every installed component, and writing to an arbitrary one
 * changes real behaviour. This entry is a string that only the "use development
 * server" checkbox consumes, and that checkbox is asserted to be off before
 * anything is written - so the round trip is inert for the running session. The
 * value is put back in the suite teardown as well as here.
 */
async function runAdvancedWriteCases(bridge, recorder, leaves, restore) {
    const { invoke, invokeRaw } = bridge;

    const target = leaves.find((leaf) => leaf.guid === DEV_SERVER_URL_GUID);
    const consumer = leaves.find((leaf) => leaf.guid === DEV_SERVER_TOGGLE_GUID);

    const [setNoGuid, setBadGuid, setUnknownGuid, resetNoGuid, resetBadGuid, resetUnknownGuid] =
        await Promise.all([
            invoke("config.setAdvancedConfigValue", {}),
            invoke("config.setAdvancedConfigValue", { guid: "not-a-guid", value: true }),
            invoke("config.setAdvancedConfigValue", { guid: ABSENT_GUID, value: true }),
            invoke("config.resetAdvancedConfig", {}),
            invoke("config.resetAdvancedConfig", { guid: "not-a-guid" }),
            invoke("config.resetAdvancedConfig", { guid: ABSENT_GUID }),
        ]);
    recorder.assertCase(
        "CD-30 the writer and the resetter refuse a missing, a malformed and an unknown GUID with the same three messages and codes",
        [setNoGuid, resetNoGuid].every((r) => refusedWith(r, "INVALID_PARAMS", "guid is required")) &&
            [setBadGuid, resetBadGuid].every((r) => refusedWith(r, "INVALID_PARAMS", "Invalid GUID format")) &&
            [setUnknownGuid, resetUnknownGuid].every((r) => refusedWith(r, "NOT_FOUND", "Config entry not found")),
        { set: { setNoGuid, setBadGuid, setUnknownGuid }, reset: { resetNoGuid, resetBadGuid, resetUnknownGuid } },
        {
            bothEndpoints: [
                { code: "INVALID_PARAMS", error: "guid is required" },
                { code: "INVALID_PARAMS", error: "Invalid GUID format" },
                { code: "NOT_FOUND", error: "Config entry not found" },
            ],
        },
    );

    if (!target || target.type !== "string" || consumer?.value !== false) {
        recorder.addCase(
            "CD-31 a value written into an advanced entry has to match the type that entry declares",
            true,
            {
                skipped:
                    "the dev-server URL entry is absent, is not a string, or its consuming checkbox is on, so writing to it would not be inert",
                target: target && { type: target.type },
                consumerEnabled: consumer?.value,
            },
            { skipped: "needs this component's dev-server URL entry with its consumer off" },
        );
        return;
    }

    restore.guid = target.guid;
    restore.value = target.value;

    // The first two are refused by the handler once it has seen the entry's type;
    // the third never reaches it, because the generated parser requires a value.
    const [boolIntoString, stringIntoCheckbox, noValue] = await Promise.all([
        invoke("config.setAdvancedConfigValue", { guid: target.guid, value: true }),
        invoke("config.setAdvancedConfigValue", { guid: consumer.guid, value: "yes" }),
        invoke("config.setAdvancedConfigValue", { guid: target.guid }),
    ]);
    const [stringUnmoved, checkboxUnmoved] = await Promise.all([
        invoke("config.getAdvancedConfigValue", { guid: target.guid }),
        invoke("config.getAdvancedConfigValue", { guid: consumer.guid }),
    ]);
    recorder.assertCase(
        "CD-31 a value written into an advanced entry has to match the type that entry declares, and a refused write leaves the entry alone",
        refusedWith(boolIntoString, "INVALID_PARAMS", "String value required") &&
            refusedWith(stringIntoCheckbox, "INVALID_PARAMS", "Boolean value required for checkbox") &&
            refusedWith(noValue, "INVALID_PARAMS", "value is required") &&
            stringUnmoved?.value === target.value &&
            checkboxUnmoved?.value === consumer.value,
        {
            boolIntoString,
            stringIntoCheckbox,
            noValue,
            unchanged: { string: stringUnmoved?.value, checkbox: checkboxUnmoved?.value },
        },
        {
            boolIntoString: { code: "INVALID_PARAMS", error: "String value required" },
            stringIntoCheckbox: { code: "INVALID_PARAMS", error: "Boolean value required for checkbox" },
            noValue: { code: "INVALID_PARAMS", error: "value is required" },
            unchanged: "both entries hold what they held",
        },
    );

    const probeValue = `http://127.0.0.1:65000/e2e-${runId}`;
    const written = await invokeRaw("config.setAdvancedConfigValue", {
        guid: target.guid,
        value: probeValue,
    });
    const afterWrite = await invoke("config.getAdvancedConfigValue", { guid: target.guid });
    const asNumber = await invokeRaw("config.setAdvancedConfigValue", {
        guid: target.guid,
        value: 4321,
    });
    const afterNumber = await invoke("config.getAdvancedConfigValue", { guid: target.guid });
    const reset = await invokeRaw("config.resetAdvancedConfig", { guid: target.guid });
    const afterReset = await invoke("config.getAdvancedConfigValue", { guid: target.guid });
    const resetAgain = await invokeRaw("config.resetAdvancedConfig", { guid: target.guid });
    const afterResetAgain = await invoke("config.getAdvancedConfigValue", { guid: target.guid });
    recorder.assertCase(
        "CD-32 a string entry round trips, a number written into one is stored as its decimal text, and reset returns it to exactly the default the tree reported",
        written?.value?.success === true &&
            afterWrite?.value === probeValue &&
            asNumber?.value?.success === true &&
            afterNumber?.value === "4321" &&
            reset?.value?.success === true &&
            afterReset?.value === target.defaultValue &&
            resetAgain?.value?.success === true &&
            afterResetAgain?.value === target.defaultValue,
        {
            entryValue: target.value,
            declaredDefault: target.defaultValue,
            afterWrite: afterWrite?.value,
            afterNumber: afterNumber?.value,
            afterReset: afterReset?.value,
            afterResetAgain: afterResetAgain?.value,
        },
        {
            afterWrite: probeValue,
            afterNumber: "4321",
            afterReset: "the defaultValue the tree reported",
            resetIsIdempotent: true,
        },
    );

    const restored = await invokeRaw("config.setAdvancedConfigValue", {
        guid: target.guid,
        value: target.value,
    });
    const afterRestore = await invoke("config.getAdvancedConfigValue", { guid: target.guid });
    if (afterRestore?.value === target.value) restore.guid = null;
    recorder.assertCase(
        "CD-33 the advanced entry is left holding the value the suite found on it, which the default it was reset to is not",
        restored?.value?.success === true &&
            afterRestore?.value === target.value &&
            target.value !== target.defaultValue,
        { entryValue: target.value, declaredDefault: target.defaultValue, afterRestore: afterRestore?.value },
        {
            afterRestore: "the entry reading",
            note: "the entry differs from the default, so the restore is observable",
        },
    );
}

async function runLibraryPatternCases(bridge, recorder) {
    const patterns = await bridge.invoke("config.getLibraryFilePatterns", {});
    // Each pattern key is present only when the host has that pattern configured,
    // so a host with neither answers with the success envelope alone.
    const configured = ["tracks", "images"].filter((key) => key in (patterns ?? {}));
    recorder.assertCase(
        "CD-34 the new-file patterns come back as the success envelope alone when the host has none configured, and a configured one carries a directory and a format",
        patterns?.success === true &&
            Object.keys(patterns).every((key) => ["success", "tracks", "images"].includes(key)) &&
            configured.every(
                (key) =>
                    typeof patterns[key]?.directory === "string" &&
                    typeof patterns[key]?.format === "string",
            ),
        { patterns, configured },
        configured.length > 0
            ? { each: "a directory and a format string under every configured key" }
            : { patterns: { success: true } },
    );
}

const recorder = createRecorder();
let client;
let bridge;
let blocked = false;
let fatalError;
let targets;
let entry;
/** Carries the advanced entry to put back if a write case throws part way. */
const advancedRestore = { guid: null, value: undefined };

try {
    const connection = await connectBridgePage();
    client = connection.client;
    targets = connection.candidates;
    console.log(`Target: ${connection.page.title} ${connection.page.url}`);
    bridge = createBridge(client.Runtime, { invokeTimeoutMs });
    await requireResponsiveBridge(bridge, { probeId: runId });

    entry = {
        replaygain: await bridge.invoke("config.getReplaygainMode", {}),
        cursorFollowPlayback: await bridge.invoke("config.getCursorFollowPlayback", {}),
        playbackFollowCursor: await bridge.invoke("config.getPlaybackFollowCursor", {}),
    };

    await runStoreCases(bridge, recorder);
    await runSettingsCases(bridge, recorder, entry);
    await runHostFactCases(bridge, recorder);
    await runIntrospectionCases(bridge, recorder);
    const advanced = await runAdvancedConfigCases(bridge, recorder);
    await runAdvancedWriteCases(bridge, recorder, advanced.leaves, advancedRestore);
    await runLibraryPatternCases(bridge, recorder);
} catch (error) {
    fatalError = error;
    blocked = Boolean(error?.blocked);
    console.error(`${blocked ? "BLOCKED" : "ERROR"}: ${error?.message || error}`);
} finally {
    if (bridge) {
        const quiet = (method, params) => bridge.invokeRaw(method, params).catch(() => undefined);
        await quiet("config.remove", { key: SCRATCH_KEY });
        if (advancedRestore.guid) {
            await quiet("config.setAdvancedConfigValue", {
                guid: advancedRestore.guid,
                value: advancedRestore.value,
            });
        }
        if (entry) {
            await quiet("config.setReplaygainMode", { mode: entry.replaygain.mode });
            await quiet("config.setCursorFollowPlayback", {
                enabled: entry.cursorFollowPlayback.enabled === true,
            });
            await quiet("config.setPlaybackFollowCursor", {
                enabled: entry.playbackFollowCursor.enabled === true,
            });
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
            runId,
            targetPort: resolvePort(),
            invokeTimeoutMs,
            scratchKey: SCRATCH_KEY,
            entrySettings: entry
                ? {
                      replaygainMode: entry.replaygain?.mode,
                      cursorFollowPlayback: entry.cursorFollowPlayback?.enabled,
                      playbackFollowCursor: entry.playbackFollowCursor?.enabled,
                  }
                : undefined,
            targets,
        },
    }),
);

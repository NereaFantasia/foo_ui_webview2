// ─────────────────────────────────────────────────────────────
// GENERATED FILE — DO NOT EDIT
// File: sdk/src/types/generated/schema-types.ts
// Source: src/api/schema/*.ts
// Emitter: scripts/gen_sdk_types.mjs
// Regenerate: npm run gen:types (in sdk/) or `node scripts/gen_sdk_types.mjs --all`
// ─────────────────────────────────────────────────────────────

/* eslint-disable */

import type { JsonValue } from '../json.js';

// ── Named interfaces from src/api/schema ────────────────────────────────
/**
 * One track, as every method whose declaration returns whole rows reports it: the playing track, a media library row, a playlist row, a queue entry. Containers add their own fields (a row number, a queue position) on top of these.
 */
export interface Track {
    /** The key that identifies the track across endpoints: `absolutePath`, with a `|subsong:N` suffix when `subsong` is not `0`. The same track yields the same handle from every such method; because of the suffix it is not a plain file path, and a method that takes a path documents whether it accepts one. */
    handle: string;
    /** Path as foobar2000 stores it: `file://` for a local file, `file-relative://` for a path stored relative to the foobar2000 folder (a portable install), or a remote URL. No subsong suffix. */
    path: string;
    /** Native filesystem path without the subsong suffix; the same as `path` for a remote URL. */
    absolutePath: string;
    /** Subsong identifier the decoder assigns inside the file, not necessarily a sequence number; `0` for a whole file and for a remote stream. */
    subsong: number;
    /** First TITLE value; empty when untagged. */
    title: string;
    /** Every ARTIST value joined with `", "`; empty when untagged. */
    artist: string;
    /** Every ARTIST value in tag order; empty when untagged. */
    artists: string[];
    /** First ALBUM value; empty when untagged. */
    album: string;
    /** Every ALBUM ARTIST value joined with `", "`; empty when untagged. */
    albumArtist: string;
    /** Every ALBUM ARTIST value in tag order, so `albumArtists.join(", ")` equals `albumArtist`; empty when untagged. `library.getAlbums` files a track that has an `album` under the name `album` and the album artist `albumArtists[0]`, or `artists[0]` when this array is empty (`""` when both are); those are the `name` and `albumArtist` of that album's row. */
    albumArtists: string[];
    /** Every GENRE value joined with `", "`; empty when untagged. */
    genre: string;
    /** First DATE value as tagged, such as `2019` or `2019-05-01`; empty when untagged. */
    date: string;
    /** TRACKNUMBER read as an integer; `0` when absent or not a number. */
    trackNumber: number;
    /** DISCNUMBER read as an integer; `0` when absent or not a number. */
    discNumber: number;
    /** Length in seconds; `0` when unknown. */
    duration: number;
    /** File size in bytes; `-1` when unknown, as for a remote stream. */
    fileSize: number;
    /** Average bitrate in kbit/s; `0` when unknown. */
    bitrate: number;
    /** Sample rate in Hz; `0` when unknown. */
    sampleRate: number;
    /** Channel count; `0` when unknown. */
    channels: number;
    /** Codec name as the decoder reports it, such as `FLAC` or `MP3`; empty when unknown. */
    codec: string;
    /** Rating from 0 to 5: the `%rating%` statistic (foo_playcount) when it is 1 to 5, otherwise the RATING tag clamped to that range; `0` when neither rates the track. */
    rating: number;
}

/**
 * A track projected down to the fields a caller asked for.
 */
export interface TrackPartial {
    /** The key that identifies the track across endpoints: `absolutePath`, with a `|subsong:N` suffix when `subsong` is not `0`. The same track yields the same handle from every such method; because of the suffix it is not a plain file path, and a method that takes a path documents whether it accepts one. */
    handle?: string;
    /** Path as foobar2000 stores it: `file://` for a local file, `file-relative://` for a path stored relative to the foobar2000 folder (a portable install), or a remote URL. No subsong suffix. */
    path?: string;
    /** Native filesystem path without the subsong suffix; the same as `path` for a remote URL. */
    absolutePath?: string;
    /** Subsong identifier the decoder assigns inside the file, not necessarily a sequence number; `0` for a whole file and for a remote stream. */
    subsong?: number;
    /** First TITLE value; empty when untagged. */
    title?: string;
    /** Every ARTIST value joined with `", "`; empty when untagged. */
    artist?: string;
    /** Every ARTIST value in tag order; empty when untagged. */
    artists?: string[];
    /** First ALBUM value; empty when untagged. */
    album?: string;
    /** Every ALBUM ARTIST value joined with `", "`; empty when untagged. */
    albumArtist?: string;
    /** Every ALBUM ARTIST value in tag order, so `albumArtists.join(", ")` equals `albumArtist`; empty when untagged. `library.getAlbums` files a track that has an `album` under the name `album` and the album artist `albumArtists[0]`, or `artists[0]` when this array is empty (`""` when both are); those are the `name` and `albumArtist` of that album's row. */
    albumArtists?: string[];
    /** Every GENRE value joined with `", "`; empty when untagged. */
    genre?: string;
    /** First DATE value as tagged, such as `2019` or `2019-05-01`; empty when untagged. */
    date?: string;
    /** TRACKNUMBER read as an integer; `0` when absent or not a number. */
    trackNumber?: number;
    /** DISCNUMBER read as an integer; `0` when absent or not a number. */
    discNumber?: number;
    /** Length in seconds; `0` when unknown. */
    duration?: number;
    /** File size in bytes; `-1` when unknown, as for a remote stream. */
    fileSize?: number;
    /** Average bitrate in kbit/s; `0` when unknown. */
    bitrate?: number;
    /** Sample rate in Hz; `0` when unknown. */
    sampleRate?: number;
    /** Channel count; `0` when unknown. */
    channels?: number;
    /** Codec name as the decoder reports it, such as `FLAC` or `MP3`; empty when unknown. */
    codec?: string;
    /** Rating from 0 to 5: the `%rating%` statistic (foo_playcount) when it is 1 to 5, otherwise the RATING tag clamped to that range; `0` when neither rates the track. */
    rating?: number;
}

/**
 * One registered method.
 */
export interface SystemApiInfo {
    /** Full method name, `namespace.method`. */
    fullName: string;
    /** Display name of the owning plugin; `foo_ui_webview2` for built-in methods. */
    plugin: string;
    /** Namespace part of the name. */
    namespace: string;
    /** Method part of the name. */
    method: string;
    /** Description given at registration; empty when none. */
    description: string;
    /** Version given at registration. */
    version: string;
    /** `true` when an external plugin registered it. */
    isExternal: boolean;
}

/**
 * One registered external plugin.
 */
export interface SystemPluginInfo {
    /** Display name. */
    name: string;
    /** Namespace the plugin owns; unique among plugins. */
    namespace: string;
    /** Plugin version. */
    version: string;
    /** Author. */
    author: string;
    /** Description. */
    description: string;
    /** Number of entries in `apis`. */
    apiCount: number;
    /** Full names of the methods the plugin registered. */
    apis: string[];
}

/**
 * One entry of `getFb2kUrlByPathBatch`'s `items`.
 */
export interface ArtworkBatchItem {
    /** Track path. */
    path: string;
    /** Picture type for this entry; the batch-wide `type` when omitted. */
    type?: "front" | "cover_front" | "back" | "cover_back" | "disc" | "icon" | "artist";
    /** Scale limit for this entry; the batch-wide `maxSize` when omitted. */
    maxSize?: number;
}

/**
 * One row of `getFb2kUrlByPathBatch`.
 */
export interface ArtworkUrlRow {
    /** The entry's path. */
    path: string;
    /** Whether a URL was built. */
    available: boolean;
    /** The type the URL carries. */
    type?: string;
    /** The `fb2k://artwork/?path=...` URL. */
    dataUrl?: string;
    /** Why no URL was built, as the URL builder's error name such as `invalid_type`. */
    error?: string;
}

/**
 * One row of `getBatch`.
 */
export interface ArtworkBatchRow {
    /** The path as given. */
    path: string;
    /** Whether the file has the picture. */
    available: boolean;
    /** MIME type detected from the bytes. */
    mimeType?: string;
    /** Picture size in bytes. */
    size?: number;
    /** `data:<mime>;base64,...` URL of the picture. */
    dataUrl?: string;
}

/**
 * One embedded picture of `getAvailableArtwork`.
 */
export interface ArtworkAvailableEntry {
    /** Picture type. */
    type: string;
    /** Where it comes from; always `embedded`, since only embedded pictures are listed. */
    source: string;
}

/**
 * One image file of `getFolderImages`.
 */
export interface ArtworkFolderImage {
    /** File name. */
    name: string;
    /** Full path. */
    path: string;
    /** File size in bytes. */
    size: number;
}

export interface SpectrumDebugSubscription {
    /** The subscription's key. */
    token: string;
    /** Window id of the subscribing page. */
    windowId: string;
    /** Handle of the window that owns the subscription, as a number. */
    ownerHwnd: number;
    /** Event name of the frames. */
    event: string;
    /** Requested FFT size. */
    fftSize: number;
    /** Frame rate after clamping. */
    fps: number;
    /** Band count after clamping. */
    bands: number;
    /** Scale in use. */
    scale: "weighted" | "db";
    /** As requested. */
    backgroundThrottle: boolean;
    /** Lower edge of the range in Hz. */
    minFrequency: number;
    /** Requested upper edge in Hz; `null` when the range follows half the stream's sample rate. */
    maxFrequency: number | null;
    /** Output in use. */
    output: "bands" | "bins";
    /** Channel layout in use. */
    channels: "mix" | "stereo";
}

export interface SpectrumDebugTarget {
    /** Window id the frames go to. */
    windowId: string;
    /** Handle of that window, as a number. */
    ownerHwnd: number;
    /** Event name of the frames. */
    event: string;
}

export interface PcmRuntimeState {
    /** Version of the WebView2 runtime installed on the machine, such as `153.0.4234.48`; empty when it cannot be read. */
    version: string;
    /** The page's environment can create shared buffers (`ICoreWebView2Environment12`). */
    environment12: boolean;
    /** The page's webview can receive shared buffers (`ICoreWebView2_17`). */
    webview17: boolean;
}

export interface PcmDecodeState {
    /** Tasks decoding, including aborted ones whose worker has not returned yet. */
    active: number;
    /** Tasks waiting for a free slot. */
    queued: number;
    /** Bytes of decode buffers the host has not closed yet. */
    openBufferBytes: number;
}

export interface PcmStreamDebugEntry {
    /** The subscription's id. */
    subscriptionId: string;
    /** Window id of the subscribing page. */
    windowId: string;
    /** Buffer generation, `0` before the first chunk of audio arrived. */
    epoch: number;
    /** Frames the current ring holds; `0` before the first chunk. */
    capacityFrames: number;
    /** Frames written into the current ring so far, wrapping at 2^32. */
    writeFrames: number;
}

export interface PcmStreamState {
    /** The component has a capture callback registered with the core; `false` when no subscription exists. */
    callbackRegistered: boolean;
    /** Interval currently requested from the core, in seconds; absent when the core's default is used. */
    interval?: number;
    /** Chunks the callback has received since the component loaded. */
    chunkCount: number;
    /** CPU cycles the callback has spent in total, from `QueryThreadCycleTime`. */
    chunkCycles: number;
    /** Every live stream subscription. */
    subscriptions: PcmStreamDebugEntry[];
}

/**
 * One output device as `config.getOutputDevices` lists it.
 */
export interface ConfigOutputDevice {
    /** Display name: the full name the output manager reports, or `<module>: <device>` when the device list comes from the output modules themselves. */
    name: string;
    /** The same as `deviceId`. */
    id: string;
    /** GUID of the output module, rendered as `{...}`. */
    outputId: string;
    /** Device GUID rendered as `{...}`. Not unique on its own: a module reports its default device with the all-zero GUID, so key devices by `(outputId, deviceId)`. */
    deviceId: string;
    /** Whether this module and device are the ones in effect, as `config.getOutputConfig` reports them. */
    isCurrent: boolean;
}

/**
 * One entry of foobar2000's Advanced preferences tree. Every entry carries `type`, and which of the other optional keys it carries follows from it: a `branch` has `children`, a `checkbox` or `radio` entry a boolean `value`, an `integer` or `string` entry a text `value` and the three `is*` flags.
 */
export interface AdvancedConfigItem {
    /** Display name. */
    name: string;
    /** Entry GUID rendered as `{...}`, as `config.getAdvancedConfigValue`, `config.setAdvancedConfigValue` and `config.resetAdvancedConfig` take it. */
    guid: string;
    /** Sort priority the entry reports. */
    sortPriority: number;
    /** Kind of an Advanced preferences entry: `branch` holds other entries, `checkbox` and `radio` hold a boolean, `integer` and `string` hold text, `unknown` is any other kind. */
    type?: "branch" | "checkbox" | "radio" | "integer" | "string" | "unknown";
    /** Current value: a boolean on `checkbox` and `radio`; the stored text on `integer` and `string`, so an integer entry reads as its decimal text here while `config.getAdvancedConfigValue` reports a number; absent on `branch` and `unknown`. */
    value?: JsonValue;
    /** Default value in the same form as `value`; present only when the entry reports one. */
    defaultValue?: JsonValue;
    /** Whether the entry is flagged as a signed integer; present on `integer` and `string`. */
    isSigned?: boolean;
    /** Whether the entry is flagged as holding a file path; present on `integer` and `string`. */
    isFilePath?: boolean;
    /** Whether the entry is flagged as holding a folder path; present on `integer` and `string`. */
    isFolderPath?: boolean;
    /** The entries inside a branch; present on `branch` only. The tree stops eleven levels below the requested parent: a branch on that level is listed with an empty `children`. */
    children?: AdvancedConfigItem[];
}

/**
 * One preferences page or preferences branch as `config.getPreferencesPages` lists it.
 */
export interface ConfigPreferencesPage {
    /** Display name. */
    name: string;
    /** GUID of the page or branch, rendered as `{...}`. */
    guid: string;
    /** GUID of the page or branch it sits under, rendered as `{...}`; a standard parent from `config.getPreferencesStandardGuids` for a top-level page. */
    parentGuid: string;
    /** Sort priority it reports: lower sorts first and `0` sorts by name; `0` as well when it reports none. */
    sortPriority: number;
    /** `true` on a branch; absent on a page. */
    isBranch?: boolean;
}

/**
 * One installed component as `config.getComponents` lists it.
 */
export interface ConfigComponentInfo {
    /** Name the component reports. */
    name: string;
    /** Version string the component reports. */
    version: string;
    /** File name the component reports for its module; absent when it reports none. */
    filename?: string;
    /** The same as `filename`. */
    fileName?: string;
}

/**
 * Where foobar2000 puts one kind of new file.
 */
export interface ConfigLibraryFilePattern {
    /** Target folder as configured. */
    directory: string;
    /** Title formatting pattern for the subfolders and the file name below that folder. */
    format: string;
}

/**
 * One stored DSP preset as `config.getDspPresets` lists it.
 */
export interface ConfigDspPreset {
    /** Position in the preset list, as `config.setActiveDspPreset` takes it. */
    index: number;
    /** Preset name. */
    name: string;
}

/**
 * One entry in the dialog's file type list.
 */
export interface FileFilter {
    /** Label shown in the file type list. Defaults to "Files", localized to the UI language. */
    name?: string;
    /** Extensions without the leading dot, such as `flac`; `*` matches every file. */
    extensions?: string[];
}

/**
 * How many entries each service family holds.
 */
export interface DiscoveryServiceCounts {
    /** Main-menu commands the host would show, dynamic children included. */
    mainMenuCommands: number;
    /** How many of `mainMenuCommands` were expanded from dynamic submenus. */
    mainMenuDynamicCommands: number;
    /** Main-menu groups. */
    mainMenuGroups: number;
    /** Context-menu commands the host would show. */
    contextMenuCommands: number;
    /** Playable file types. */
    inputFormats: number;
    /** Registered UI elements. */
    uiElements: number;
    /** Registered DSPs. */
    dspEntries: number;
    /** Registered output backends. */
    outputDevices: number;
    /** Preference pages. */
    preferencePages: number;
    /** Installed components. */
    components: number;
}

/**
 * Display state of a menu entry, as the host would draw it.
 */
export interface MenuNodeState {
    /** Whether the entry can be clicked. Meaningless while `stateKnown` is false. */
    enabled: boolean;
    /** Whether the entry shows a check mark (a radio mark counts too). */
    checked: boolean;
    /** Whether the check mark is a radio mark. */
    radioChecked: boolean;
    /** Whether the host would not draw the entry. */
    hidden: boolean;
    /** Whether `enabled` and `checked` were observed. False when the state could not be evaluated, such as a context-menu entry with nothing selected or playing. */
    stateKnown: boolean;
    /** The raw display flags the component reported. Main menu: `1` disabled, `2` checked, `4` radio, `8` default-hidden. Context menu: `1` checked, `2` disabled, `4` grayed, `8` radio. */
    flags: number;
}

/**
 * One main-menu command, static or expanded from a dynamic submenu.
 */
export interface DiscoveryMainMenuCommand extends MenuNodeState {
    /** Label as the host reports it (localized on a translated build). */
    name: string;
    /** Description the component provides; empty when it has none. */
    description: string;
    /** Command GUID. For an entry expanded from a dynamic submenu this is the owning command's GUID. */
    guid: string;
    /** GUID of the group the command is filed under. */
    parentGuid: string;
    /** Index of the command within its service. */
    index: number;
    /** Slash-separated label path; for a static slot just its label. */
    path: string;
    /** Whether the entry belongs to a dynamic submenu, as its parent slot or as an expanded child. */
    isDynamic: boolean;
    /** Whether the entry is the parent slot of a dynamic submenu: a container that cannot be executed on its own. */
    isDynamicParent: boolean;
    /** GUID of the child node inside a dynamic submenu; pass it with `guid` to `discovery.executeMainMenuCommand`. Absent on static entries. */
    subGuid?: string;
    /** Where the entry came from. */
    source: "mainmenu_static" | "mainmenu_dynamic" | "contextmenu_static" | "contextmenu_dynamic" | "hmenu_fallback";
    /** Whether the entry can be executed. */
    executable: boolean;
    /** Why it cannot be executed; empty when it can. */
    unaddressableReason: "" | "separator" | "dynamicParent" | "noStableIdentifier" | "emptyNode";
}

/**
 * One group of the main menu.
 */
export interface DiscoveryMainMenuGroup {
    /** Group GUID. */
    guid: string;
    /** GUID of the parent group. */
    parentGuid: string;
    /** Display name; empty for a group that is not a popup. */
    name: string;
    /** Sort priority within the parent. */
    sortPriority: number;
}

/**
 * One context-menu command.
 */
export interface DiscoveryContextMenuCommand extends MenuNodeState {
    /** Label as the host reports it. */
    name: string;
    /** Description the component provides; empty when it has none. */
    description: string;
    /** Command GUID. */
    guid: string;
    /** GUID of the parent group; the null GUID for a service without one. */
    parentGuid: string;
    /** Index of the command within its service. */
    index: number;
    /** Where the entry came from. */
    source: "mainmenu_static" | "mainmenu_dynamic" | "contextmenu_static" | "contextmenu_dynamic" | "hmenu_fallback";
    /** Whether the entry can be executed. */
    executable: boolean;
    /** Why it cannot be executed; empty when it can. */
    unaddressableReason: "" | "separator" | "dynamicParent" | "noStableIdentifier" | "emptyNode";
}

/**
 * One node of the context-menu tree. State fields are absent on a separator, which has neither state nor identity.
 */
export interface DiscoveryContextMenuTreeNode {
    /** Label as the host reports it. */
    name: string;
    /** Node kind. */
    type: "command" | "popup" | "separator" | "unknown";
    /** Depth below the root, which is `0`. */
    depth: number;
    /** Whether the entry can be clicked. Absent on a separator. */
    enabled?: boolean;
    /** Whether the entry shows a check mark. Absent on a separator. */
    checked?: boolean;
    /** Whether the check mark is a radio mark. Absent on a separator. */
    radioChecked?: boolean;
    /** Whether the host would not draw the entry; always false here, because the tree is built from what the host shows. Absent on a separator. */
    hidden?: boolean;
    /** Whether the state was observed; always true here. Absent on a separator. */
    stateKnown?: boolean;
    /** The raw display flags. Absent on a separator. */
    flags?: number;
    /** Full slash-separated name; only on a `command` node. */
    fullName?: string;
    /** The host's real number of children; only on a `popup` node. */
    childCount?: number;
    /** How many children this response holds; only on a `popup` node. */
    childrenReturned?: number;
    /** The children, in menu order; only on a `popup` node. */
    children?: DiscoveryContextMenuTreeNode[];
    /** Whether anything below this node was clipped. */
    truncated: boolean;
    /** Whether the depth limit clipped something below this node. */
    depthExceeded: boolean;
    /** Whether the per-node children limit clipped something below this node. */
    childrenExceeded: boolean;
}

/**
 * One playable file type.
 */
export interface DiscoveryInputFormatType {
    /** Display name, such as `FLAC`. */
    name: string;
    /** Filename mask, such as `*.FLAC`. */
    mask: string;
    /** Index within the service that registered it. */
    index: number;
}

/**
 * One installed component.
 */
export interface DiscoveryComponentInfo {
    /** DLL file name. */
    filename: string;
    /** Component name. */
    name: string;
    /** Version string as the component reports it. */
    version: string;
    /** About text. */
    about: string;
}

/**
 * One registered UI element.
 */
export interface DiscoveryUIElementInfo {
    /** Element GUID. */
    guid: string;
    /** Subclass GUID. */
    subclassGuid: string;
    /** Display name. */
    name: string;
    /** Description. */
    description: string;
    /** Whether a user may add it to a layout. */
    isUserAddable: boolean;
}

/**
 * One registered DSP.
 */
export interface DiscoveryDspEntryInfo {
    /** DSP GUID. */
    guid: string;
    /** Display name. */
    name: string;
}

/**
 * One registered output backend.
 */
export interface DiscoveryOutputDeviceEntry {
    /** Backend GUID. */
    guid: string;
}

/**
 * One page of the Preferences dialog.
 */
export interface DiscoveryPreferencePageInfo {
    /** Page GUID. */
    guid: string;
    /** GUID of the parent page. */
    parentGuid: string;
    /** Display name. */
    name: string;
}

/**
 * One search hit.
 */
export interface DiscoverySearchResult extends MenuNodeState {
    /** Label as the host reports it. */
    name: string;
    /** Description; empty when the component has none. */
    description: string;
    /** Command GUID; for a dynamic child, the owning command's GUID. */
    guid: string;
    /** Slash-separated label path. A context-menu entry is registered flat, so its path is just its label. */
    path: string;
    /** Whether the hit was expanded from a dynamic submenu. */
    isDynamic: boolean;
    /** Which menu family the hit belongs to. */
    type: "mainmenu" | "contextmenu";
    /** GUID of the dynamic child node; pass it with `guid` to `discovery.executeMainMenuCommand`. */
    subGuid?: string;
    /** Where the entry came from. */
    source: "mainmenu_static" | "mainmenu_dynamic" | "contextmenu_static" | "contextmenu_dynamic" | "hmenu_fallback";
    /** Whether the entry can be executed. */
    executable: boolean;
    /** Why it cannot be executed; empty when it can. */
    unaddressableReason: "" | "separator" | "dynamicParent" | "noStableIdentifier" | "emptyNode";
}

/**
 * One entry of the active chain.
 */
export interface DspChainEntry {
    /** Position in the chain. */
    index: number;
    /** GUID of the processor, rendered as `{...}`. */
    guid: string;
    /** Display name of the processor. */
    name: string;
}

/**
 * One stored preset.
 */
export interface DspPresetEntry {
    /** Index in the preset list, as `applyPreset` takes it. */
    index: number;
    /** Preset name, which is also its file name under `dsp-presets` in the profile. */
    name: string;
    /** Whether this is the selected preset. */
    active: boolean;
}

/**
 * One installed processor.
 */
export interface DspAvailableEntry {
    /** GUID of the processor, rendered as `{...}`; what `addDsp` and `setChain` take. */
    guid: string;
    /** Display name of the processor. */
    name: string;
    /** Whether the processor has a configuration dialog. */
    hasConfig: boolean;
}

/**
 * One entry of a chain to apply.
 */
export interface DspChainSpec {
    /** GUID of an installed processor, as `dsp.getAvailable` reports it. */
    guid: string;
}

/**
 * One entry of a `file.copyAsync` or `file.moveAsync` batch.
 */
export interface FileOpEntry {
    /** File or directory to copy or move. Echoed back verbatim, `%variable%` placeholders unexpanded, in the `file:opProgress` results. */
    source: string;
    /** Target path; a missing parent directory is created. When it names an existing directory and `source` is a file, the file is placed inside it under its own name. Echoed back verbatim like `source`. */
    destination: string;
}

/**
 * The result of one requested entry. A directory is one entry, reported once its whole tree has been handled.
 */
export interface FileOpResultItem {
    /** The source as requested, path variables unexpanded. */
    source: string;
    /** The destination as requested; absent for `file.deleteAsync`. */
    destination?: string;
    /** `ok` when carried out; `skipped` when deliberately not carried out, because the destination existed or the run was cancelled first; `failed` otherwise. */
    status: "ok" | "skipped" | "failed";
    /** Why: `already-exists` or `cancelled` with `skipped`; `not-found`, `permission`, `path-too-long` or `io-error` with `failed`; `cross-volume` with `ok`, for a move across volumes done as a copy and a delete. `path-too-long` means a path the entry needed, inside a copied folder included, is longer than 259 characters, or a folder it had to create is longer than 247. Absent when the entry was carried out plainly. */
    reason?: "already-exists" | "not-found" | "permission" | "cross-volume" | "path-too-long" | "io-error" | "cancelled";
}

/**
 * One registered entry.
 */
export interface KeyboardHotkey {
    /** Hotkey id; `0` for a shortcut. */
    id: number;
    /** The combination as registered. */
    key: string;
    /** The action name. */
    action: string;
    /** The `global` flag given at registration; `false` for a shortcut. */
    global: boolean;
}

/**
 * One value of a tag and the number of tracks carrying it.
 */
export interface LibraryValueCount {
    /** The value. */
    name: string;
    /** Tracks carrying the value. */
    trackCount: number;
}

/**
 * One track of an album row.
 */
export interface AlbumTrackRef {
    /** Track number; `0` when the track has none. */
    trackNumber: number;
    /** Track path, as foobar2000 stores it. */
    path: string;
    /** `path` as a native file path. */
    absolutePath: string;
}

/**
 * One album row, as `library.getAlbums` and `library.getArtistAlbums` return it; the `row` of `library.getAlbumTracks` has the same shape.
 */
export interface AlbumInfo {
    /** Album name. */
    name: string;
    /** `albumArtist` when it is set, otherwise the first `artist` value found among the tracks. */
    artist: string;
    /** The first `album artist` value found among the tracks, falling back to the track's `artist`; empty when neither exists. */
    albumArtist: string;
    /** Tracks counted into the row. */
    trackCount: number;
    /** Distinct disc numbers among those tracks; `1` when none carries one. */
    discCount: number;
    /** Summed length of those tracks, in seconds. */
    duration: number;
    /** The first `date` value found among the tracks; empty when none has one. */
    year: string;
    /** The first `genre` value found among the tracks; empty when none has one. */
    genre: string;
    /** The first `publisher` value found among the tracks, or else the first `label` value; empty when none has either. */
    label: string;
    /** Path of the first track counted, as foobar2000 stores it; it can be a `file-relative://` URI. */
    firstTrackPath: string;
    /** `firstTrackPath` as a native file path, the form to pass to `artwork.getForTrack`; absent when there is no first track path. */
    firstTrackAbsolutePath?: string;
    /** Front cover as a `data:image/...` URL; only `library.getAlbums` with `includeCover` fills it, and only when a cover exists. */
    coverDataUrl?: string;
    /** The album's tracks in track-number order; only `library.getAlbums` with `includeTracks` fills it. */
    tracks?: AlbumTrackRef[];
}

/**
 * One library root folder.
 */
export interface LibraryRootInfo {
    /** Stable identifier of the root, currently its `absolutePath`; `library.browseTree` takes it. */
    id: string;
    /** The folder name, or the full path when two roots share a name. */
    displayName: string;
    /** Currently the same as `absolutePath`. */
    rawPath: string;
    /** Canonical local path of the folder. */
    absolutePath: string;
    /** Library tracks under the folder. */
    trackCount: number;
}

/**
 * One album an artist is credited on. `(name, artist)` is the grouping `library.getAlbums` uses, so the pair matches exactly one of its rows.
 */
export interface ArtistAlbumRef {
    /** Album name. */
    name: string;
    /** The first `album artist` value, or the first `artist` value when there is none. */
    artist: string;
}

/**
 * One artist row of `library.getArtists`. `albumCount` counts album names, while `albums` keeps albums of the same name by different album artists apart, so the two can differ.
 */
export interface ArtistInfo {
    /** The artist. */
    name: string;
    /** Distinct album names among the artist's tracks. */
    albumCount: number;
    /** Tracks the artist is credited on. */
    trackCount: number;
    /** Summed length of those tracks, in seconds. */
    totalDuration: number;
    /** The albums the artist is credited on, sorted by name and then artist; present only with `includeAlbums`, and never cut by `limit`. */
    albums?: ArtistAlbumRef[];
}

/**
 * One track row of a library list: the shared track fields plus `index`.
 */
export interface LibraryTrack extends Track {
    /** Row number; the list the row is in says what it counts. */
    index: number;
}

/**
 * One row of `library.getRecentlyAdded`.
 */
export interface RecentLibraryTrack extends LibraryTrack {
    /** The `%added%` value as foo_playcount formats it, such as `2024-05-01 12:34:56`; present only when the list is ordered by it and the track has one. */
    added?: string;
    /** File modification time, in seconds since the Unix epoch; present only when the list is ordered by it and the time is known. */
    modified?: number;
}

/**
 * One folder of the directory tree index.
 */
export interface LibraryDirectoryNodeInfo {
    /** `rootId`, then `::`, then `pathId`. */
    id: string;
    /** The root the folder is under. */
    rootId: string;
    /** Path below the root with `/` between folder names; pass it to `library.browseTree` to open the folder. */
    pathId: string;
    /** `pathId` of the parent folder; `""` directly under the root. */
    parentPathId: string;
    /** Folder name. */
    name: string;
    /** Currently the same as `name`. */
    displayName: string;
    /** Currently the same as `absolutePath`. */
    rawPath: string;
    /** Local path of the folder. */
    absolutePath: string;
    /** Currently the same as `pathId`. */
    relativePath: string;
    /** Folder names in `pathId`: `1` directly under the root. */
    depth: number;
    /** Library tracks in the folder and every folder below it. */
    trackCount: number;
    /** Subfolders directly inside. */
    childDirectoryCount: number;
    /** Whether `childDirectoryCount` is above `0`. */
    hasChildren: boolean;
}

/**
 * One track row of a library list: the shared track fields plus `index`.
 */
export interface LibraryTrackPartial {
    /** The key that identifies the track across endpoints: `absolutePath`, with a `|subsong:N` suffix when `subsong` is not `0`. The same track yields the same handle from every such method; because of the suffix it is not a plain file path, and a method that takes a path documents whether it accepts one. */
    handle?: string;
    /** Path as foobar2000 stores it: `file://` for a local file, `file-relative://` for a path stored relative to the foobar2000 folder (a portable install), or a remote URL. No subsong suffix. */
    path?: string;
    /** Native filesystem path without the subsong suffix; the same as `path` for a remote URL. */
    absolutePath?: string;
    /** Subsong identifier the decoder assigns inside the file, not necessarily a sequence number; `0` for a whole file and for a remote stream. */
    subsong?: number;
    /** First TITLE value; empty when untagged. */
    title?: string;
    /** Every ARTIST value joined with `", "`; empty when untagged. */
    artist?: string;
    /** Every ARTIST value in tag order; empty when untagged. */
    artists?: string[];
    /** First ALBUM value; empty when untagged. */
    album?: string;
    /** Every ALBUM ARTIST value joined with `", "`; empty when untagged. */
    albumArtist?: string;
    /** Every ALBUM ARTIST value in tag order, so `albumArtists.join(", ")` equals `albumArtist`; empty when untagged. `library.getAlbums` files a track that has an `album` under the name `album` and the album artist `albumArtists[0]`, or `artists[0]` when this array is empty (`""` when both are); those are the `name` and `albumArtist` of that album's row. */
    albumArtists?: string[];
    /** Every GENRE value joined with `", "`; empty when untagged. */
    genre?: string;
    /** First DATE value as tagged, such as `2019` or `2019-05-01`; empty when untagged. */
    date?: string;
    /** TRACKNUMBER read as an integer; `0` when absent or not a number. */
    trackNumber?: number;
    /** DISCNUMBER read as an integer; `0` when absent or not a number. */
    discNumber?: number;
    /** Length in seconds; `0` when unknown. */
    duration?: number;
    /** File size in bytes; `-1` when unknown, as for a remote stream. */
    fileSize?: number;
    /** Average bitrate in kbit/s; `0` when unknown. */
    bitrate?: number;
    /** Sample rate in Hz; `0` when unknown. */
    sampleRate?: number;
    /** Channel count; `0` when unknown. */
    channels?: number;
    /** Codec name as the decoder reports it, such as `FLAC` or `MP3`; empty when unknown. */
    codec?: string;
    /** Rating from 0 to 5: the `%rating%` statistic (foo_playcount) when it is 1 to 5, otherwise the RATING tag clamped to that range; `0` when neither rates the track. */
    rating?: number;
    /** Row number; the list the row is in says what it counts. */
    index?: number;
}

/**
 * Metadata of one declared container track; absence means unknown, not zero.
 */
export interface ContainerTrack {
    /** Container track identifier, preserved as a string. */
    id: string;
    /** Content category; cover images are not video tracks. */
    type: "video" | "audio" | "subtitle" | "image" | "other";
    /** Original sample entry or Matroska CodecID. */
    codec: string;
    /** RFC 6381 codecs value when fully determined. */
    codecs?: string;
    /** Container MIME applicable to this track. */
    mimeType?: string;
    /** Declared track duration in seconds. */
    duration?: number;
    /** First presentation time on the container timeline, in seconds, when determined. */
    startTime?: number;
    /** Track language as stored in the container. */
    language?: string;
    /** Track display name stored in the container. */
    name?: string;
    /** Container default-track flag, when the format defines it. */
    default?: boolean;
    /** Container forced-track flag, when the format defines it. */
    forced?: boolean;
    /** Coded width in pixels. */
    width?: number;
    /** Coded height in pixels. */
    height?: number;
    /** Display width divided by display height. */
    displayAspectRatio?: number;
    /** Clockwise display rotation in degrees when the transform is a pure rotation. */
    rotation?: number;
    /** Declared or average frame rate, in frames per second; not a VFR maximum. */
    frameRate?: number;
    /** Declared average bit rate in bits per second. */
    bitrate?: number;
    /** Declared sample precision in bits. */
    bitDepth?: number;
    /** Colour primaries identifier from the container or codec configuration. */
    colorPrimaries?: number;
    /** Transfer characteristic identifier, including HDR transfer functions. */
    colorTransfer?: number;
    /** Matrix coefficients identifier. */
    colorMatrix?: number;
    /** Full-range video flag when explicitly present. */
    fullRange?: boolean;
    /** Audio sampling frequency in hertz. */
    sampleRate?: number;
    /** Declared audio channel count. */
    channels?: number;
}

/**
 * One container attachment, with no content or extraction URL.
 */
export interface ContainerAttachment {
    /** Container attachment identifier. */
    id: string;
    /** Stored filename or descriptive name. */
    name: string;
    /** Declared attachment MIME. */
    mimeType?: string;
    /** Attachment content length in bytes. */
    size?: number;
}

/**
 * One chapter on the container timeline; absent fields are unknown.
 */
export interface ContainerChapter {
    /** Start on the container timeline, in seconds. */
    start: number;
    /** End in seconds: the declared end, else the next chapter's start in the same edition, else the container duration for the last chapter. */
    end?: number;
    /** Chapter title as stored in the container. */
    title?: string;
    /** Language of the title as stored in the container. */
    language?: string;
    /** Position of the chapter's Matroska edition in the container, from 0; present only when the file has more than one edition. */
    edition?: number;
    /** foobar2000 subsong that plays this chapter, for `path|subsong:N`. Present only when foobar2000 splits the file into as many subsongs as there are chapters and their durations agree. */
    subsong?: number;
}

/**
 * Command counts below a submenu.
 */
export interface MenuAvailability {
    /** Commands anywhere below the submenu. */
    totalCommands: number;
    /** Commands that are enabled. */
    availableCommands: number;
    /** Commands that are disabled. */
    disabledCommands: number;
    /** Whether every command is enabled; also true when there are none. */
    allAvailable: boolean;
}

/**
 * One node of a menu tree. `type` tells the kind: a separator has no other fields, a submenu has `children`, and a command has the state and address fields.
 */
export interface MenuTreeNode {
    /** Node kind. */
    type: "separator" | "submenu" | "command";
    /** Label as the host reports it (localized on a translated build). */
    label?: string;
    /** `label` translated for `locale`; the same as `label` when translation is off or has no entry for it. */
    displayLabel?: string;
    /** Slash-separated labels from the top of the menu. */
    path?: string;
    /** `path` made of display labels. */
    displayPath?: string;
    /** Raw display flags. Their bits depend on where the node came from: SDK menu flags from the menu tree and the flat list, Win32 menu state from the Win32 menus. Absent on a submenu of the Win32 menus. */
    flags?: number;
    /** The children, in menu order; only on a submenu. */
    children?: MenuTreeNode[];
    /** Command counts below this submenu; only with `withAvailability`, and not on submenus nested inside the Win32 menus. */
    availability?: MenuAvailability;
    /** Whether the command can run; only on a command. */
    enabled?: boolean;
    /** The same as `enabled`; only on a command. */
    available?: boolean;
    /** Whether the command shows a check mark (a radio mark counts too); only on a command. */
    checked?: boolean;
    /** Whether the check mark is a radio mark; only on a command. */
    radioChecked?: boolean;
    /** Whether the host would not draw the command; only on a command. */
    hidden?: boolean;
    /** Menu item id. For a context-menu command, `menu.runContextCommandById` runs it. The id is valid only for the menu it came from, so do not store it. Absent on the flat list. */
    commandId?: number;
    /** Command GUID, the stable address to pass to `menu.runMainMenuCommand` or `menu.runContextCommand`; absent when it could not be resolved. */
    guid?: string;
    /** GUID of a dynamic child command, passed together with `guid`. */
    subGuid?: string;
    /** Where the command came from: the dynamic value when it carries `subGuid`, the static value otherwise, and `hmenu_fallback` for every command of the Win32 menus. */
    source?: "mainmenu_static" | "mainmenu_dynamic" | "contextmenu_static" | "contextmenu_dynamic" | "hmenu_fallback";
    /** Whether the command has a `guid` to run it by. */
    executable?: boolean;
    /** Why the command cannot be run; present when `executable` is false. */
    unaddressableReason?: "noStableIdentifier";
    /** `true` on a command of the flat list. */
    fallback?: boolean;
}

/**
 * Technical info of a track, as `metadata.read`, `metadata.readRaw` and the `metadata:probeProgress` results report it.
 */
export interface TrackTechnicalInfo {
    /** Length in seconds. */
    duration: number;
    /** Bitrate in kbit/s as the decoder reports it; `0` when unknown. */
    bitrate: number;
    /** Sample rate in Hz; `0` when unknown. */
    sampleRate: number;
    /** Channel count; `0` when unknown. */
    channels: number;
    /** Codec name as the decoder reports it, such as `FLAC` or `MP3`; empty when unknown. */
    codec: string;
}

/**
 * One row of `metadata.readBatch`.
 */
export interface MetadataReadBatchItem {
    /** The path of this row, as given. */
    path: string;
    /** Whether this row was read. */
    success: boolean;
    /** The flat fields of the track, as in `metadata.readByPath` but without `path` and without a `TRACKNUMBER` taken from the file name; present when `success` is `true`. */
    tags?: Record<string, JsonValue>;
    /** Why this row failed, such as `Failed to get track info`; present when `success` is `false`. */
    error?: string;
}

/**
 * One entry of `metadata.writeBatch`.
 */
export interface MetadataWriteBatchItem {
    /** Track path, as in `metadata.write`. */
    path: string;
    /** Tags to change, as in `metadata.write`. An entry whose `tags` is missing or not an object is reported in `errors` as `Missing tags` instead of refusing the call. An invalid array value also fails only this entry before any of its tags are queued; other entries still run. */
    tags?: JsonValue;
    /**
     * Subsong index for this entry, as in `metadata.write`.
     * @default -1
     */
    cueIndex?: number;
}

/**
 * One failed entry of `metadata.writeBatch`.
 */
export interface MetadataWriteBatchError {
    /** The entry's path. */
    path: string;
    /** Why it failed: `Missing tags`, or the error `metadata.write` gives for that path. */
    error: string;
}

/**
 * The result of probing one path.
 */
export interface MetadataProbeResultItem {
    /** The path as requested, `|subsong:N` included. */
    path: string;
    /** Whether the track was read. */
    success: boolean;
    /** `cached` when the host's complete cached info was used, `direct` when the file was read, `none` with a failure. */
    infoSource: "cached" | "direct" | "none";
    /** Why the path was not read; present when `success` is `false`. */
    failure?: "not-found" | "unsupported-format" | "read-error";
    /** Technical info of the track, as in `metadata.read`; present when `success` is `true`. */
    info?: TrackTechnicalInfo;
    /** The flat fields of the track, as `includeTags` describes; present when `success` is `true` and the call did not pass `includeTags: false`. */
    tags?: Record<string, JsonValue>;
}

/**
 * One changed track in `metadb:changed`.
 */
export interface MetadbChangedTrackItem {
    /** The track's key, built the way a track row's `handle` is: the native path (foobar2000's own path for a location that has none), with `|subsong:N` when `subsong` is not `0`. It equals the `handle` of the same track in rows from `library.query`, `playlist.getTracks` and the like. */
    handle: string;
    /** The file path in native form, or foobar2000's own path for a location that has none, such as a stream. It carries no subsong, so every track of a cue sheet or a multi-track file has the same `path`; `handle` tells them apart. */
    path: string;
    /** Subsong identifier inside the file, as a track row's `subsong`; `0` for a whole file. */
    subsong: number;
    /** Rating `0` to `5`, `0` for unrated, read the same way as `rating.get`. When foobar2000 holds no information for the track, only the playback statistics are read, and the key is absent unless they carry a rating. */
    rating?: number;
    /** The file's `PLAY_COUNT` tag, when it has one; not the playback statistics. */
    playCount?: number;
    /** The first `TITLE` value, when the file has one. */
    title?: string;
    /** All `ARTIST` values joined with `, `, when the file has any. */
    artist?: string;
}

/**
 * One output device.
 */
export interface OutputDevice {
    /** Device GUID rendered as `{...}`. Not unique on its own: a module reports its default device with the all-zero GUID, so key devices by `(entryGuid, guid)`. */
    guid: string;
    /** Display name of the device. */
    name: string;
    /** Display name of the output module that provides the device. */
    entry: string;
    /** GUID of that output module, rendered as `{...}`. */
    entryGuid: string;
}

/**
 * One output module and what it needs configured.
 */
export interface OutputEntry {
    /** Module GUID rendered as `{...}`. */
    guid: string;
    /** Display name of the module. Several modules may share one, and some report an empty name. */
    name: string;
    /** The module wants an output bit depth configured. */
    needsBitdepthConfig: boolean;
    /** The module wants dithering configured. */
    needsDitherConfig: boolean;
    /** The module can play several streams at once. */
    supportsMultipleStreams: boolean;
    /** The module declares itself high latency. */
    isHighLatency: boolean;
    /** The module declares itself low latency. */
    isLowLatency: boolean;
}

/**
 * A panel's configuration.
 */
export interface PanelConfig {
    /** Display name of the panel. */
    panelName: string;
    /** Name of the page template the panel loads. */
    templateName: string;
    /** Edge style of the panel frame: `0` none, `1` sunken, `2` grey. */
    edgeStyle: number;
    /** URL loaded instead of the template; empty when none. */
    urlOverride: string;
    /** Whether the panel renders with a transparent background. */
    transparentBackground: boolean;
    /** Whether a click gives the panel keyboard focus. */
    grabFocus: boolean;
    /** Whether files can be dropped onto the panel. */
    enableDragDrop: boolean;
    /** Whether the developer tools are enabled. */
    enableDevTools: boolean;
}

/**
 * Statistics of one requested track.
 */
export interface PlaycountRow {
    /** The path as requested, including any `|subsong:N` suffix, on successful and failed rows alike. */
    path: string;
    /** Whether the track could be resolved. */
    success: boolean;
    /** Why the track could not be resolved; present when `success` is `false`. */
    error?: string;
    /** Number of plays; `0` when never played. Present when `success` is `true`. */
    playCount?: number;
    /** Time of the first play, as foo_playcount formats it. Absent when unknown. */
    firstPlayed?: string;
    /** Time of the most recent play. Absent when unknown. */
    lastPlayed?: string;
    /** Time the track was added to the media library. Absent when unknown. */
    added?: string;
    /** Rating from 1 to 5. Absent when the track is unrated, never `0`. */
    rating?: number;
    /** Whether the track is in the media library. Present when `success` is `true`. */
    inLibrary?: boolean;
}

/**
 * One entry of the playlist list.
 */
export interface PlaylistInfo {
    /** Position in the playlist list, from `0`. */
    index: number;
    /** The playlist's GUID, written as `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`. Pass it as `playlistGuid` to address this playlist even after the playlist list has changed. */
    guid: string;
    /** Playlist name. */
    name: string;
    /** Number of tracks. */
    trackCount: number;
    /** Whether this is the active playlist. */
    isActive: boolean;
    /** Whether this is the playing playlist. */
    isPlaying: boolean;
    /** Whether the playlist carries a lock. */
    isLocked: boolean;
    /** Whether the playlist is an autoplaylist, as `playlist.isAutoplaylist` reports it. */
    isAutoplaylist: boolean;
}

/**
 * A playlist column the Default UI playlist view offers.
 */
export interface PlaylistColumnDefinition {
    /** Column GUID, written as `XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX` in upper case without braces, unlike a playlist GUID. */
    id: string;
    /** Display name. */
    name: string;
    /** Title Formatting pattern of the cell text. */
    pattern: string;
    /** Alignment of the cell text. */
    alignment: "left" | "right" | "center";
    /** Whether the column holds numbers. */
    numeric: boolean;
    /** Title Formatting pattern used to sort by this column; absent when sorting uses `pattern`. */
    sortPattern?: string;
}

/**
 * One playlist row: the shared track fields plus the row number and two more tags.
 */
export interface PlaylistTrack extends Track {
    /** Row in the playlist, from `0`. */
    index: number;
    /** Every COMPOSER value joined with `", "`; empty when untagged. */
    composer: string;
    /** First COMMENT value; empty when untagged. */
    comment: string;
    /** `%play_count%` from foo_playcount; absent when it gives no number, and on rows of `playlist.getSelectedTracks` or of a `fields` request. */
    playCount?: number;
    /** `%first_played%` as foo_playcount formats it; absent when empty, on rows of `playlist.getSelectedTracks` and on rows of a `fields` request. */
    firstPlayed?: string;
    /** `%last_played%` as foo_playcount formats it; absent when empty, on rows of `playlist.getSelectedTracks` and on rows of a `fields` request. */
    lastPlayed?: string;
    /** `%added%` as foo_playcount formats it; absent when empty, on rows of `playlist.getSelectedTracks` and on rows of a `fields` request. */
    added?: string;
    /** The `formats` columns of `playlist.getTracks`, by the names given; present only when `formats` was passed. */
    formats?: Record<string, string>;
}

/**
 * One playlist row: the shared track fields plus the row number and two more tags.
 */
export interface PlaylistTrackPartial {
    /** The key that identifies the track across endpoints: `absolutePath`, with a `|subsong:N` suffix when `subsong` is not `0`. The same track yields the same handle from every such method; because of the suffix it is not a plain file path, and a method that takes a path documents whether it accepts one. */
    handle?: string;
    /** Path as foobar2000 stores it: `file://` for a local file, `file-relative://` for a path stored relative to the foobar2000 folder (a portable install), or a remote URL. No subsong suffix. */
    path?: string;
    /** Native filesystem path without the subsong suffix; the same as `path` for a remote URL. */
    absolutePath?: string;
    /** Subsong identifier the decoder assigns inside the file, not necessarily a sequence number; `0` for a whole file and for a remote stream. */
    subsong?: number;
    /** First TITLE value; empty when untagged. */
    title?: string;
    /** Every ARTIST value joined with `", "`; empty when untagged. */
    artist?: string;
    /** Every ARTIST value in tag order; empty when untagged. */
    artists?: string[];
    /** First ALBUM value; empty when untagged. */
    album?: string;
    /** Every ALBUM ARTIST value joined with `", "`; empty when untagged. */
    albumArtist?: string;
    /** Every ALBUM ARTIST value in tag order, so `albumArtists.join(", ")` equals `albumArtist`; empty when untagged. `library.getAlbums` files a track that has an `album` under the name `album` and the album artist `albumArtists[0]`, or `artists[0]` when this array is empty (`""` when both are); those are the `name` and `albumArtist` of that album's row. */
    albumArtists?: string[];
    /** Every GENRE value joined with `", "`; empty when untagged. */
    genre?: string;
    /** First DATE value as tagged, such as `2019` or `2019-05-01`; empty when untagged. */
    date?: string;
    /** TRACKNUMBER read as an integer; `0` when absent or not a number. */
    trackNumber?: number;
    /** DISCNUMBER read as an integer; `0` when absent or not a number. */
    discNumber?: number;
    /** Length in seconds; `0` when unknown. */
    duration?: number;
    /** File size in bytes; `-1` when unknown, as for a remote stream. */
    fileSize?: number;
    /** Average bitrate in kbit/s; `0` when unknown. */
    bitrate?: number;
    /** Sample rate in Hz; `0` when unknown. */
    sampleRate?: number;
    /** Channel count; `0` when unknown. */
    channels?: number;
    /** Codec name as the decoder reports it, such as `FLAC` or `MP3`; empty when unknown. */
    codec?: string;
    /** Rating from 0 to 5: the `%rating%` statistic (foo_playcount) when it is 1 to 5, otherwise the RATING tag clamped to that range; `0` when neither rates the track. */
    rating?: number;
    /** Row in the playlist, from `0`. */
    index?: number;
    /** Every COMPOSER value joined with `", "`; empty when untagged. */
    composer?: string;
    /** First COMMENT value; empty when untagged. */
    comment?: string;
    /** `%play_count%` from foo_playcount; absent when it gives no number, and on rows of `playlist.getSelectedTracks` or of a `fields` request. */
    playCount?: number;
    /** `%first_played%` as foo_playcount formats it; absent when empty, on rows of `playlist.getSelectedTracks` and on rows of a `fields` request. */
    firstPlayed?: string;
    /** `%last_played%` as foo_playcount formats it; absent when empty, on rows of `playlist.getSelectedTracks` and on rows of a `fields` request. */
    lastPlayed?: string;
    /** `%added%` as foo_playcount formats it; absent when empty, on rows of `playlist.getSelectedTracks` and on rows of a `fields` request. */
    added?: string;
    /** The `formats` columns of `playlist.getTracks`, by the names given; present only when `formats` was passed. */
    formats?: Record<string, string>;
}

/**
 * One second-level run. `start` is a row of the playlist, not an offset within the parent run.
 */
export interface PlaylistGroupSubRun {
    /** First row of the run. */
    start: number;
    /** Rows in the run. */
    count: number;
    /** The second-level key as the first row of the run spells it. */
    key: string;
}

/**
 * One run of adjacent rows sharing a group key.
 */
export interface PlaylistGroupRun {
    /** First row of the run. */
    start: number;
    /** Rows in the run. */
    count: number;
    /** The group key as the first row of the run spells it. */
    key: string;
    /** Second-level runs inside this one; present only when two patterns were given. */
    sub?: PlaylistGroupSubRun[];
}

/**
 * An open port.
 */
export interface PortInfo {
    /** Id of the port; the other port methods take it. */
    portId: string;
    /** Id of the window the port belongs to, such as `main`. */
    windowId: string;
    /** The channel name. */
    name: string;
}

/**
 * One queue entry: the shared track row plus its queue position and, when it has one, the playlist position it was queued from.
 */
export interface QueueItem extends Track {
    /** Position in the queue, from `0`. */
    queueIndex: number;
    /** Playlist the entry was queued from; `null` when it carries no playlist position: queued by path, or the position foobar2000 keeps for it no longer holds this track (the row or the playlist was removed, or the rows moved). */
    playlist: number | null;
    /** GUID of that playlist, as `playlistGuid` takes it; `null` exactly when `playlist` is `null`. */
    playlistGuid: string | null;
    /** Row in that playlist; `null` exactly when `playlist` is `null`. */
    playlistItem: number | null;
}

/**
 * A reference `queue.setContents` accepts: `{ queueIndex }` keeps a slot the queue already holds, `{ playlist, item }` or `{ playlistGuid, item }` adds a playlist row. Exactly one of the forms per entry.
 */
export interface QueueContentRef {
    /** Position of an entry already in the queue. */
    queueIndex?: number;
    /** Playlist of the row to add; needs `item` as well. */
    playlist?: number;
    /** The playlist of the row to add by its `guid`, instead of `playlist`; needs `item` as well. */
    playlistGuid?: string;
    /** Row in that playlist; needs `playlist` or `playlistGuid` as well. */
    item?: number;
}

/**
 * A playlist row, addressed by the playlist's index or `guid` and the row.
 */
export interface QueueListRef {
    /** Playlist index; give this or `playlistGuid`. */
    playlist?: number;
    /** The playlist's `guid`, instead of `playlist`. */
    playlistGuid?: string;
    /** Row in that playlist. */
    item: number;
}

/**
 * The ReplayGain values of one file. Each value appears only when the file stores it.
 */
export interface ReplayGainTrackInfo {
    /** The path as requested, including any `|subsong:N` suffix. */
    path: string;
    /** Whether the file could be read. */
    success: boolean;
    /** Why the file could not be read; present when `success` is `false`. */
    error?: string;
    /** Whether the file stores a track gain or an album gain. Present when `success` is `true`. */
    hasReplayGain?: boolean;
    /** Track gain formatted with two decimals and a `dB` suffix, such as `-7.25 dB`. */
    trackGain?: string;
    /** Track gain in dB. */
    trackGainRaw?: number;
    /** Track peak formatted with six decimals. */
    trackPeak?: string;
    /** Track peak as a linear amplitude. */
    trackPeakRaw?: number;
    /** Album gain formatted with two decimals and a `dB` suffix. */
    albumGain?: string;
    /** Album gain in dB. */
    albumGainRaw?: number;
    /** Album peak formatted with six decimals. */
    albumPeak?: string;
    /** Album peak as a linear amplitude. */
    albumPeakRaw?: number;
}

/**
 * One button of the thumbnail toolbar.
 */
export interface ThumbnailButton {
    /** Identifier reported by `taskbar:buttonClicked` and used by `taskbar.updateButton`. */
    id: string;
    /** Button icon as raw Base64 of an `.ico` file, without a prefix. Empty, `null` or omitted uses the foobar2000 main icon. */
    icon?: string | null;
    /**
     * Hover text.
     * @default ""
     */
    tooltip?: string;
    /**
     * Whether the button accepts clicks.
     * @default true
     */
    enabled?: boolean;
    /**
     * Whether the button is shown.
     * @default true
     */
    visible?: boolean;
    /**
     * Close the thumbnail preview after a click.
     * @default false
     */
    dismissOnClick?: boolean;
}

export interface EvalBatchRow {
    /** The path of this row. */
    path: string;
    /** Whether this row evaluated. */
    success: boolean;
    /** Formatted text; present when `success` is `true`. */
    result?: string;
    /** Same meaning as in `titleformat.eval`; present when `success` is `true`. */
    infoAvailable?: boolean;
    /** Why this row failed; present when `success` is `false`. */
    error?: string;
}

export interface EvalFieldsNamed {
    /** The path that was evaluated. */
    path: string;
    /** `false` under the same conditions as in `titleformat.eval`, so tag-derived values are untrustworthy. One flag covers the whole request. Absent when `fields` is empty. */
    infoAvailable?: boolean;
}

export interface EvalFieldsBatchRow {
    /** The path of this row. */
    path: string;
    /** Whether this row evaluated. */
    success: boolean;
    /** Same meaning as in `titleformat.evalFields`; present when `success` is `true`. */
    infoAvailable?: boolean;
    /** Why this row failed; present when `success` is `false`. */
    error?: string;
}

/**
 * An inline monochrome SVG icon: the `viewBox` and the inner markup (for example `<path d="..."/>`). Drawn by the `webview` backend only, through an allowlist of shape elements and attributes; an illegal or oversized (over 32 KiB) icon is dropped and the row is shown without it.
 */
export interface TrayIconSvg {
    /** The SVG `viewBox` attribute. */
    viewBox: string;
    /** The SVG inner markup. */
    content: string;
}

/**
 * One option of a `segmented` row.
 */
export interface TraySegment {
    /** Text of the segment, shown when it has no icon. */
    label?: string;
    /** Icon of the segment, preferred over the label. */
    iconSvg?: TrayIconSvg;
    /**
     * `false` greys the segment out so it cannot be picked.
     * @default true
     */
    enabled?: boolean;
}

/**
 * One tray menu row. The rich kinds (`nowplaying`, `rating`, `slider`, `segmented`) are fully rendered by the `webview` backend only; the native backend degrades them (a disabled header line, a 1 to 5 stars submenu, a stepped-level submenu, a plain text row).
 */
export interface TrayMenuItem {
    /** Identifier reported by `tray:menuItemClicked`; omit for separators. The exact ids `_sys_show` and `_sys_exit` are the native show-main-window and exit rows, which keep the caller's label and skip the matching built-in injection. */
    id?: string;
    /** Display text. */
    label?: string;
    /**
     * Row kind. `checkbox` is an older spelling of a checkable `normal` row; a checkmark is otherwise driven by `checked`.
     * @default "normal"
     */
    type?: "normal" | "separator" | "checkbox" | "submenu" | "nowplaying" | "rating" | "slider" | "segmented";
    /**
     * Whether the row can be clicked.
     * @default true
     */
    enabled?: boolean;
    /**
     * Whether the row is shown.
     * @default true
     */
    visible?: boolean;
    /** Checkmark state. Giving the key, `false` included, makes the row checkable: the `webview` backend maps it to `menuitemcheckbox` and `getMenuItems` reports the key back. Omitted, the row is not checkable. */
    checked?: boolean;
    /** Reserved; neither backend draws it. Use `iconSvg` for a row icon. */
    icon?: string;
    /** Icon drawn before the label by the `webview` backend. When any row of a menu level has a renderable icon, every `normal` and `submenu` row of that level reserves the icon column. */
    iconSvg?: TrayIconSvg;
    /** `nowplaying` album art: a `data:` URL, an `http(s)://` URL, or raw Base64 JPEG. With `config.autoNowPlaying` an empty value is filled from the playing track's front art (`webview` only). */
    cover?: string;
    /** `nowplaying` first line, usually the track title; falls back to `label`. */
    title?: string;
    /** `nowplaying` second line, usually artist or album. */
    subtitle?: string;
    /** Current value: stars `0` to `5` for `rating`, a value within `min` to `max` for `slider` (clamped), the selected index for `segmented`. Reported back only for those kinds. */
    value?: number;
    /** `slider` range minimum; swapped with `max` when larger. */
    min?: number;
    /** `slider` range maximum. */
    max?: number;
    /** `slider` axis (`webview` only); horizontal when omitted. Vertical puts `min` at the bottom. */
    orientation?: "horizontal" | "vertical";
    /** `segmented` options, one row of mutually exclusive choices; `value` is the selected index. Picking one reports `{ id, value }` and keeps the menu open. */
    segments?: TraySegment[];
    /** Child rows of a `submenu` row. */
    submenu?: TrayMenuItem[];
    /** A playback command the plugin runs natively when the row is picked, so it keeps working while the page is suspended (minimized, hidden to the tray, session locked). Such a row does not fire `tray:menuItemClicked`. Allowed on a `normal` leaf only; any other placement fails the whole call. */
    playbackAction?: "play-pause" | "previous" | "next" | "stop";
}

/**
 * Menu-wide options. Every key is optional and only the keys given change the stored configuration.
 */
export interface TrayMenuConfig {
    /** Inject the built-in previous, play/pause, next and stop rows into the `playback` zone. */
    showPlaybackControls?: boolean;
    /** Inject the native show-main-window and exit rows into the `bottom` zone; both keep working while the page is suspended. */
    showSystemItems?: boolean;
    /** The zone `setContextMenu` writes its rows into. */
    customPosition?: "top" | "playback" | "bottom";
    /** Menu backend: the Win32 menu, or the self-drawn WebView2 overlay that renders the rich row kinds and the styling options below. */
    render?: "native" | "webview";
    /** Fill the empty `cover`, `title` and `subtitle` of `nowplaying` rows from the playing track when the menu opens; a value given by the caller always wins. Art is downscaled to 64 px and omitted above 256 KiB; `cover` filling is `webview` only. */
    autoNowPlaying?: boolean;
    /** Stylesheet injected into the `webview` menu on every opening, on top of the built-in styles; target the menu's stable class names (`.fb-menu`, `.fb-item`, `.fb-sep`, ...). */
    css?: string;
    /** `true` disables the built-in styles so only `css` and the protected structural layer apply (`webview` only). */
    cssReplace?: boolean;
    /** DWM backdrop of the `webview` menu, the same vocabulary as the windows. It snaps in and out with the window and cannot fade with CSS. */
    backdrop?: "acrylic" | "mica" | "mica-alt" | "none";
    /** Dark tint for the backdrop (`webview` only); `false` follows a light theme. */
    backdropDarkMode?: boolean;
    /** Milliseconds the `webview` menu plays its exit transition (`#menu.out`) before hiding on a user close; clamped to `0` to `1000`, `0` hides at once. */
    closeAnimationMs?: number;
    /** DOM layout of the `webview` menu: `flat` keeps rows as direct children of the root, `zones` wraps each non-empty zone in `.fb-zone[data-zone]`. */
    layoutMode?: "flat" | "zones";
}

/**
 * One row of a custom menu.
 */
export interface UiMenuItem {
    /** Identifier reported as `selectedId` and in `ui:menuItemClicked`. */
    id?: string;
    /** Display text. */
    label?: string;
    /**
     * `separator` draws a separator; any other value is an ordinary row.
     * @default "item"
     */
    type?: string;
    /**
     * Whether the row can be picked.
     * @default true
     */
    enabled?: boolean;
    /**
     * Whether the row shows a checkmark.
     * @default false
     */
    checked?: boolean;
    /** Shortcut hint drawn right-aligned after the label. */
    shortcut?: string;
    /** Child rows; a row with this key opens a submenu instead of being picked. */
    submenu?: UiMenuItem[];
}

/**
 * A rectangle in CSS pixels, from the top-left corner of the page. Fractions are dropped before scaling.
 */
export interface WindowRegion {
    /**
     * Left edge.
     * @default 0
     */
    x?: number;
    /**
     * Top edge.
     * @default 0
     */
    y?: number;
    /**
     * Width.
     * @default 0
     */
    width?: number;
    /**
     * Height.
     * @default 0
     */
    height?: number;
}

/**
 * The behavior a popup uses after its preset and overrides are combined.
 */
export interface WindowPopupBehaviorState {
    /** Whether the popup shows on the taskbar. */
    showInTaskbar: boolean;
    /** Whether the popup shows in Alt+Tab. */
    showInAltTab: boolean;
    /** Whether the popup stays visible when the desktop is shown. */
    keepVisibleOnShowDesktop: boolean;
    /** Whether the popup can be minimized. */
    allowMinimize: boolean;
    /** `main` keeps the popup above the main window and minimizes it with the main window; `none` makes it independent. */
    owner: "none" | "main";
    /** Whether showing or clicking the popup leaves the focus where it was. */
    noActivate: boolean;
}

/**
 * The backdrop policy a window uses after defaults and overrides are combined.
 */
export interface WindowBackdropPolicyState {
    /** Backdrop while the window is active; `inherit` follows the foobar2000 preferences. */
    activeEffect: "inherit" | "none" | "mica" | "mica-alt" | "acrylic";
    /** Backdrop while the window is inactive: `inherit` keeps the active one and lets Windows dim it, `system` hands the frame back to the platform backdrop. */
    inactiveEffect: "inherit" | "system" | "none" | "mica" | "mica-alt" | "acrylic";
    /** Whether the backdrop uses its dark variant. */
    darkMode: boolean;
    /** Whether the backdrop is written again on every activation. */
    reapplyOnActivate: boolean;
}

/**
 * Features a window supports; the last three are reported for popups only.
 */
export interface WindowObservationCapabilities {
    /** Whether `window.setBackdropPolicy` applies. */
    supportsBackdropPolicy: boolean;
    /** Whether `window.setFrameless` applies. */
    supportsFrameless: boolean;
    /** Whether `window.setCornerPreference` applies. */
    supportsCornerPreference: boolean;
    /** Whether `window.setPopupBehavior` applies. */
    supportsPopupBehavior: boolean;
    /** Whether the Mica Alt backdrop can be drawn. */
    supportsMicaAlt: boolean;
    /** Whether the window can go fullscreen. */
    supportsFullscreen: boolean;
    /** Whether `behavior.owner` applies. */
    supportsOwnerPolicy?: boolean;
    /** Whether `behavior.noActivate` applies. */
    supportsNoActivate?: boolean;
    /** Whether `beforeClose` applies. */
    supportsBeforeClose?: boolean;
}

/**
 * A window rectangle in screen coordinates, physical pixels, frame included.
 */
export interface WindowBounds {
    /** Left edge. */
    x: number;
    /** Top edge. */
    y: number;
    /** Width. */
    width: number;
    /** Height. */
    height: number;
}

/**
 * One window of `window.getAllWindows`. The popup-only fields are absent for the main window.
 */
export interface WindowInfo {
    /** `main` or the popup id. */
    windowId: string;
    /** Whether this is the main window. */
    isMain: boolean;
    /** Window title. */
    title: string;
    /** Popups only: the `url` the popup was created with. */
    url?: string;
    /** Popups only: the behavior preset; `legacy` when the popup was created without one. */
    profile?: "legacy" | "standard" | "miniPlayer" | "desktopLyrics";
    /** Popups only: the behavior overrides set on the popup. */
    behavior?: Record<string, JsonValue>;
    /** Popups only: the behavior in effect. */
    resolvedBehavior?: WindowPopupBehaviorState;
    /** The backdrop policy overrides set on the window. */
    backdropPolicy: Record<string, JsonValue>;
    /** The backdrop policy in effect. */
    resolvedBackdropPolicy: WindowBackdropPolicyState;
    /** Features the window supports. */
    capabilities: WindowObservationCapabilities;
    /** The window rectangle. */
    bounds: WindowBounds;
    /** Diagnostic snapshot of the window shell (lifecycle and startup state); its shape can change between versions. */
    shell: JsonValue;
}

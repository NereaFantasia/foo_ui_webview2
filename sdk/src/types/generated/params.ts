// ─────────────────────────────────────────────────────────────
// GENERATED FILE — DO NOT EDIT
// File: sdk/src/types/generated/params.ts
// Source: docs/graph/auto/code-index.json
// Emitter: scripts/gen_sdk_types.mjs --all
// schema_version: auto-1.0.0
// Regenerate: npm run gen:types (in sdk/) or `node scripts/gen_sdk_types.mjs --all`
// ─────────────────────────────────────────────────────────────

/* eslint-disable */

import type { JsonObject } from '../json.js';

// ── Hand-written overrides — see sdk/src/types/overrides/ ────────────────
import type { ConfigSetReplaygainModeParams } from "../overrides/config.js";
import type { CursorSetHiddenParams } from "../overrides/cursor.js";
import type { TraySetIconParams } from "../overrides/tray.js";
import type { WindowCreatePopupParams, WindowSetPopupBehaviorParams } from "../overrides/window.js";
export type { ConfigSetReplaygainModeParams, CursorSetHiddenParams, TraySetIconParams, WindowCreatePopupParams, WindowSetPopupBehaviorParams };

/**
 * Parameters for `artwork.getAvailableArtwork`.
 */
export interface ArtworkGetAvailableArtworkParams {
    /** @default "" */
    path?: string;
}

/**
 * Parameters for `artwork.getAvailableTypes`.
 */
export interface ArtworkGetAvailableTypesParams {
    /** @default "" */
    path?: string;
}

/**
 * Parameters for `artwork.getBatch`.
 */
export interface ArtworkGetBatchParams {
    paths?: string[];
    /** @default "front" */
    type?: string;
}

/**
 * Parameters for `artwork.getByPath`.
 */
export interface ArtworkGetByPathParams {
    /** @default "" */
    path?: string;
    /** @default "front" */
    type?: string;
}

/**
 * Parameters for `artwork.getByPlaylistItem`.
 */
export interface ArtworkGetByPlaylistItemParams {
    /** @default `- 1` */
    playlist?: number;
    /** @default `- 1` */
    index?: number;
    /** @default "front" */
    type?: string;
}

/**
 * Parameters for `artwork.getCurrent`.
 */
export interface ArtworkGetCurrentParams {
    /** @default "front" */
    type?: string;
}

/**
 * Parameters for `artwork.getFb2kUrl`.
 */
export interface ArtworkGetFb2kUrlParams {
    /** @default "front" */
    type?: string;
    /** @default 0 */
    maxSize?: number;
}

/**
 * Parameters for `artwork.getFb2kUrlByPath`.
 */
export interface ArtworkGetFb2kUrlByPathParams {
    /** @default "" */
    path?: string;
    /** @default "front" */
    type?: string;
    /** @default 0 */
    maxSize?: number;
}

/**
 * Parameters for `artwork.getFb2kUrlByPathBatch`.
 */
export interface ArtworkGetFb2kUrlByPathBatchParams {
    paths?: unknown;
    items?: unknown;
    /** @default "front" */
    type?: string;
    /** @default 0 */
    maxSize?: number;
    /** @default "" */
    path?: string;
}

/**
 * Parameters for `artwork.getFolderImages`.
 */
export interface ArtworkGetFolderImagesParams {
    /** @default "" */
    directory?: string;
}

/**
 * Parameters for `artwork.getForTrack`.
 */
export interface ArtworkGetForTrackParams {
    /** @default "" */
    path?: string;
    /** @default "front" */
    type?: string;
}

/**
 * Parameters for `artwork.getLyrics`.
 */
export interface ArtworkGetLyricsParams {
    /** @default "" */
    path?: string;
}

/**
 * Parameters for `artwork.getMetadata`.
 */
export interface ArtworkGetMetadataParams {
    /** @default "" */
    path?: string;
    lyric?: unknown;
}

/**
 * Parameters for `audio.analyzeBPM`.
 */
export interface AudioAnalyzeBPMParams {
    /** @default "" */
    path?: string;
    /** @default false */
    forceAnalysis?: boolean;
}

/**
 * Parameters for `audio.generateFullWaveform`.
 */
export interface AudioGenerateFullWaveformParams {
    /** @default "" */
    path?: string;
    /** @default 256 */
    resolution?: number;
    /** @default "rms" */
    method?: string;
    /** @default "linear" */
    scale?: string;
    /** @default false */
    signed?: boolean;
    /** @default true */
    preferCache?: boolean;
    /** @default `- 1` */
    cueIndex?: number;
}

/**
 * Parameters for `audio.generateWaveform`.
 */
export interface AudioGenerateWaveformParams {
    /** @default "" */
    path?: string;
    /** @default 800 */
    resolution?: number;
}

/**
 * Parameters for `audio.getOutputInfo` (this API takes no parameters).
 */
export type AudioGetOutputInfoParams = Record<string, never>;

/**
 * Parameters for `audio.getSpectrum`.
 */
export interface AudioGetSpectrumParams {
    /** @default 0 */
    bands?: number;
}

/**
 * Parameters for `audio.getSpectrumDebugState` (this API takes no parameters).
 */
export type AudioGetSpectrumDebugStateParams = Record<string, never>;

/**
 * Parameters for `audio.getStreamInfo` (this API takes no parameters).
 */
export type AudioGetStreamInfoParams = Record<string, never>;

/**
 * Parameters for `audio.getWaveform`.
 */
export interface AudioGetWaveformParams {
    /** @default 0.05 */
    duration?: number;
    /** @default false */
    signed?: boolean;
}

/**
 * Parameters for `audio.isVisualizationAvailable` (this API takes no parameters).
 */
export type AudioIsVisualizationAvailableParams = Record<string, never>;

/**
 * Parameters for `audio.setChannelMode`.
 */
export interface AudioSetChannelModeParams {
    /** @default "default" */
    mode?: string;
}

/**
 * Parameters for `audio.subscribeSpectrum`.
 */
export interface AudioSubscribeSpectrumParams {
    /** @default 1024 */
    fftSize?: number;
    /** @default "audio:spectrum" */
    event?: string;
    /** @default 30 */
    fps?: number;
    /** @default 48 */
    bands?: number;
    /** @default "" */
    subscriptionId?: string;
}

/**
 * Parameters for `audio.subscribeStream`.
 */
export interface AudioSubscribeStreamParams {
    /** @default "audio:stream" */
    event?: string;
    /** @default 0.05 */
    interval?: number;
}

/**
 * Parameters for `audio.unsubscribeSpectrum`.
 */
export interface AudioUnsubscribeSpectrumParams {
    /** @default "" */
    subscriptionId?: string;
}

/**
 * Parameters for `audio.unsubscribeStream` (this API takes no parameters).
 */
export type AudioUnsubscribeStreamParams = Record<string, never>;

/**
 * Parameters for `clipboard.read` (this API takes no parameters).
 */
export type ClipboardReadParams = Record<string, never>;

/**
 * Parameters for `clipboard.write`.
 */
export interface ClipboardWriteParams {
    /** @default "" */
    text?: string;
}

/**
 * Parameters for `clipboard.writeFiles`.
 */
export interface ClipboardWriteFilesParams {
    paths?: string[];
}

/**
 * Parameters for `clipboard.writeHTML`.
 */
export interface ClipboardWriteHTMLParams {
    /** @default "" */
    html?: string;
    /** @default "" */
    plainText?: string;
}

/**
 * Parameters for `config.export` (this API takes no parameters).
 */
export type ConfigExportParams = Record<string, never>;

/**
 * Parameters for `config.get`.
 */
export interface ConfigGetParams {
    /** @default "" */
    key?: string;
    default?: unknown;
}

/**
 * Parameters for `config.getActiveDspPreset` (this API takes no parameters).
 */
export type ConfigGetActiveDspPresetParams = Record<string, never>;

/**
 * Parameters for `config.getAdvancedConfig`.
 */
export interface ConfigGetAdvancedConfigParams {
    /** @default "" */
    parentGuid?: string;
}

/**
 * Parameters for `config.getAdvancedConfigValue`.
 */
export interface ConfigGetAdvancedConfigValueParams {
    /** @default "" */
    guid?: string;
}

/**
 * Parameters for `config.getAll` (this API takes no parameters).
 */
export type ConfigGetAllParams = Record<string, never>;

/**
 * Parameters for `config.getComponents` (this API takes no parameters).
 */
export type ConfigGetComponentsParams = Record<string, never>;

/**
 * Parameters for `config.getCursorFollowPlayback` (this API takes no parameters).
 */
export type ConfigGetCursorFollowPlaybackParams = Record<string, never>;

/**
 * Parameters for `config.getDspPresets` (this API takes no parameters).
 */
export type ConfigGetDspPresetsParams = Record<string, never>;

/**
 * Parameters for `config.getLibraryFilePatterns` (this API takes no parameters).
 */
export type ConfigGetLibraryFilePatternsParams = Record<string, never>;

/**
 * Parameters for `config.getLibraryStatus` (this API takes no parameters).
 */
export type ConfigGetLibraryStatusParams = Record<string, never>;

/**
 * Parameters for `config.getOutputConfig` (this API takes no parameters).
 */
export type ConfigGetOutputConfigParams = Record<string, never>;

/**
 * Parameters for `config.getOutputDevices` (this API takes no parameters).
 */
export type ConfigGetOutputDevicesParams = Record<string, never>;

/**
 * Parameters for `config.getPlaybackFollowCursor` (this API takes no parameters).
 */
export type ConfigGetPlaybackFollowCursorParams = Record<string, never>;

/**
 * Parameters for `config.getPreferencesPages` (this API takes no parameters).
 */
export type ConfigGetPreferencesPagesParams = Record<string, never>;

/**
 * Parameters for `config.getPreferencesStandardGuids` (this API takes no parameters).
 */
export type ConfigGetPreferencesStandardGuidsParams = Record<string, never>;

/**
 * Parameters for `config.getReplaygainMode` (this API takes no parameters).
 */
export type ConfigGetReplaygainModeParams = Record<string, never>;

/**
 * Parameters for `config.getVersionInfo` (this API takes no parameters).
 */
export type ConfigGetVersionInfoParams = Record<string, never>;

/**
 * Parameters for `config.remove`.
 */
export interface ConfigRemoveParams {
    /** @default "" */
    key?: string;
}

/**
 * Parameters for `config.resetAdvancedConfig`.
 */
export interface ConfigResetAdvancedConfigParams {
    /** @default "" */
    guid?: string;
}

/**
 * Parameters for `config.set`.
 */
export interface ConfigSetParams {
    /** @default "" */
    key?: string;
    value?: unknown;
}

/**
 * Parameters for `config.setActiveDspPreset`.
 */
export interface ConfigSetActiveDspPresetParams {
    index?: number;
}

/**
 * Parameters for `config.setAdvancedConfigValue`.
 */
export interface ConfigSetAdvancedConfigValueParams {
    /** @default "" */
    guid?: string;
    value?: boolean;
}

/**
 * Parameters for `config.setCursorFollowPlayback`.
 */
export interface ConfigSetCursorFollowPlaybackParams {
    /** @default false */
    enabled?: boolean;
    /** @default false */
    value?: boolean;
}

/**
 * Parameters for `config.setOutputBuffer`.
 */
export interface ConfigSetOutputBufferParams {
    /** @default 0 */
    milliseconds?: number;
    /** @default 0 */
    bufferLength?: number;
}

/**
 * Parameters for `config.setOutputDevice`.
 */
export interface ConfigSetOutputDeviceParams {
    /** @default "" */
    outputId?: string;
    /** @default "" */
    deviceId?: string;
}

/**
 * Parameters for `config.setPlaybackFollowCursor`.
 */
export interface ConfigSetPlaybackFollowCursorParams {
    /** @default false */
    enabled?: boolean;
    /** @default false */
    value?: boolean;
}

/**
 * Parameters for `config.showLibraryPreferences` (this API takes no parameters).
 */
export type ConfigShowLibraryPreferencesParams = Record<string, never>;

/**
 * Parameters for `console.error`.
 */
export interface ConsoleErrorParams {
    message?: string;
    args?: string[];
}

/**
 * Parameters for `console.log`.
 */
export interface ConsoleLogParams {
    message?: string;
    args?: string[];
}

/**
 * Parameters for `console.warn`.
 */
export interface ConsoleWarnParams {
    message?: string;
    args?: string[];
}

/**
 * Parameters for `cursor.isHidden` (shape unspecified — pass any JSON object).
 */
export type CursorIsHiddenParams = JsonObject;

/**
 * Parameters for `dialog.confirm`.
 */
export interface DialogConfirmParams {
    /** @default "Confirm" */
    title?: string;
    /** @default "" */
    message?: string;
    /** @default "question" */
    type?: string;
    /** @default 0 */
    defaultButton?: number;
    buttons?: string[];
}

/**
 * Parameters for `dialog.openFile`.
 */
export interface DialogOpenFileParams {
    /** @default "Open File" */
    title?: string;
    /** @default false */
    multiple?: boolean;
    /** @default "" */
    defaultPath?: string;
    filters?: string[];
    /** @default "Files" */
    name?: string;
    extensions?: unknown;
}

/**
 * Parameters for `dialog.openFolder`.
 */
export interface DialogOpenFolderParams {
    /** @default "Select Folder" */
    title?: string;
}

/**
 * Parameters for `dialog.saveFile`.
 */
export interface DialogSaveFileParams {
    /** @default "Save File" */
    title?: string;
    /** @default "" */
    defaultName?: string;
    filters?: string[];
    /** @default "Files" */
    name?: string;
    extensions?: unknown;
}

/**
 * Parameters for `discovery.executeContextMenuByPath`.
 */
export interface DiscoveryExecuteContextMenuByPathParams {
    /** @default "" */
    path?: string;
    /** @default "" */
    trackPath?: string;
}

/**
 * Parameters for `discovery.executeContextMenuCommand`.
 */
export interface DiscoveryExecuteContextMenuCommandParams {
    /** @default "" */
    guid?: string;
}

/**
 * Parameters for `discovery.executeMainMenuCommand`.
 */
export interface DiscoveryExecuteMainMenuCommandParams {
    /** @default "" */
    guid?: string;
}

/**
 * Parameters for `discovery.getAllServices` (this API takes no parameters).
 */
export type DiscoveryGetAllServicesParams = Record<string, never>;

/**
 * Parameters for `discovery.getComponents` (this API takes no parameters).
 */
export type DiscoveryGetComponentsParams = Record<string, never>;

/**
 * Parameters for `discovery.getContextMenuCommands` (this API takes no parameters).
 */
export type DiscoveryGetContextMenuCommandsParams = Record<string, never>;

/**
 * Parameters for `discovery.getContextMenuTree` (this API takes no parameters).
 */
export type DiscoveryGetContextMenuTreeParams = Record<string, never>;

/**
 * Parameters for `discovery.getDspEntries` (this API takes no parameters).
 */
export type DiscoveryGetDspEntriesParams = Record<string, never>;

/**
 * Parameters for `discovery.getInputFormats` (this API takes no parameters).
 */
export type DiscoveryGetInputFormatsParams = Record<string, never>;

/**
 * Parameters for `discovery.getMainMenuCommands` (this API takes no parameters).
 */
export type DiscoveryGetMainMenuCommandsParams = Record<string, never>;

/**
 * Parameters for `discovery.getMainMenuGroups` (this API takes no parameters).
 */
export type DiscoveryGetMainMenuGroupsParams = Record<string, never>;

/**
 * Parameters for `discovery.getOutputDevices` (this API takes no parameters).
 */
export type DiscoveryGetOutputDevicesParams = Record<string, never>;

/**
 * Parameters for `discovery.getPreferencePages` (this API takes no parameters).
 */
export type DiscoveryGetPreferencePagesParams = Record<string, never>;

/**
 * Parameters for `discovery.getUIElements` (this API takes no parameters).
 */
export type DiscoveryGetUIElementsParams = Record<string, never>;

/**
 * Parameters for `discovery.searchCommands`.
 */
export interface DiscoverySearchCommandsParams {
    /** @default "" */
    query?: string;
}

/**
 * Parameters for `dnd.getDropZones` (this API takes no parameters).
 */
export type DndGetDropZonesParams = Record<string, never>;

/**
 * Parameters for `dnd.registerDropZone`.
 */
export interface DndRegisterDropZoneParams {
    /** @default "" */
    selector?: string;
    /** @default "dnd:drop" */
    event?: string;
    accept?: string[];
}

/**
 * Parameters for `dnd.startDrag`.
 */
export interface DndStartDragParams {
    /** @default "text" */
    type?: string;
    /** @default "" */
    data?: string;
    paths?: unknown;
}

/**
 * Parameters for `dnd.unregisterDropZone`.
 */
export interface DndUnregisterDropZoneParams {
    /** @default "" */
    zoneId?: string;
}

/**
 * Parameters for `dsp.addDsp`.
 */
export interface DspAddDspParams {
    /** @default "" */
    guid?: string;
    /** @default `- 1` */
    position?: number;
}

/**
 * Parameters for `dsp.applyPreset`.
 */
export interface DspApplyPresetParams {
    /** int64 — may lose precision above 2^53 */
    index?: number;
    name?: string;
}

/**
 * Parameters for `dsp.getAvailable` (this API takes no parameters).
 */
export type DspGetAvailableParams = Record<string, never>;

/**
 * Parameters for `dsp.getChain` (this API takes no parameters).
 */
export type DspGetChainParams = Record<string, never>;

/**
 * Parameters for `dsp.getPresets` (this API takes no parameters).
 */
export type DspGetPresetsParams = Record<string, never>;

/**
 * Parameters for `dsp.moveDsp`.
 */
export interface DspMoveDspParams {
    /** int64 — may lose precision above 2^53 */
    from?: number;
    /** int64 — may lose precision above 2^53 */
    to?: number;
}

/**
 * Parameters for `dsp.removeDsp`.
 */
export interface DspRemoveDspParams {
    /** int64 — may lose precision above 2^53 */
    index?: number;
}

/**
 * Parameters for `dsp.setChain`.
 */
export interface DspSetChainParams {
    dsps?: string[];
    /** @default "" */
    guid?: string;
}

/**
 * Parameters for `event.emit`.
 */
export interface EventEmitParams {
    /** @default "" */
    event?: string;
    /** @default `json :: object ( )` */
    payload?: JsonObject;
    /** @default false */
    excludeSelf?: boolean;
}

/**
 * Parameters for `event.emitTo`.
 */
export interface EventEmitToParams {
    /** @default "" */
    event?: string;
    /** @default "" */
    targetWindowId?: string;
    /** @default `json :: object ( )` */
    payload?: JsonObject;
}

/**
 * Parameters for `file.copy`.
 */
export interface FileCopyParams {
    /** @default "" */
    source?: string;
    /** @default "" */
    destination?: string;
    /** @default false */
    overwrite?: boolean;
}

/**
 * Parameters for `file.delete`.
 */
export interface FileDeleteParams {
    /** @default "" */
    path?: string;
    /** @default true */
    moveToTrash?: boolean;
}

/**
 * Parameters for `file.exists`.
 */
export interface FileExistsParams {
    /** @default "" */
    path?: string;
}

/**
 * Parameters for `file.getInfo`.
 */
export interface FileGetInfoParams {
    /** @default "" */
    path?: string;
}

/**
 * Parameters for `file.list`.
 */
export interface FileListParams {
    /** @default "" */
    path?: string;
    /** @default "*" */
    pattern?: string;
    /** @default false */
    recursive?: boolean;
}

/**
 * Parameters for `file.mkdir`.
 */
export interface FileMkdirParams {
    /** @default "" */
    path?: string;
}

/**
 * Parameters for `file.move`.
 */
export interface FileMoveParams {
    /** @default "" */
    source?: string;
    /** @default "" */
    destination?: string;
}

/**
 * Parameters for `file.read`.
 */
export interface FileReadParams {
    /** @default "" */
    path?: string;
    /** @default "utf-8" */
    encoding?: string;
}

/**
 * Parameters for `file.rename`.
 */
export interface FileRenameParams {
    /** @default "" */
    path?: string;
    /** @default "" */
    newName?: string;
}

/**
 * Parameters for `file.write`.
 */
export interface FileWriteParams {
    /** @default "" */
    path?: string;
    /** @default "" */
    content?: string;
    /** @default "utf-8" */
    encoding?: string;
    /** @default false */
    append?: boolean;
}

/**
 * Parameters for `http.abort`.
 */
export interface HttpAbortParams {
    /** @default "" */
    requestId?: string;
}

/**
 * Parameters for `http.delete`.
 */
export interface HttpDeleteParams {
    /** @default "" */
    url?: string;
    /** @default 30000 */
    timeout?: number;
    /** @default true */
    async?: boolean;
    /** @default "follow" */
    redirect?: string;
    /** @default "text" */
    responseType?: string;
    /** @default false */
    insecureTls?: boolean;
    headers?: unknown;
    body?: unknown;
}

/**
 * Parameters for `http.download`.
 */
export interface HttpDownloadParams {
    /** @default "" */
    url?: string;
    /** @default "" */
    saveTo?: string;
    /** @default 60000 */
    timeout?: number;
    /** @default "follow" */
    redirect?: string;
    /** @default false */
    async?: boolean;
    /** @default false */
    insecureTls?: boolean;
    headers?: unknown;
}

/**
 * Parameters for `http.get`.
 */
export interface HttpGetParams {
    /** @default "" */
    url?: string;
    /** @default 30000 */
    timeout?: number;
    /** @default true */
    async?: boolean;
    /** @default "follow" */
    redirect?: string;
    /** @default "text" */
    responseType?: string;
    /** @default false */
    insecureTls?: boolean;
    headers?: unknown;
}

/**
 * Parameters for `http.head`.
 */
export interface HttpHeadParams {
    /** @default "" */
    url?: string;
    /** @default 30000 */
    timeout?: number;
    /** @default true */
    async?: boolean;
    /** @default "follow" */
    redirect?: string;
    /** @default false */
    insecureTls?: boolean;
    headers?: unknown;
    /** @default false */
    success?: boolean;
}

/**
 * Parameters for `http.patch`.
 */
export interface HttpPatchParams {
    /** @default "" */
    url?: string;
    /** @default 30000 */
    timeout?: number;
    /** @default true */
    async?: boolean;
    /** @default "follow" */
    redirect?: string;
    /** @default "text" */
    responseType?: string;
    /** @default false */
    insecureTls?: boolean;
    headers?: unknown;
    body?: unknown;
}

/**
 * Parameters for `http.post`.
 */
export interface HttpPostParams {
    /** @default "" */
    url?: string;
    /** @default 30000 */
    timeout?: number;
    /** @default true */
    async?: boolean;
    /** @default "follow" */
    redirect?: string;
    /** @default "text" */
    responseType?: string;
    /** @default false */
    insecureTls?: boolean;
    headers?: unknown;
    body?: unknown;
}

/**
 * Parameters for `http.put`.
 */
export interface HttpPutParams {
    /** @default "" */
    url?: string;
    /** @default 30000 */
    timeout?: number;
    /** @default true */
    async?: boolean;
    /** @default "follow" */
    redirect?: string;
    /** @default "text" */
    responseType?: string;
    /** @default false */
    insecureTls?: boolean;
    headers?: unknown;
    body?: unknown;
}

/**
 * Parameters for `jitQueue.clear` (this API takes no parameters).
 */
export type JitQueueClearParams = Record<string, never>;

/**
 * Parameters for `jitQueue.enqueueNext`.
 */
export interface JitQueueEnqueueNextParams {
    /** @default "" */
    trackId?: string;
    /** @default "" */
    title?: string;
    /** @default "" */
    url?: string;
}

/**
 * Parameters for `jitQueue.getState` (this API takes no parameters).
 */
export type JitQueueGetStateParams = Record<string, never>;

/**
 * Parameters for `jitQueue.notifyEmpty` (this API takes no parameters).
 */
export type JitQueueNotifyEmptyParams = Record<string, never>;

/**
 * Parameters for `jitQueue.playNow`.
 */
export interface JitQueuePlayNowParams {
    /** @default "" */
    trackId?: string;
    /** @default "" */
    title?: string;
    /** @default "" */
    url?: string;
}

/**
 * Parameters for `jitQueue.preloadBatch`.
 */
export interface JitQueuePreloadBatchParams {
    urls?: string[];
    /**
     * int64 — may lose precision above 2^53
     * @default `static_cast < size_t > ( 0 )`
     */
    startIndex?: number;
    /** @default true */
    replace?: boolean;
}

/**
 * Parameters for `jitQueue.skip` (this API takes no parameters).
 */
export type JitQueueSkipParams = Record<string, never>;

/**
 * Parameters for `jitQueue.stop`.
 */
export interface JitQueueStopParams {
    /** @default true */
    clearBuffer?: boolean;
}

/**
 * Parameters for `keyboard.getRegisteredHotkeys` (this API takes no parameters).
 */
export type KeyboardGetRegisteredHotkeysParams = Record<string, never>;

/**
 * Parameters for `keyboard.registerHotkey`.
 */
export interface KeyboardRegisterHotkeyParams {
    /** @default "" */
    key?: string;
    /** @default "" */
    action?: string;
    /** @default true */
    global?: boolean;
}

/**
 * Parameters for `keyboard.registerShortcut`.
 */
export interface KeyboardRegisterShortcutParams {
    /** @default "" */
    key?: string;
    /** @default "" */
    action?: string;
}

/**
 * Parameters for `keyboard.unregisterHotkey`.
 */
export interface KeyboardUnregisterHotkeyParams {
    /** @default "" */
    key?: string;
    id?: number;
}

/**
 * Parameters for `library.addToPlaylist`.
 */
export interface LibraryAddToPlaylistParams {
    /** @default `json :: array ( )` */
    paths?: unknown[];
    /** @default `pfc :: infinite_size` */
    playlist?: unknown;
}

/**
 * Parameters for `library.browseDirectory`.
 */
export interface LibraryBrowseDirectoryParams {
    /** @default "" */
    path?: string;
    /** @default true */
    includeFiles?: boolean;
}

/**
 * Parameters for `library.browseTree`.
 */
export interface LibraryBrowseTreeParams {
    rootId?: string;
    /** @default "" */
    pathId?: string;
    /** @default false */
    includeFiles?: boolean;
    /** @default false */
    recursiveFiles?: boolean;
    /** @default false */
    success?: boolean;
}

/**
 * Parameters for `library.getAlbums`.
 */
export interface LibraryGetAlbumsParams {
    /** @default "name" */
    sort?: string;
    /** @default "" */
    query?: string;
    /**
     * int64 — may lose precision above 2^53
     * @default `static_cast < size_t > ( 0 )`
     */
    offset?: number;
    /**
     * int64 — may lose precision above 2^53
     * @default `static_cast < size_t > ( 100 )`
     */
    limit?: number;
    /** @default false */
    includeTracks?: boolean;
    /** @default false */
    includeCover?: boolean;
    /** @default 500 */
    coverMaxSize?: number;
    /** @default true */
    useCache?: boolean;
}

/**
 * Parameters for `library.getAlbumTracks`.
 */
export interface LibraryGetAlbumTracksParams {
    /** @default "" */
    album?: string;
    /** @default "" */
    artist?: string;
}

/**
 * Parameters for `library.getAll`.
 */
export interface LibraryGetAllParams {
    /** @default 0 */
    start?: number;
    /** @default 0 */
    offset?: number;
    /** @default 100 */
    count?: number;
    /** @default 100 */
    limit?: number;
    /** @default true */
    useCache?: boolean;
    /** @default false */
    asyncResult?: boolean;
}

/**
 * Parameters for `library.getArtistAlbums`.
 */
export interface LibraryGetArtistAlbumsParams {
    /** @default "" */
    artist?: string;
    /**
     * int64 — may lose precision above 2^53
     * @default `static_cast < size_t > ( 100 )`
     */
    limit?: number;
}

/**
 * Parameters for `library.getArtists`.
 */
export interface LibraryGetArtistsParams {
    /** @default "name" */
    sort?: string;
    /**
     * int64 — may lose precision above 2^53
     * @default `static_cast < size_t > ( 1000 )`
     */
    limit?: number;
}

/**
 * Parameters for `library.getArtistTracks`.
 */
export interface LibraryGetArtistTracksParams {
    /** @default "" */
    artist?: string;
    /**
     * int64 — may lose precision above 2^53
     * @default `static_cast < size_t > ( 500 )`
     */
    limit?: number;
}

/**
 * Parameters for `library.getByPath`.
 */
export interface LibraryGetByPathParams {
    /** @default "" */
    path?: string;
}

/**
 * Parameters for `library.getCacheStats` (this API takes no parameters).
 */
export type LibraryGetCacheStatsParams = Record<string, never>;

/**
 * Parameters for `library.getCount` (this API takes no parameters).
 */
export type LibraryGetCountParams = Record<string, never>;

/**
 * Parameters for `library.getFieldValues`.
 */
export interface LibraryGetFieldValuesParams {
    /** @default "" */
    field?: string;
    /** @default "" */
    separator?: string;
    /**
     * int64 — may lose precision above 2^53
     * @default `static_cast < size_t > ( 5000 )`
     */
    limit?: number;
}

/**
 * Parameters for `library.getGenres` (this API takes no parameters).
 */
export type LibraryGetGenresParams = Record<string, never>;

/**
 * Parameters for `library.getRandomTracks`.
 */
export interface LibraryGetRandomTracksParams {
    /**
     * int64 — may lose precision above 2^53
     * @default `static_cast < size_t > ( 10 )`
     */
    count?: number;
}

/**
 * Parameters for `library.getRecentlyAdded`.
 */
export interface LibraryGetRecentlyAddedParams {
    /** @default 50 */
    limit?: number;
    /** @default "added" */
    sortBy?: string;
}

/**
 * Parameters for `library.getRoots`.
 */
export interface LibraryGetRootsParams {
    /** @default false */
    success?: boolean;
}

/**
 * Parameters for `library.getStats` (this API takes no parameters).
 */
export type LibraryGetStatsParams = Record<string, never>;

/**
 * Parameters for `library.getStatus` (this API takes no parameters).
 */
export type LibraryGetStatusParams = Record<string, never>;

/**
 * Parameters for `library.invalidateCache` (this API takes no parameters).
 */
export type LibraryInvalidateCacheParams = Record<string, never>;

/**
 * Parameters for `library.isEnabled` (this API takes no parameters).
 */
export type LibraryIsEnabledParams = Record<string, never>;

/**
 * Parameters for `library.query`.
 */
export interface LibraryQueryParams {
    /** @default "" */
    query?: string;
    /**
     * int64 — may lose precision above 2^53
     * @default `static_cast < size_t > ( 100 )`
     */
    limit?: number;
    /** @default "" */
    sort?: string;
}

/**
 * Parameters for `library.refresh` (this API takes no parameters).
 */
export type LibraryRefreshParams = Record<string, never>;

/**
 * Parameters for `library.rescan` (this API takes no parameters).
 */
export type LibraryRescanParams = Record<string, never>;

/**
 * Parameters for `library.search`.
 */
export interface LibrarySearchParams {
    /** @default "" */
    query?: string;
    /**
     * int64 — may lose precision above 2^53
     * @default `static_cast < size_t > ( 0 )`
     */
    offset?: number;
    /**
     * int64 — may lose precision above 2^53
     * @default `static_cast < size_t > ( 100 )`
     */
    limit?: number;
}

/**
 * Parameters for `log.clear` (this API takes no parameters).
 */
export type LogClearParams = Record<string, never>;

/**
 * Parameters for `log.read`.
 */
export interface LogReadParams {
    /** @default 100 */
    lines?: number;
}

/**
 * Parameters for `log.write`.
 */
export interface LogWriteParams {
    /** @default "info" */
    level?: string;
    /** @default true */
    append?: boolean;
    /** @default true */
    timestamp?: boolean;
    file?: string;
    message?: string;
    args?: string[];
}

/**
 * Parameters for `lyrics.exists`.
 */
export interface LyricsExistsParams {
    /** @default "" */
    path?: string;
}

/**
 * Parameters for `lyrics.get`.
 */
export interface LyricsGetParams {
    /** @default "" */
    path?: string;
    /** @default "any" */
    source?: string;
    /** @default "any" */
    type?: string;
    /** @default "any" */
    format?: string;
}

/**
 * Parameters for `lyrics.save`.
 */
export interface LyricsSaveParams {
    /** @default "" */
    path?: string;
    /** @default "" */
    lyrics?: string;
    /** @default "" */
    filename?: string;
    /** @default "LYRICS" */
    tagName?: string;
    /** @default "lrc" */
    format?: string;
    /** @default "file" */
    target?: string[];
    /** @default false */
    success?: boolean;
}

/**
 * Parameters for `menu.close`.
 */
export interface MenuCloseParams {
    /** @default `std :: string ( "api" )` */
    reason?: unknown;
}

/**
 * Parameters for `menu.getContextMenu`.
 */
export interface MenuGetContextMenuParams {
    /** @default "auto" */
    mode?: string;
    /** @default "auto" */
    locale?: string;
    /** @default true */
    i18n?: boolean;
    /** @default true */
    withAvailability?: boolean;
    /** @default `json :: array ( )` */
    handles?: unknown[];
    /** @default "" */
    path?: string;
    /** @default 0 */
    subsong?: number;
}

/**
 * Parameters for `menu.getMainMenu`.
 */
export interface MenuGetMainMenuParams {
    /** @default "" */
    root?: string;
    /** @default "auto" */
    locale?: string;
    /** @default true */
    i18n?: boolean;
    /** @default true */
    withAvailability?: boolean;
}

/**
 * Parameters for `menu.runContextCommand`.
 */
export interface MenuRunContextCommandParams {
    /** @default "" */
    command?: string;
}

/**
 * Parameters for `menu.runContextCommandById`.
 */
export interface MenuRunContextCommandByIdParams {
    /** @default `- 1` */
    id?: number;
    /** @default "auto" */
    mode?: string;
    /** @default `json :: array ( )` */
    handles?: unknown[];
    /** @default "" */
    path?: string;
    /** @default 0 */
    subsong?: number;
}

/**
 * Parameters for `menu.runMainMenuCommand`.
 */
export interface MenuRunMainMenuCommandParams {
    /** @default "" */
    command?: string;
}

/**
 * Parameters for `menu.show`.
 */
export interface MenuShowParams {
    items?: unknown;
    /** @default `- 1` */
    x?: number;
    /** @default `- 1` */
    y?: number;
}

/**
 * Parameters for `menu.showNativePopup`.
 */
export interface MenuShowNativePopupParams {
    /** @default "playlist" */
    mode?: string;
    /** @default `json :: array ( )` */
    handles?: unknown[];
    /** @default "" */
    path?: string;
    /** @default 0 */
    subsong?: number;
}

/**
 * Parameters for `metadata.embedArtwork`.
 */
export interface MetadataEmbedArtworkParams {
    /** @default "" */
    path?: string;
    /** @default "" */
    imageData?: string;
    /** @default "front" */
    type?: string;
    /** @default "" */
    filename?: string;
    /** @default "embedded" */
    target?: string[];
    /** @default false */
    success?: boolean;
}

/**
 * Parameters for `metadata.read`.
 */
export interface MetadataReadParams {
    /** @default "" */
    path?: string;
}

/**
 * Parameters for `metadata.readBatch`.
 */
export interface MetadataReadBatchParams {
    paths?: unknown;
}

/**
 * Parameters for `metadata.readByPath`.
 */
export interface MetadataReadByPathParams {
    /** @default "" */
    path?: string;
    TRACKNUMBER?: unknown;
}

/**
 * Parameters for `metadata.readRaw`.
 */
export interface MetadataReadRawParams {
    /** @default "" */
    path?: string;
    /** @default `- 1` */
    cueIndex?: number;
}

/**
 * Parameters for `metadata.removeEmbeddedArt`.
 */
export interface MetadataRemoveEmbeddedArtParams {
    /** @default "" */
    path?: string;
    /** @default "" */
    type?: string;
    /** @default false */
    removeAll?: boolean;
}

/**
 * Parameters for `metadata.removeField`.
 */
export interface MetadataRemoveFieldParams {
    /** @default "" */
    path?: string;
    tags?: string[];
    /** @default `- 1` */
    cueIndex?: number;
}

/**
 * Parameters for `metadata.removeTag`.
 */
export interface MetadataRemoveTagParams {
    /** @default "" */
    path?: string;
    tags?: string[];
    /** @default `- 1` */
    cueIndex?: number;
}

/**
 * Parameters for `metadata.write`.
 */
export interface MetadataWriteParams {
    /** @default "" */
    path?: string;
    tags?: unknown;
    /** @default `- 1` */
    cueIndex?: number;
}

/**
 * Parameters for `metadata.writeBatch`.
 */
export interface MetadataWriteBatchParams {
    items?: string[];
    /** @default "" */
    path?: string;
    tags?: unknown;
    /** @default false */
    success?: boolean;
    /** @default "Unknown error" */
    error?: string;
}

/**
 * Parameters for `misc.exit` (this API takes no parameters).
 */
export type MiscExitParams = Record<string, never>;

/**
 * Parameters for `misc.getComponentPath` (this API takes no parameters).
 */
export type MiscGetComponentPathParams = Record<string, never>;

/**
 * Parameters for `misc.getFoobarPath` (this API takes no parameters).
 */
export type MiscGetFoobarPathParams = Record<string, never>;

/**
 * Parameters for `misc.getProfilePath` (this API takes no parameters).
 */
export type MiscGetProfilePathParams = Record<string, never>;

/**
 * Parameters for `misc.restart` (this API takes no parameters).
 */
export type MiscRestartParams = Record<string, never>;

/**
 * Parameters for `misc.showConsole` (this API takes no parameters).
 */
export type MiscShowConsoleParams = Record<string, never>;

/**
 * Parameters for `misc.showLibrarySearch`.
 */
export interface MiscShowLibrarySearchParams {
    /** @default "" */
    query?: string;
}

/**
 * Parameters for `misc.showPopupMessage`.
 */
export interface MiscShowPopupMessageParams {
    /** @default `params . value ( "msg" , "" )` */
    message?: unknown;
    /** @default "" */
    msg?: string;
    /** @default "Message" */
    title?: string;
}

/**
 * Parameters for `misc.showPreferences` (this API takes no parameters).
 */
export type MiscShowPreferencesParams = Record<string, never>;

/**
 * Parameters for `output.getDevices` (this API takes no parameters).
 */
export type OutputGetDevicesParams = Record<string, never>;

/**
 * Parameters for `output.getEntries` (this API takes no parameters).
 */
export type OutputGetEntriesParams = Record<string, never>;

/**
 * Parameters for `output.getSettings` (this API takes no parameters).
 */
export type OutputGetSettingsParams = Record<string, never>;

/**
 * Parameters for `panel.getConfig` (shape unspecified — pass any JSON object).
 */
export type PanelGetConfigParams = JsonObject;

/**
 * Parameters for `panel.setConfig`.
 */
export interface PanelSetConfigParams {
    panelName?: string;
    transparentBackground?: boolean;
    grabFocus?: boolean;
    enableDragDrop?: boolean;
}

/**
 * Parameters for `playback.getCurrentTrack` (this API takes no parameters).
 */
export type PlaybackGetCurrentTrackParams = Record<string, never>;

/**
 * Parameters for `playback.getCurrentTrackIndex`.
 */
export interface PlaybackGetCurrentTrackIndexParams {
    /** @default false */
    includeTrackInfo?: boolean;
}

/**
 * Parameters for `playback.getPlaybackOrder` (this API takes no parameters).
 */
export type PlaybackGetPlaybackOrderParams = Record<string, never>;

/**
 * Parameters for `playback.getPlayingPlaylist` (this API takes no parameters).
 */
export type PlaybackGetPlayingPlaylistParams = Record<string, never>;

/**
 * Parameters for `playback.getPosition` (this API takes no parameters).
 */
export type PlaybackGetPositionParams = Record<string, never>;

/**
 * Parameters for `playback.getState` (this API takes no parameters).
 */
export type PlaybackGetStateParams = Record<string, never>;

/**
 * Parameters for `playback.getStopAfterCurrent` (this API takes no parameters).
 */
export type PlaybackGetStopAfterCurrentParams = Record<string, never>;

/**
 * Parameters for `playback.getVolume` (this API takes no parameters).
 */
export type PlaybackGetVolumeParams = Record<string, never>;

/**
 * Parameters for `playback.mute`.
 */
export interface PlaybackMuteParams {
    /** @default true */
    muted?: boolean;
}

/**
 * Parameters for `playback.next` (this API takes no parameters).
 */
export type PlaybackNextParams = Record<string, never>;

/**
 * Parameters for `playback.pause` (this API takes no parameters).
 */
export type PlaybackPauseParams = Record<string, never>;

/**
 * Parameters for `playback.play` (this API takes no parameters).
 */
export type PlaybackPlayParams = Record<string, never>;

/**
 * Parameters for `playback.playOrPause` (this API takes no parameters).
 */
export type PlaybackPlayOrPauseParams = Record<string, never>;

/**
 * Parameters for `playback.playPath`.
 */
export interface PlaybackPlayPathParams {
    /** @default "" */
    path?: string;
}

/**
 * Parameters for `playback.playPaths`.
 */
export interface PlaybackPlayPathsParams {
    paths?: string[];
    /**
     * int64 — may lose precision above 2^53
     * @default `static_cast < size_t > ( 0 )`
     */
    startIndex?: number;
    /** @default false */
    replace?: boolean;
}

/**
 * Parameters for `playback.playPause` (this API takes no parameters).
 */
export type PlaybackPlayPauseParams = Record<string, never>;

/**
 * Parameters for `playback.previous` (this API takes no parameters).
 */
export type PlaybackPreviousParams = Record<string, never>;

/**
 * Parameters for `playback.random` (this API takes no parameters).
 */
export type PlaybackRandomParams = Record<string, never>;

/**
 * Parameters for `playback.setPlaybackOrder`.
 */
export interface PlaybackSetPlaybackOrderParams {
    order?: unknown;
}

/**
 * Parameters for `playback.setPosition`.
 */
export interface PlaybackSetPositionParams {
    /** @default 0 */
    position?: number;
    /** @default 0 */
    seconds?: number;
}

/**
 * Parameters for `playback.setStopAfterCurrent`.
 */
export interface PlaybackSetStopAfterCurrentParams {
    /** @default false */
    enabled?: boolean;
}

/**
 * Parameters for `playback.setVolume`.
 */
export interface PlaybackSetVolumeParams {
    /** @default 100 */
    volume?: number;
}

/**
 * Parameters for `playback.stop` (this API takes no parameters).
 */
export type PlaybackStopParams = Record<string, never>;

/**
 * Parameters for `playback.toggleMute` (this API takes no parameters).
 */
export type PlaybackToggleMuteParams = Record<string, never>;

/**
 * Parameters for `playback.toggleStopAfterCurrent` (this API takes no parameters).
 */
export type PlaybackToggleStopAfterCurrentParams = Record<string, never>;

/**
 * Parameters for `playback.volumeDown` (this API takes no parameters).
 */
export type PlaybackVolumeDownParams = Record<string, never>;

/**
 * Parameters for `playback.volumeUp` (this API takes no parameters).
 */
export type PlaybackVolumeUpParams = Record<string, never>;

/**
 * Parameters for `playcount.get`.
 */
export interface PlaycountGetParams {
    paths?: unknown;
}

/**
 * Parameters for `playcount.getBatch`.
 */
export interface PlaycountGetBatchParams {
    paths?: unknown;
}

/**
 * Parameters for `playcount.getStats` (this API takes no parameters).
 */
export type PlaycountGetStatsParams = Record<string, never>;

/**
 * Parameters for `playcount.set`.
 */
export interface PlaycountSetParams {
    path?: string;
}

/**
 * Parameters for `playlist.addHandles`.
 */
export interface PlaylistAddHandlesParams {
    playlist?: number;
    /** @default `json :: array ( )` */
    handles?: unknown[];
}

/**
 * Parameters for `playlist.addPaths`.
 */
export interface PlaylistAddPathsParams {
    playlist?: number;
    /** @default `json :: array ( )` */
    paths?: unknown[];
}

/**
 * Parameters for `playlist.addPathsAsync`.
 */
export interface PlaylistAddPathsAsyncParams {
    playlist?: number;
    /** @default `json :: array ( )` */
    paths?: unknown[];
}

/**
 * Parameters for `playlist.addPathsSequential`.
 */
export interface PlaylistAddPathsSequentialParams {
    playlist?: number;
    /** @default `json :: array ( )` */
    paths?: unknown[];
}

/**
 * Parameters for `playlist.clear`.
 */
export interface PlaylistClearParams {
    playlist?: number;
}

/**
 * Parameters for `playlist.convertToAutoplaylist`.
 */
export interface PlaylistConvertToAutoplaylistParams {
    playlist?: number;
    /** @default "" */
    query?: string;
    /** @default "" */
    sort?: string;
    /** @default false */
    keepSorted?: boolean;
}

/**
 * Parameters for `playlist.create`.
 */
export interface PlaylistCreateParams {
    /** @default "New Playlist" */
    name?: string;
    position?: number;
}

/**
 * Parameters for `playlist.createAutoplaylist`.
 */
export interface PlaylistCreateAutoplaylistParams {
    /** @default "New Autoplaylist" */
    name?: string;
    /** @default "" */
    query?: string;
    /** @default "" */
    sort?: string;
    /** @default false */
    keepSorted?: boolean;
}

/**
 * Parameters for `playlist.deselectAll`.
 */
export interface PlaylistDeselectAllParams {
    playlist?: number;
}

/**
 * Parameters for `playlist.duplicate`.
 */
export interface PlaylistDuplicateParams {
    playlist?: number;
    /** @default "" */
    name?: string;
}

/**
 * Parameters for `playlist.focusTrack`.
 */
export interface PlaylistFocusTrackParams {
    playlist?: number;
    /** @default `params . value ( "track" , SIZE_MAX )` */
    index?: unknown;
    track?: number;
}

/**
 * Parameters for `playlist.getActive` (this API takes no parameters).
 */
export type PlaylistGetActiveParams = Record<string, never>;

/**
 * Parameters for `playlist.getAll` (this API takes no parameters).
 */
export type PlaylistGetAllParams = Record<string, never>;

/**
 * Parameters for `playlist.getAutoplaylistInfo`.
 */
export interface PlaylistGetAutoplaylistInfoParams {
    playlist?: number;
}

/**
 * Parameters for `playlist.getAutoplaylistQuery`.
 */
export interface PlaylistGetAutoplaylistQueryParams {
    playlist?: number;
}

/**
 * Parameters for `playlist.getAvailableColumns` (this API takes no parameters).
 */
export type PlaylistGetAvailableColumnsParams = Record<string, never>;

/**
 * Parameters for `playlist.getCount` (this API takes no parameters).
 */
export type PlaylistGetCountParams = Record<string, never>;

/**
 * Parameters for `playlist.getFocusedTrack`.
 */
export interface PlaylistGetFocusedTrackParams {
    playlist?: number;
}

/**
 * Parameters for `playlist.getFocusTrack`.
 */
export interface PlaylistGetFocusTrackParams {
    playlist?: number;
}

/**
 * Parameters for `playlist.getLockInfo`.
 */
export interface PlaylistGetLockInfoParams {
    playlist?: number;
}

/**
 * Parameters for `playlist.getPlaying` (this API takes no parameters).
 */
export type PlaylistGetPlayingParams = Record<string, never>;

/**
 * Parameters for `playlist.getSelectedTracks`.
 */
export interface PlaylistGetSelectedTracksParams {
    /** @default `pfc :: infinite_size` */
    playlist?: boolean;
    /** @default `pfc :: infinite_size` */
    index?: unknown;
}

/**
 * Parameters for `playlist.getSelection`.
 */
export interface PlaylistGetSelectionParams {
    playlist?: number;
}

/**
 * Parameters for `playlist.getTrackCount`.
 */
export interface PlaylistGetTrackCountParams {
    /** @default `pfc :: infinite_size` */
    playlist?: boolean;
    /** @default `pfc :: infinite_size` */
    index?: unknown;
}

/**
 * Parameters for `playlist.getTracks`.
 */
export interface PlaylistGetTracksParams {
    /**
     * int64 — may lose precision above 2^53
     * @default `static_cast < size_t > ( 0 )`
     */
    start?: number;
    /**
     * int64 — may lose precision above 2^53
     * @default `static_cast < size_t > ( 100 )`
     */
    count?: number;
    /** @default `json :: object ( )` */
    formats?: JsonObject;
    /** @default `pfc :: infinite_size` */
    playlist?: boolean;
    /** @default `pfc :: infinite_size` */
    index?: unknown;
}

/**
 * Parameters for `playlist.insertTracks`.
 */
export interface PlaylistInsertTracksParams {
    playlist?: number;
    /**
     * int64 — may lose precision above 2^53
     * @default `params . value ( "index" , static_cast <`
     */
    position?: number;
    /**
     * int64 — may lose precision above 2^53
     * @default `static_cast < size_t > ( 0 )`
     */
    index?: number;
    /** @default `json :: array ( )` */
    handles?: unknown[];
}

/**
 * Parameters for `playlist.isAutoplaylist`.
 */
export interface PlaylistIsAutoplaylistParams {
    playlist?: number;
}

/**
 * Parameters for `playlist.isLocked`.
 */
export interface PlaylistIsLockedParams {
    playlist?: number;
}

/**
 * Parameters for `playlist.moveTracks`.
 */
export interface PlaylistMoveTracksParams {
    /** @default `json :: array ( )` */
    items?: unknown[];
    /** @default 0 */
    delta?: number;
    /** @default `pfc :: infinite_size` */
    playlist?: boolean;
    /** @default `pfc :: infinite_size` */
    index?: unknown;
}

/**
 * Parameters for `playlist.playTrack`.
 */
export interface PlaylistPlayTrackParams {
    playlist?: number;
    /**
     * int64 — may lose precision above 2^53
     * @default `params . value ( "track" , static_cast <`
     */
    index?: number;
    /**
     * int64 — may lose precision above 2^53
     * @default `static_cast < size_t > ( 0 )`
     */
    track?: number;
    /** @default false */
    deferred?: boolean;
    /** @default false */
    muted?: boolean;
}

/**
 * Parameters for `playlist.redo`.
 */
export interface PlaylistRedoParams {
    playlist?: number;
}

/**
 * Parameters for `playlist.remove`.
 */
export interface PlaylistRemoveParams {
    playlist?: number;
}

/**
 * Parameters for `playlist.removeAutoplaylist`.
 */
export interface PlaylistRemoveAutoplaylistParams {
    playlist?: number;
}

/**
 * Parameters for `playlist.removeSelectedTracks`.
 */
export interface PlaylistRemoveSelectedTracksParams {
    /** @default `pfc :: infinite_size` */
    playlist?: boolean;
    /** @default `pfc :: infinite_size` */
    index?: unknown;
}

/**
 * Parameters for `playlist.removeTracks`.
 */
export interface PlaylistRemoveTracksParams {
    /** @default `json :: array ( )` */
    items?: unknown[];
    /** @default `pfc :: infinite_size` */
    playlist?: boolean;
    /** @default `pfc :: infinite_size` */
    index?: unknown;
}

/**
 * Parameters for `playlist.rename`.
 */
export interface PlaylistRenameParams {
    playlist?: number;
    /** @default "" */
    name?: string;
}

/**
 * Parameters for `playlist.reorder`.
 */
export interface PlaylistReorderParams {
    playlist?: number;
    /** @default `json :: array ( )` */
    newOrder?: unknown[];
}

/**
 * Parameters for `playlist.reorderPlaylists`.
 */
export interface PlaylistReorderPlaylistsParams {
    /** @default `json :: array ( )` */
    newOrder?: unknown[];
}

/**
 * Parameters for `playlist.replaceAllAndPlay`.
 */
export interface PlaylistReplaceAllAndPlayParams {
    playlist?: number;
    /** @default `json :: array ( )` */
    paths?: unknown[];
    /**
     * int64 — may lose precision above 2^53
     * @default `static_cast < size_t > ( 0 )`
     */
    playIndex?: number;
    /** @default true */
    stopFirst?: boolean;
    /** @default true */
    autoPlay?: boolean;
}

/**
 * Parameters for `playlist.reverse`.
 */
export interface PlaylistReverseParams {
    playlist?: number;
}

/**
 * Parameters for `playlist.selectAll`.
 */
export interface PlaylistSelectAllParams {
    playlist?: number;
}

/**
 * Parameters for `playlist.setActive`.
 */
export interface PlaylistSetActiveParams {
    playlist?: number;
}

/**
 * Parameters for `playlist.setFocusedTrack`.
 */
export interface PlaylistSetFocusedTrackParams {
    playlist?: number;
    index?: number;
}

/**
 * Parameters for `playlist.setSelection`.
 */
export interface PlaylistSetSelectionParams {
    /** @default `json :: array ( )` */
    indices?: unknown[];
    /** @default true */
    clearOthers?: boolean;
    /** @default `pfc :: infinite_size` */
    playlist?: boolean;
    /** @default `pfc :: infinite_size` */
    index?: unknown;
}

/**
 * Parameters for `playlist.shuffle`.
 */
export interface PlaylistShuffleParams {
    /** @default `pfc :: infinite_size` */
    playlist?: boolean;
    /** @default `pfc :: infinite_size` */
    index?: unknown;
}

/**
 * Parameters for `playlist.sort`.
 */
export interface PlaylistSortParams {
    /** @default "%title%" */
    pattern?: string;
    /** @default false */
    descending?: boolean;
    /** @default false */
    selectedOnly?: boolean;
    /** @default `pfc :: infinite_size` */
    playlist?: boolean;
    /** @default `pfc :: infinite_size` */
    index?: unknown;
}

/**
 * Parameters for `playlist.undo`.
 */
export interface PlaylistUndoParams {
    playlist?: number;
}

/**
 * Parameters for `port.connect`.
 */
export interface PortConnectParams {
    /** @default "" */
    name?: string;
}

/**
 * Parameters for `port.disconnect`.
 */
export interface PortDisconnectParams {
    /** @default "" */
    portId?: string;
}

/**
 * Parameters for `port.getPorts`.
 */
export interface PortGetPortsParams {
    name?: string;
}

/**
 * Parameters for `port.postMessage`.
 */
export interface PortPostMessageParams {
    /** @default "" */
    portId?: string;
    message?: unknown;
}

/**
 * Parameters for `port.postMessageTo`.
 */
export interface PortPostMessageToParams {
    /** @default "" */
    portId?: string;
    /** @default "" */
    targetPortId?: string;
    message?: unknown;
}

/**
 * Parameters for `queue.add`.
 */
export interface QueueAddParams {
    /** @default `pfc :: infinite_size` */
    playlist?: unknown;
    /** int64 — may lose precision above 2^53 */
    tracks?: number[];
    /** @default `pfc :: infinite_size` */
    track?: boolean;
}

/**
 * Parameters for `queue.addPaths`.
 */
export interface QueueAddPathsParams {
    /** @default `json :: array ( )` */
    paths?: unknown[];
    /** @default true */
    useQueuePlaylist?: boolean;
    /** @default `pfc :: infinite_size` */
    playlist?: boolean;
}

/**
 * Parameters for `queue.clear` (this API takes no parameters).
 */
export type QueueClearParams = Record<string, never>;

/**
 * Parameters for `queue.flush` (this API takes no parameters).
 */
export type QueueFlushParams = Record<string, never>;

/**
 * Parameters for `queue.get` (this API takes no parameters).
 */
export type QueueGetParams = Record<string, never>;

/**
 * Parameters for `queue.getCount` (this API takes no parameters).
 */
export type QueueGetCountParams = Record<string, never>;

/**
 * Parameters for `queue.moveToTop`.
 */
export interface QueueMoveToTopParams {
    /** @default `pfc :: infinite_size` */
    index?: unknown;
}

/**
 * Parameters for `queue.remove`.
 */
export interface QueueRemoveParams {
    /** @default `pfc :: infinite_size` */
    index?: boolean;
    /** int64 — may lose precision above 2^53 */
    indices?: number[];
}

/**
 * Parameters for `rating.get`.
 */
export interface RatingGetParams {
    /** @default "" */
    path?: string;
    /** @default `- 1` */
    cueIndex?: number;
}

/**
 * Parameters for `rating.set`.
 */
export interface RatingSetParams {
    /** @default "" */
    path?: string;
    /** @default `- 1` */
    rating?: number;
    /** @default `- 1` */
    cueIndex?: number;
    /** @default false */
    success?: boolean;
}

/**
 * Parameters for `replaygain.clear`.
 */
export interface ReplaygainClearParams {
    paths?: unknown;
}

/**
 * Parameters for `replaygain.get`.
 */
export interface ReplaygainGetParams {
    paths?: unknown;
}

/**
 * Parameters for `replaygain.getMode` (this API takes no parameters).
 */
export type ReplaygainGetModeParams = Record<string, never>;

/**
 * Parameters for `replaygain.getPreamp` (this API takes no parameters).
 */
export type ReplaygainGetPreampParams = Record<string, never>;

/**
 * Parameters for `replaygain.getSettings` (this API takes no parameters).
 */
export type ReplaygainGetSettingsParams = Record<string, never>;

/**
 * Parameters for `replaygain.scan`.
 */
export interface ReplaygainScanParams {
    paths?: unknown;
    /** @default "track" */
    mode?: string;
    album?: unknown;
    track?: unknown;
    Scan?: unknown;
}

/**
 * Parameters for `replaygain.setMode`.
 */
export interface ReplaygainSetModeParams {
    sourceMode?: string;
    processingMode?: string;
}

/**
 * Parameters for `replaygain.setPreamp`.
 */
export interface ReplaygainSetPreampParams {
    withRg?: number;
    withoutRg?: number;
}

/**
 * Parameters for `selection.get`.
 */
export interface SelectionGetParams {
    /**
     * int64 — may lose precision above 2^53
     * @default `static_cast < size_t > ( 0 )`
     */
    offset?: number;
    /**
     * int64 — may lose precision above 2^53
     * @default `static_cast < size_t > ( 100 )`
     */
    limit?: number;
}

/**
 * Parameters for `selection.getType` (this API takes no parameters).
 */
export type SelectionGetTypeParams = Record<string, never>;

/**
 * Parameters for `selection.getViewerMode` (this API takes no parameters).
 */
export type SelectionGetViewerModeParams = Record<string, never>;

/**
 * Parameters for `selection.getViewingTrack`.
 */
export interface SelectionGetViewingTrackParams {
    /** @default false */
    includeTrackInfo?: boolean;
}

/**
 * Parameters for `selection.set`.
 */
export interface SelectionSetParams {
    handles?: unknown;
}

/**
 * Parameters for `selection.setPlaylistTracking`.
 */
export interface SelectionSetPlaylistTrackingParams {
    /** @default "selection" */
    mode?: string;
}

/**
 * Parameters for `shell.exec`.
 */
export interface ShellExecParams {
    /** @default "" */
    command?: string;
    /** @default true */
    hidden?: boolean;
    /** @default "" */
    cwd?: string;
    args?: string[];
}

/**
 * Parameters for `shell.openExternal`.
 */
export interface ShellOpenExternalParams {
    /** @default "" */
    url?: string;
}

/**
 * Parameters for `shell.openWith`.
 */
export interface ShellOpenWithParams {
    /** @default "" */
    path?: string;
}

/**
 * Parameters for `shell.showInExplorer`.
 */
export interface ShellShowInExplorerParams {
    /** @default "" */
    path?: string;
}

/**
 * Parameters for `shell.spawn`.
 */
export interface ShellSpawnParams {
    /** @default "" */
    executable?: string;
    /** @default true */
    hidden?: boolean;
    /** @default "" */
    cwd?: string;
    /** @default 0 */
    waitForExitMs?: number;
    args?: string[];
}

/**
 * Parameters for `state.delete`.
 */
export interface StateDeleteParams {
    /** @default "" */
    key?: string;
}

/**
 * Parameters for `state.get`.
 */
export interface StateGetParams {
    /** @default "" */
    key?: string;
}

/**
 * Parameters for `state.keys`.
 */
export interface StateKeysParams {
    /** @default "*" */
    pattern?: string;
}

/**
 * Parameters for `state.set`.
 */
export interface StateSetParams {
    /** @default "" */
    key?: string;
    value?: unknown;
    /** @default false */
    silent?: boolean;
    /** int64 — may lose precision above 2^53 */
    ttlMs?: number;
}

/**
 * Parameters for `system.getApisByNamespace`.
 */
export interface SystemGetApisByNamespaceParams {
    /** @default "" */
    namespace?: string;
}

/**
 * Parameters for `system.getApiStats` (this API takes no parameters).
 */
export type SystemGetApiStatsParams = Record<string, never>;

/**
 * Parameters for `system.getDPI` (shape unspecified — pass any JSON object).
 */
export type SystemGetDPIParams = JsonObject;

/**
 * Parameters for `system.getLocale` (this API takes no parameters).
 */
export type SystemGetLocaleParams = Record<string, never>;

/**
 * Parameters for `system.getRegisteredPlugins` (this API takes no parameters).
 */
export type SystemGetRegisteredPluginsParams = Record<string, never>;

/**
 * Parameters for `system.getTheme` (this API takes no parameters).
 */
export type SystemGetThemeParams = Record<string, never>;

/**
 * Parameters for `system.isPluginRegistered`.
 */
export interface SystemIsPluginRegisteredParams {
    /** @default "" */
    namespace?: string;
}

/**
 * Parameters for `system.listAvailableApis`.
 */
export interface SystemListAvailableApisParams {
    /** @default true */
    includeInternal?: boolean;
    /** @default true */
    includeExternal?: boolean;
}

/**
 * Parameters for `system.searchApis`.
 */
export interface SystemSearchApisParams {
    /** @default "" */
    query?: string;
}

/**
 * Parameters for `taskbar.flash`.
 */
export interface TaskbarFlashParams {
    /** @default 3 */
    count?: number;
    /** @default 0 */
    interval?: number;
}

/**
 * Parameters for `taskbar.setOverlayIcon`.
 */
export interface TaskbarSetOverlayIconParams {
    description?: string;
}

/**
 * Parameters for `taskbar.setProgress`.
 */
export interface TaskbarSetProgressParams {
    /** @default "none" */
    state?: string;
    value?: number;
}

/**
 * Parameters for `taskbar.setThumbnailButtons`.
 */
export interface TaskbarSetThumbnailButtonsParams {
    buttons?: string[];
    /** @default "" */
    id?: string;
    /** @default "" */
    icon?: string;
    /** @default "" */
    tooltip?: string;
    /** @default true */
    enabled?: boolean;
    /** @default true */
    visible?: boolean;
    /** @default false */
    dismissOnClick?: boolean;
}

/**
 * Parameters for `taskbar.updateButton`.
 */
export interface TaskbarUpdateButtonParams {
    /** @default "" */
    id?: string;
    enabled?: boolean;
    visible?: boolean;
    tooltip?: string;
}

/**
 * Parameters for `test.echo`.
 */
export interface TestEchoParams {
    message?: unknown;
}

/**
 * Parameters for `test.ping` (this API takes no parameters).
 */
export type TestPingParams = Record<string, never>;

/**
 * Parameters for `titleformat.eval`.
 */
export interface TitleformatEvalParams {
    /** @default "" */
    path?: string;
    /** @default "" */
    pattern?: string;
}

/**
 * Parameters for `titleformat.evalBatch`.
 */
export interface TitleformatEvalBatchParams {
    paths?: unknown;
    /** @default "" */
    pattern?: string;
}

/**
 * Parameters for `titleformat.evalFields`.
 */
export interface TitleformatEvalFieldsParams {
    /** @default "" */
    path?: string;
    fields?: unknown;
}

/**
 * Parameters for `titleformat.evalFieldsBatch`.
 */
export interface TitleformatEvalFieldsBatchParams {
    paths?: unknown;
    fields?: unknown;
}

/**
 * Parameters for `titleformat.getBuiltinFields` (this API takes no parameters).
 */
export type TitleformatGetBuiltinFieldsParams = Record<string, never>;

/**
 * Parameters for `tray.appendMenuItems`.
 */
export interface TrayAppendMenuItemsParams {
    items?: unknown;
    position?: string;
}

/**
 * Parameters for `tray.clearMenuItems`.
 */
export interface TrayClearMenuItemsParams {
    position?: string;
}

/**
 * Parameters for `tray.create`.
 */
export interface TrayCreateParams {
    /** @default "foobar2000" */
    tooltip?: string;
}

/**
 * Parameters for `tray.destroy` (this API takes no parameters).
 */
export type TrayDestroyParams = Record<string, never>;

/**
 * Parameters for `tray.getMenuItems` (this API takes no parameters).
 */
export type TrayGetMenuItemsParams = Record<string, never>;

/**
 * Parameters for `tray.isVisible` (this API takes no parameters).
 */
export type TrayIsVisibleParams = Record<string, never>;

/**
 * Parameters for `tray.removeMenuItems`.
 */
export interface TrayRemoveMenuItemsParams {
    ids?: string[];
}

/**
 * Parameters for `tray.setCloseToTray`.
 */
export interface TraySetCloseToTrayParams {
    /** @default false */
    enabled?: boolean;
}

/**
 * Parameters for `tray.setContextMenu`.
 */
export interface TraySetContextMenuParams {
    items?: unknown;
    config?: unknown;
    showPlaybackControls?: unknown;
    showSystemItems?: unknown;
    customPosition?: unknown;
    render?: unknown;
}

/**
 * Parameters for `tray.setMenuItemState`.
 */
export interface TraySetMenuItemStateParams {
    /** @default "" */
    id?: string;
    checked?: boolean;
    enabled?: boolean;
}

/**
 * Parameters for `tray.setMinimizeToTray`.
 */
export interface TraySetMinimizeToTrayParams {
    /** @default false */
    enabled?: boolean;
}

/**
 * Parameters for `tray.setTooltip`.
 */
export interface TraySetTooltipParams {
    /** @default "" */
    tooltip?: string;
}

/**
 * Parameters for `tray.showBalloon`.
 */
export interface TrayShowBalloonParams {
    /** @default "" */
    title?: string;
    /** @default "" */
    message?: string;
    /** @default "info" */
    icon?: string;
}

/**
 * Parameters for `ui.hideNotification` (this API takes no parameters).
 */
export type UiHideNotificationParams = Record<string, never>;

/**
 * Parameters for `ui.showContextMenu`.
 */
export interface UiShowContextMenuParams {
    /** @default `- 1` */
    x?: number;
    /** @default `- 1` */
    y?: number;
}

/**
 * Parameters for `ui.showCustomMenu`.
 */
export interface UiShowCustomMenuParams {
    items?: unknown;
    /** @default 0 */
    x?: number;
    /** @default 0 */
    y?: number;
    /** @default false */
    suppressDefault?: boolean;
    /** @default 0 */
    w?: number;
    /** @default 0 */
    h?: number;
    /** @default "item" */
    type?: string;
    submenu?: unknown;
    /** @default "" */
    label?: string;
    /** @default "" */
    id?: string;
    /** @default true */
    enabled?: boolean;
    /** @default "" */
    shortcut?: string;
    /** @default false */
    checked?: boolean;
}

/**
 * Parameters for `ui.showNotification`.
 */
export interface UiShowNotificationParams {
    /** @default "" */
    title?: string;
    /** @default "" */
    body?: string;
    /** @default false */
    silent?: boolean;
    /** @default 5000 */
    timeout?: number;
}

/**
 * Parameters for `ui.showToast`.
 */
export interface UiShowToastParams {
    /** @default "" */
    message?: string;
    /** @default 3000 */
    duration?: number;
    /** @default "info" */
    type?: string;
    /** @default "bottom-right" */
    position?: string;
}

/**
 * Parameters for `window.blur` (shape unspecified — pass any JSON object).
 */
export type WindowBlurParams = JsonObject;

/**
 * Parameters for `window.broadcast`.
 */
export interface WindowBroadcastParams {
    message?: unknown;
}

/**
 * Parameters for `window.cancelClose` (shape unspecified — pass any JSON object).
 */
export type WindowCancelCloseParams = JsonObject;

/**
 * Parameters for `window.center` (shape unspecified — pass any JSON object).
 */
export type WindowCenterParams = JsonObject;

/**
 * Parameters for `window.clearClickThroughExcludeRegions`.
 */
export interface WindowClearClickThroughExcludeRegionsParams {
    /** @default "" */
    windowId?: string;
}

/**
 * Parameters for `window.clearDragRegions` (shape unspecified — pass any JSON object).
 */
export type WindowClearDragRegionsParams = JsonObject;

/**
 * Parameters for `window.clearNoDragRegions` (shape unspecified — pass any JSON object).
 */
export type WindowClearNoDragRegionsParams = JsonObject;

/**
 * Parameters for `window.close` (shape unspecified — pass any JSON object).
 */
export type WindowCloseParams = JsonObject;

/**
 * Parameters for `window.closeAllPopups` (this API takes no parameters).
 */
export type WindowCloseAllPopupsParams = Record<string, never>;

/**
 * Parameters for `window.closePopup`.
 */
export interface WindowClosePopupParams {
    /** @default "" */
    windowId?: string;
}

/**
 * Parameters for `window.confirmClose` (shape unspecified — pass any JSON object).
 */
export type WindowConfirmCloseParams = JsonObject;

/**
 * Parameters for `window.enterFullscreen` (this API takes no parameters).
 */
export type WindowEnterFullscreenParams = Record<string, never>;

/**
 * Parameters for `window.exitFullscreen` (this API takes no parameters).
 */
export type WindowExitFullscreenParams = Record<string, never>;

/**
 * Parameters for `window.flash`.
 */
export interface WindowFlashParams {
    /** @default true */
    enabled?: boolean;
    /** @default 3 */
    count?: number;
}

/**
 * Parameters for `window.flashTaskbar`.
 */
export interface WindowFlashTaskbarParams {
    /** @default 3 */
    count?: number;
}

/**
 * Parameters for `window.focus`.
 */
export interface WindowFocusParams {
    /** @default "" */
    windowId?: string;
}

/**
 * Parameters for `window.getAllWindows` (this API takes no parameters).
 */
export type WindowGetAllWindowsParams = Record<string, never>;

/**
 * Parameters for `window.getBackdropPolicy` (this API takes no parameters).
 */
export type WindowGetBackdropPolicyParams = Record<string, never>;

/**
 * Parameters for `window.getBounds` (shape unspecified — pass any JSON object).
 */
export type WindowGetBoundsParams = JsonObject;

/**
 * Parameters for `window.getCaptionButtonsWidth` (this API takes no parameters).
 */
export type WindowGetCaptionButtonsWidthParams = Record<string, never>;

/**
 * Parameters for `window.getCornerPreference` (this API takes no parameters).
 */
export type WindowGetCornerPreferenceParams = Record<string, never>;

/**
 * Parameters for `window.getCurrentWindowId` (shape unspecified — pass any JSON object).
 */
export type WindowGetCurrentWindowIdParams = JsonObject;

/**
 * Parameters for `window.getDevServerConfig` (this API takes no parameters).
 */
export type WindowGetDevServerConfigParams = Record<string, never>;

/**
 * Parameters for `window.getDpiScale` (shape unspecified — pass any JSON object).
 */
export type WindowGetDpiScaleParams = JsonObject;

/**
 * Parameters for `window.getMaxSize` (this API takes no parameters).
 */
export type WindowGetMaxSizeParams = Record<string, never>;

/**
 * Parameters for `window.getMinSize` (this API takes no parameters).
 */
export type WindowGetMinSizeParams = Record<string, never>;

/**
 * Parameters for `window.getMode` (shape unspecified — pass any JSON object).
 */
export type WindowGetModeParams = JsonObject;

/**
 * Parameters for `window.getPopupBehavior`.
 */
export interface WindowGetPopupBehaviorParams {
    /** @default "" */
    windowId?: string;
}

/**
 * Parameters for `window.getState` (shape unspecified — pass any JSON object).
 */
export type WindowGetStateParams = JsonObject;

/**
 * Parameters for `window.getTitle` (shape unspecified — pass any JSON object).
 */
export type WindowGetTitleParams = JsonObject;

/**
 * Parameters for `window.getTitlebarHeight` (shape unspecified — pass any JSON object).
 */
export type WindowGetTitlebarHeightParams = JsonObject;

/**
 * Parameters for `window.getTitlebarInfo` (this API takes no parameters).
 */
export type WindowGetTitlebarInfoParams = Record<string, never>;

/**
 * Parameters for `window.getZoom` (shape unspecified — pass any JSON object).
 */
export type WindowGetZoomParams = JsonObject;

/**
 * Parameters for `window.hasSavedBounds` (this API takes no parameters).
 */
export type WindowHasSavedBoundsParams = Record<string, never>;

/**
 * Parameters for `window.isAlwaysOnTop` (shape unspecified — pass any JSON object).
 */
export type WindowIsAlwaysOnTopParams = JsonObject;

/**
 * Parameters for `window.isClickThrough`.
 */
export interface WindowIsClickThroughParams {
    /** @default "" */
    windowId?: string;
}

/**
 * Parameters for `window.isFullscreen` (this API takes no parameters).
 */
export type WindowIsFullscreenParams = Record<string, never>;

/**
 * Parameters for `window.isMaximized` (shape unspecified — pass any JSON object).
 */
export type WindowIsMaximizedParams = JsonObject;

/**
 * Parameters for `window.isMinimized` (shape unspecified — pass any JSON object).
 */
export type WindowIsMinimizedParams = JsonObject;

/**
 * Parameters for `window.isResizable` (this API takes no parameters).
 */
export type WindowIsResizableParams = Record<string, never>;

/**
 * Parameters for `window.maximize` (shape unspecified — pass any JSON object).
 */
export type WindowMaximizeParams = JsonObject;

/**
 * Parameters for `window.minimize` (shape unspecified — pass any JSON object).
 */
export type WindowMinimizeParams = JsonObject;

/**
 * Parameters for `window.refreshWebView` (shape unspecified — pass any JSON object).
 */
export type WindowRefreshWebViewParams = JsonObject;

/**
 * Parameters for `window.reload` (shape unspecified — pass any JSON object).
 */
export type WindowReloadParams = JsonObject;

/**
 * Parameters for `window.resetZoom` (shape unspecified — pass any JSON object).
 */
export type WindowResetZoomParams = JsonObject;

/**
 * Parameters for `window.restore` (shape unspecified — pass any JSON object).
 */
export type WindowRestoreParams = JsonObject;

/**
 * Parameters for `window.sendMessage`.
 */
export interface WindowSendMessageParams {
    /** @default "" */
    targetWindowId?: string;
    message?: unknown;
}

/**
 * Parameters for `window.setAcrylic`.
 */
export interface WindowSetAcrylicParams {
    /** @default true */
    enabled?: boolean;
    /** @default true */
    darkMode?: boolean;
}

/**
 * Parameters for `window.setAlwaysOnTop`.
 */
export interface WindowSetAlwaysOnTopParams {
    /** @default true */
    enabled?: boolean;
}

/**
 * Parameters for `window.setBackdropPolicy`.
 */
export interface WindowSetBackdropPolicyParams {
    backdropPolicy?: unknown;
}

/**
 * Parameters for `window.setBackgroundTransparency`.
 */
export interface WindowSetBackgroundTransparencyParams {
    /** @default true */
    transparent?: boolean;
}

/**
 * Parameters for `window.setBlur`.
 */
export interface WindowSetBlurParams {
    /** @default true */
    enabled?: boolean;
}

/**
 * Parameters for `window.setBounds`.
 */
export interface WindowSetBoundsParams {
    x?: number;
    y?: number;
    width?: number;
    height?: number;
}

/**
 * Parameters for `window.setClickThrough`.
 */
export interface WindowSetClickThroughParams {
    /** @default true */
    enabled?: boolean;
    /** @default "" */
    windowId?: string;
}

/**
 * Parameters for `window.setClickThroughExcludeRegions`.
 */
export interface WindowSetClickThroughExcludeRegionsParams {
    /** @default "" */
    windowId?: string;
    regions?: unknown;
    /** @default 0 */
    width?: number;
    /** @default 0 */
    height?: number;
    /** @default 0 */
    x?: number;
    /** @default 0 */
    y?: number;
}

/**
 * Parameters for `window.setCornerPreference`.
 */
export interface WindowSetCornerPreferenceParams {
    /** @default "default" */
    mode?: string;
}

/**
 * Parameters for `window.setDarkMode`.
 */
export interface WindowSetDarkModeParams {
    /** @default true */
    enabled?: boolean;
}

/**
 * Parameters for `window.setDevServerConfig`.
 */
export interface WindowSetDevServerConfigParams {
    /** @default false */
    useDevServer?: boolean;
    /** @default "" */
    devServerUrl?: string;
}

/**
 * Parameters for `window.setDragRegions`.
 */
export interface WindowSetDragRegionsParams {
    regions?: unknown;
    /** @default 0 */
    x?: number;
    /** @default 0 */
    y?: number;
    /** @default 0 */
    width?: number;
    /** @default 0 */
    height?: number;
}

/**
 * Parameters for `window.setFrameless`.
 */
export interface WindowSetFramelessParams {
    /** @default true */
    frameless?: boolean;
}

/**
 * Parameters for `window.setFullscreen`.
 */
export interface WindowSetFullscreenParams {
    /** @default true */
    enabled?: boolean;
}

/**
 * Parameters for `window.setMaxSize`.
 */
export interface WindowSetMaxSizeParams {
    /** @default 0 */
    width?: number;
    /** @default 0 */
    height?: number;
}

/**
 * Parameters for `window.setMica`.
 */
export interface WindowSetMicaParams {
    /** @default true */
    enabled?: boolean;
    /** @default "mica" */
    variant?: string;
    /** @default true */
    darkMode?: boolean;
}

/**
 * Parameters for `window.setMicaEffect`.
 */
export interface WindowSetMicaEffectParams {
    /** @default true */
    enabled?: boolean;
    /** @default "mica" */
    variant?: string;
    /** @default true */
    darkMode?: boolean;
}

/**
 * Parameters for `window.setMinSize`.
 */
export interface WindowSetMinSizeParams {
    /** @default 0 */
    width?: number;
    /** @default 0 */
    height?: number;
}

/**
 * Parameters for `window.setNoDragRegions`.
 */
export interface WindowSetNoDragRegionsParams {
    regions?: unknown;
    /** @default 0 */
    x?: number;
    /** @default 0 */
    y?: number;
    /** @default 0 */
    width?: number;
    /** @default 0 */
    height?: number;
}

/**
 * Parameters for `window.setPosition`.
 */
export interface WindowSetPositionParams {
    /** @default 0 */
    x?: number;
    /** @default 0 */
    y?: number;
}

/**
 * Parameters for `window.setResizable`.
 */
export interface WindowSetResizableParams {
    /** @default true */
    resizable?: boolean;
}

/**
 * Parameters for `window.setSize`.
 */
export interface WindowSetSizeParams {
    /** @default 800 */
    width?: number;
    /** @default 600 */
    height?: number;
}

/**
 * Parameters for `window.setTitle`.
 */
export interface WindowSetTitleParams {
    /** @default "foobar2000" */
    title?: string;
}

/**
 * Parameters for `window.setTitlebarHeight`.
 */
export interface WindowSetTitlebarHeightParams {
    /** @default 32 */
    height?: number;
}

/**
 * Parameters for `window.setZoom`.
 */
export interface WindowSetZoomParams {
    /** @default 1 */
    zoom?: number;
}

/**
 * Parameters for `window.setZoomForDpi`.
 */
export interface WindowSetZoomForDpiParams {
    /** @default 0 */
    dpi?: number;
}

/**
 * Parameters for `window.showSystemMenu`.
 */
export interface WindowShowSystemMenuParams {
    /** @default 0 */
    x?: number;
    /** @default 0 */
    y?: number;
    /** @default 0 */
    w?: number;
    /** @default 0 */
    h?: number;
}

/**
 * Parameters for `window.startDrag` (shape unspecified — pass any JSON object).
 */
export type WindowStartDragParams = JsonObject;

/**
 * Parameters for `window.startResize`.
 */
export interface WindowStartResizeParams {
    /** @default "bottomright" */
    edge?: string;
}

/**
 * Parameters for `window.toggleAlwaysOnTop` (shape unspecified — pass any JSON object).
 */
export type WindowToggleAlwaysOnTopParams = JsonObject;

/**
 * Parameters for `window.toggleFullscreen` (this API takes no parameters).
 */
export type WindowToggleFullscreenParams = Record<string, never>;

/**
 * Parameters for `window.toggleMaximize` (shape unspecified — pass any JSON object).
 */
export type WindowToggleMaximizeParams = JsonObject;
// ── API-name → Params map ────────────────────────────────────────────────
/**
 * Master map from C++ `api_name` to the generated / override `*Params` type.
 * Keys mirror `cpp_api_handler.metadata.api_name` one-to-one.
 */
export interface ApiParamsMap {
    "artwork.getAvailableArtwork": ArtworkGetAvailableArtworkParams;
    "artwork.getAvailableTypes": ArtworkGetAvailableTypesParams;
    "artwork.getBatch": ArtworkGetBatchParams;
    "artwork.getByPath": ArtworkGetByPathParams;
    "artwork.getByPlaylistItem": ArtworkGetByPlaylistItemParams;
    "artwork.getCurrent": ArtworkGetCurrentParams;
    "artwork.getFb2kUrl": ArtworkGetFb2kUrlParams;
    "artwork.getFb2kUrlByPath": ArtworkGetFb2kUrlByPathParams;
    "artwork.getFb2kUrlByPathBatch": ArtworkGetFb2kUrlByPathBatchParams;
    "artwork.getFolderImages": ArtworkGetFolderImagesParams;
    "artwork.getForTrack": ArtworkGetForTrackParams;
    "artwork.getLyrics": ArtworkGetLyricsParams;
    "artwork.getMetadata": ArtworkGetMetadataParams;
    "audio.analyzeBPM": AudioAnalyzeBPMParams;
    "audio.generateFullWaveform": AudioGenerateFullWaveformParams;
    "audio.generateWaveform": AudioGenerateWaveformParams;
    "audio.getOutputInfo": AudioGetOutputInfoParams;
    "audio.getSpectrum": AudioGetSpectrumParams;
    "audio.getSpectrumDebugState": AudioGetSpectrumDebugStateParams;
    "audio.getStreamInfo": AudioGetStreamInfoParams;
    "audio.getWaveform": AudioGetWaveformParams;
    "audio.isVisualizationAvailable": AudioIsVisualizationAvailableParams;
    "audio.setChannelMode": AudioSetChannelModeParams;
    "audio.subscribeSpectrum": AudioSubscribeSpectrumParams;
    "audio.subscribeStream": AudioSubscribeStreamParams;
    "audio.unsubscribeSpectrum": AudioUnsubscribeSpectrumParams;
    "audio.unsubscribeStream": AudioUnsubscribeStreamParams;
    "clipboard.read": ClipboardReadParams;
    "clipboard.write": ClipboardWriteParams;
    "clipboard.writeFiles": ClipboardWriteFilesParams;
    "clipboard.writeHTML": ClipboardWriteHTMLParams;
    "config.export": ConfigExportParams;
    "config.get": ConfigGetParams;
    "config.getActiveDspPreset": ConfigGetActiveDspPresetParams;
    "config.getAdvancedConfig": ConfigGetAdvancedConfigParams;
    "config.getAdvancedConfigValue": ConfigGetAdvancedConfigValueParams;
    "config.getAll": ConfigGetAllParams;
    "config.getComponents": ConfigGetComponentsParams;
    "config.getCursorFollowPlayback": ConfigGetCursorFollowPlaybackParams;
    "config.getDspPresets": ConfigGetDspPresetsParams;
    "config.getLibraryFilePatterns": ConfigGetLibraryFilePatternsParams;
    "config.getLibraryStatus": ConfigGetLibraryStatusParams;
    "config.getOutputConfig": ConfigGetOutputConfigParams;
    "config.getOutputDevices": ConfigGetOutputDevicesParams;
    "config.getPlaybackFollowCursor": ConfigGetPlaybackFollowCursorParams;
    "config.getPreferencesPages": ConfigGetPreferencesPagesParams;
    "config.getPreferencesStandardGuids": ConfigGetPreferencesStandardGuidsParams;
    "config.getReplaygainMode": ConfigGetReplaygainModeParams;
    "config.getVersionInfo": ConfigGetVersionInfoParams;
    "config.remove": ConfigRemoveParams;
    "config.resetAdvancedConfig": ConfigResetAdvancedConfigParams;
    "config.set": ConfigSetParams;
    "config.setActiveDspPreset": ConfigSetActiveDspPresetParams;
    "config.setAdvancedConfigValue": ConfigSetAdvancedConfigValueParams;
    "config.setCursorFollowPlayback": ConfigSetCursorFollowPlaybackParams;
    "config.setOutputBuffer": ConfigSetOutputBufferParams;
    "config.setOutputDevice": ConfigSetOutputDeviceParams;
    "config.setPlaybackFollowCursor": ConfigSetPlaybackFollowCursorParams;
    "config.setReplaygainMode": ConfigSetReplaygainModeParams;
    "config.showLibraryPreferences": ConfigShowLibraryPreferencesParams;
    "console.error": ConsoleErrorParams;
    "console.log": ConsoleLogParams;
    "console.warn": ConsoleWarnParams;
    "cursor.isHidden": CursorIsHiddenParams;
    "cursor.setHidden": CursorSetHiddenParams;
    "dialog.confirm": DialogConfirmParams;
    "dialog.openFile": DialogOpenFileParams;
    "dialog.openFolder": DialogOpenFolderParams;
    "dialog.saveFile": DialogSaveFileParams;
    "discovery.executeContextMenuByPath": DiscoveryExecuteContextMenuByPathParams;
    "discovery.executeContextMenuCommand": DiscoveryExecuteContextMenuCommandParams;
    "discovery.executeMainMenuCommand": DiscoveryExecuteMainMenuCommandParams;
    "discovery.getAllServices": DiscoveryGetAllServicesParams;
    "discovery.getComponents": DiscoveryGetComponentsParams;
    "discovery.getContextMenuCommands": DiscoveryGetContextMenuCommandsParams;
    "discovery.getContextMenuTree": DiscoveryGetContextMenuTreeParams;
    "discovery.getDspEntries": DiscoveryGetDspEntriesParams;
    "discovery.getInputFormats": DiscoveryGetInputFormatsParams;
    "discovery.getMainMenuCommands": DiscoveryGetMainMenuCommandsParams;
    "discovery.getMainMenuGroups": DiscoveryGetMainMenuGroupsParams;
    "discovery.getOutputDevices": DiscoveryGetOutputDevicesParams;
    "discovery.getPreferencePages": DiscoveryGetPreferencePagesParams;
    "discovery.getUIElements": DiscoveryGetUIElementsParams;
    "discovery.searchCommands": DiscoverySearchCommandsParams;
    "dnd.getDropZones": DndGetDropZonesParams;
    "dnd.registerDropZone": DndRegisterDropZoneParams;
    "dnd.startDrag": DndStartDragParams;
    "dnd.unregisterDropZone": DndUnregisterDropZoneParams;
    "dsp.addDsp": DspAddDspParams;
    "dsp.applyPreset": DspApplyPresetParams;
    "dsp.getAvailable": DspGetAvailableParams;
    "dsp.getChain": DspGetChainParams;
    "dsp.getPresets": DspGetPresetsParams;
    "dsp.moveDsp": DspMoveDspParams;
    "dsp.removeDsp": DspRemoveDspParams;
    "dsp.setChain": DspSetChainParams;
    "event.emit": EventEmitParams;
    "event.emitTo": EventEmitToParams;
    "file.copy": FileCopyParams;
    "file.delete": FileDeleteParams;
    "file.exists": FileExistsParams;
    "file.getInfo": FileGetInfoParams;
    "file.list": FileListParams;
    "file.mkdir": FileMkdirParams;
    "file.move": FileMoveParams;
    "file.read": FileReadParams;
    "file.rename": FileRenameParams;
    "file.write": FileWriteParams;
    "http.abort": HttpAbortParams;
    "http.delete": HttpDeleteParams;
    "http.download": HttpDownloadParams;
    "http.get": HttpGetParams;
    "http.head": HttpHeadParams;
    "http.patch": HttpPatchParams;
    "http.post": HttpPostParams;
    "http.put": HttpPutParams;
    "jitQueue.clear": JitQueueClearParams;
    "jitQueue.enqueueNext": JitQueueEnqueueNextParams;
    "jitQueue.getState": JitQueueGetStateParams;
    "jitQueue.notifyEmpty": JitQueueNotifyEmptyParams;
    "jitQueue.playNow": JitQueuePlayNowParams;
    "jitQueue.preloadBatch": JitQueuePreloadBatchParams;
    "jitQueue.skip": JitQueueSkipParams;
    "jitQueue.stop": JitQueueStopParams;
    "keyboard.getRegisteredHotkeys": KeyboardGetRegisteredHotkeysParams;
    "keyboard.registerHotkey": KeyboardRegisterHotkeyParams;
    "keyboard.registerShortcut": KeyboardRegisterShortcutParams;
    "keyboard.unregisterHotkey": KeyboardUnregisterHotkeyParams;
    "library.addToPlaylist": LibraryAddToPlaylistParams;
    "library.browseDirectory": LibraryBrowseDirectoryParams;
    "library.browseTree": LibraryBrowseTreeParams;
    "library.getAlbums": LibraryGetAlbumsParams;
    "library.getAlbumTracks": LibraryGetAlbumTracksParams;
    "library.getAll": LibraryGetAllParams;
    "library.getArtistAlbums": LibraryGetArtistAlbumsParams;
    "library.getArtists": LibraryGetArtistsParams;
    "library.getArtistTracks": LibraryGetArtistTracksParams;
    "library.getByPath": LibraryGetByPathParams;
    "library.getCacheStats": LibraryGetCacheStatsParams;
    "library.getCount": LibraryGetCountParams;
    "library.getFieldValues": LibraryGetFieldValuesParams;
    "library.getGenres": LibraryGetGenresParams;
    "library.getRandomTracks": LibraryGetRandomTracksParams;
    "library.getRecentlyAdded": LibraryGetRecentlyAddedParams;
    "library.getRoots": LibraryGetRootsParams;
    "library.getStats": LibraryGetStatsParams;
    "library.getStatus": LibraryGetStatusParams;
    "library.invalidateCache": LibraryInvalidateCacheParams;
    "library.isEnabled": LibraryIsEnabledParams;
    "library.query": LibraryQueryParams;
    "library.refresh": LibraryRefreshParams;
    "library.rescan": LibraryRescanParams;
    "library.search": LibrarySearchParams;
    "log.clear": LogClearParams;
    "log.read": LogReadParams;
    "log.write": LogWriteParams;
    "lyrics.exists": LyricsExistsParams;
    "lyrics.get": LyricsGetParams;
    "lyrics.save": LyricsSaveParams;
    "menu.close": MenuCloseParams;
    "menu.getContextMenu": MenuGetContextMenuParams;
    "menu.getMainMenu": MenuGetMainMenuParams;
    "menu.runContextCommand": MenuRunContextCommandParams;
    "menu.runContextCommandById": MenuRunContextCommandByIdParams;
    "menu.runMainMenuCommand": MenuRunMainMenuCommandParams;
    "menu.show": MenuShowParams;
    "menu.showNativePopup": MenuShowNativePopupParams;
    "metadata.embedArtwork": MetadataEmbedArtworkParams;
    "metadata.read": MetadataReadParams;
    "metadata.readBatch": MetadataReadBatchParams;
    "metadata.readByPath": MetadataReadByPathParams;
    "metadata.readRaw": MetadataReadRawParams;
    "metadata.removeEmbeddedArt": MetadataRemoveEmbeddedArtParams;
    "metadata.removeField": MetadataRemoveFieldParams;
    "metadata.removeTag": MetadataRemoveTagParams;
    "metadata.write": MetadataWriteParams;
    "metadata.writeBatch": MetadataWriteBatchParams;
    "misc.exit": MiscExitParams;
    "misc.getComponentPath": MiscGetComponentPathParams;
    "misc.getFoobarPath": MiscGetFoobarPathParams;
    "misc.getProfilePath": MiscGetProfilePathParams;
    "misc.restart": MiscRestartParams;
    "misc.showConsole": MiscShowConsoleParams;
    "misc.showLibrarySearch": MiscShowLibrarySearchParams;
    "misc.showPopupMessage": MiscShowPopupMessageParams;
    "misc.showPreferences": MiscShowPreferencesParams;
    "output.getDevices": OutputGetDevicesParams;
    "output.getEntries": OutputGetEntriesParams;
    "output.getSettings": OutputGetSettingsParams;
    "panel.getConfig": PanelGetConfigParams;
    "panel.setConfig": PanelSetConfigParams;
    "playback.getCurrentTrack": PlaybackGetCurrentTrackParams;
    "playback.getCurrentTrackIndex": PlaybackGetCurrentTrackIndexParams;
    "playback.getPlaybackOrder": PlaybackGetPlaybackOrderParams;
    "playback.getPlayingPlaylist": PlaybackGetPlayingPlaylistParams;
    "playback.getPosition": PlaybackGetPositionParams;
    "playback.getState": PlaybackGetStateParams;
    "playback.getStopAfterCurrent": PlaybackGetStopAfterCurrentParams;
    "playback.getVolume": PlaybackGetVolumeParams;
    "playback.mute": PlaybackMuteParams;
    "playback.next": PlaybackNextParams;
    "playback.pause": PlaybackPauseParams;
    "playback.play": PlaybackPlayParams;
    "playback.playOrPause": PlaybackPlayOrPauseParams;
    "playback.playPath": PlaybackPlayPathParams;
    "playback.playPaths": PlaybackPlayPathsParams;
    "playback.playPause": PlaybackPlayPauseParams;
    "playback.previous": PlaybackPreviousParams;
    "playback.random": PlaybackRandomParams;
    "playback.setPlaybackOrder": PlaybackSetPlaybackOrderParams;
    "playback.setPosition": PlaybackSetPositionParams;
    "playback.setStopAfterCurrent": PlaybackSetStopAfterCurrentParams;
    "playback.setVolume": PlaybackSetVolumeParams;
    "playback.stop": PlaybackStopParams;
    "playback.toggleMute": PlaybackToggleMuteParams;
    "playback.toggleStopAfterCurrent": PlaybackToggleStopAfterCurrentParams;
    "playback.volumeDown": PlaybackVolumeDownParams;
    "playback.volumeUp": PlaybackVolumeUpParams;
    "playcount.get": PlaycountGetParams;
    "playcount.getBatch": PlaycountGetBatchParams;
    "playcount.getStats": PlaycountGetStatsParams;
    "playcount.set": PlaycountSetParams;
    "playlist.addHandles": PlaylistAddHandlesParams;
    "playlist.addPaths": PlaylistAddPathsParams;
    "playlist.addPathsAsync": PlaylistAddPathsAsyncParams;
    "playlist.addPathsSequential": PlaylistAddPathsSequentialParams;
    "playlist.clear": PlaylistClearParams;
    "playlist.convertToAutoplaylist": PlaylistConvertToAutoplaylistParams;
    "playlist.create": PlaylistCreateParams;
    "playlist.createAutoplaylist": PlaylistCreateAutoplaylistParams;
    "playlist.deselectAll": PlaylistDeselectAllParams;
    "playlist.duplicate": PlaylistDuplicateParams;
    "playlist.focusTrack": PlaylistFocusTrackParams;
    "playlist.getActive": PlaylistGetActiveParams;
    "playlist.getAll": PlaylistGetAllParams;
    "playlist.getAutoplaylistInfo": PlaylistGetAutoplaylistInfoParams;
    "playlist.getAutoplaylistQuery": PlaylistGetAutoplaylistQueryParams;
    "playlist.getAvailableColumns": PlaylistGetAvailableColumnsParams;
    "playlist.getCount": PlaylistGetCountParams;
    "playlist.getFocusedTrack": PlaylistGetFocusedTrackParams;
    "playlist.getFocusTrack": PlaylistGetFocusTrackParams;
    "playlist.getLockInfo": PlaylistGetLockInfoParams;
    "playlist.getPlaying": PlaylistGetPlayingParams;
    "playlist.getSelectedTracks": PlaylistGetSelectedTracksParams;
    "playlist.getSelection": PlaylistGetSelectionParams;
    "playlist.getTrackCount": PlaylistGetTrackCountParams;
    "playlist.getTracks": PlaylistGetTracksParams;
    "playlist.insertTracks": PlaylistInsertTracksParams;
    "playlist.isAutoplaylist": PlaylistIsAutoplaylistParams;
    "playlist.isLocked": PlaylistIsLockedParams;
    "playlist.moveTracks": PlaylistMoveTracksParams;
    "playlist.playTrack": PlaylistPlayTrackParams;
    "playlist.redo": PlaylistRedoParams;
    "playlist.remove": PlaylistRemoveParams;
    "playlist.removeAutoplaylist": PlaylistRemoveAutoplaylistParams;
    "playlist.removeSelectedTracks": PlaylistRemoveSelectedTracksParams;
    "playlist.removeTracks": PlaylistRemoveTracksParams;
    "playlist.rename": PlaylistRenameParams;
    "playlist.reorder": PlaylistReorderParams;
    "playlist.reorderPlaylists": PlaylistReorderPlaylistsParams;
    "playlist.replaceAllAndPlay": PlaylistReplaceAllAndPlayParams;
    "playlist.reverse": PlaylistReverseParams;
    "playlist.selectAll": PlaylistSelectAllParams;
    "playlist.setActive": PlaylistSetActiveParams;
    "playlist.setFocusedTrack": PlaylistSetFocusedTrackParams;
    "playlist.setSelection": PlaylistSetSelectionParams;
    "playlist.shuffle": PlaylistShuffleParams;
    "playlist.sort": PlaylistSortParams;
    "playlist.undo": PlaylistUndoParams;
    "port.connect": PortConnectParams;
    "port.disconnect": PortDisconnectParams;
    "port.getPorts": PortGetPortsParams;
    "port.postMessage": PortPostMessageParams;
    "port.postMessageTo": PortPostMessageToParams;
    "queue.add": QueueAddParams;
    "queue.addPaths": QueueAddPathsParams;
    "queue.clear": QueueClearParams;
    "queue.flush": QueueFlushParams;
    "queue.get": QueueGetParams;
    "queue.getCount": QueueGetCountParams;
    "queue.moveToTop": QueueMoveToTopParams;
    "queue.remove": QueueRemoveParams;
    "rating.get": RatingGetParams;
    "rating.set": RatingSetParams;
    "replaygain.clear": ReplaygainClearParams;
    "replaygain.get": ReplaygainGetParams;
    "replaygain.getMode": ReplaygainGetModeParams;
    "replaygain.getPreamp": ReplaygainGetPreampParams;
    "replaygain.getSettings": ReplaygainGetSettingsParams;
    "replaygain.scan": ReplaygainScanParams;
    "replaygain.setMode": ReplaygainSetModeParams;
    "replaygain.setPreamp": ReplaygainSetPreampParams;
    "selection.get": SelectionGetParams;
    "selection.getType": SelectionGetTypeParams;
    "selection.getViewerMode": SelectionGetViewerModeParams;
    "selection.getViewingTrack": SelectionGetViewingTrackParams;
    "selection.set": SelectionSetParams;
    "selection.setPlaylistTracking": SelectionSetPlaylistTrackingParams;
    "shell.exec": ShellExecParams;
    "shell.openExternal": ShellOpenExternalParams;
    "shell.openWith": ShellOpenWithParams;
    "shell.showInExplorer": ShellShowInExplorerParams;
    "shell.spawn": ShellSpawnParams;
    "state.delete": StateDeleteParams;
    "state.get": StateGetParams;
    "state.keys": StateKeysParams;
    "state.set": StateSetParams;
    "system.getApisByNamespace": SystemGetApisByNamespaceParams;
    "system.getApiStats": SystemGetApiStatsParams;
    "system.getDPI": SystemGetDPIParams;
    "system.getLocale": SystemGetLocaleParams;
    "system.getRegisteredPlugins": SystemGetRegisteredPluginsParams;
    "system.getTheme": SystemGetThemeParams;
    "system.isPluginRegistered": SystemIsPluginRegisteredParams;
    "system.listAvailableApis": SystemListAvailableApisParams;
    "system.searchApis": SystemSearchApisParams;
    "taskbar.flash": TaskbarFlashParams;
    "taskbar.setOverlayIcon": TaskbarSetOverlayIconParams;
    "taskbar.setProgress": TaskbarSetProgressParams;
    "taskbar.setThumbnailButtons": TaskbarSetThumbnailButtonsParams;
    "taskbar.updateButton": TaskbarUpdateButtonParams;
    "test.echo": TestEchoParams;
    "test.ping": TestPingParams;
    "titleformat.eval": TitleformatEvalParams;
    "titleformat.evalBatch": TitleformatEvalBatchParams;
    "titleformat.evalFields": TitleformatEvalFieldsParams;
    "titleformat.evalFieldsBatch": TitleformatEvalFieldsBatchParams;
    "titleformat.getBuiltinFields": TitleformatGetBuiltinFieldsParams;
    "tray.appendMenuItems": TrayAppendMenuItemsParams;
    "tray.clearMenuItems": TrayClearMenuItemsParams;
    "tray.create": TrayCreateParams;
    "tray.destroy": TrayDestroyParams;
    "tray.getMenuItems": TrayGetMenuItemsParams;
    "tray.isVisible": TrayIsVisibleParams;
    "tray.removeMenuItems": TrayRemoveMenuItemsParams;
    "tray.setCloseToTray": TraySetCloseToTrayParams;
    "tray.setContextMenu": TraySetContextMenuParams;
    "tray.setIcon": TraySetIconParams;
    "tray.setMenuItemState": TraySetMenuItemStateParams;
    "tray.setMinimizeToTray": TraySetMinimizeToTrayParams;
    "tray.setTooltip": TraySetTooltipParams;
    "tray.showBalloon": TrayShowBalloonParams;
    "ui.hideNotification": UiHideNotificationParams;
    "ui.showContextMenu": UiShowContextMenuParams;
    "ui.showCustomMenu": UiShowCustomMenuParams;
    "ui.showNotification": UiShowNotificationParams;
    "ui.showToast": UiShowToastParams;
    "window.blur": WindowBlurParams;
    "window.broadcast": WindowBroadcastParams;
    "window.cancelClose": WindowCancelCloseParams;
    "window.center": WindowCenterParams;
    "window.clearClickThroughExcludeRegions": WindowClearClickThroughExcludeRegionsParams;
    "window.clearDragRegions": WindowClearDragRegionsParams;
    "window.clearNoDragRegions": WindowClearNoDragRegionsParams;
    "window.close": WindowCloseParams;
    "window.closeAllPopups": WindowCloseAllPopupsParams;
    "window.closePopup": WindowClosePopupParams;
    "window.confirmClose": WindowConfirmCloseParams;
    "window.createPopup": WindowCreatePopupParams;
    "window.enterFullscreen": WindowEnterFullscreenParams;
    "window.exitFullscreen": WindowExitFullscreenParams;
    "window.flash": WindowFlashParams;
    "window.flashTaskbar": WindowFlashTaskbarParams;
    "window.focus": WindowFocusParams;
    "window.getAllWindows": WindowGetAllWindowsParams;
    "window.getBackdropPolicy": WindowGetBackdropPolicyParams;
    "window.getBounds": WindowGetBoundsParams;
    "window.getCaptionButtonsWidth": WindowGetCaptionButtonsWidthParams;
    "window.getCornerPreference": WindowGetCornerPreferenceParams;
    "window.getCurrentWindowId": WindowGetCurrentWindowIdParams;
    "window.getDevServerConfig": WindowGetDevServerConfigParams;
    "window.getDpiScale": WindowGetDpiScaleParams;
    "window.getMaxSize": WindowGetMaxSizeParams;
    "window.getMinSize": WindowGetMinSizeParams;
    "window.getMode": WindowGetModeParams;
    "window.getPopupBehavior": WindowGetPopupBehaviorParams;
    "window.getState": WindowGetStateParams;
    "window.getTitle": WindowGetTitleParams;
    "window.getTitlebarHeight": WindowGetTitlebarHeightParams;
    "window.getTitlebarInfo": WindowGetTitlebarInfoParams;
    "window.getZoom": WindowGetZoomParams;
    "window.hasSavedBounds": WindowHasSavedBoundsParams;
    "window.isAlwaysOnTop": WindowIsAlwaysOnTopParams;
    "window.isClickThrough": WindowIsClickThroughParams;
    "window.isFullscreen": WindowIsFullscreenParams;
    "window.isMaximized": WindowIsMaximizedParams;
    "window.isMinimized": WindowIsMinimizedParams;
    "window.isResizable": WindowIsResizableParams;
    "window.maximize": WindowMaximizeParams;
    "window.minimize": WindowMinimizeParams;
    "window.refreshWebView": WindowRefreshWebViewParams;
    "window.reload": WindowReloadParams;
    "window.resetZoom": WindowResetZoomParams;
    "window.restore": WindowRestoreParams;
    "window.sendMessage": WindowSendMessageParams;
    "window.setAcrylic": WindowSetAcrylicParams;
    "window.setAlwaysOnTop": WindowSetAlwaysOnTopParams;
    "window.setBackdropPolicy": WindowSetBackdropPolicyParams;
    "window.setBackgroundTransparency": WindowSetBackgroundTransparencyParams;
    "window.setBlur": WindowSetBlurParams;
    "window.setBounds": WindowSetBoundsParams;
    "window.setClickThrough": WindowSetClickThroughParams;
    "window.setClickThroughExcludeRegions": WindowSetClickThroughExcludeRegionsParams;
    "window.setCornerPreference": WindowSetCornerPreferenceParams;
    "window.setDarkMode": WindowSetDarkModeParams;
    "window.setDevServerConfig": WindowSetDevServerConfigParams;
    "window.setDragRegions": WindowSetDragRegionsParams;
    "window.setFrameless": WindowSetFramelessParams;
    "window.setFullscreen": WindowSetFullscreenParams;
    "window.setMaxSize": WindowSetMaxSizeParams;
    "window.setMica": WindowSetMicaParams;
    "window.setMicaEffect": WindowSetMicaEffectParams;
    "window.setMinSize": WindowSetMinSizeParams;
    "window.setNoDragRegions": WindowSetNoDragRegionsParams;
    "window.setPopupBehavior": WindowSetPopupBehaviorParams;
    "window.setPosition": WindowSetPositionParams;
    "window.setResizable": WindowSetResizableParams;
    "window.setSize": WindowSetSizeParams;
    "window.setTitle": WindowSetTitleParams;
    "window.setTitlebarHeight": WindowSetTitlebarHeightParams;
    "window.setZoom": WindowSetZoomParams;
    "window.setZoomForDpi": WindowSetZoomForDpiParams;
    "window.showSystemMenu": WindowShowSystemMenuParams;
    "window.startDrag": WindowStartDragParams;
    "window.startResize": WindowStartResizeParams;
    "window.toggleAlwaysOnTop": WindowToggleAlwaysOnTopParams;
    "window.toggleFullscreen": WindowToggleFullscreenParams;
    "window.toggleMaximize": WindowToggleMaximizeParams;
}

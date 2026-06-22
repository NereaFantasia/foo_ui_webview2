// ─────────────────────────────────────────────────────────────
// GENERATED FILE — DO NOT EDIT
// File: sdk/src/types/generated/responses.ts
// Source: docs/graph/auto/code-index.json
// Emitter: scripts/gen_sdk_types.mjs --all
// schema_version: auto-1.0.0
// Regenerate: npm run gen:types (in sdk/) or `node scripts/gen_sdk_types.mjs --all`
// ─────────────────────────────────────────────────────────────

/* eslint-disable */

import type { JsonObject } from '../json.js';

// ── Hand-written overrides — see sdk/src/types/overrides/ ────────────────
import type { ConfigGetReplaygainModeResponse, ConfigSetReplaygainModeResponse } from "../overrides/config.js";
import type { CursorIsHiddenResponse, CursorSetHiddenResponse } from "../overrides/cursor.js";
export type { ConfigGetReplaygainModeResponse, ConfigSetReplaygainModeResponse, CursorIsHiddenResponse, CursorSetHiddenResponse };

/**
 * Response from `artwork.getAvailableArtwork`.
 */
export interface ArtworkGetAvailableArtworkResponse {
    success: boolean;
    error?: string;
    available?: boolean;
    artworks?: JsonObject;
    sources?: JsonObject;
}

/**
 * Response from `artwork.getAvailableTypes`.
 */
export interface ArtworkGetAvailableTypesResponse {
    types: JsonObject;
    error?: string;
    success?: boolean;
}

/**
 * Response from `artwork.getBatch`.
 */
export interface ArtworkGetBatchResponse {
    success: boolean;
    error?: string;
    artworks?: JsonObject;
    available: boolean;
    code: string;
}

/**
 * Response from `artwork.getByPath`.
 */
export interface ArtworkGetByPathResponse {
    available: boolean;
    error?: string;
    type?: string;
    path?: string;
    mimeType?: unknown;
    /** int64 — may lose precision above 2^53 */
    size?: number;
    dataUrl?: string;
    code: string;
}

/**
 * Response from `artwork.getByPlaylistItem`.
 */
export interface ArtworkGetByPlaylistItemResponse {
    available: boolean;
    error?: string;
    type?: string;
    /** int64 — may lose precision above 2^53 */
    playlist?: number;
    /** int64 — may lose precision above 2^53 */
    index?: number;
    mimeType?: unknown;
    /** int64 — may lose precision above 2^53 */
    size?: number;
    dataUrl?: string;
    code: string;
}

/**
 * Response from `artwork.getCurrent`.
 */
export interface ArtworkGetCurrentResponse {
    available: boolean;
    type: string;
    reason?: string;
    source?: string;
    mimeType?: unknown;
    /** int64 — may lose precision above 2^53 */
    size?: number;
    dataUrl?: string;
    path?: unknown;
    error?: string;
    code: string;
}

/**
 * Response from `artwork.getFb2kUrl`.
 */
export interface ArtworkGetFb2kUrlResponse {
    available: boolean;
    type: string;
    reason?: string;
    dataUrl?: string;
    error?: string;
    code: string;
}

/**
 * Response from `artwork.getFb2kUrlByPath`.
 */
export interface ArtworkGetFb2kUrlByPathResponse {
    available: boolean;
    type: string;
    error?: string;
    path?: string;
    dataUrl?: string;
    code: string;
}

/**
 * Response from `artwork.getFb2kUrlByPathBatch`.
 */
export interface ArtworkGetFb2kUrlByPathBatchResponse {
    success: boolean;
    error?: string;
    artworks?: JsonObject;
    available: boolean;
    code: string;
}

/**
 * Response from `artwork.getFolderImages`.
 */
export interface ArtworkGetFolderImagesResponse {
    success: boolean;
    error?: string;
    images?: JsonObject;
}

/**
 * Response from `artwork.getForTrack`.
 */
export interface ArtworkGetForTrackResponse {
    available: boolean;
    error?: string;
    type?: string;
    path?: string;
    mimeType?: unknown;
    width?: number;
    height?: number;
    /** int64 — may lose precision above 2^53 */
    size?: number;
    dataUrl?: string;
    code: string;
}

/**
 * Response from `artwork.getLyrics`.
 */
export interface ArtworkGetLyricsResponse {
    available: boolean;
    error?: string;
    tag?: unknown;
    lyrics?: unknown;
    synced?: boolean;
}

/**
 * Response from `artwork.getMetadata`.
 */
export interface ArtworkGetMetadataResponse {
    available: boolean;
    error?: string;
    album?: unknown;
    artist?: unknown;
    albumArtist?: unknown;
    title?: unknown;
    year?: unknown;
    genre?: unknown;
    trackNumber?: unknown;
    discNumber?: unknown;
    hasEmbedded?: unknown;
    hasLyrics?: unknown;
}

/**
 * Response from `audio.analyzeBPM`.
 */
export interface AudioAnalyzeBPMResponse {
    success: boolean;
    error?: string;
    bpm?: number;
    confidence?: number;
    source?: string;
}

/**
 * Response from `audio.generateFullWaveform`.
 */
export interface AudioGenerateFullWaveformResponse {
    success: boolean;
    status: string;
    cached: boolean;
    waveform?: number[];
    duration?: number;
    sampleRate?: number;
    channels?: number;
    resolution: number;
    method: string;
    scale: string;
    path: string;
    taskId?: string;
    signed?: boolean;
    error: unknown;
    code: unknown;
}

/**
 * Response from `audio.generateWaveform`.
 */
export interface AudioGenerateWaveformResponse {
    success: boolean;
    error: string;
    duration?: number;
    sampleRate?: number;
    channels?: number;
    requestedResolution?: number;
}

/**
 * Response from `audio.getOutputInfo`.
 */
export interface AudioGetOutputInfoResponse {
    success: boolean;
    volume?: number;
    volumePercent?: number;
    error?: string;
}

/**
 * Response from `audio.getSpectrum`.
 */
export interface AudioGetSpectrumResponse {
    success: boolean;
    error?: string;
    spectrum?: number[];
    fftSize?: number;
    bands?: number;
}

/**
 * Response from `audio.getSpectrumDebugState`.
 */
export interface AudioGetSpectrumDebugStateResponse {
    success: boolean;
    active: boolean;
    timerRunning: boolean;
    /** int64 — may lose precision above 2^53 */
    timerHwnd: number;
    effectiveFftSize: number;
    effectiveFps: number;
    effectiveBands: number;
    skipFrames: number;
    streamReady: boolean;
    subscriptionCount: number;
    dispatchTargetCount: number;
    subscriptions: JsonObject;
    dispatchTargets: JsonObject;
    /** int64 — may lose precision above 2^53 */
    instanceCount: number;
    /** int64 — may lose precision above 2^53 */
    callerHwnd: number;
    callerWindowId: string;
    callerOwnsSubscription: boolean;
    /** int64 — may lose precision above 2^53 */
    foregroundHwnd: number;
    foregroundPid: unknown;
    foregroundIsExternal: boolean;
    foregroundTitle: string;
}

/**
 * Response from `audio.getStreamInfo`.
 */
export interface AudioGetStreamInfoResponse {
    success: boolean;
    playing?: boolean;
    /** int64 — may lose precision above 2^53 */
    sampleRate?: number;
    /** int64 — may lose precision above 2^53 */
    channels?: number;
    /** int64 — may lose precision above 2^53 */
    bitrate?: number;
    codec?: unknown;
    duration?: number;
    error?: string;
}

/**
 * Response from `audio.getWaveform`.
 */
export interface AudioGetWaveformResponse {
    success: boolean;
    error?: string;
    waveform?: number[];
    duration?: number;
    signed?: boolean;
}

/**
 * Response from `audio.isVisualizationAvailable`.
 */
export interface AudioIsVisualizationAvailableResponse {
    success: boolean;
    available: boolean;
}

/**
 * Response from `audio.setChannelMode`.
 */
export interface AudioSetChannelModeResponse {
    success: boolean;
    mode: string;
}

/**
 * Response from `audio.subscribeSpectrum`.
 */
export interface AudioSubscribeSpectrumResponse {
    success: boolean;
    error?: string;
    subscriptionId?: string;
    fftSize?: number;
    bands?: number;
    fps?: number;
    event?: string;
}

/**
 * Response from `audio.subscribeStream`.
 */
export interface AudioSubscribeStreamResponse {
    success: boolean;
    error: string;
    event: string;
    interval: number;
}

/**
 * Response from `audio.unsubscribeSpectrum`.
 */
export interface AudioUnsubscribeSpectrumResponse {
    success: boolean;
    /** int64 — may lose precision above 2^53 */
    removed: number;
    subscriptionId: string;
}

/**
 * Response from `audio.unsubscribeStream`.
 */
export interface AudioUnsubscribeStreamResponse {
    success: boolean;
}

/**
 * Response from `clipboard.read`.
 */
export interface ClipboardReadResponse {
    success: boolean;
    error?: string;
    hasText?: unknown;
    hasImage?: unknown;
    hasFiles?: unknown;
    text?: unknown;
    files?: unknown;
}

/**
 * Response from `clipboard.write`.
 */
export interface ClipboardWriteResponse {
    success: boolean;
    error?: string;
}

/**
 * Response from `clipboard.writeFiles`.
 */
export interface ClipboardWriteFilesResponse {
    success: boolean;
    error?: string;
    fileCount?: number;
}

/**
 * Response from `clipboard.writeHTML`.
 */
export interface ClipboardWriteHTMLResponse {
    success: boolean;
    error?: string;
    htmlWritten?: boolean;
    textWritten?: boolean;
}

/**
 * Response from `config.export`.
 */
export interface ConfigExportResponse {
    success: boolean;
    data: JsonObject;
    json: string;
    count: number;
}

/**
 * Response from `config.get`.
 */
export interface ConfigGetResponse {
    success: boolean;
    error?: string;
    key?: string;
    value?: unknown;
    found?: boolean;
}

/**
 * Response from `config.getActiveDspPreset`.
 */
export interface ConfigGetActiveDspPresetResponse {
    index: unknown;
    name: unknown;
    isActive: unknown;
}

/**
 * Response from `config.getAdvancedConfig` (shape unspecified).
 */
export type ConfigGetAdvancedConfigResponse = JsonObject;

/**
 * Response from `config.getAdvancedConfigValue`.
 */
export interface ConfigGetAdvancedConfigValueResponse {
    name: unknown;
    guid: unknown;
    type: unknown;
    value: unknown;
}

/**
 * Response from `config.getAll`.
 */
export interface ConfigGetAllResponse {
    success: boolean;
    items: JsonObject;
    configs: JsonObject;
    count: number;
}

/**
 * Response from `config.getComponents` (shape unspecified).
 */
export type ConfigGetComponentsResponse = JsonObject;

/**
 * Response from `config.getCursorFollowPlayback`.
 */
export interface ConfigGetCursorFollowPlaybackResponse {
    enabled: boolean;
    value: boolean;
}

/**
 * Response from `config.getDspPresets` (shape unspecified).
 */
export type ConfigGetDspPresetsResponse = JsonObject;

/**
 * Response from `config.getLibraryFilePatterns` (shape unspecified).
 */
export type ConfigGetLibraryFilePatternsResponse = JsonObject;

/**
 * Response from `config.getLibraryStatus`.
 */
export interface ConfigGetLibraryStatusResponse {
    enabled: unknown;
    itemCount: unknown;
    initialized: unknown;
}

/**
 * Response from `config.getOutputConfig`.
 */
export interface ConfigGetOutputConfigResponse {
    outputId: unknown;
    deviceId: unknown;
    bufferLength: unknown;
    bitDepth: unknown;
    useDither: unknown;
    useFades: unknown;
    outputName: unknown;
    deviceName: unknown;
}

/**
 * Response from `config.getOutputDevices` (shape unspecified).
 */
export type ConfigGetOutputDevicesResponse = JsonObject;

/**
 * Response from `config.getPlaybackFollowCursor`.
 */
export interface ConfigGetPlaybackFollowCursorResponse {
    enabled: boolean;
    value: boolean;
}

/**
 * Response from `config.getPreferencesPages` (shape unspecified).
 */
export type ConfigGetPreferencesPagesResponse = JsonObject;

/**
 * Response from `config.getPreferencesStandardGuids`.
 */
export interface ConfigGetPreferencesStandardGuidsResponse {
    root: unknown;
    hidden: unknown;
    tools: unknown;
    core: unknown;
    display: unknown;
    playback: unknown;
    visualisations: unknown;
    input: unknown;
    tagWriting: unknown;
    mediaLibrary: unknown;
    tagging: unknown;
    output: unknown;
    advanced: unknown;
    components: unknown;
    dsp: unknown;
    shell: unknown;
    keyboardShortcuts: unknown;
}

/**
 * Response from `config.getVersionInfo`.
 */
export interface ConfigGetVersionInfoResponse {
    version: unknown;
    foobar2000: unknown;
    versionFull: unknown;
    is64bit: unknown;
    isPortable: unknown;
    plugin: unknown;
    profilePath: unknown;
}

/**
 * Response from `config.remove`.
 */
export interface ConfigRemoveResponse {
    success: boolean;
    error?: string;
    key?: string;
    existed?: boolean;
}

/**
 * Response from `config.resetAdvancedConfig`.
 */
export interface ConfigResetAdvancedConfigResponse {
    success: boolean;
}

/**
 * Response from `config.set`.
 */
export interface ConfigSetResponse {
    success: boolean;
    error?: string;
    key?: string;
}

/**
 * Response from `config.setActiveDspPreset`.
 */
export interface ConfigSetActiveDspPresetResponse {
    success: boolean;
}

/**
 * Response from `config.setAdvancedConfigValue`.
 */
export interface ConfigSetAdvancedConfigValueResponse {
    success: boolean;
}

/**
 * Response from `config.setCursorFollowPlayback`.
 */
export interface ConfigSetCursorFollowPlaybackResponse {
    success: boolean;
    enabled: boolean;
}

/**
 * Response from `config.setOutputBuffer`.
 */
export interface ConfigSetOutputBufferResponse {
    success: boolean;
}

/**
 * Response from `config.setOutputDevice`.
 */
export interface ConfigSetOutputDeviceResponse {
    success: boolean;
}

/**
 * Response from `config.setPlaybackFollowCursor`.
 */
export interface ConfigSetPlaybackFollowCursorResponse {
    success: boolean;
    enabled: boolean;
}

/**
 * Response from `config.showLibraryPreferences`.
 */
export interface ConfigShowLibraryPreferencesResponse {
    success: boolean;
}

/**
 * Response from `console.error`.
 */
export interface ConsoleErrorResponse {
    success: boolean;
    error?: string;
}

/**
 * Response from `console.log`.
 */
export interface ConsoleLogResponse {
    success: boolean;
    error?: string;
}

/**
 * Response from `console.warn`.
 */
export interface ConsoleWarnResponse {
    success: boolean;
    error?: string;
}

/**
 * Response from `dialog.confirm`.
 */
export interface DialogConfirmResponse {
    response: unknown;
}

/**
 * Response from `dialog.openFile`.
 */
export interface DialogOpenFileResponse {
    canceled: boolean;
    filePaths: unknown[];
    error: string;
}

/**
 * Response from `dialog.openFolder`.
 */
export interface DialogOpenFolderResponse {
    canceled: boolean;
    folderPath: string;
    error: string;
}

/**
 * Response from `dialog.saveFile`.
 */
export interface DialogSaveFileResponse {
    canceled: boolean;
    filePath: string;
    error: string;
}

/**
 * Response from `discovery.executeContextMenuByPath`.
 */
export interface DiscoveryExecuteContextMenuByPathResponse {
    success: boolean;
    error?: string;
    path?: string;
    foundName?: string;
    itemCount?: number;
}

/**
 * Response from `discovery.executeContextMenuCommand`.
 */
export interface DiscoveryExecuteContextMenuCommandResponse {
    success: boolean;
    error?: string;
    guid?: string;
    itemCount?: number;
}

/**
 * Response from `discovery.executeMainMenuCommand`.
 */
export interface DiscoveryExecuteMainMenuCommandResponse {
    success: boolean;
    error?: string;
    guid?: string;
}

/**
 * Response from `discovery.getAllServices`.
 */
export interface DiscoveryGetAllServicesResponse {
    success: boolean;
    services: JsonObject;
    totalServices: number;
}

/**
 * Response from `discovery.getComponents`.
 */
export interface DiscoveryGetComponentsResponse {
    success: boolean;
    components: JsonObject;
    count: number;
}

/**
 * Response from `discovery.getContextMenuCommands`.
 */
export interface DiscoveryGetContextMenuCommandsResponse {
    success: boolean;
    commands: JsonObject;
    count: number;
}

/**
 * Response from `discovery.getContextMenuTree`.
 */
export interface DiscoveryGetContextMenuTreeResponse {
    success: boolean;
    error?: string;
    tree?: JsonObject;
    itemCount?: number;
    name: unknown;
    type: unknown;
    fullName: unknown;
    childCount: unknown;
    children: unknown;
}

/**
 * Response from `discovery.getDspEntries`.
 */
export interface DiscoveryGetDspEntriesResponse {
    success: boolean;
    entries: JsonObject;
    count: number;
}

/**
 * Response from `discovery.getInputFormats`.
 */
export interface DiscoveryGetInputFormatsResponse {
    success: boolean;
    fileTypes: JsonObject;
    count: number;
}

/**
 * Response from `discovery.getMainMenuCommands`.
 */
export interface DiscoveryGetMainMenuCommandsResponse {
    success: boolean;
    commands: JsonObject;
    count: number;
}

/**
 * Response from `discovery.getMainMenuGroups`.
 */
export interface DiscoveryGetMainMenuGroupsResponse {
    success: boolean;
    groups: JsonObject;
    count: number;
}

/**
 * Response from `discovery.getOutputDevices`.
 */
export interface DiscoveryGetOutputDevicesResponse {
    success: boolean;
    devices: JsonObject;
    count: number;
}

/**
 * Response from `discovery.getPreferencePages`.
 */
export interface DiscoveryGetPreferencePagesResponse {
    success: boolean;
    pages: JsonObject;
    count: number;
}

/**
 * Response from `discovery.getUIElements`.
 */
export interface DiscoveryGetUIElementsResponse {
    success: boolean;
    elements: JsonObject;
    count: number;
}

/**
 * Response from `discovery.searchCommands`.
 */
export interface DiscoverySearchCommandsResponse {
    success: boolean;
    error?: string;
    query?: string;
    results?: JsonObject;
    count?: number;
}

/**
 * Response from `dnd.getDropZones`.
 */
export interface DndGetDropZonesResponse {
    success: boolean;
    zones: JsonObject;
    count: number;
}

/**
 * Response from `dnd.registerDropZone`.
 */
export interface DndRegisterDropZoneResponse {
    success: boolean;
    error?: string;
    zoneId?: string;
    selector?: string;
    accept?: string[];
    event?: string;
    script?: string;
}

/**
 * Response from `dnd.startDrag`.
 */
export interface DndStartDragResponse {
    success: boolean;
    error?: string;
    type?: string;
    note?: string;
    trackCount?: number;
}

/**
 * Response from `dnd.unregisterDropZone`.
 */
export interface DndUnregisterDropZoneResponse {
    success: boolean;
    error?: string;
    zoneId?: string;
    script?: string;
}

/**
 * Response from `dsp.addDsp`.
 */
export interface DspAddDspResponse {
    success: boolean;
    error?: string;
    addedDsp?: unknown;
    position?: number;
}

/**
 * Response from `dsp.applyPreset`.
 */
export interface DspApplyPresetResponse {
    success: boolean;
    error?: string;
    appliedPreset?: unknown;
    /** int64 — may lose precision above 2^53 */
    appliedIndex?: number;
}

/**
 * Response from `dsp.getAvailable`.
 */
export interface DspGetAvailableResponse {
    dsps?: JsonObject;
    count?: number;
    success?: boolean;
    error?: string;
}

/**
 * Response from `dsp.getChain`.
 */
export interface DspGetChainResponse {
    activePreset?: unknown;
    activePresetIndex?: unknown;
    dsps?: JsonObject;
    success?: boolean;
    error?: string;
}

/**
 * Response from `dsp.getPresets`.
 */
export interface DspGetPresetsResponse {
    presets?: JsonObject;
    /** int64 — may lose precision above 2^53 */
    count?: number;
    /** int64 — may lose precision above 2^53 */
    selectedIndex?: number;
    success?: boolean;
    error?: string;
}

/**
 * Response from `dsp.moveDsp`.
 */
export interface DspMoveDspResponse {
    success: boolean;
    error?: string;
    message?: string;
    movedDsp?: unknown;
    /** int64 — may lose precision above 2^53 */
    from?: number;
    /** int64 — may lose precision above 2^53 */
    to?: number;
}

/**
 * Response from `dsp.removeDsp`.
 */
export interface DspRemoveDspResponse {
    success: boolean;
    error?: string;
    removedDsp?: unknown;
    /** int64 — may lose precision above 2^53 */
    removedIndex?: number;
}

/**
 * Response from `dsp.setChain`.
 */
export interface DspSetChainResponse {
    success: boolean;
    error?: string;
    count?: number;
}

/**
 * Response from `event.emit`.
 */
export interface EventEmitResponse {
    success: boolean;
    error: string;
    code: string;
}

/**
 * Response from `event.emitTo`.
 */
export interface EventEmitToResponse {
    success: boolean;
    error: string;
    code: string;
}

/**
 * Response from `file.copy`.
 */
export interface FileCopyResponse {
    success: boolean;
    error?: string;
    source?: string;
    destination?: string;
}

/**
 * Response from `file.delete`.
 */
export interface FileDeleteResponse {
    success: boolean;
    error?: string;
}

/**
 * Response from `file.exists`.
 */
export interface FileExistsResponse {
    success?: boolean;
    error?: string;
    exists?: boolean;
    isFile?: boolean;
    isDirectory?: boolean;
}

/**
 * Response from `file.getInfo`.
 */
export interface FileGetInfoResponse {
    success: boolean;
    error?: string;
    exists?: boolean;
    isDirectory?: boolean;
    isFile?: boolean;
    size?: unknown;
    /** int64 — may lose precision above 2^53 */
    modified?: number;
    name?: string;
    extension?: string;
    parent?: string;
}

/**
 * Response from `file.list`.
 */
export interface FileListResponse {
    success: boolean;
    error?: string;
    files?: JsonObject;
    directories?: JsonObject;
    items?: JsonObject;
}

/**
 * Response from `file.mkdir`.
 */
export interface FileMkdirResponse {
    success: boolean;
    error?: string;
    created?: boolean;
    message?: string;
}

/**
 * Response from `file.move`.
 */
export interface FileMoveResponse {
    success: boolean;
    error?: string;
    source?: string;
    destination?: string;
}

/**
 * Response from `file.read`.
 */
export interface FileReadResponse {
    success: boolean;
    error?: string;
    content?: string;
    size?: unknown;
    encoding?: string;
}

/**
 * Response from `file.rename`.
 */
export interface FileRenameResponse {
    success: boolean;
    error?: string;
    oldPath?: string;
    newPath?: string;
}

/**
 * Response from `file.write`.
 */
export interface FileWriteResponse {
    success: boolean;
    error?: string;
    bytesWritten?: unknown;
}

/**
 * Response from `http.abort`.
 */
export interface HttpAbortResponse {
    success: boolean;
    error?: string;
    requestId?: string;
    cancelled?: boolean;
}

/**
 * Response from `http.delete`.
 */
export interface HttpDeleteResponse {
    success: boolean;
    error?: string;
    requestId?: string;
    async?: boolean;
    status?: unknown;
    cancelled?: boolean;
    code?: unknown;
    headers?: JsonObject;
    body?: JsonObject;
    responseType?: string;
}

/**
 * Response from `http.download`.
 */
export interface HttpDownloadResponse {
    success: boolean;
    error?: string;
    code?: unknown;
    requestId?: string;
    async?: boolean;
    message?: string;
    cancelled?: boolean;
    status?: unknown;
    /** int64 — may lose precision above 2^53 */
    bytesWritten?: number;
    path?: string;
}

/**
 * Response from `http.get`.
 */
export interface HttpGetResponse {
    success: boolean;
    error?: string;
    requestId?: string;
    async?: boolean;
    status?: unknown;
    cancelled?: boolean;
    code?: unknown;
    headers?: JsonObject;
    body?: JsonObject;
    responseType?: string;
}

/**
 * Response from `http.head`.
 */
export interface HttpHeadResponse {
    success?: boolean;
    error?: string;
    requestId?: string;
    async?: boolean;
    contentLength?: unknown;
    status?: unknown;
    cancelled?: boolean;
    code?: unknown;
    headers?: JsonObject;
    body?: JsonObject;
    responseType?: string;
}

/**
 * Response from `http.patch`.
 */
export interface HttpPatchResponse {
    success: boolean;
    error?: string;
    requestId?: string;
    async?: boolean;
    status?: unknown;
    cancelled?: boolean;
    code?: unknown;
    headers?: JsonObject;
    body?: JsonObject;
    responseType?: string;
}

/**
 * Response from `http.post`.
 */
export interface HttpPostResponse {
    success: boolean;
    error?: string;
    requestId?: string;
    async?: boolean;
    status?: unknown;
    cancelled?: boolean;
    code?: unknown;
    headers?: JsonObject;
    body?: JsonObject;
    responseType?: string;
}

/**
 * Response from `http.put`.
 */
export interface HttpPutResponse {
    success: boolean;
    error?: string;
    requestId?: string;
    async?: boolean;
    status?: unknown;
    cancelled?: boolean;
    code?: unknown;
    headers?: JsonObject;
    body?: JsonObject;
    responseType?: string;
}

/**
 * Response from `jitQueue.clear`.
 */
export interface JitQueueClearResponse {
    success: boolean;
}

/**
 * Response from `jitQueue.enqueueNext`.
 */
export interface JitQueueEnqueueNextResponse {
    success: boolean;
    error?: string;
    trackId?: string;
    /** int64 — may lose precision above 2^53 */
    bufferSize?: number;
}

/**
 * Response from `jitQueue.getState`.
 */
export interface JitQueueGetStateResponse {
    isActive: boolean;
    state: string;
    currentTrackId: string;
    nextTrackId: string;
    /** int64 — may lose precision above 2^53 */
    bufferSize: number;
    /** int64 — may lose precision above 2^53 */
    shadowPlaylist: number;
}

/**
 * Response from `jitQueue.notifyEmpty`.
 */
export interface JitQueueNotifyEmptyResponse {
    success: boolean;
}

/**
 * Response from `jitQueue.playNow`.
 */
export interface JitQueuePlayNowResponse {
    success: boolean;
    error?: string;
    trackId?: string;
    /** int64 — may lose precision above 2^53 */
    shadowPlaylist?: number;
}

/**
 * Response from `jitQueue.preloadBatch`.
 */
export interface JitQueuePreloadBatchResponse {
    success: boolean;
    error: string;
    invalidCount?: unknown;
    /** int64 — may lose precision above 2^53 */
    tracksAdded?: number;
}

/**
 * Response from `jitQueue.skip`.
 */
export interface JitQueueSkipResponse {
    success: boolean;
    currentTrackId: string;
}

/**
 * Response from `jitQueue.stop`.
 */
export interface JitQueueStopResponse {
    success: boolean;
}

/**
 * Response from `keyboard.getRegisteredHotkeys`.
 */
export interface KeyboardGetRegisteredHotkeysResponse {
    success: boolean;
    hotkeys: JsonObject;
}

/**
 * Response from `keyboard.registerHotkey`.
 */
export interface KeyboardRegisterHotkeyResponse {
    success: boolean;
    error?: string;
    id?: number;
}

/**
 * Response from `keyboard.registerShortcut`.
 */
export interface KeyboardRegisterShortcutResponse {
    success: boolean;
    error?: string;
}

/**
 * Response from `keyboard.unregisterHotkey`.
 */
export interface KeyboardUnregisterHotkeyResponse {
    success: boolean;
    error?: string;
}

/**
 * Response from `library.addToPlaylist`.
 */
export interface LibraryAddToPlaylistResponse {
    success: boolean;
    error?: string;
    added?: number;
}

/**
 * Response from `library.browseDirectory`.
 */
export interface LibraryBrowseDirectoryResponse {
    success: boolean;
    error?: string;
    directories?: JsonObject;
    files?: JsonObject;
    items?: JsonObject;
}

/**
 * Response from `library.browseTree`.
 */
export interface LibraryBrowseTreeResponse {
    files: unknown;
    success: boolean;
    error: unknown;
    code: unknown;
}

/**
 * Response from `library.getAlbums`.
 */
export interface LibraryGetAlbumsResponse {
    success?: boolean;
    albums: unknown[];
    total?: number;
    hasMore?: unknown;
    fromCache?: unknown;
}

/**
 * Response from `library.getAlbumTracks`.
 */
export interface LibraryGetAlbumTracksResponse {
    success?: boolean;
    items: unknown[];
    tracks: unknown[];
    total: number;
    album: string;
    artist?: string;
}

/**
 * Response from `library.getAll`.
 */
export interface LibraryGetAllResponse {
    tracks?: JsonObject;
    items?: JsonObject;
    /** int64 — may lose precision above 2^53 */
    total?: number;
    offset?: number;
    limit?: number;
    fromCache?: boolean;
    pending?: boolean;
    requestId?: string;
}

/**
 * Response from `library.getArtistAlbums`.
 */
export interface LibraryGetArtistAlbumsResponse {
    success: boolean;
    error?: string;
    albums?: JsonObject;
}

/**
 * Response from `library.getArtists`.
 */
export interface LibraryGetArtistsResponse {
    success: boolean;
    error?: string;
    items: unknown[];
    count: number;
}

/**
 * Response from `library.getArtistTracks`.
 */
export interface LibraryGetArtistTracksResponse {
    success?: boolean;
    tracks: unknown[];
    count: number;
    artist: string;
    items?: JsonObject;
    /** int64 — may lose precision above 2^53 */
    total?: number;
}

/**
 * Response from `library.getByPath`.
 */
export interface LibraryGetByPathResponse {
    success: boolean;
    error?: string;
    found?: boolean;
    path?: string;
    absolutePath?: unknown;
    title?: string;
    artist?: string;
    album?: string;
    duration?: number;
    trackNumber?: string;
    genre?: string;
    date?: string;
}

/**
 * Response from `library.getCacheStats` (shape unspecified).
 */
export type LibraryGetCacheStatsResponse = JsonObject;

/**
 * Response from `library.getCount`.
 */
export interface LibraryGetCountResponse {
    success: boolean;
    /** int64 — may lose precision above 2^53 */
    count: number;
}

/**
 * Response from `library.getFieldValues`.
 */
export interface LibraryGetFieldValuesResponse {
    success: boolean;
    error?: string;
    values?: unknown[];
    total?: number;
    field?: string;
}

/**
 * Response from `library.getGenres`.
 */
export interface LibraryGetGenresResponse {
    success: boolean;
    error?: string;
    genres?: JsonObject;
}

/**
 * Response from `library.getRandomTracks`.
 */
export interface LibraryGetRandomTracksResponse {
    success: boolean;
    tracks: unknown[];
    count: number;
}

/**
 * Response from `library.getRecentlyAdded`.
 */
export interface LibraryGetRecentlyAddedResponse {
    success: boolean;
    tracks: unknown[];
    total: number;
    limit: number;
    sortBy: string;
    fallback: boolean;
    /** int64 — may lose precision above 2^53 */
    index: number;
    title: string;
    artist: string;
    album: string;
    duration: number;
    path: unknown;
    absolutePath: string;
    rating: number;
    albumArtist?: string;
    genre?: string;
    date?: string;
    trackNumber?: number;
    discNumber?: number;
    /** int64 — may lose precision above 2^53 */
    fileSize?: number;
    /** int64 — may lose precision above 2^53 */
    bitrate?: number;
    /** int64 — may lose precision above 2^53 */
    sampleRate?: number;
    /** int64 — may lose precision above 2^53 */
    channels?: number;
    codec?: string;
    subsong?: number;
}

/**
 * Response from `library.getRoots`.
 */
export interface LibraryGetRootsResponse {
    fromCache: unknown;
}

/**
 * Response from `library.getStats`.
 */
export interface LibraryGetStatsResponse {
    totalTracks: number;
    totalAlbums: number;
    totalArtists: number;
    totalDuration: number;
    /** int64 — may lose precision above 2^53 */
    totalSize: number;
    cacheValid?: boolean;
    /** int64 — may lose precision above 2^53 */
    lastModified?: number;
}

/**
 * Response from `library.getStatus`.
 */
export interface LibraryGetStatusResponse {
    enabled: boolean;
    initialized: boolean;
    scanning: boolean;
    /** int64 — may lose precision above 2^53 */
    itemCount: number;
    /** int64 — may lose precision above 2^53 */
    count: number;
}

/**
 * Response from `library.invalidateCache`.
 */
export interface LibraryInvalidateCacheResponse {
    success: boolean;
    /** int64 — may lose precision above 2^53 */
    timestamp: number;
}

/**
 * Response from `library.isEnabled`.
 */
export interface LibraryIsEnabledResponse {
    success: boolean;
    enabled: boolean;
}

/**
 * Response from `library.query`.
 */
export interface LibraryQueryResponse {
    success: boolean;
    error?: string;
    tracks?: JsonObject;
    total?: number;
}

/**
 * Response from `library.refresh`.
 */
export interface LibraryRefreshResponse {
    success: boolean;
}

/**
 * Response from `library.rescan`.
 */
export interface LibraryRescanResponse {
    success: boolean;
}

/**
 * Response from `library.search`.
 */
export interface LibrarySearchResponse {
    success: boolean;
    tracks: unknown[];
    total: number;
    /** int64 — may lose precision above 2^53 */
    offset?: number;
    /** int64 — may lose precision above 2^53 */
    limit?: number;
    items?: JsonObject;
    hasMore?: boolean;
    error?: string;
}

/**
 * Response from `log.clear`.
 */
export interface LogClearResponse {
    success: boolean;
    error?: string;
}

/**
 * Response from `log.read`.
 */
export interface LogReadResponse {
    success: boolean;
    error?: string;
    content?: string;
    lineCount?: number;
    lines?: JsonObject;
    totalLines?: number;
}

/**
 * Response from `log.write`.
 */
export interface LogWriteResponse {
    success: boolean;
    error?: string;
    path?: unknown;
}

/**
 * Response from `lyrics.exists`.
 */
export interface LyricsExistsResponse {
    success?: boolean;
    error?: string;
    exists?: boolean;
    sources?: JsonObject;
}

/**
 * Response from `lyrics.get`.
 */
export interface LyricsGetResponse {
    success: boolean;
    error?: string;
    available?: unknown;
    path?: unknown;
    source?: unknown;
    sourcePath?: unknown;
    lyrics?: unknown;
    synced?: unknown;
}

/**
 * Response from `lyrics.save`.
 */
export interface LyricsSaveResponse {
    success: boolean;
    error?: string;
    results?: JsonObject;
    savedTo?: string;
}

/**
 * Response from `menu.close`.
 */
export interface MenuCloseResponse {
    success: boolean;
}

/**
 * Response from `menu.getContextMenu`.
 */
export interface MenuGetContextMenuResponse {
    success: boolean;
    error?: string;
    mode?: string;
    locale?: string;
    i18n?: boolean;
    withAvailability?: boolean;
    items?: JsonObject;
    type?: string;
    availability?: unknown;
    label?: string;
    displayLabel?: string;
    path?: string;
    displayPath?: string;
    flags?: unknown;
    children?: JsonObject;
    commandId?: unknown;
    available?: unknown;
    guid?: unknown;
    subGuid?: unknown;
}

/**
 * Response from `menu.getMainMenu`.
 */
export interface MenuGetMainMenuResponse {
    fallback?: unknown;
    success?: boolean;
    error?: string;
    items?: unknown[];
    type?: string;
    availability?: unknown;
    label?: string;
    displayLabel?: string;
    path?: string;
    displayPath?: string;
    flags?: unknown;
    children?: JsonObject;
    commandId?: unknown;
    available?: unknown;
    guid?: unknown;
    subGuid?: unknown;
    source: unknown;
    root: string;
    requestedRoot: string;
    rootMatched: boolean;
    locale: string;
    i18n: boolean;
    withAvailability: boolean;
}

/**
 * Response from `menu.runContextCommand`.
 */
export interface MenuRunContextCommandResponse {
    success: boolean;
    error?: string;
    guid?: string;
    itemCount?: number;
}

/**
 * Response from `menu.runContextCommandById`.
 */
export interface MenuRunContextCommandByIdResponse {
    success: boolean;
    error?: string;
}

/**
 * Response from `menu.runMainMenuCommand`.
 */
export interface MenuRunMainMenuCommandResponse {
    success: boolean;
    error?: string;
    guid?: string;
}

/**
 * Response from `menu.show`.
 */
export interface MenuShowResponse {
    success: boolean;
    error?: string;
    menuId?: string;
}

/**
 * Response from `menu.showNativePopup`.
 */
export interface MenuShowNativePopupResponse {
    success: boolean;
    error?: string;
}

/**
 * Response from `metadata.embedArtwork`.
 */
export interface MetadataEmbedArtworkResponse {
    success: boolean;
    error?: string;
    path?: string;
    type?: string;
    results?: JsonObject;
    size?: number;
    savedTo?: string;
}

/**
 * Response from `metadata.read`.
 */
export interface MetadataReadResponse {
    success: boolean;
    error?: string;
    path?: string;
    tags?: JsonObject;
    info?: JsonObject;
}

/**
 * Response from `metadata.readBatch`.
 */
export interface MetadataReadBatchResponse {
    success: boolean;
    error?: string;
    total?: number;
    successCount?: number;
    errorCount?: number;
    results?: JsonObject;
}

/**
 * Response from `metadata.readByPath`.
 */
export interface MetadataReadByPathResponse {
    success: boolean;
    error?: string;
    path?: string;
    canonicalPath?: string;
    TRACKNUMBER?: unknown;
    DURATION: unknown;
    FILESIZE: unknown;
}

/**
 * Response from `metadata.readRaw`.
 */
export interface MetadataReadRawResponse {
    success: boolean;
    error?: string;
    path?: string;
    tags?: JsonObject;
    info?: JsonObject;
    source?: string;
}

/**
 * Response from `metadata.removeEmbeddedArt`.
 */
export interface MetadataRemoveEmbeddedArtResponse {
    success: boolean;
    error?: string;
    path?: string;
    removedTypes?: JsonObject;
}

/**
 * Response from `metadata.removeField`.
 */
export interface MetadataRemoveFieldResponse {
    success: boolean;
    error?: string;
    path?: string;
    removedTags?: JsonObject;
    removedCount?: number;
    dispatched?: boolean;
    subsong?: number;
    note?: string;
}

/**
 * Response from `metadata.removeTag`.
 */
export interface MetadataRemoveTagResponse {
    success: boolean;
    error?: string;
    path?: string;
    removedTags?: JsonObject;
    removedCount?: number;
    dispatched?: boolean;
    subsong?: number;
    note?: string;
}

/**
 * Response from `metadata.write`.
 */
export interface MetadataWriteResponse {
    success: boolean;
    error?: string;
    path?: string;
    canonicalPath?: string;
    note?: string;
    dispatched?: boolean;
    handlePath?: string;
    subsong?: number;
    tagsApplied?: JsonObject;
    tagsSet?: number;
    tagsRemoved?: number;
}

/**
 * Response from `metadata.writeBatch`.
 */
export interface MetadataWriteBatchResponse {
    success: boolean;
    error?: string;
    successCount?: number;
    failCount?: number;
    errors?: JsonObject;
    path?: string;
    canonicalPath?: string;
    note?: string;
    dispatched?: boolean;
    handlePath?: string;
    subsong?: number;
    tagsApplied?: JsonObject;
    tagsSet?: number;
    tagsRemoved?: number;
}

/**
 * Response from `misc.exit`.
 */
export interface MiscExitResponse {
    success: boolean;
}

/**
 * Response from `misc.getComponentPath`.
 */
export interface MiscGetComponentPathResponse {
    path: string;
    value: string;
}

/**
 * Response from `misc.getFoobarPath`.
 */
export interface MiscGetFoobarPathResponse {
    path: string;
    value: string;
}

/**
 * Response from `misc.getProfilePath`.
 */
export interface MiscGetProfilePathResponse {
    path: string;
    value: string;
}

/**
 * Response from `misc.restart`.
 */
export interface MiscRestartResponse {
    success: boolean;
}

/**
 * Response from `misc.showConsole`.
 */
export interface MiscShowConsoleResponse {
    success: boolean;
}

/**
 * Response from `misc.showLibrarySearch`.
 */
export interface MiscShowLibrarySearchResponse {
    success: boolean;
    query: string;
}

/**
 * Response from `misc.showPopupMessage`.
 */
export interface MiscShowPopupMessageResponse {
    success: boolean;
}

/**
 * Response from `misc.showPreferences`.
 */
export interface MiscShowPreferencesResponse {
    success: boolean;
}

/**
 * Response from `output.getDevices`.
 */
export interface OutputGetDevicesResponse {
    devices?: JsonObject;
    count?: number;
    success?: boolean;
    error?: string;
}

/**
 * Response from `output.getEntries`.
 */
export interface OutputGetEntriesResponse {
    entries?: JsonObject;
    count?: number;
    success?: boolean;
    error?: string;
}

/**
 * Response from `output.getSettings`.
 */
export interface OutputGetSettingsResponse {
    note?: unknown;
    availableOutputs?: unknown;
    success?: boolean;
    error?: string;
}

/**
 * Response from `panel.getConfig`.
 */
export interface PanelGetConfigResponse {
    success: boolean;
    config?: JsonObject;
    error?: string;
}

/**
 * Response from `panel.setConfig`.
 */
export interface PanelSetConfigResponse {
    success: boolean;
    changed?: boolean;
    error?: string;
}

/**
 * Response from `playback.getCurrentTrack`.
 */
export interface PlaybackGetCurrentTrackResponse {
    success: boolean;
    found: boolean;
    playing: boolean;
}

/**
 * Response from `playback.getCurrentTrackIndex`.
 */
export interface PlaybackGetCurrentTrackIndexResponse {
    success: boolean;
    found: boolean;
    playlist: null;
    index: null;
    track?: unknown;
}

/**
 * Response from `playback.getPlaybackOrder`.
 */
export interface PlaybackGetPlaybackOrderResponse {
    order: number;
    orderName: string;
    name: string;
    orderIndex: number;
}

/**
 * Response from `playback.getPlayingPlaylist`.
 */
export interface PlaybackGetPlayingPlaylistResponse {
    success: boolean;
    playlist: null;
    found: boolean;
    name?: string;
}

/**
 * Response from `playback.getPosition`.
 */
export interface PlaybackGetPositionResponse {
    position: number;
    duration: number;
    subsong: unknown;
    path: string;
}

/**
 * Response from `playback.getState`.
 */
export interface PlaybackGetStateResponse {
    state: string;
    canSeek: boolean;
    canPause: boolean;
}

/**
 * Response from `playback.getStopAfterCurrent`.
 */
export interface PlaybackGetStopAfterCurrentResponse {
    enabled: boolean;
}

/**
 * Response from `playback.getVolume`.
 */
export interface PlaybackGetVolumeResponse {
    volume: number;
    volumeDb: number;
    muted: boolean;
    isMuted: boolean;
}

/**
 * Response from `playback.mute`.
 */
export interface PlaybackMuteResponse {
    success: boolean;
}

/**
 * Response from `playback.next`.
 */
export interface PlaybackNextResponse {
    success: boolean;
}

/**
 * Response from `playback.pause`.
 */
export interface PlaybackPauseResponse {
    success: boolean;
}

/**
 * Response from `playback.play`.
 */
export interface PlaybackPlayResponse {
    success: boolean;
}

/**
 * Response from `playback.playOrPause`.
 */
export interface PlaybackPlayOrPauseResponse {
    success: boolean;
    isPlaying: boolean;
}

/**
 * Response from `playback.playPath`.
 */
export interface PlaybackPlayPathResponse {
    success: boolean;
    error?: string;
    path?: unknown;
    subsong?: unknown;
    tracksAdded?: number;
}

/**
 * Response from `playback.playPaths`.
 */
export interface PlaybackPlayPathsResponse {
    success: boolean;
    error?: string;
    /** int64 — may lose precision above 2^53 */
    tracksAdded?: number;
    /** int64 — may lose precision above 2^53 */
    startedAt?: number;
}

/**
 * Response from `playback.playPause`.
 */
export interface PlaybackPlayPauseResponse {
    success: boolean;
    isPlaying: boolean;
}

/**
 * Response from `playback.previous`.
 */
export interface PlaybackPreviousResponse {
    success: boolean;
}

/**
 * Response from `playback.random`.
 */
export interface PlaybackRandomResponse {
    success: boolean;
}

/**
 * Response from `playback.setPlaybackOrder`.
 */
export interface PlaybackSetPlaybackOrderResponse {
    success: boolean;
    order: number;
    orderName: string;
}

/**
 * Response from `playback.setPosition`.
 */
export interface PlaybackSetPositionResponse {
    success: boolean;
    error?: string;
    requestedPosition?: number;
    actualPosition?: number;
    newPosition?: number;
    oldPosition?: number;
    duration?: number;
    subsong?: unknown;
}

/**
 * Response from `playback.setStopAfterCurrent`.
 */
export interface PlaybackSetStopAfterCurrentResponse {
    success: boolean;
    enabled: boolean;
}

/**
 * Response from `playback.setVolume`.
 */
export interface PlaybackSetVolumeResponse {
    success: boolean;
}

/**
 * Response from `playback.stop`.
 */
export interface PlaybackStopResponse {
    success: boolean;
}

/**
 * Response from `playback.toggleMute`.
 */
export interface PlaybackToggleMuteResponse {
    success: boolean;
    muted: boolean;
}

/**
 * Response from `playback.toggleStopAfterCurrent`.
 */
export interface PlaybackToggleStopAfterCurrentResponse {
    enabled: boolean;
}

/**
 * Response from `playback.volumeDown`.
 */
export interface PlaybackVolumeDownResponse {
    success: boolean;
}

/**
 * Response from `playback.volumeUp`.
 */
export interface PlaybackVolumeUpResponse {
    success: boolean;
}

/**
 * Response from `playcount.get`.
 */
export interface PlaycountGetResponse {
    success: boolean;
    error?: string;
    count?: number;
    results?: JsonObject;
}

/**
 * Response from `playcount.getBatch`.
 */
export interface PlaycountGetBatchResponse {
    success: boolean;
    error?: string;
    count?: number;
    results?: JsonObject;
}

/**
 * Response from `playcount.getStats`.
 */
export interface PlaycountGetStatsResponse {
    success: boolean;
    totalTracks?: number;
    playedTracks?: number;
    unplayedTracks?: number;
    ratedTracks?: number;
    totalPlayCount?: number;
    maxPlayCount?: number;
    averagePlayCount?: number;
    averageRating?: number;
    error?: string;
}

/**
 * Response from `playcount.set`.
 */
export interface PlaycountSetResponse {
    success: boolean;
    error: string;
}

/**
 * Response from `playlist.addHandles`.
 */
export interface PlaylistAddHandlesResponse {
    success: boolean;
    error?: string;
    /** int64 — may lose precision above 2^53 */
    playlist?: number;
    requestedCount?: number;
    /** int64 — may lose precision above 2^53 */
    invalidCount?: number;
    /** int64 — may lose precision above 2^53 */
    addedCount?: number;
    /** int64 — may lose precision above 2^53 */
    countBefore?: number;
    /** int64 — may lose precision above 2^53 */
    totalCount?: number;
    code: unknown;
    details: JsonObject;
}

/**
 * Response from `playlist.addPaths`.
 */
export interface PlaylistAddPathsResponse {
    success: boolean;
    error?: string;
    /** int64 — may lose precision above 2^53 */
    playlist?: number;
    requestedPaths?: number;
    /** int64 — may lose precision above 2^53 */
    invalidCount?: number;
    /** int64 — may lose precision above 2^53 */
    countBefore?: number;
    /** int64 — may lose precision above 2^53 */
    addedCount?: number;
    /** int64 — may lose precision above 2^53 */
    totalCount?: number;
    code: unknown;
    details: JsonObject;
}

/**
 * Response from `playlist.addPathsAsync`.
 */
export interface PlaylistAddPathsAsyncResponse {
    success: boolean;
    error?: string;
    /** int64 — may lose precision above 2^53 */
    invalidCount?: number;
    operationId?: string;
    status?: string;
    /** int64 — may lose precision above 2^53 */
    totalCount?: number;
    code: unknown;
    details: JsonObject;
}

/**
 * Response from `playlist.addPathsSequential`.
 */
export interface PlaylistAddPathsSequentialResponse {
    success: boolean;
    error?: string;
    /** int64 — may lose precision above 2^53 */
    playlist?: number;
    /** int64 — may lose precision above 2^53 */
    addedCount?: number;
    order?: JsonObject;
    code: unknown;
    details: JsonObject;
}

/**
 * Response from `playlist.clear`.
 */
export interface PlaylistClearResponse {
    success: boolean;
    error?: string;
    /** int64 — may lose precision above 2^53 */
    playlist?: number;
    /** int64 — may lose precision above 2^53 */
    clearedCount?: number;
    /** int64 — may lose precision above 2^53 */
    remainingCount?: number;
    code: unknown;
    details: JsonObject;
}

/**
 * Response from `playlist.convertToAutoplaylist`.
 */
export interface PlaylistConvertToAutoplaylistResponse {
    success: boolean;
    error?: string;
    /** int64 — may lose precision above 2^53 */
    playlist?: number;
}

/**
 * Response from `playlist.create`.
 */
export interface PlaylistCreateResponse {
    success: boolean;
    /** int64 — may lose precision above 2^53 */
    index: number;
}

/**
 * Response from `playlist.createAutoplaylist`.
 */
export interface PlaylistCreateAutoplaylistResponse {
    success: boolean;
    error?: string;
    /** int64 — may lose precision above 2^53 */
    index?: number;
    /** int64 — may lose precision above 2^53 */
    playlist?: number;
    name?: string;
    query?: string;
}

/**
 * Response from `playlist.deselectAll`.
 */
export interface PlaylistDeselectAllResponse {
    success: boolean;
}

/**
 * Response from `playlist.duplicate`.
 */
export interface PlaylistDuplicateResponse {
    success: boolean;
    error?: string;
    /** int64 — may lose precision above 2^53 */
    index?: number;
    /** int64 — may lose precision above 2^53 */
    sourcePlaylist?: number;
    /** int64 — may lose precision above 2^53 */
    newPlaylist?: number;
    name?: string;
    /** int64 — may lose precision above 2^53 */
    trackCount?: number;
}

/**
 * Response from `playlist.focusTrack`.
 */
export interface PlaylistFocusTrackResponse {
    success: boolean;
    error?: string;
}

/**
 * Response from `playlist.getActive`.
 */
export interface PlaylistGetActiveResponse {
    success?: boolean;
    found?: boolean;
    /** int64 — may lose precision above 2^53 */
    index?: number;
    name?: string;
    /** int64 — may lose precision above 2^53 */
    trackCount?: number;
    isActive?: boolean;
    isPlaying?: boolean;
    isLocked?: boolean;
    duration?: number;
}

/**
 * Response from `playlist.getAll` (shape unspecified).
 */
export type PlaylistGetAllResponse = JsonObject;

/**
 * Response from `playlist.getAutoplaylistInfo`.
 */
export interface PlaylistGetAutoplaylistInfoResponse {
    success?: boolean;
    error?: string;
    isAutoplaylist?: boolean;
    /** int64 — may lose precision above 2^53 */
    playlist?: number;
    lockName?: unknown;
    keepSorted?: boolean;
    source?: unknown;
}

/**
 * Response from `playlist.getAutoplaylistQuery`.
 */
export interface PlaylistGetAutoplaylistQueryResponse {
    success?: boolean;
    error?: string;
    isAutoplaylist?: boolean;
    /** int64 — may lose precision above 2^53 */
    playlist?: number;
    query?: null;
    lockName?: unknown;
    keepSorted?: boolean;
    source?: unknown;
    note?: string;
}

/**
 * Response from `playlist.getAvailableColumns` (shape unspecified).
 */
export type PlaylistGetAvailableColumnsResponse = JsonObject;

/**
 * Response from `playlist.getCount`.
 */
export interface PlaylistGetCountResponse {
    /** int64 — may lose precision above 2^53 */
    count: number;
}

/**
 * Response from `playlist.getFocusedTrack`.
 */
export interface PlaylistGetFocusedTrackResponse {
    success: boolean;
    index: number;
    /** int64 — may lose precision above 2^53 */
    playlist?: number;
}

/**
 * Response from `playlist.getFocusTrack`.
 */
export interface PlaylistGetFocusTrackResponse {
    success: boolean;
    error?: string;
    /** int64 — may lose precision above 2^53 */
    playlist?: number;
    /** int64 — may lose precision above 2^53 */
    index?: number;
}

/**
 * Response from `playlist.getLockInfo`.
 */
export interface PlaylistGetLockInfoResponse {
    success?: boolean;
    error?: string;
    /** int64 — may lose precision above 2^53 */
    playlist?: number;
    isLocked?: boolean;
}

/**
 * Response from `playlist.getPlaying`.
 */
export interface PlaylistGetPlayingResponse {
    success?: boolean;
    found?: boolean;
    /** int64 — may lose precision above 2^53 */
    index?: number;
    name?: string;
    /** int64 — may lose precision above 2^53 */
    trackCount?: number;
    isActive?: boolean;
    isPlaying?: boolean;
    isLocked?: boolean;
    duration?: number;
}

/**
 * Response from `playlist.getSelectedTracks`.
 */
export interface PlaylistGetSelectedTracksResponse {
    success: boolean;
    error: string;
    tracks: unknown[];
}

/**
 * Response from `playlist.getSelection`.
 */
export interface PlaylistGetSelectionResponse {
    success: boolean;
    error?: string;
    items?: JsonObject;
    count?: number;
    /** int64 — may lose precision above 2^53 */
    playlist?: number;
}

/**
 * Response from `playlist.getTrackCount`.
 */
export interface PlaylistGetTrackCountResponse {
    count: number;
}

/**
 * Response from `playlist.getTracks`.
 */
export interface PlaylistGetTracksResponse {
    /** int64 — may lose precision above 2^53 */
    playlist: number;
    /** int64 — may lose precision above 2^53 */
    start: number;
    count: number;
    total: number;
    tracks: unknown[];
}

/**
 * Response from `playlist.insertTracks`.
 */
export interface PlaylistInsertTracksResponse {
    success: boolean;
    error?: string;
    /** int64 — may lose precision above 2^53 */
    playlist?: number;
    requestedCount?: number;
    /** int64 — may lose precision above 2^53 */
    invalidCount?: number;
    /** int64 — may lose precision above 2^53 */
    insertIndex?: number;
    /** int64 — may lose precision above 2^53 */
    addedCount?: number;
    /** int64 — may lose precision above 2^53 */
    countBefore?: number;
    /** int64 — may lose precision above 2^53 */
    totalCount?: number;
    code: unknown;
    details: JsonObject;
}

/**
 * Response from `playlist.isAutoplaylist`.
 */
export interface PlaylistIsAutoplaylistResponse {
    success?: boolean;
    error?: string;
    lockName?: unknown;
    /** int64 — may lose precision above 2^53 */
    playlist?: number;
    isAutoplaylist?: boolean;
}

/**
 * Response from `playlist.isLocked`.
 */
export interface PlaylistIsLockedResponse {
    success: boolean;
    isLocked: boolean;
    error?: string;
}

/**
 * Response from `playlist.moveTracks`.
 */
export interface PlaylistMoveTracksResponse {
    success: boolean;
    error?: string;
    code: unknown;
    details: JsonObject;
}

/**
 * Response from `playlist.playTrack`.
 */
export interface PlaylistPlayTrackResponse {
    success: boolean;
    error?: string;
}

/**
 * Response from `playlist.redo`.
 */
export interface PlaylistRedoResponse {
    success: boolean;
    error?: string;
}

/**
 * Response from `playlist.remove`.
 */
export interface PlaylistRemoveResponse {
    success: boolean;
    error?: string;
    code: unknown;
    details: JsonObject;
}

/**
 * Response from `playlist.removeAutoplaylist`.
 */
export interface PlaylistRemoveAutoplaylistResponse {
    success: boolean;
    error?: string;
    /** int64 — may lose precision above 2^53 */
    playlist?: number;
    source?: string;
    note?: string;
}

/**
 * Response from `playlist.removeSelectedTracks`.
 */
export interface PlaylistRemoveSelectedTracksResponse {
    success: boolean;
    error?: string;
    code: unknown;
    details: JsonObject;
}

/**
 * Response from `playlist.removeTracks`.
 */
export interface PlaylistRemoveTracksResponse {
    success: boolean;
    error?: string;
    code: unknown;
    details: JsonObject;
}

/**
 * Response from `playlist.rename`.
 */
export interface PlaylistRenameResponse {
    success: boolean;
    error?: string;
}

/**
 * Response from `playlist.reorder`.
 */
export interface PlaylistReorderResponse {
    success: boolean;
    error?: string;
    /** int64 — may lose precision above 2^53 */
    expected?: number;
    got?: number;
    /** int64 — may lose precision above 2^53 */
    index?: number;
    /** int64 — may lose precision above 2^53 */
    playlist?: number;
    /** int64 — may lose precision above 2^53 */
    itemCount?: number;
    code: unknown;
    details: JsonObject;
}

/**
 * Response from `playlist.reorderPlaylists`.
 */
export interface PlaylistReorderPlaylistsResponse {
    success: boolean;
    error?: string;
    /** int64 — may lose precision above 2^53 */
    expected?: number;
    got?: number;
    /** int64 — may lose precision above 2^53 */
    index?: number;
    /** int64 — may lose precision above 2^53 */
    count?: number;
}

/**
 * Response from `playlist.replaceAllAndPlay`.
 */
export interface PlaylistReplaceAllAndPlayResponse {
    success: boolean;
    error?: string;
    /** int64 — may lose precision above 2^53 */
    clearedCount?: number;
    /** int64 — may lose precision above 2^53 */
    invalidCount?: number;
    /** int64 — may lose precision above 2^53 */
    playlist?: number;
    /** int64 — may lose precision above 2^53 */
    addedCount?: number;
    /** int64 — may lose precision above 2^53 */
    totalCount?: number;
    /** int64 — may lose precision above 2^53 */
    playIndex?: number;
    code: unknown;
    details: JsonObject;
}

/**
 * Response from `playlist.reverse`.
 */
export interface PlaylistReverseResponse {
    success: boolean;
    error: unknown;
    code: unknown;
    details: JsonObject;
}

/**
 * Response from `playlist.selectAll`.
 */
export interface PlaylistSelectAllResponse {
    success: boolean;
}

/**
 * Response from `playlist.setActive`.
 */
export interface PlaylistSetActiveResponse {
    success: boolean;
    error?: string;
}

/**
 * Response from `playlist.setFocusedTrack`.
 */
export interface PlaylistSetFocusedTrackResponse {
    success: boolean;
    error?: string;
}

/**
 * Response from `playlist.setSelection`.
 */
export interface PlaylistSetSelectionResponse {
    success: boolean;
    error?: string;
}

/**
 * Response from `playlist.shuffle`.
 */
export interface PlaylistShuffleResponse {
    success: boolean;
    error?: string;
    code: unknown;
    details: JsonObject;
}

/**
 * Response from `playlist.sort`.
 */
export interface PlaylistSortResponse {
    success: boolean;
    error?: string;
    code: unknown;
    details: JsonObject;
}

/**
 * Response from `playlist.undo`.
 */
export interface PlaylistUndoResponse {
    success: boolean;
    error?: string;
}

/**
 * Response from `port.connect`.
 */
export interface PortConnectResponse {
    error: string;
    code: string;
}

/**
 * Response from `port.disconnect`.
 */
export interface PortDisconnectResponse {
    error: string;
    code: string;
}

/**
 * Response from `port.getPorts` (shape unspecified).
 */
export type PortGetPortsResponse = JsonObject;

/**
 * Response from `port.postMessage`.
 */
export interface PortPostMessageResponse {
    success: boolean;
    error: string;
    code: string;
}

/**
 * Response from `port.postMessageTo`.
 */
export interface PortPostMessageToResponse {
    success: boolean;
    error: string;
    code: string;
}

/**
 * Response from `queue.add`.
 */
export interface QueueAddResponse {
    success: boolean;
    error?: string;
    /** int64 — may lose precision above 2^53 */
    addedCount?: number;
    queueCount?: number;
}

/**
 * Response from `queue.addPaths`.
 */
export interface QueueAddPathsResponse {
    success: boolean;
    error?: string;
    /** int64 — may lose precision above 2^53 */
    playlist?: number;
    isLocked?: boolean;
    /** int64 — may lose precision above 2^53 */
    invalidCount?: number;
    /** int64 — may lose precision above 2^53 */
    addedCount?: number;
    queueCount?: number;
}

/**
 * Response from `queue.clear`.
 */
export interface QueueClearResponse {
    success: boolean;
    /** int64 — may lose precision above 2^53 */
    clearedCount: number;
}

/**
 * Response from `queue.flush`.
 */
export interface QueueFlushResponse {
    success: boolean;
    /** int64 — may lose precision above 2^53 */
    clearedCount: number;
}

/**
 * Response from `queue.get`.
 */
export interface QueueGetResponse {
    items: JsonObject;
    /** int64 — may lose precision above 2^53 */
    count: number;
    queueIndex: unknown;
    path: unknown;
    absolutePath: unknown;
    subsong: unknown;
    fileSize: unknown;
    title: unknown;
    artist: unknown;
    album: unknown;
    albumArtist: unknown;
    genre: unknown;
    date: unknown;
    trackNumber: unknown;
    discNumber: unknown;
    duration: unknown;
    bitrate: unknown;
    sampleRate: unknown;
    channels: unknown;
    codec: unknown;
}

/**
 * Response from `queue.getCount`.
 */
export interface QueueGetCountResponse {
    count: number;
    hasItems: boolean;
}

/**
 * Response from `queue.moveToTop`.
 */
export interface QueueMoveToTopResponse {
    success: boolean;
    error?: string;
    /** int64 — may lose precision above 2^53 */
    movedIndex?: number;
    queueCount?: number;
}

/**
 * Response from `queue.remove`.
 */
export interface QueueRemoveResponse {
    success: boolean;
    error?: string;
    /** int64 — may lose precision above 2^53 */
    removedIndex?: number;
    queueCount?: number;
    /** int64 — may lose precision above 2^53 */
    removedCount?: number;
}

/**
 * Response from `rating.get`.
 */
export interface RatingGetResponse {
    success: boolean;
    error?: string;
    path?: string;
    rating?: number;
    storage?: string;
}

/**
 * Response from `rating.set`.
 */
export interface RatingSetResponse {
    success: boolean;
    error?: string;
    path?: string;
    rating?: number;
    storage?: string;
    note?: string;
    menuPath?: unknown;
    canonicalPath?: string;
    dispatched?: boolean;
    handlePath?: string;
    subsong?: number;
    tagsApplied?: JsonObject;
    tagsSet?: number;
    tagsRemoved?: number;
}

/**
 * Response from `replaygain.clear`.
 */
export interface ReplaygainClearResponse {
    success: boolean;
    error?: string;
    clearedCount?: number;
}

/**
 * Response from `replaygain.get`.
 */
export interface ReplaygainGetResponse {
    success: boolean;
    error?: string;
    count?: number;
    results?: JsonObject;
}

/**
 * Response from `replaygain.getMode`.
 */
export interface ReplaygainGetModeResponse {
    sourceMode?: string;
    processingMode?: string;
    success?: boolean;
    error?: string;
}

/**
 * Response from `replaygain.getPreamp`.
 */
export interface ReplaygainGetPreampResponse {
    withRg?: number;
    withoutRg?: number;
    success?: boolean;
    error?: string;
}

/**
 * Response from `replaygain.getSettings`.
 */
export interface ReplaygainGetSettingsResponse {
    sourceMode?: string;
    processingMode?: string;
    preampWithRg?: number;
    preampWithoutRg?: number;
    active?: boolean;
    success?: boolean;
    error?: string;
}

/**
 * Response from `replaygain.scan`.
 */
export interface ReplaygainScanResponse {
    success: boolean;
    error?: string;
    scannedCount?: number;
    mode?: string;
    note?: string;
}

/**
 * Response from `replaygain.setMode`.
 */
export interface ReplaygainSetModeResponse {
    success: boolean;
    sourceMode?: string;
    processingMode?: string;
    changed?: boolean;
    error?: string;
}

/**
 * Response from `replaygain.setPreamp`.
 */
export interface ReplaygainSetPreampResponse {
    success: boolean;
    withRg?: number;
    withoutRg?: number;
    changed?: boolean;
    error?: string;
}

/**
 * Response from `selection.get`.
 */
export interface SelectionGetResponse {
    truncated: unknown;
    hasMore: unknown;
    /** int64 — may lose precision above 2^53 */
    count: number;
    type: string;
    handles: JsonObject;
    /** int64 — may lose precision above 2^53 */
    offset: number;
}

/**
 * Response from `selection.getType`.
 */
export interface SelectionGetTypeResponse {
    type: number;
    typeName: string;
}

/**
 * Response from `selection.getViewerMode`.
 */
export interface SelectionGetViewerModeResponse {
    mode: string;
}

/**
 * Response from `selection.getViewingTrack`.
 */
export interface SelectionGetViewingTrackResponse {
    success: boolean;
    found: boolean;
    mode: string;
    playlistIndex?: unknown;
    itemIndex?: unknown;
    track?: JsonObject;
    source?: string;
    handle?: string;
}

/**
 * Response from `selection.set`.
 */
export interface SelectionSetResponse {
    success: boolean;
    error?: string;
    count?: number;
}

/**
 * Response from `selection.setPlaylistTracking`.
 */
export interface SelectionSetPlaylistTrackingResponse {
    success: boolean;
    error?: string;
    mode?: string;
}

/**
 * Response from `shell.exec`.
 */
export interface ShellExecResponse {
    success: boolean;
    error?: string;
    processId?: unknown;
}

/**
 * Response from `shell.openExternal`.
 */
export interface ShellOpenExternalResponse {
    success: boolean;
    error?: string;
}

/**
 * Response from `shell.openWith`.
 */
export interface ShellOpenWithResponse {
    success: boolean;
    error?: string;
}

/**
 * Response from `shell.showInExplorer`.
 */
export interface ShellShowInExplorerResponse {
    success: boolean;
    error?: string;
}

/**
 * Response from `shell.spawn`.
 */
export interface ShellSpawnResponse {
    success: boolean;
    error: string;
    exited?: unknown;
    exitCode?: unknown;
    processId?: unknown;
}

/**
 * Response from `state.delete`.
 */
export interface StateDeleteResponse {
    success: boolean;
    error: string;
    code: string;
}

/**
 * Response from `state.get`.
 */
export interface StateGetResponse {
    error: string;
    code: string;
}

/**
 * Response from `state.keys` (shape unspecified).
 */
export type StateKeysResponse = JsonObject;

/**
 * Response from `state.set`.
 */
export interface StateSetResponse {
    success: boolean;
    error: string;
    code: string;
}

/**
 * Response from `system.getApisByNamespace` (shape unspecified).
 */
export type SystemGetApisByNamespaceResponse = JsonObject;

/**
 * Response from `system.getApiStats`.
 */
export interface SystemGetApiStatsResponse {
    totalApis: number;
    internalApis: number;
    externalApis: number;
    pluginCount: number;
    byNamespace: JsonObject;
}

/**
 * Response from `system.getDPI`.
 */
export interface SystemGetDPIResponse {
    dpi: unknown;
    scale: number;
}

/**
 * Response from `system.getLocale`.
 */
export interface SystemGetLocaleResponse {
    locale: string;
    language: string;
    country: string;
}

/**
 * Response from `system.getRegisteredPlugins` (shape unspecified).
 */
export type SystemGetRegisteredPluginsResponse = JsonObject;

/**
 * Response from `system.getTheme`.
 */
export interface SystemGetThemeResponse {
    darkMode: boolean;
    isDark: boolean;
    accentColor: string;
    transparency: boolean;
}

/**
 * Response from `system.isPluginRegistered`.
 */
export interface SystemIsPluginRegisteredResponse {
    success: boolean;
    registered: boolean;
}

/**
 * Response from `system.listAvailableApis` (shape unspecified).
 */
export type SystemListAvailableApisResponse = JsonObject;

/**
 * Response from `system.searchApis` (shape unspecified).
 */
export type SystemSearchApisResponse = JsonObject;

/**
 * Response from `taskbar.flash`.
 */
export interface TaskbarFlashResponse {
    success: boolean;
    panelMode: boolean;
}

/**
 * Response from `taskbar.setOverlayIcon`.
 */
export interface TaskbarSetOverlayIconResponse {
    success: boolean;
    panelMode: boolean;
}

/**
 * Response from `taskbar.setProgress`.
 */
export interface TaskbarSetProgressResponse {
    success: boolean;
    panelMode: boolean;
}

/**
 * Response from `taskbar.setThumbnailButtons`.
 */
export interface TaskbarSetThumbnailButtonsResponse {
    success: boolean;
    error?: string;
    panelMode: boolean;
}

/**
 * Response from `taskbar.updateButton`.
 */
export interface TaskbarUpdateButtonResponse {
    success: boolean;
    error?: string;
    panelMode: boolean;
}

/**
 * Response from `test.echo`.
 */
export interface TestEchoResponse {
    success: boolean;
    echo: unknown;
    input: JsonObject;
}

/**
 * Response from `test.ping`.
 */
export interface TestPingResponse {
    pong: boolean;
    /** int64 — may lose precision above 2^53 */
    timestamp: number;
}

/**
 * Response from `titleformat.eval`.
 */
export interface TitleformatEvalResponse {
    success: boolean;
    error?: string;
    path?: string;
    pattern?: string;
    result?: string;
}

/**
 * Response from `titleformat.evalBatch`.
 */
export interface TitleformatEvalBatchResponse {
    success: boolean;
    error?: string;
    pattern?: string;
    total?: number;
    successCount?: number;
    errorCount?: number;
    results?: JsonObject;
}

/**
 * Response from `titleformat.evalFields`.
 */
export interface TitleformatEvalFieldsResponse {
    success: boolean;
    error?: string;
    path?: string;
}

/**
 * Response from `titleformat.evalFieldsBatch`.
 */
export interface TitleformatEvalFieldsBatchResponse {
    success: boolean;
    error?: string;
    total?: number;
    successCount?: number;
    errorCount?: number;
    results?: unknown[];
}

/**
 * Response from `titleformat.getBuiltinFields`.
 */
export interface TitleformatGetBuiltinFieldsResponse {
    success: boolean;
    fields: JsonObject;
}

/**
 * Response from `tray.appendMenuItems`.
 */
export interface TrayAppendMenuItemsResponse {
    success: boolean;
    error?: string;
    panelMode: boolean;
}

/**
 * Response from `tray.clearMenuItems`.
 */
export interface TrayClearMenuItemsResponse {
    success: boolean;
    panelMode: boolean;
}

/**
 * Response from `tray.create`.
 */
export interface TrayCreateResponse {
    success: boolean;
    error?: string;
    panelMode: boolean;
}

/**
 * Response from `tray.destroy`.
 */
export interface TrayDestroyResponse {
    success: boolean;
    panelMode: boolean;
}

/**
 * Response from `tray.getMenuItems`.
 */
export interface TrayGetMenuItemsResponse {
    success: boolean;
    items: JsonObject;
    panelMode: boolean;
}

/**
 * Response from `tray.isVisible`.
 */
export interface TrayIsVisibleResponse {
    success: boolean;
    visible: boolean;
    panelMode: boolean;
}

/**
 * Response from `tray.removeMenuItems`.
 */
export interface TrayRemoveMenuItemsResponse {
    success: boolean;
    error?: string;
    removed?: number;
    panelMode: boolean;
}

/**
 * Response from `tray.setCloseToTray`.
 */
export interface TraySetCloseToTrayResponse {
    success: boolean;
    panelMode: boolean;
}

/**
 * Response from `tray.setContextMenu`.
 */
export interface TraySetContextMenuResponse {
    success: boolean;
    error?: string;
    panelMode: boolean;
}

/**
 * Response from `tray.setIcon`.
 */
export interface TraySetIconResponse {
    success: boolean;
    panelMode: boolean;
}

/**
 * Response from `tray.setMenuItemState`.
 */
export interface TraySetMenuItemStateResponse {
    success: boolean;
    error?: string;
    found?: boolean;
    panelMode: boolean;
}

/**
 * Response from `tray.setMinimizeToTray`.
 */
export interface TraySetMinimizeToTrayResponse {
    success: boolean;
    panelMode: boolean;
}

/**
 * Response from `tray.setTooltip`.
 */
export interface TraySetTooltipResponse {
    success: boolean;
    panelMode: boolean;
}

/**
 * Response from `tray.showBalloon`.
 */
export interface TrayShowBalloonResponse {
    success: boolean;
    panelMode: boolean;
}

/**
 * Response from `ui.hideNotification`.
 */
export interface UiHideNotificationResponse {
    success: boolean;
    error?: string;
}

/**
 * Response from `ui.showContextMenu`.
 */
export interface UiShowContextMenuResponse {
    success: boolean;
    error?: string;
}

/**
 * Response from `ui.showCustomMenu`.
 */
export interface UiShowCustomMenuResponse {
    success: boolean;
    error?: string;
    selectedId?: string;
}

/**
 * Response from `ui.showNotification`.
 */
export interface UiShowNotificationResponse {
    success: boolean;
    error?: string;
    id?: number;
}

/**
 * Response from `ui.showToast`.
 */
export interface UiShowToastResponse {
    success: boolean;
    error?: string;
}

/**
 * Response from `window.blur`.
 */
export interface WindowBlurResponse {
    success: boolean;
    error?: string;
}

/**
 * Response from `window.broadcast`.
 */
export interface WindowBroadcastResponse {
    success: boolean;
    error?: unknown;
}

/**
 * Response from `window.cancelClose`.
 */
export interface WindowCancelCloseResponse {
    success: boolean;
    error?: unknown;
}

/**
 * Response from `window.center`.
 */
export interface WindowCenterResponse {
    success: boolean;
    error?: string;
    supported: boolean;
    panelMode: boolean;
}

/**
 * Response from `window.clearClickThroughExcludeRegions`.
 */
export interface WindowClearClickThroughExcludeRegionsResponse {
    success: boolean;
    error?: unknown;
    windowId?: string;
}

/**
 * Response from `window.clearDragRegions`.
 */
export interface WindowClearDragRegionsResponse {
    success: boolean;
    error?: string;
    supported: boolean;
    panelMode: boolean;
}

/**
 * Response from `window.clearNoDragRegions`.
 */
export interface WindowClearNoDragRegionsResponse {
    success: boolean;
    error?: string;
    supported: boolean;
    panelMode: boolean;
}

/**
 * Response from `window.close`.
 */
export interface WindowCloseResponse {
    success: boolean;
    supported: boolean;
    panelMode: boolean;
    error: string;
}

/**
 * Response from `window.closeAllPopups`.
 */
export interface WindowCloseAllPopupsResponse {
    success: boolean;
}

/**
 * Response from `window.closePopup`.
 */
export interface WindowClosePopupResponse {
    success: boolean;
    error?: unknown;
}

/**
 * Response from `window.confirmClose`.
 */
export interface WindowConfirmCloseResponse {
    success: boolean;
    error?: unknown;
}

/**
 * Response from `window.createPopup`.
 */
export interface WindowCreatePopupResponse {
    success: boolean;
    error?: string;
    windowId?: string;
}

/**
 * Response from `window.enterFullscreen`.
 */
export interface WindowEnterFullscreenResponse {
    success: boolean;
    isFullscreen?: boolean;
    error?: string;
    supported: boolean;
    panelMode: boolean;
}

/**
 * Response from `window.exitFullscreen`.
 */
export interface WindowExitFullscreenResponse {
    success: boolean;
    isFullscreen?: boolean;
    error?: string;
    supported: boolean;
    panelMode: boolean;
}

/**
 * Response from `window.flash`.
 */
export interface WindowFlashResponse {
    success: boolean;
    error?: string;
    supported: boolean;
    panelMode: boolean;
}

/**
 * Response from `window.flashTaskbar`.
 */
export interface WindowFlashTaskbarResponse {
    success: boolean;
    supported: boolean;
    panelMode: boolean;
    error: string;
}

/**
 * Response from `window.focus`.
 */
export interface WindowFocusResponse {
    success: boolean;
    error?: string;
}

/**
 * Response from `window.getAllWindows`.
 */
export interface WindowGetAllWindowsResponse {
    success: boolean;
    items: JsonObject;
}

/**
 * Response from `window.getBackdropPolicy`.
 */
export interface WindowGetBackdropPolicyResponse {
    success: unknown;
    windowId: unknown;
    error: string;
}

/**
 * Response from `window.getBounds`.
 */
export interface WindowGetBoundsResponse {
    x: number;
    y: number;
    width: number;
    height: number;
}

/**
 * Response from `window.getCaptionButtonsWidth`.
 */
export interface WindowGetCaptionButtonsWidthResponse {
    width: number;
    buttonWidth: number;
}

/**
 * Response from `window.getCornerPreference`.
 */
export interface WindowGetCornerPreferenceResponse {
    mode: string;
    preference: string;
}

/**
 * Response from `window.getCurrentWindowId`.
 */
export interface WindowGetCurrentWindowIdResponse {
    success: boolean;
    windowId: string;
}

/**
 * Response from `window.getDevServerConfig`.
 */
export interface WindowGetDevServerConfigResponse {
    success: boolean;
    useDevServer: boolean;
    devServerUrl: string;
}

/**
 * Response from `window.getDpiScale`.
 */
export interface WindowGetDpiScaleResponse {
    success: boolean;
    dpi: number;
    scale: number;
}

/**
 * Response from `window.getMaxSize`.
 */
export interface WindowGetMaxSizeResponse {
    width: number;
    height: number;
}

/**
 * Response from `window.getMinSize`.
 */
export interface WindowGetMinSizeResponse {
    width: number;
    height: number;
}

/**
 * Response from `window.getMode`.
 */
export interface WindowGetModeResponse {
    mode: string;
    panelMode: boolean;
    windowId: string;
}

/**
 * Response from `window.getPopupBehavior`.
 */
export interface WindowGetPopupBehaviorResponse {
    success: boolean;
    error?: string;
    windowId?: unknown;
}

/**
 * Response from `window.getState`.
 */
export interface WindowGetStateResponse {
    maximized: boolean;
    minimized: boolean;
    fullscreen: boolean;
    alwaysOnTop: boolean;
    focused: boolean;
    width: number;
    height: number;
    x: number;
    y: number;
    isMaximized?: boolean;
    isMinimized?: boolean;
    isFullscreen?: boolean;
    isAlwaysOnTop?: boolean;
    isFocused?: boolean;
}

/**
 * Response from `window.getTitle`.
 */
export interface WindowGetTitleResponse {
    title: string;
}

/**
 * Response from `window.getTitlebarHeight`.
 */
export interface WindowGetTitlebarHeightResponse {
    height: number;
}

/**
 * Response from `window.getTitlebarInfo`.
 */
export interface WindowGetTitlebarInfoResponse {
    height: number;
    captionButtonsWidth: number;
    captionButtonWidth: number;
    isMaximized: boolean;
}

/**
 * Response from `window.getZoom`.
 */
export interface WindowGetZoomResponse {
    success: boolean;
    zoom: number;
    dpi?: number;
    dpiScale?: number;
}

/**
 * Response from `window.hasSavedBounds`.
 */
export interface WindowHasSavedBoundsResponse {
    hasSavedBounds: boolean;
    description: unknown;
}

/**
 * Response from `window.isAlwaysOnTop`.
 */
export interface WindowIsAlwaysOnTopResponse {
    enabled: boolean;
    isAlwaysOnTop: boolean;
}

/**
 * Response from `window.isClickThrough`.
 */
export interface WindowIsClickThroughResponse {
    success: boolean;
    error?: unknown;
    clickThrough?: boolean;
}

/**
 * Response from `window.isFullscreen`.
 */
export interface WindowIsFullscreenResponse {
    fullscreen: boolean;
    isFullscreen: boolean;
}

/**
 * Response from `window.isMaximized`.
 */
export interface WindowIsMaximizedResponse {
    maximized: boolean;
    isMaximized: boolean;
}

/**
 * Response from `window.isMinimized`.
 */
export interface WindowIsMinimizedResponse {
    minimized: boolean;
}

/**
 * Response from `window.isResizable`.
 */
export interface WindowIsResizableResponse {
    resizable: boolean;
}

/**
 * Response from `window.maximize`.
 */
export interface WindowMaximizeResponse {
    success: boolean;
    error?: string;
    supported: boolean;
    panelMode: boolean;
}

/**
 * Response from `window.minimize`.
 */
export interface WindowMinimizeResponse {
    success: boolean;
    error?: string;
    supported: boolean;
    panelMode: boolean;
}

/**
 * Response from `window.refreshWebView`.
 */
export interface WindowRefreshWebViewResponse {
    success: boolean;
    error?: string;
}

/**
 * Response from `window.reload`.
 */
export interface WindowReloadResponse {
    success: boolean;
    error?: string;
}

/**
 * Response from `window.resetZoom`.
 */
export interface WindowResetZoomResponse {
    success: boolean;
    error?: string;
    zoom?: number;
}

/**
 * Response from `window.restore`.
 */
export interface WindowRestoreResponse {
    success: boolean;
    error?: string;
    supported: boolean;
    panelMode: boolean;
}

/**
 * Response from `window.sendMessage`.
 */
export interface WindowSendMessageResponse {
    success: boolean;
    error?: unknown;
}

/**
 * Response from `window.setAcrylic`.
 */
export interface WindowSetAcrylicResponse {
    darkMode: unknown;
    success: boolean;
    enabled: boolean;
    supported: boolean;
    panelMode: boolean;
    error: string;
}

/**
 * Response from `window.setAlwaysOnTop`.
 */
export interface WindowSetAlwaysOnTopResponse {
    success: boolean;
    error?: string;
    supported: boolean;
    panelMode: boolean;
}

/**
 * Response from `window.setBackdropPolicy`.
 */
export interface WindowSetBackdropPolicyResponse {
    success: boolean;
    error?: string;
    windowId?: unknown;
}

/**
 * Response from `window.setBackgroundTransparency`.
 */
export interface WindowSetBackgroundTransparencyResponse {
    success: boolean;
    error?: string;
    transparent?: boolean;
    description?: unknown;
}

/**
 * Response from `window.setBlur`.
 */
export interface WindowSetBlurResponse {
    success: boolean;
    enabled: boolean;
    supported: boolean;
    panelMode: boolean;
    error: string;
}

/**
 * Response from `window.setBounds`.
 */
export interface WindowSetBoundsResponse {
    success: boolean;
    error?: string;
    supported: boolean;
    panelMode: boolean;
}

/**
 * Response from `window.setClickThrough`.
 */
export interface WindowSetClickThroughResponse {
    success: boolean;
    error?: unknown;
    clickThrough?: boolean;
}

/**
 * Response from `window.setClickThroughExcludeRegions`.
 */
export interface WindowSetClickThroughExcludeRegionsResponse {
    success: boolean;
    error?: unknown;
    warning?: unknown;
    windowId?: string;
    count?: number;
    dpiScale?: number;
}

/**
 * Response from `window.setCornerPreference`.
 */
export interface WindowSetCornerPreferenceResponse {
    success: boolean;
    error?: string;
    supported: boolean;
    panelMode: boolean;
}

/**
 * Response from `window.setDarkMode`.
 */
export interface WindowSetDarkModeResponse {
    success: boolean;
    enabled: boolean;
    supported: boolean;
    panelMode: boolean;
    error: string;
}

/**
 * Response from `window.setDevServerConfig`.
 */
export interface WindowSetDevServerConfigResponse {
    success: boolean;
    useDevServer: boolean;
    devServerUrl: string;
}

/**
 * Response from `window.setDragRegions`.
 */
export interface WindowSetDragRegionsResponse {
    success: boolean;
    error?: string;
    count?: number;
    dpiScale?: number;
    supported: boolean;
    panelMode: boolean;
}

/**
 * Response from `window.setFrameless`.
 */
export interface WindowSetFramelessResponse {
    success: boolean;
    error?: string;
    frameless?: boolean;
}

/**
 * Response from `window.setFullscreen`.
 */
export interface WindowSetFullscreenResponse {
    success: boolean;
    fullscreen: boolean;
    error?: string;
    supported: boolean;
    panelMode: boolean;
}

/**
 * Response from `window.setMaxSize`.
 */
export interface WindowSetMaxSizeResponse {
    success: boolean;
    error?: string;
    supported: boolean;
    panelMode: boolean;
}

/**
 * Response from `window.setMica`.
 */
export interface WindowSetMicaResponse {
    darkMode: unknown;
    success: boolean;
    enabled: boolean;
    variant: string;
    supported: boolean;
    panelMode: boolean;
    error: string;
}

/**
 * Response from `window.setMicaEffect`.
 */
export interface WindowSetMicaEffectResponse {
    darkMode: unknown;
    success: boolean;
    enabled: boolean;
    variant: string;
    supported: boolean;
    panelMode: boolean;
    error: string;
}

/**
 * Response from `window.setMinSize`.
 */
export interface WindowSetMinSizeResponse {
    success: boolean;
    error?: string;
    supported: boolean;
    panelMode: boolean;
}

/**
 * Response from `window.setNoDragRegions`.
 */
export interface WindowSetNoDragRegionsResponse {
    success: boolean;
    error?: string;
    count?: number;
    dpiScale?: number;
    supported: boolean;
    panelMode: boolean;
}

/**
 * Response from `window.setPopupBehavior`.
 */
export interface WindowSetPopupBehaviorResponse {
    success: boolean;
    error?: string;
    windowId?: unknown;
}

/**
 * Response from `window.setPosition`.
 */
export interface WindowSetPositionResponse {
    success: boolean;
    supported: boolean;
    panelMode: boolean;
    error: string;
}

/**
 * Response from `window.setResizable`.
 */
export interface WindowSetResizableResponse {
    success: boolean;
    error?: string;
    supported: boolean;
    panelMode: boolean;
}

/**
 * Response from `window.setSize`.
 */
export interface WindowSetSizeResponse {
    success: boolean;
    supported: boolean;
    panelMode: boolean;
    error: string;
}

/**
 * Response from `window.setTitle`.
 */
export interface WindowSetTitleResponse {
    success: boolean;
    error?: string;
    supported: boolean;
    panelMode: boolean;
}

/**
 * Response from `window.setTitlebarHeight`.
 */
export interface WindowSetTitlebarHeightResponse {
    success: boolean;
    error?: string;
    height?: number;
    supported: boolean;
    panelMode: boolean;
}

/**
 * Response from `window.setZoom`.
 */
export interface WindowSetZoomResponse {
    success: boolean;
    error?: string;
    zoom?: number;
}

/**
 * Response from `window.setZoomForDpi`.
 */
export interface WindowSetZoomForDpiResponse {
    success: boolean;
    error?: string;
    dpi?: number;
    zoom?: number;
}

/**
 * Response from `window.showSystemMenu`.
 */
export interface WindowShowSystemMenuResponse {
    success: boolean;
    error?: string;
    supported: boolean;
    panelMode: boolean;
}

/**
 * Response from `window.startDrag`.
 */
export interface WindowStartDragResponse {
    success: boolean;
    error?: string;
    supported: boolean;
    panelMode: boolean;
}

/**
 * Response from `window.startResize`.
 */
export interface WindowStartResizeResponse {
    success: boolean;
    error?: string;
    supported: boolean;
    panelMode: boolean;
}

/**
 * Response from `window.toggleAlwaysOnTop`.
 */
export interface WindowToggleAlwaysOnTopResponse {
    success: boolean;
    enabled: boolean;
    supported: boolean;
    panelMode: boolean;
    error: string;
}

/**
 * Response from `window.toggleFullscreen`.
 */
export interface WindowToggleFullscreenResponse {
    success: boolean;
    fullscreen: boolean;
    error?: string;
    supported: boolean;
    panelMode: boolean;
}

/**
 * Response from `window.toggleMaximize`.
 */
export interface WindowToggleMaximizeResponse {
    success: boolean;
    maximized: boolean;
    supported: boolean;
    panelMode: boolean;
    error: string;
}
// ── API-name → Response map ──────────────────────────────────────────────
/**
 * Master map from C++ `api_name` to the generated / override `*Response` type.
 * Keys mirror `cpp_api_handler.metadata.api_name` one-to-one.
 */
export interface ApiResponseMap {
    "artwork.getAvailableArtwork": ArtworkGetAvailableArtworkResponse;
    "artwork.getAvailableTypes": ArtworkGetAvailableTypesResponse;
    "artwork.getBatch": ArtworkGetBatchResponse;
    "artwork.getByPath": ArtworkGetByPathResponse;
    "artwork.getByPlaylistItem": ArtworkGetByPlaylistItemResponse;
    "artwork.getCurrent": ArtworkGetCurrentResponse;
    "artwork.getFb2kUrl": ArtworkGetFb2kUrlResponse;
    "artwork.getFb2kUrlByPath": ArtworkGetFb2kUrlByPathResponse;
    "artwork.getFb2kUrlByPathBatch": ArtworkGetFb2kUrlByPathBatchResponse;
    "artwork.getFolderImages": ArtworkGetFolderImagesResponse;
    "artwork.getForTrack": ArtworkGetForTrackResponse;
    "artwork.getLyrics": ArtworkGetLyricsResponse;
    "artwork.getMetadata": ArtworkGetMetadataResponse;
    "audio.analyzeBPM": AudioAnalyzeBPMResponse;
    "audio.generateFullWaveform": AudioGenerateFullWaveformResponse;
    "audio.generateWaveform": AudioGenerateWaveformResponse;
    "audio.getOutputInfo": AudioGetOutputInfoResponse;
    "audio.getSpectrum": AudioGetSpectrumResponse;
    "audio.getSpectrumDebugState": AudioGetSpectrumDebugStateResponse;
    "audio.getStreamInfo": AudioGetStreamInfoResponse;
    "audio.getWaveform": AudioGetWaveformResponse;
    "audio.isVisualizationAvailable": AudioIsVisualizationAvailableResponse;
    "audio.setChannelMode": AudioSetChannelModeResponse;
    "audio.subscribeSpectrum": AudioSubscribeSpectrumResponse;
    "audio.subscribeStream": AudioSubscribeStreamResponse;
    "audio.unsubscribeSpectrum": AudioUnsubscribeSpectrumResponse;
    "audio.unsubscribeStream": AudioUnsubscribeStreamResponse;
    "clipboard.read": ClipboardReadResponse;
    "clipboard.write": ClipboardWriteResponse;
    "clipboard.writeFiles": ClipboardWriteFilesResponse;
    "clipboard.writeHTML": ClipboardWriteHTMLResponse;
    "config.export": ConfigExportResponse;
    "config.get": ConfigGetResponse;
    "config.getActiveDspPreset": ConfigGetActiveDspPresetResponse;
    "config.getAdvancedConfig": ConfigGetAdvancedConfigResponse;
    "config.getAdvancedConfigValue": ConfigGetAdvancedConfigValueResponse;
    "config.getAll": ConfigGetAllResponse;
    "config.getComponents": ConfigGetComponentsResponse;
    "config.getCursorFollowPlayback": ConfigGetCursorFollowPlaybackResponse;
    "config.getDspPresets": ConfigGetDspPresetsResponse;
    "config.getLibraryFilePatterns": ConfigGetLibraryFilePatternsResponse;
    "config.getLibraryStatus": ConfigGetLibraryStatusResponse;
    "config.getOutputConfig": ConfigGetOutputConfigResponse;
    "config.getOutputDevices": ConfigGetOutputDevicesResponse;
    "config.getPlaybackFollowCursor": ConfigGetPlaybackFollowCursorResponse;
    "config.getPreferencesPages": ConfigGetPreferencesPagesResponse;
    "config.getPreferencesStandardGuids": ConfigGetPreferencesStandardGuidsResponse;
    "config.getReplaygainMode": ConfigGetReplaygainModeResponse;
    "config.getVersionInfo": ConfigGetVersionInfoResponse;
    "config.remove": ConfigRemoveResponse;
    "config.resetAdvancedConfig": ConfigResetAdvancedConfigResponse;
    "config.set": ConfigSetResponse;
    "config.setActiveDspPreset": ConfigSetActiveDspPresetResponse;
    "config.setAdvancedConfigValue": ConfigSetAdvancedConfigValueResponse;
    "config.setCursorFollowPlayback": ConfigSetCursorFollowPlaybackResponse;
    "config.setOutputBuffer": ConfigSetOutputBufferResponse;
    "config.setOutputDevice": ConfigSetOutputDeviceResponse;
    "config.setPlaybackFollowCursor": ConfigSetPlaybackFollowCursorResponse;
    "config.setReplaygainMode": ConfigSetReplaygainModeResponse;
    "config.showLibraryPreferences": ConfigShowLibraryPreferencesResponse;
    "console.error": ConsoleErrorResponse;
    "console.log": ConsoleLogResponse;
    "console.warn": ConsoleWarnResponse;
    "cursor.isHidden": CursorIsHiddenResponse;
    "cursor.setHidden": CursorSetHiddenResponse;
    "dialog.confirm": DialogConfirmResponse;
    "dialog.openFile": DialogOpenFileResponse;
    "dialog.openFolder": DialogOpenFolderResponse;
    "dialog.saveFile": DialogSaveFileResponse;
    "discovery.executeContextMenuByPath": DiscoveryExecuteContextMenuByPathResponse;
    "discovery.executeContextMenuCommand": DiscoveryExecuteContextMenuCommandResponse;
    "discovery.executeMainMenuCommand": DiscoveryExecuteMainMenuCommandResponse;
    "discovery.getAllServices": DiscoveryGetAllServicesResponse;
    "discovery.getComponents": DiscoveryGetComponentsResponse;
    "discovery.getContextMenuCommands": DiscoveryGetContextMenuCommandsResponse;
    "discovery.getContextMenuTree": DiscoveryGetContextMenuTreeResponse;
    "discovery.getDspEntries": DiscoveryGetDspEntriesResponse;
    "discovery.getInputFormats": DiscoveryGetInputFormatsResponse;
    "discovery.getMainMenuCommands": DiscoveryGetMainMenuCommandsResponse;
    "discovery.getMainMenuGroups": DiscoveryGetMainMenuGroupsResponse;
    "discovery.getOutputDevices": DiscoveryGetOutputDevicesResponse;
    "discovery.getPreferencePages": DiscoveryGetPreferencePagesResponse;
    "discovery.getUIElements": DiscoveryGetUIElementsResponse;
    "discovery.searchCommands": DiscoverySearchCommandsResponse;
    "dnd.getDropZones": DndGetDropZonesResponse;
    "dnd.registerDropZone": DndRegisterDropZoneResponse;
    "dnd.startDrag": DndStartDragResponse;
    "dnd.unregisterDropZone": DndUnregisterDropZoneResponse;
    "dsp.addDsp": DspAddDspResponse;
    "dsp.applyPreset": DspApplyPresetResponse;
    "dsp.getAvailable": DspGetAvailableResponse;
    "dsp.getChain": DspGetChainResponse;
    "dsp.getPresets": DspGetPresetsResponse;
    "dsp.moveDsp": DspMoveDspResponse;
    "dsp.removeDsp": DspRemoveDspResponse;
    "dsp.setChain": DspSetChainResponse;
    "event.emit": EventEmitResponse;
    "event.emitTo": EventEmitToResponse;
    "file.copy": FileCopyResponse;
    "file.delete": FileDeleteResponse;
    "file.exists": FileExistsResponse;
    "file.getInfo": FileGetInfoResponse;
    "file.list": FileListResponse;
    "file.mkdir": FileMkdirResponse;
    "file.move": FileMoveResponse;
    "file.read": FileReadResponse;
    "file.rename": FileRenameResponse;
    "file.write": FileWriteResponse;
    "http.abort": HttpAbortResponse;
    "http.delete": HttpDeleteResponse;
    "http.download": HttpDownloadResponse;
    "http.get": HttpGetResponse;
    "http.head": HttpHeadResponse;
    "http.patch": HttpPatchResponse;
    "http.post": HttpPostResponse;
    "http.put": HttpPutResponse;
    "jitQueue.clear": JitQueueClearResponse;
    "jitQueue.enqueueNext": JitQueueEnqueueNextResponse;
    "jitQueue.getState": JitQueueGetStateResponse;
    "jitQueue.notifyEmpty": JitQueueNotifyEmptyResponse;
    "jitQueue.playNow": JitQueuePlayNowResponse;
    "jitQueue.preloadBatch": JitQueuePreloadBatchResponse;
    "jitQueue.skip": JitQueueSkipResponse;
    "jitQueue.stop": JitQueueStopResponse;
    "keyboard.getRegisteredHotkeys": KeyboardGetRegisteredHotkeysResponse;
    "keyboard.registerHotkey": KeyboardRegisterHotkeyResponse;
    "keyboard.registerShortcut": KeyboardRegisterShortcutResponse;
    "keyboard.unregisterHotkey": KeyboardUnregisterHotkeyResponse;
    "library.addToPlaylist": LibraryAddToPlaylistResponse;
    "library.browseDirectory": LibraryBrowseDirectoryResponse;
    "library.browseTree": LibraryBrowseTreeResponse;
    "library.getAlbums": LibraryGetAlbumsResponse;
    "library.getAlbumTracks": LibraryGetAlbumTracksResponse;
    "library.getAll": LibraryGetAllResponse;
    "library.getArtistAlbums": LibraryGetArtistAlbumsResponse;
    "library.getArtists": LibraryGetArtistsResponse;
    "library.getArtistTracks": LibraryGetArtistTracksResponse;
    "library.getByPath": LibraryGetByPathResponse;
    "library.getCacheStats": LibraryGetCacheStatsResponse;
    "library.getCount": LibraryGetCountResponse;
    "library.getFieldValues": LibraryGetFieldValuesResponse;
    "library.getGenres": LibraryGetGenresResponse;
    "library.getRandomTracks": LibraryGetRandomTracksResponse;
    "library.getRecentlyAdded": LibraryGetRecentlyAddedResponse;
    "library.getRoots": LibraryGetRootsResponse;
    "library.getStats": LibraryGetStatsResponse;
    "library.getStatus": LibraryGetStatusResponse;
    "library.invalidateCache": LibraryInvalidateCacheResponse;
    "library.isEnabled": LibraryIsEnabledResponse;
    "library.query": LibraryQueryResponse;
    "library.refresh": LibraryRefreshResponse;
    "library.rescan": LibraryRescanResponse;
    "library.search": LibrarySearchResponse;
    "log.clear": LogClearResponse;
    "log.read": LogReadResponse;
    "log.write": LogWriteResponse;
    "lyrics.exists": LyricsExistsResponse;
    "lyrics.get": LyricsGetResponse;
    "lyrics.save": LyricsSaveResponse;
    "menu.close": MenuCloseResponse;
    "menu.getContextMenu": MenuGetContextMenuResponse;
    "menu.getMainMenu": MenuGetMainMenuResponse;
    "menu.runContextCommand": MenuRunContextCommandResponse;
    "menu.runContextCommandById": MenuRunContextCommandByIdResponse;
    "menu.runMainMenuCommand": MenuRunMainMenuCommandResponse;
    "menu.show": MenuShowResponse;
    "menu.showNativePopup": MenuShowNativePopupResponse;
    "metadata.embedArtwork": MetadataEmbedArtworkResponse;
    "metadata.read": MetadataReadResponse;
    "metadata.readBatch": MetadataReadBatchResponse;
    "metadata.readByPath": MetadataReadByPathResponse;
    "metadata.readRaw": MetadataReadRawResponse;
    "metadata.removeEmbeddedArt": MetadataRemoveEmbeddedArtResponse;
    "metadata.removeField": MetadataRemoveFieldResponse;
    "metadata.removeTag": MetadataRemoveTagResponse;
    "metadata.write": MetadataWriteResponse;
    "metadata.writeBatch": MetadataWriteBatchResponse;
    "misc.exit": MiscExitResponse;
    "misc.getComponentPath": MiscGetComponentPathResponse;
    "misc.getFoobarPath": MiscGetFoobarPathResponse;
    "misc.getProfilePath": MiscGetProfilePathResponse;
    "misc.restart": MiscRestartResponse;
    "misc.showConsole": MiscShowConsoleResponse;
    "misc.showLibrarySearch": MiscShowLibrarySearchResponse;
    "misc.showPopupMessage": MiscShowPopupMessageResponse;
    "misc.showPreferences": MiscShowPreferencesResponse;
    "output.getDevices": OutputGetDevicesResponse;
    "output.getEntries": OutputGetEntriesResponse;
    "output.getSettings": OutputGetSettingsResponse;
    "panel.getConfig": PanelGetConfigResponse;
    "panel.setConfig": PanelSetConfigResponse;
    "playback.getCurrentTrack": PlaybackGetCurrentTrackResponse;
    "playback.getCurrentTrackIndex": PlaybackGetCurrentTrackIndexResponse;
    "playback.getPlaybackOrder": PlaybackGetPlaybackOrderResponse;
    "playback.getPlayingPlaylist": PlaybackGetPlayingPlaylistResponse;
    "playback.getPosition": PlaybackGetPositionResponse;
    "playback.getState": PlaybackGetStateResponse;
    "playback.getStopAfterCurrent": PlaybackGetStopAfterCurrentResponse;
    "playback.getVolume": PlaybackGetVolumeResponse;
    "playback.mute": PlaybackMuteResponse;
    "playback.next": PlaybackNextResponse;
    "playback.pause": PlaybackPauseResponse;
    "playback.play": PlaybackPlayResponse;
    "playback.playOrPause": PlaybackPlayOrPauseResponse;
    "playback.playPath": PlaybackPlayPathResponse;
    "playback.playPaths": PlaybackPlayPathsResponse;
    "playback.playPause": PlaybackPlayPauseResponse;
    "playback.previous": PlaybackPreviousResponse;
    "playback.random": PlaybackRandomResponse;
    "playback.setPlaybackOrder": PlaybackSetPlaybackOrderResponse;
    "playback.setPosition": PlaybackSetPositionResponse;
    "playback.setStopAfterCurrent": PlaybackSetStopAfterCurrentResponse;
    "playback.setVolume": PlaybackSetVolumeResponse;
    "playback.stop": PlaybackStopResponse;
    "playback.toggleMute": PlaybackToggleMuteResponse;
    "playback.toggleStopAfterCurrent": PlaybackToggleStopAfterCurrentResponse;
    "playback.volumeDown": PlaybackVolumeDownResponse;
    "playback.volumeUp": PlaybackVolumeUpResponse;
    "playcount.get": PlaycountGetResponse;
    "playcount.getBatch": PlaycountGetBatchResponse;
    "playcount.getStats": PlaycountGetStatsResponse;
    "playcount.set": PlaycountSetResponse;
    "playlist.addHandles": PlaylistAddHandlesResponse;
    "playlist.addPaths": PlaylistAddPathsResponse;
    "playlist.addPathsAsync": PlaylistAddPathsAsyncResponse;
    "playlist.addPathsSequential": PlaylistAddPathsSequentialResponse;
    "playlist.clear": PlaylistClearResponse;
    "playlist.convertToAutoplaylist": PlaylistConvertToAutoplaylistResponse;
    "playlist.create": PlaylistCreateResponse;
    "playlist.createAutoplaylist": PlaylistCreateAutoplaylistResponse;
    "playlist.deselectAll": PlaylistDeselectAllResponse;
    "playlist.duplicate": PlaylistDuplicateResponse;
    "playlist.focusTrack": PlaylistFocusTrackResponse;
    "playlist.getActive": PlaylistGetActiveResponse;
    "playlist.getAll": PlaylistGetAllResponse;
    "playlist.getAutoplaylistInfo": PlaylistGetAutoplaylistInfoResponse;
    "playlist.getAutoplaylistQuery": PlaylistGetAutoplaylistQueryResponse;
    "playlist.getAvailableColumns": PlaylistGetAvailableColumnsResponse;
    "playlist.getCount": PlaylistGetCountResponse;
    "playlist.getFocusedTrack": PlaylistGetFocusedTrackResponse;
    "playlist.getFocusTrack": PlaylistGetFocusTrackResponse;
    "playlist.getLockInfo": PlaylistGetLockInfoResponse;
    "playlist.getPlaying": PlaylistGetPlayingResponse;
    "playlist.getSelectedTracks": PlaylistGetSelectedTracksResponse;
    "playlist.getSelection": PlaylistGetSelectionResponse;
    "playlist.getTrackCount": PlaylistGetTrackCountResponse;
    "playlist.getTracks": PlaylistGetTracksResponse;
    "playlist.insertTracks": PlaylistInsertTracksResponse;
    "playlist.isAutoplaylist": PlaylistIsAutoplaylistResponse;
    "playlist.isLocked": PlaylistIsLockedResponse;
    "playlist.moveTracks": PlaylistMoveTracksResponse;
    "playlist.playTrack": PlaylistPlayTrackResponse;
    "playlist.redo": PlaylistRedoResponse;
    "playlist.remove": PlaylistRemoveResponse;
    "playlist.removeAutoplaylist": PlaylistRemoveAutoplaylistResponse;
    "playlist.removeSelectedTracks": PlaylistRemoveSelectedTracksResponse;
    "playlist.removeTracks": PlaylistRemoveTracksResponse;
    "playlist.rename": PlaylistRenameResponse;
    "playlist.reorder": PlaylistReorderResponse;
    "playlist.reorderPlaylists": PlaylistReorderPlaylistsResponse;
    "playlist.replaceAllAndPlay": PlaylistReplaceAllAndPlayResponse;
    "playlist.reverse": PlaylistReverseResponse;
    "playlist.selectAll": PlaylistSelectAllResponse;
    "playlist.setActive": PlaylistSetActiveResponse;
    "playlist.setFocusedTrack": PlaylistSetFocusedTrackResponse;
    "playlist.setSelection": PlaylistSetSelectionResponse;
    "playlist.shuffle": PlaylistShuffleResponse;
    "playlist.sort": PlaylistSortResponse;
    "playlist.undo": PlaylistUndoResponse;
    "port.connect": PortConnectResponse;
    "port.disconnect": PortDisconnectResponse;
    "port.getPorts": PortGetPortsResponse;
    "port.postMessage": PortPostMessageResponse;
    "port.postMessageTo": PortPostMessageToResponse;
    "queue.add": QueueAddResponse;
    "queue.addPaths": QueueAddPathsResponse;
    "queue.clear": QueueClearResponse;
    "queue.flush": QueueFlushResponse;
    "queue.get": QueueGetResponse;
    "queue.getCount": QueueGetCountResponse;
    "queue.moveToTop": QueueMoveToTopResponse;
    "queue.remove": QueueRemoveResponse;
    "rating.get": RatingGetResponse;
    "rating.set": RatingSetResponse;
    "replaygain.clear": ReplaygainClearResponse;
    "replaygain.get": ReplaygainGetResponse;
    "replaygain.getMode": ReplaygainGetModeResponse;
    "replaygain.getPreamp": ReplaygainGetPreampResponse;
    "replaygain.getSettings": ReplaygainGetSettingsResponse;
    "replaygain.scan": ReplaygainScanResponse;
    "replaygain.setMode": ReplaygainSetModeResponse;
    "replaygain.setPreamp": ReplaygainSetPreampResponse;
    "selection.get": SelectionGetResponse;
    "selection.getType": SelectionGetTypeResponse;
    "selection.getViewerMode": SelectionGetViewerModeResponse;
    "selection.getViewingTrack": SelectionGetViewingTrackResponse;
    "selection.set": SelectionSetResponse;
    "selection.setPlaylistTracking": SelectionSetPlaylistTrackingResponse;
    "shell.exec": ShellExecResponse;
    "shell.openExternal": ShellOpenExternalResponse;
    "shell.openWith": ShellOpenWithResponse;
    "shell.showInExplorer": ShellShowInExplorerResponse;
    "shell.spawn": ShellSpawnResponse;
    "state.delete": StateDeleteResponse;
    "state.get": StateGetResponse;
    "state.keys": StateKeysResponse;
    "state.set": StateSetResponse;
    "system.getApisByNamespace": SystemGetApisByNamespaceResponse;
    "system.getApiStats": SystemGetApiStatsResponse;
    "system.getDPI": SystemGetDPIResponse;
    "system.getLocale": SystemGetLocaleResponse;
    "system.getRegisteredPlugins": SystemGetRegisteredPluginsResponse;
    "system.getTheme": SystemGetThemeResponse;
    "system.isPluginRegistered": SystemIsPluginRegisteredResponse;
    "system.listAvailableApis": SystemListAvailableApisResponse;
    "system.searchApis": SystemSearchApisResponse;
    "taskbar.flash": TaskbarFlashResponse;
    "taskbar.setOverlayIcon": TaskbarSetOverlayIconResponse;
    "taskbar.setProgress": TaskbarSetProgressResponse;
    "taskbar.setThumbnailButtons": TaskbarSetThumbnailButtonsResponse;
    "taskbar.updateButton": TaskbarUpdateButtonResponse;
    "test.echo": TestEchoResponse;
    "test.ping": TestPingResponse;
    "titleformat.eval": TitleformatEvalResponse;
    "titleformat.evalBatch": TitleformatEvalBatchResponse;
    "titleformat.evalFields": TitleformatEvalFieldsResponse;
    "titleformat.evalFieldsBatch": TitleformatEvalFieldsBatchResponse;
    "titleformat.getBuiltinFields": TitleformatGetBuiltinFieldsResponse;
    "tray.appendMenuItems": TrayAppendMenuItemsResponse;
    "tray.clearMenuItems": TrayClearMenuItemsResponse;
    "tray.create": TrayCreateResponse;
    "tray.destroy": TrayDestroyResponse;
    "tray.getMenuItems": TrayGetMenuItemsResponse;
    "tray.isVisible": TrayIsVisibleResponse;
    "tray.removeMenuItems": TrayRemoveMenuItemsResponse;
    "tray.setCloseToTray": TraySetCloseToTrayResponse;
    "tray.setContextMenu": TraySetContextMenuResponse;
    "tray.setIcon": TraySetIconResponse;
    "tray.setMenuItemState": TraySetMenuItemStateResponse;
    "tray.setMinimizeToTray": TraySetMinimizeToTrayResponse;
    "tray.setTooltip": TraySetTooltipResponse;
    "tray.showBalloon": TrayShowBalloonResponse;
    "ui.hideNotification": UiHideNotificationResponse;
    "ui.showContextMenu": UiShowContextMenuResponse;
    "ui.showCustomMenu": UiShowCustomMenuResponse;
    "ui.showNotification": UiShowNotificationResponse;
    "ui.showToast": UiShowToastResponse;
    "window.blur": WindowBlurResponse;
    "window.broadcast": WindowBroadcastResponse;
    "window.cancelClose": WindowCancelCloseResponse;
    "window.center": WindowCenterResponse;
    "window.clearClickThroughExcludeRegions": WindowClearClickThroughExcludeRegionsResponse;
    "window.clearDragRegions": WindowClearDragRegionsResponse;
    "window.clearNoDragRegions": WindowClearNoDragRegionsResponse;
    "window.close": WindowCloseResponse;
    "window.closeAllPopups": WindowCloseAllPopupsResponse;
    "window.closePopup": WindowClosePopupResponse;
    "window.confirmClose": WindowConfirmCloseResponse;
    "window.createPopup": WindowCreatePopupResponse;
    "window.enterFullscreen": WindowEnterFullscreenResponse;
    "window.exitFullscreen": WindowExitFullscreenResponse;
    "window.flash": WindowFlashResponse;
    "window.flashTaskbar": WindowFlashTaskbarResponse;
    "window.focus": WindowFocusResponse;
    "window.getAllWindows": WindowGetAllWindowsResponse;
    "window.getBackdropPolicy": WindowGetBackdropPolicyResponse;
    "window.getBounds": WindowGetBoundsResponse;
    "window.getCaptionButtonsWidth": WindowGetCaptionButtonsWidthResponse;
    "window.getCornerPreference": WindowGetCornerPreferenceResponse;
    "window.getCurrentWindowId": WindowGetCurrentWindowIdResponse;
    "window.getDevServerConfig": WindowGetDevServerConfigResponse;
    "window.getDpiScale": WindowGetDpiScaleResponse;
    "window.getMaxSize": WindowGetMaxSizeResponse;
    "window.getMinSize": WindowGetMinSizeResponse;
    "window.getMode": WindowGetModeResponse;
    "window.getPopupBehavior": WindowGetPopupBehaviorResponse;
    "window.getState": WindowGetStateResponse;
    "window.getTitle": WindowGetTitleResponse;
    "window.getTitlebarHeight": WindowGetTitlebarHeightResponse;
    "window.getTitlebarInfo": WindowGetTitlebarInfoResponse;
    "window.getZoom": WindowGetZoomResponse;
    "window.hasSavedBounds": WindowHasSavedBoundsResponse;
    "window.isAlwaysOnTop": WindowIsAlwaysOnTopResponse;
    "window.isClickThrough": WindowIsClickThroughResponse;
    "window.isFullscreen": WindowIsFullscreenResponse;
    "window.isMaximized": WindowIsMaximizedResponse;
    "window.isMinimized": WindowIsMinimizedResponse;
    "window.isResizable": WindowIsResizableResponse;
    "window.maximize": WindowMaximizeResponse;
    "window.minimize": WindowMinimizeResponse;
    "window.refreshWebView": WindowRefreshWebViewResponse;
    "window.reload": WindowReloadResponse;
    "window.resetZoom": WindowResetZoomResponse;
    "window.restore": WindowRestoreResponse;
    "window.sendMessage": WindowSendMessageResponse;
    "window.setAcrylic": WindowSetAcrylicResponse;
    "window.setAlwaysOnTop": WindowSetAlwaysOnTopResponse;
    "window.setBackdropPolicy": WindowSetBackdropPolicyResponse;
    "window.setBackgroundTransparency": WindowSetBackgroundTransparencyResponse;
    "window.setBlur": WindowSetBlurResponse;
    "window.setBounds": WindowSetBoundsResponse;
    "window.setClickThrough": WindowSetClickThroughResponse;
    "window.setClickThroughExcludeRegions": WindowSetClickThroughExcludeRegionsResponse;
    "window.setCornerPreference": WindowSetCornerPreferenceResponse;
    "window.setDarkMode": WindowSetDarkModeResponse;
    "window.setDevServerConfig": WindowSetDevServerConfigResponse;
    "window.setDragRegions": WindowSetDragRegionsResponse;
    "window.setFrameless": WindowSetFramelessResponse;
    "window.setFullscreen": WindowSetFullscreenResponse;
    "window.setMaxSize": WindowSetMaxSizeResponse;
    "window.setMica": WindowSetMicaResponse;
    "window.setMicaEffect": WindowSetMicaEffectResponse;
    "window.setMinSize": WindowSetMinSizeResponse;
    "window.setNoDragRegions": WindowSetNoDragRegionsResponse;
    "window.setPopupBehavior": WindowSetPopupBehaviorResponse;
    "window.setPosition": WindowSetPositionResponse;
    "window.setResizable": WindowSetResizableResponse;
    "window.setSize": WindowSetSizeResponse;
    "window.setTitle": WindowSetTitleResponse;
    "window.setTitlebarHeight": WindowSetTitlebarHeightResponse;
    "window.setZoom": WindowSetZoomResponse;
    "window.setZoomForDpi": WindowSetZoomForDpiResponse;
    "window.showSystemMenu": WindowShowSystemMenuResponse;
    "window.startDrag": WindowStartDragResponse;
    "window.startResize": WindowStartResizeResponse;
    "window.toggleAlwaysOnTop": WindowToggleAlwaysOnTopResponse;
    "window.toggleFullscreen": WindowToggleFullscreenResponse;
    "window.toggleMaximize": WindowToggleMaximizeResponse;
}

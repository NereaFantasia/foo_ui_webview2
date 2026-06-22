import type { TrackInfo } from '../responses.js';

/**
 * Per-track snapshot embedded in {@link MetadbChangedPayload}.`tracks`.
 * Only `path` is guaranteed; tag-derived fields are present when the
 * underlying metadb entry exposes them.
 */
export interface MetadbChangedTrackItem {
    /** Absolute filesystem path (native form). */
    path: string;
    /** Rating 0-5; 0 when unknown. Prefers `foo_playcount` over the file tag. */
    rating?: number;
    /** `PLAY_COUNT` tag value, when present. */
    playCount?: number;
    /** `TITLE` tag value, when present. */
    title?: string;
    /** `ARTIST` tag value, when present. */
    artist?: string;
}

/**
 * Payload for `metadb:changed`.
 *
 * `tracks` is capped at 50 entries; `count` reports the full affected set
 * which may exceed `tracks.length`.
 *
 * @codegen-override event:metadb:changed
 * @codegen-snapshot count:callexpr,fromHook:callexpr,timestamp:callexpr,tracks:callexpr
 */
export interface MetadbChangedPayload {
    /** Snapshot of affected tracks; capped at 50 entries. */
    tracks: MetadbChangedTrackItem[];
    /** Full affected-track count; may exceed `tracks.length` when capped. */
    count: number;
    /** `true` when the change originated from an internal hook such as `foo_playcount`. */
    fromHook: boolean;
    /** Emission time, ms since UTC epoch. */
    timestamp: number;
}

/**
 * Payload for `state:changed`. Narrow via the optional generic parameter
 * `T` to type the stored value.
 *
 * @codegen-override event:state:changed
 * @codegen-snapshot expiresAt:callexpr,key:primitive,previousValue:object,sourceWindowId:primitive,value:object
 */
export interface StateChangedPayload<T = unknown> {
    /** Logical key, namespaced via `'<namespace>:<id>'`. */
    key: string;
    /** Newly-applied value. */
    value: T;
    /** Previous value, or `null` when the key was newly created. */
    previousValue: T | null;
    /** Window-id of the writer (`'main'` for the host window). */
    sourceWindowId: string;
    /** Optional TTL, ms since UTC epoch. Treat `null` and missing as equivalent. */
    expiresAt?: number;
}

/**
 * Payload for `state:deleted`.
 *
 * @codegen-override event:state:deleted
 * @codegen-snapshot key:unknown,reason:primitive,sourceWindowId:primitive
 */
export interface StateDeletedPayload {
    /** Logical key that was removed. */
    key: string;
    /** Window-id that initiated the delete (`'main'` for the host). */
    sourceWindowId: string;
    /** `'deleted'` for explicit removal, `'expired'` for TTL-triggered eviction. */
    reason: 'deleted' | 'expired';
}

/**
 * Track snapshot published with `playback:trackChanged`, `playback:edited`
 * and `playback:itemPlayed`.
 *
 * `id`, `title`, `artist`, `album`, `duration`, `path`, `absolutePath` and
 * `subsong` are always emitted (titles fall back to empty strings when the
 * tag is missing). The remaining fields are emitted only when the host
 * exposes a valid info container; treat them as optional.
 *
 * @codegen-override event:playback:trackChanged
 * @codegen-snapshot
 */
export interface PlaybackTrackChangedPayload {
    id: string;
    title: string;
    artist: string;
    album: string;
    duration: number;
    /** Original (unnormalised) foobar path. */
    path: string;
    /** Normalised absolute path (no `|subsong:N` suffix). */
    absolutePath: string;
    subsong: number;
    albumArtist?: string;
    genre?: string;
    date?: string;
    /** `tracknumber` tag as int (0 when missing). */
    trackNumber?: number;
    /** `discnumber` tag as int (0 when missing). */
    discNumber?: number;
    /** `absolutePath` + `|subsong:N` when `subsong > 0`. */
    fullPath?: string;
    /** Filesystem size in bytes. */
    fileSize?: number;
    /** Audio bitrate in kbps. */
    bitrate?: number;
    /** Audio sample rate in Hz. */
    sampleRate?: number;
    /** Channel count. */
    channels?: number;
    /** Codec id string. */
    codec?: string;
}

/**
 * Payload for `playback:edited` - same shape as
 * {@link PlaybackTrackChangedPayload}.
 *
 * @codegen-override event:playback:edited
 * @codegen-snapshot
 */
export type PlaybackEditedPayload = PlaybackTrackChangedPayload;

/**
 * Payload for `playback:itemPlayed` - same shape as
 * {@link PlaybackTrackChangedPayload}.
 *
 * @codegen-override event:playback:itemPlayed
 * @codegen-snapshot
 */
export type PlaybackItemPlayedPayload = PlaybackTrackChangedPayload;

/**
 * Payload for `playback:stateChanged`.
 *
 * `position` and `duration` are in seconds.
 *
 * @codegen-override event:playback:stateChanged
 * @codegen-snapshot duration:primitive,position:primitive,state:unknown
 */
export interface PlaybackStateChangedPayload {
    state: 'playing' | 'paused' | 'stopped';
    position?: number;
    duration?: number;
}

/**
 * Payload for `playback:stopped`.
 *
 * @codegen-override event:playback:stopped
 * @codegen-snapshot reason:primitive
 */
export interface PlaybackStoppedPayload {
    reason:
        | 'user'
        | 'eof'
        | 'starting_another'
        | 'shutting_down'
        | 'unknown';
}

/**
 * Payload for `playback:starting`.
 *
 * @codegen-override event:playback:starting
 * @codegen-snapshot command:primitive,paused:primitive
 */
export interface PlaybackStartingPayload {
    command: 'default' | 'play' | 'next' | 'prev' | 'random';
    paused?: boolean;
}

/**
 * Payload for `playback:queueChanged`.
 *
 * @codegen-override event:playback:queueChanged
 * @codegen-snapshot origin:primitive
 */
export interface PlaybackQueueChangedPayload {
    origin:
        | 'user_added'
        | 'user_removed'
        | 'playback_advance'
        | 'unknown';
}

/**
 * Payload for `playback:dynamicInfo`.
 *
 * @codegen-override event:playback:dynamicInfo
 * @codegen-snapshot bitrate:callexpr,streamTitle:callexpr
 */
export interface PlaybackDynamicInfoPayload {
    bitrate: number;
    streamTitle?: string;
}

/**
 * Payload for `playback:dynamicInfoTrack`.
 *
 * @codegen-override event:playback:dynamicInfoTrack
 * @codegen-snapshot artist:callexpr,title:callexpr
 */
export interface PlaybackDynamicInfoTrackPayload {
    artist?: string;
    title?: string;
}

/**
 * Payload for `playback:stopAfterCurrentChanged`.
 *
 * @codegen-override event:playback:stopAfterCurrentChanged
 * @codegen-snapshot
 */
export interface PlaybackStopAfterCurrentChangedPayload {
    enabled: boolean;
}

/**
 * Payload for `playback:followCursorChanged`.
 *
 * @codegen-override event:playback:followCursorChanged
 * @codegen-snapshot
 */
export interface PlaybackFollowCursorChangedPayload {
    enabled: boolean;
}

/**
 * Payload for `playback:cursorFollowChanged`.
 *
 * @codegen-override event:playback:cursorFollowChanged
 * @codegen-snapshot
 */
export interface PlaybackCursorFollowChangedPayload {
    enabled: boolean;
}

/**
 * Payload for `audio:spectrum` and the push-mode stream of
 * `audio.subscribeSpectrum`.
 *
 * @codegen-override event:audio:spectrum
 * @codegen-snapshot
 */
export interface AudioSpectrumPayload {
    spectrum: number[];
    /** Effective FFT size used by the host (echo of the subscription). */
    fftSize?: number;
    /** Effective number of frequency bands (echo of the subscription). */
    bands?: number;
}

/**
 * Payload for `audio:fullWaveformReady`.
 *
 * @codegen-override event:audio:fullWaveformReady
 * @codegen-snapshot
 */
export interface AudioFullWaveformReadyPayload {
    taskId: string;
    waveform: number[];
    path: string;
    /** Sample resolution (number of waveform points). */
    resolution: number;
    /** Aggregation method used for each waveform point. */
    method: 'peak' | 'rms';
    /** Source file size in bytes. */
    fileSize?: number;
    /** Cache key under which the waveform was stored. */
    cacheKey?: string;
}

/**
 * Payload for `audio:fullWaveformFailed`. `code` is an `ApiErrorCode`
 * value; see `responses.ts`.
 *
 * @codegen-override event:audio:fullWaveformFailed
 * @codegen-snapshot
 */
export interface AudioFullWaveformFailedPayload {
    taskId: string;
    path: string;
    error: string;
    code: number;
}

/**
 * Payload for `audio:replaygainModeChanged`. `mode` is the numeric mode id
 * (0=none, 1=track, 2=album, ...).
 *
 * @codegen-override event:audio:replaygainModeChanged
 * @codegen-snapshot mode:unknown
 */
export interface AudioReplaygainModeChangedPayload {
    mode: number;
}

/**
 * Payload for `audio:outputDeviceChanged` (no fields).
 *
 * @codegen-override event:audio:outputDeviceChanged
 * @codegen-snapshot
 */
export type AudioOutputDeviceChangedPayload = Record<string, never>;

/**
 * Payload for `metadata:writeComplete`.
 *
 * @codegen-override event:metadata:writeComplete
 * @codegen-snapshot code:unknown,operation:primitive,path:primitive,status:unknown,subsong:primitive,success:primitive
 */
export interface MetadataWriteCompletePayload {
    /** Operation label, e.g. `"write"` or `"remove"`. */
    operation: string;
    /** Absolute file path the operation targeted. */
    path: string;
    /** Subsong index within the target file. */
    subsong: number;
    /** Raw completion code (0 = success). */
    code: number;
    /** Convenience flag; equals `code === 0`. */
    success: boolean;
    status: 'success' | 'aborted' | 'error';
}

/**
 * Payload for `library:itemsAdded`.
 *
 * @codegen-override event:library:itemsAdded
 * @codegen-snapshot count:callexpr,timestamp:primitive
 */
export interface LibraryItemsAddedPayload {
    count: number;
    /** int64 - may lose precision above 2^53. */
    timestamp: number;
}

/**
 * Payload for `library:itemsRemoved`.
 *
 * @codegen-override event:library:itemsRemoved
 * @codegen-snapshot count:callexpr,timestamp:primitive
 */
export interface LibraryItemsRemovedPayload {
    count: number;
    /** int64 - may lose precision above 2^53. */
    timestamp: number;
}

/**
 * Payload for `library:itemsModified`.
 *
 * @codegen-override event:library:itemsModified
 * @codegen-snapshot count:callexpr,timestamp:primitive
 */
export interface LibraryItemsModifiedPayload {
    count: number;
    /** int64 - may lose precision above 2^53. */
    timestamp: number;
}

/**
 * Payload for `library:getAllResult`.
 *
 * Emitted when `library.getAll` runs asynchronously: the call first resolves
 * with `{ pending: true, requestId }`, then this event fires once the result
 * is ready. Filter on `requestId` to match the originating call. The shape
 * mirrors the synchronous `library.getAll` response (`tracks` / `items` /
 * `total` / `offset` / `limit` / `fromCache`) plus the correlation
 * `requestId`; `error` is present only when the result could not be built.
 *
 * @codegen-override event:library:getAllResult
 * @codegen-snapshot
 */
export interface LibraryGetAllResultPayload {
    /** Correlation id echoed from the synchronous `{ pending, requestId }`. */
    requestId: string;
    tracks: TrackInfo[];
    /** Compatibility alias, identical to `tracks`. */
    items: TrackInfo[];
    total: number;
    offset?: number;
    limit?: number;
    /** Always `false` for the worker-built result. */
    fromCache?: boolean;
    /** Present only when the worker failed to build the payload. */
    error?: string;
}

/**
 * Payload for `http:response`.
 *
 * `body` is always a string at the transport layer. When `responseType`
 * is `'base64'` the host has encoded a binary payload and the caller
 * must decode the body. `http.get` / `http.request` decode automatically
 * when invoked with `responseType: 'arraybuffer'` or `'binary'`.
 *
 * @codegen-override event:http:response
 * @codegen-snapshot 
 */
export interface HttpResponsePayload {
    requestId: string;
    success: boolean;
    status: number;
    body: string;
    headers: Record<string, string>;
    error?: string;
    /** Actual transport encoding used by the host (`'text'` or `'base64'`). */
    responseType?: 'text' | 'base64';
}

/**
 * Payload for `http:downloadComplete`.
 *
 * @codegen-override event:http:downloadComplete
 * @codegen-snapshot 
 */
export interface HttpDownloadCompletePayload {
    requestId: string;
    success: boolean;
    status?: number;
    bytesWritten?: number;
    path?: string;
    error?: string;
    cancelled?: boolean;
}

/**
 * Payload for `jitQueue:error`. Exactly one of `url` / `path` is present
 * depending on the source of the failed track.
 *
 * @codegen-override event:jitQueue:error
 * @codegen-snapshot error:primitive,path:primitive,trackId:primitive,url:primitive
 */
export interface JitQueueErrorPayload {
    trackId: string;
    error: string;
    /** Original URL when the track came from a remote source. */
    url?: string;
    /** Local filesystem path when the track came from a local source. */
    path?: string;
}

/**
 * Payload for `jitQueue:preloadComplete`.
 *
 * @codegen-override event:jitQueue:preloadComplete
 * @codegen-snapshot count:callexpr,replace:primitive,startIndex:primitive
 */
export interface JitQueuePreloadCompletePayload {
    count: number;
    /** int64 - may lose precision above 2^53. */
    startIndex: number;
    replace: boolean;
}

/**
 * Payload for `panel:focus`.
 *
 * @codegen-override event:panel:focus
 * @codegen-snapshot
 */
export interface PanelFocusPayload {
    windowId: string;
}

/**
 * Payload for `panel:blur`.
 *
 * @codegen-override event:panel:blur
 * @codegen-snapshot
 */
export interface PanelBlurPayload {
    windowId: string;
}

/**
 * Payload for `panel:initialized`. `mode` distinguishes Default-UI panels
 * from Columns-UI panels.
 *
 * @codegen-override event:panel:initialized
 * @codegen-snapshot mode:primitive,panelMode:primitive,windowId:primitive
 */
export interface PanelInitializedPayload {
    mode: 'dui' | 'cui';
    panelMode: boolean;
    windowId: string;
}

/**
 * Payload for `panel:visibilityChanged`.
 *
 * @codegen-override event:panel:visibilityChanged
 * @codegen-snapshot darkMode:primitive
 */
export interface PanelVisibilityChangedPayload {
    visible: boolean;
}

/**
 * Payload for `ui:toast`.
 *
 * @codegen-override event:ui:toast
 * @codegen-snapshot duration:primitive,message:primitive,position:primitive,type:primitive
 */
export interface UiToastPayload {
    message: string;
    type: 'info' | 'success' | 'warning' | 'error';
    duration: number;
    position: string;
}

/**
 * Payload for `window:alwaysOnTopChanged`.
 *
 * @codegen-override event:window:alwaysOnTopChanged
 * @codegen-snapshot
 */
export interface WindowAlwaysOnTopChangedPayload {
    enabled: boolean;
}

/**
 * Payload for `plugin:registered`.
 *
 * @codegen-override event:plugin:registered
 * @codegen-snapshot
 */
export interface PluginRegisteredPayload {
    /** Display name. */
    name: string;
    /** Plugin namespace - unique identifier and API prefix. */
    namespace: string;
    version: string;
    author: string;
    description: string;
    /** Number of APIs registered under this plugin. */
    apiCount: number;
    /** Fully-qualified API names registered under this plugin. */
    apis: string[];
}

/**
 * Payload for `plugin:unregistered` - same shape as
 * {@link PluginRegisteredPayload}.
 *
 * @codegen-override event:plugin:unregistered
 * @codegen-snapshot
 */
export interface PluginUnregisteredPayload {
    name: string;
    namespace: string;
    version: string;
    author: string;
    description: string;
    apiCount: number;
    apis: string[];
}

/**
 * Payload for `api:registered`.
 *
 * @codegen-override event:api:registered
 * @codegen-snapshot
 */
export interface ApiRegisteredPayload {
    /** Fully-qualified name, e.g. `"playback.play"`. */
    fullName: string;
    /** Owning plugin's display name. */
    plugin: string;
    /** Owning plugin's namespace. */
    namespace: string;
    /** Bare method name (singular). */
    method: string;
    description: string;
    version: string;
    /** `true` for plugin-registered APIs, `false` for built-ins. */
    isExternal: boolean;
}

/**
 * Payload for `api:unregistered` - same shape as
 * {@link ApiRegisteredPayload}.
 *
 * @codegen-override event:api:unregistered
 * @codegen-snapshot
 */
export interface ApiUnregisteredPayload {
    fullName: string;
    plugin: string;
    namespace: string;
    method: string;
    description: string;
    version: string;
    isExternal: boolean;
}

/**
 * Payload for `app:beforeQuit` (no fields).
 *
 * @codegen-override event:app:beforeQuit
 * @codegen-snapshot
 */
export type AppBeforeQuitPayload = Record<string, never>;

/**
 * Payload for `tray:menuItemClicked`.
 *
 * Ordinary items and the now-playing card report `{ id }` and close the menu.
 * Rich controls report `{ id, value }` and keep the menu open: a `'rating'`
 * change reports `0..5`; a `'slider'` change reports the new integer value
 * within `[min, max]`.
 *
 * @codegen-override event:tray:menuItemClicked
 * @codegen-snapshot id:primitive
 */
export interface TrayMenuItemClickedPayload {
    id: string;
    /** Present only for rich `'rating'` / `'slider'` controls (see above). */
    value?: number;
}

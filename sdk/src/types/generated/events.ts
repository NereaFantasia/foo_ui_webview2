// ─────────────────────────────────────────────────────────────
// GENERATED FILE — DO NOT EDIT
// File: sdk/src/types/generated/events.ts
// Source: src/api/schema/*.ts
// Emitter: scripts/gen_sdk_types.mjs
// Regenerate: npm run gen:types (in sdk/) or `node scripts/gen_sdk_types.mjs --all`
// ─────────────────────────────────────────────────────────────

/* eslint-disable */

import type { JsonValue } from '../json.js';

import type { FileOpResultItem, LibraryTrack, MetadataProbeResultItem, MetadbChangedTrackItem, SystemApiInfo, SystemPluginInfo, Track, WindowPopupBehaviorState } from './schema-types.js';

/**
 * Payload of the `api:registered` event.
 * An external plugin registered a method, or replaced one it had registered before. `isExternal` is always `true`.
 */
export type ApiRegisteredPayload = SystemApiInfo;

/**
 * Payload of the `api:unregistered` event.
 * An external plugin removed one of its methods. Unregistering the whole plugin sends `plugin:unregistered` instead.
 */
export type ApiUnregisteredPayload = SystemApiInfo;

/**
 * Payload of the `app:beforeQuit` event.
 * foobar2000 is exiting. Sent once, before any window or panel of the plugin closes, to every page: the main window's, popups' and panels', whatever the interface and whether or not background mode is on. Best effort only: the event is queued to each page and the host does not wait. The pages close right after, so a handler may not run or finish, and a call it makes to the host is unlikely to be answered.
 */
export type AppBeforeQuitPayload = Record<string, never>;

/**
 * Payload of the `audio:dspPresetChanged` event.
 * foobar2000's playback DSP chain changed, for example a DSP was added or a preset was loaded.
 */
export type AudioDspPresetChangedPayload = Record<string, never>;

/**
 * Payload of the `audio:fullWaveformFailed` event.
 * An `audio.generateFullWaveform` request that answered `pending` ended without a waveform: decoding failed, or the request was cancelled with `audio.cancelFullWaveform`. Delivered like `audio:fullWaveformReady`.
 */
export interface AudioFullWaveformFailedPayload {
    /** Id from the `pending` answer. */
    taskId: string;
    /** The path the request gave. */
    path: string;
    /** What went wrong. */
    error: string;
    /** `CANCELLED` after `audio.cancelFullWaveform`; otherwise why decoding failed. */
    code: "CANCELLED" | "INVALID_HANDLE" | "NO_INFO" | "INVALID_PARAMS" | "DECODER_FAILED" | "DECODE_FAILED" | "UNKNOWN_ERROR";
}

/**
 * Payload of the `audio:fullWaveformReady` event.
 * An `audio.generateFullWaveform` request that answered `pending` finished decoding. Every request waiting on the same decode gets its own event, rendered with its own `method`, `scale` and `signed`. Sent to the page that made the request, or to a page under the same top-level window when that one cannot be found, or else to the main window's page.
 */
export interface AudioFullWaveformReadyPayload {
    /** Id from the `pending` answer. */
    taskId: string;
    /** The path the request gave. */
    path: string;
    /** One value per point, in `scale`; in `[-1, 1]` when `signed`, else in `[0, 1]`. */
    waveform: number[];
    /** The largest value of the selected sequence before normalisation, in linear full-scale units: with `linear`, `waveform[i] * maxAmplitude` is the absolute level; with `db`, dBFS is `(v * 60 - 60) + 20 * log10(maxAmplitude)`. */
    maxAmplitude: number;
    /** Track duration in seconds. */
    duration: number;
    /** Sample rate of the decoded track in Hz. */
    sampleRate: number;
    /** Channel count of the decoded track. */
    channels: number;
    /** Number of points. */
    resolution: number;
    /** How each point was aggregated, as requested. */
    method: "rms" | "peak";
    /** Scale of the points, as requested. */
    scale: "linear" | "db";
    /** The points keep PCM polarity, as requested. */
    signed: boolean;
    /** Always `false`: this event only follows a decode. */
    cached: boolean;
}

/**
 * Payload of the `audio:outputDeviceChanged` event.
 * foobar2000's output configuration changed, such as the output device.
 */
export type AudioOutputDeviceChangedPayload = Record<string, never>;

/**
 * Payload of the `audio:pcmFailed` event.
 * An `audio.decodePcm` task ended without samples: decoding failed, the page origin stopped being trusted, the buffer could not be allocated or posted, or the task was cancelled with `audio.cancelDecodePcm`. Delivered like `audio:pcmReady`.
 */
export interface AudioPcmFailedPayload {
    /** Id from the `audio.decodePcm` answer. */
    taskId: string;
    /** The path the request gave. */
    path: string;
    /** What went wrong. */
    error: string;
    /** `CANCELLED` after `audio.cancelDecodePcm`; `ORIGIN_DENIED` when the page origin stopped being trusted; `OPERATION_FAILED` when the buffer could not be allocated or posted; otherwise why decoding failed. `INVALID_PARAMS` means `start` is at or beyond the end of the track. */
    code: "CANCELLED" | "ORIGIN_DENIED" | "OPERATION_FAILED" | "INVALID_PARAMS" | "DECODER_FAILED" | "DECODE_FAILED" | "NOT_SUPPORTED" | "UNKNOWN_ERROR";
}

/**
 * Payload of the `audio:pcmReady` event.
 * An `audio.decodePcm` task finished. Its samples reach the page as a `sharedbufferreceived` event on `chrome.webview` with the same `taskId` in `additionalData`, before or after this event. Sent to the page that started the task, only while that page still shows the document that started it.
 */
export interface AudioPcmReadyPayload {
    /** Id from the `audio.decodePcm` answer. */
    taskId: string;
    /** The path the request gave. */
    path: string;
    /** Samples per second per channel. */
    sampleRate: number;
    /** Channel count in the buffer; `1` when the request set `mono`. */
    channels: number;
    /** Frames per channel in the buffer. */
    frames: number;
    /** Track time of the first frame, in seconds. */
    start: number;
    /** Track time right after the last frame, in seconds. */
    end: number;
    /** `frames / sampleRate`, in seconds. */
    duration: number;
    /** The audio ran past the size estimated from the track length, or its format changed partway; the buffer holds the part before the cut. */
    truncated: boolean;
    /** Converted to `sampleRate` with foobar2000's resampler. */
    resampled: boolean;
}

/**
 * Payload of the `audio:replaygainModeChanged` event.
 * A ReplayGain setting of foobar2000's playback changed, not only the source mode.
 */
export interface AudioReplaygainModeChangedPayload {
    /** The source mode after the change: `0` none, `1` track, `2` album, `3` by playback order. */
    mode: number;
}

/**
 * Payload of the `audio:spectrum` event.
 * A frame of an `audio.subscribeSpectrum` subscription, at the subscription's `fps` while playing, plus one silence frame when playback pauses or stops. The fields are those of an `audio.getSpectrum` answer for the subscription. Sent to the page that subscribed: by its window id, else by its window handle, else to a page under the same top-level window; when none is found the frame is dropped.
 */
export interface AudioSpectrumPayload {
    /** Subscription the frame belongs to; absent when the request named none. */
    subscriptionId?: string;
    /** What the frame carries. */
    output: "bands" | "bins";
    /** Band frames: one value per band in the frame's `scale`; `weighted` values lie in `[0, 1]`, `db` values have a floor of `-160`. Bin frames with `channels: 'mix'`: one power value in dB per bin. */
    spectrum?: number[];
    /** Bin frames with `channels: 'stereo'`: the first channel, one dB value per bin. */
    left?: number[];
    /** Bin frames with `channels: 'stereo'`: the second channel, or the first again for a mono stream. */
    right?: number[];
    /** Band frames: length of `spectrum`. */
    bands?: number;
    /** Bin frames: FFT bin index of the first value; value `i` is centred at `(firstBin + i) * sampleRate / fftSize` Hz. `0`, with empty arrays, when the range holds no bin. */
    firstBin?: number;
    /** FFT size actually used. */
    fftSize: number;
    /** Scale of the values; always `db` for bin frames. */
    scale: "weighted" | "db";
    /** Bin frames: the subscription's channel layout. */
    channels?: "mix" | "stereo";
    /** Bin frames: channel count of the visualization stream; `0` when unknown. */
    channelCount?: number;
    /** Sample rate of the visualization stream in Hz; `0` when unknown. */
    sampleRate: number;
    /** Lower edge of the frequency range in Hz. */
    minFrequency: number;
    /** Upper edge in Hz: the requested one capped at `sampleRate / 2`, or `sampleRate / 2` when none was given. */
    maxFrequency: number;
    /** Paused or stopped frames are silence. */
    state: "playing" | "paused" | "stopped";
    /** Seconds since the visualization stream last started; the frame is computed from the `fftSize` samples ending at this time. */
    streamTime: number;
    /** Host system time when the frame was computed, Unix epoch milliseconds. */
    hostTime: number;
}

/**
 * Payload of the `audio:stream` event.
 * The host stopped writing one epoch of an `audio.subscribeStream` buffer because the sample rate or channel count changed; the next epoch's buffer arrives as its own `sharedbufferreceived` event. Other endings (unsubscribe, page gone, host exit) are not reported this way: the buffer's `ended` flag is the signal. Sent to the page that subscribed.
 */
export interface AudioStreamPayload {
    /** Id from the `audio.subscribeStream` answer. */
    subscriptionId: string;
    /** Always `ended`. */
    type: "ended";
    /** The epoch that ended; its buffer has the `ended` flag set. */
    epoch: number;
    /** Always `format-change`. */
    reason: "format-change";
}

/**
 * Payload of the `cursor:hiddenChanged` event.
 * The calling window's cursor was hidden or restored by `cursor.setHidden`; a call that leaves the state as it was sends nothing.
 */
export interface CursorHiddenChangedPayload {
    /** Whether the cursor is hidden now. */
    hidden: boolean;
}

/**
 * Payload of the `dnd:capabilitiesChanged` event.
 * What the window's drag-drop integration can deliver changed after the page loaded, for instance when a navigation changes the document origin. The payload has the same fields as `dnd.getCapabilities` returns.
 */
export interface DndCapabilitiesChangedPayload {
    /** The page receives standard HTML5 drag events; `false` means the window has no drag-drop support at all. */
    html5: boolean;
    /** Real filesystem paths are obtainable through `dnd.getPathsAsync` and the `dnd:*` events. Not fixed for the window's lifetime: a navigation or Chromium re-registering its own drop target can withdraw it, announced by `dnd:capabilitiesChanged`. */
    paths: boolean;
    /** How the window hosts its WebView: `visual` for the main and popup windows, `standard` for a DUI or CUI panel. */
    hosting: "visual" | "standard";
    /** Why `paths` is `false`; its presence always means `paths` is `false`. */
    pathsUnavailableReason?: "register-failed" | "forward-unavailable" | "inner-target-not-found" | "chain-failed" | "displaced" | "origin-untrusted";
    /** Files can be dragged out of the window through `dnd.prepareDrag`. Independent of `paths`. */
    dragOut: boolean;
    /** Why `dragOut` is `false`; its presence always means `dragOut` is `false`. */
    dragOutUnavailableReason?: "not-visual-hosting" | "runtime-too-old" | "register-failed";
}

/**
 * Payload of the `dnd:dragEnded` event.
 * The host attached no files to a drag out of the window, decided at the moment the drag started. The drag itself goes on as an ordinary page drag without files, so a drop inside the page still works; the token text is blanked first, and in the rare case that it cannot be the drag is cancelled instead. It is not a completion notice: when the host accepts the token and hands the files over, nothing follows, and the page's own `dragend` reports the outcome through `dataTransfer.dropEffect` (`copy` when a target took the files, `none` when the drag was cancelled or refused). Only a drag that carried a drag token can cause it; a page dragging its own text, image or link never does.
 */
export interface DndDragEndedPayload {
    /** Always `failed`: the host reports only refusals. */
    result: "failed";
    /** Why no files were attached: `PERMISSION_DENIED` when the token was unknown, expired, already spent, superseded by a later `prepareDrag` or minted for another window; `INVALID_PARAMS` when `dataTransfer.effectAllowed` was not exactly `copy`, which `dnd.applyDragToken` sets correctly; `OPERATION_FAILED` when the file list could not be attached to the drag. */
    code: "PERMISSION_DENIED" | "INVALID_PARAMS" | "OPERATION_FAILED";
    /** Human-readable reason; never contains a filesystem path. */
    error: string;
}

/**
 * Payload of the `dnd:drop` event.
 * The drag was dropped on the window. A drop lands only where the page accepts it, by calling `preventDefault()` in its HTML5 `dragover` handler, and elsewhere only when released within half a second of entering, before the page has answered. `paths` is the final list, which the drag source may have changed since `dnd:enter`. The event can arrive before or after the page's own HTML5 `drop` handler runs; call `dnd.getPathsAsync` from that handler when the paths are needed together with the drop.
 */
export interface DndDropPayload {
    /** The drag's session, as in `dnd:enter`. */
    sessionId: string;
    /** Absolute filesystem paths, in `DataTransfer.files` order; empty when the drag carries no file list or the document origin is not trusted with paths. */
    paths: string[];
    /** Target of the `.lnk` shortcut at the same index of `paths`, or `null`; same length and same rules as the field of this name in `dnd:enter`. */
    resolvedPaths: Array<string | null>;
    /** Cursor x in client-area physical pixels. */
    x: number;
    /** Cursor y in client-area physical pixels. */
    y: number;
    /** Win32 modifier and mouse-button mask at drop time (`MK_*` flags), for a page that wants modifier-dependent behaviour. It does not change the drop effect reported to the drag source, which is never move or link. */
    keyState: number;
    /** Where the drag came from: `self` when it started in this window's page, `other-window` when it started in another window of the same foobar2000 (the main window or a popup), `external` for everything else, such as Explorer, another application, another foobar2000 process or a drag that started in a Default UI or Columns UI panel. The host reads it from a marker it adds to every drag that starts in a window it hosts; any program can imitate that marker, so treat the value as a hint about the drag's origin, not as a security check. */
    source: "self" | "other-window" | "external";
}

/**
 * Payload of the `dnd:enter` event.
 * A drag entered the window. Only the window under the cursor gets it, never another one, because real filesystem paths are sensitive. `paths` is empty when the drag carries no `CF_HDROP` file list (browser links, virtual shell objects, archive entries) or when the document origin is not trusted with real paths.
 */
export interface DndEnterPayload {
    /** Ties the `dnd:enter`, `dnd:leave` and `dnd:drop` of one drag together. Unique across the host process, so it also tells which window the drag belongs to. */
    sessionId: string;
    /** Absolute filesystem paths, in `DataTransfer.files` order; empty when the drag carries no file list or the document origin is not trusted with paths. */
    paths: string[];
    /** Target of the `.lnk` shortcut at the same index of `paths`, or `null`; always as long as `paths`, including when both are withheld and empty. Only `.lnk` is resolved: a shortcut to a shell object, a target too long to read back intact (Windows caps it at `MAX_PATH`), an unavailable COM apartment and an entry skipped to keep the drop responsive all report `null`, never an empty string. A broken shortcut reports the path it recorded, so a non-null entry says where the shortcut points, not that a file is there. */
    resolvedPaths: Array<string | null>;
    /** Whether the drag carries a `CF_HDROP` file list; reported truthfully even when `paths` is withheld, since it reveals nothing by itself. */
    hasFiles: boolean;
    /** Where the drag came from: `self` when it started in this window's page, `other-window` when it started in another window of the same foobar2000 (the main window or a popup), `external` for everything else, such as Explorer, another application, another foobar2000 process or a drag that started in a Default UI or Columns UI panel. The host reads it from a marker it adds to every drag that starts in a window it hosts; any program can imitate that marker, so treat the value as a hint about the drag's origin, not as a security check. */
    source: "self" | "other-window" | "external";
    /** Cursor x in client-area physical pixels; divide by `devicePixelRatio` for CSS pixels. */
    x: number;
    /** Cursor y in client-area physical pixels; divide by `devicePixelRatio` for CSS pixels. */
    y: number;
}

/**
 * Payload of the `dnd:leave` event.
 * The drag left the window without dropping.
 */
export interface DndLeavePayload {
    /** The drag's session, as in `dnd:enter`. */
    sessionId: string;
}

/**
 * Payload of the `file:opComplete` event.
 * The last event of a run, sent after a cancel and after failed entries too. It is skipped only when foobar2000 is shutting down or the worker fails unexpectedly, so a listener that cleans up on it should keep its own timeout. Afterwards the `operationId` is gone and `file.cancelOp` reports `cancelled: false` for it. Delivered like `file:opProgress`.
 */
export interface FileOpCompletePayload {
    /** Id from the receipt of the call that started the run. */
    operationId: string;
    /** Which operation: `copy`, `move` or `delete`. */
    op: "copy" | "move" | "delete";
    /** Entries the call accepted. */
    total: number;
    /** Entries reported `ok`. */
    successCount: number;
    /** Entries reported `skipped`. */
    skippedCount: number;
    /** Entries reported `failed`. */
    failureCount: number;
    /** `true` when at least one entry was reported with reason `cancelled`. */
    cancelled: boolean;
}

/**
 * Payload of the `file:opProgress` event.
 * Results of a `file.copyAsync`, `file.moveAsync` or `file.deleteAsync` run, in batches: a batch goes out once 64 results are pending or 100 ms have passed since the previous one, and the last partial batch arrives before `file:opComplete`. Sent to the page that started the run; when that window is gone by then, to the main window's page.
 */
export interface FileOpProgressPayload {
    /** Id from the receipt of the call that started the run. */
    operationId: string;
    /** Which operation: `copy`, `move` or `delete`. */
    op: "copy" | "move" | "delete";
    /** Entries reported so far across all batches, failures included. */
    done: number;
    /** Entries the call accepted. */
    total: number;
    /** The results of this batch only, in request order. */
    results: FileOpResultItem[];
}

/**
 * Payload of the `http:downloadComplete` event.
 * The outcome of `http.download` with `async: true`, carrying the receipt's `requestId`. On success it has the fields of a synchronous download; on failure `error` and `code`. Delivered like `http:response`.
 */
export interface HttpDownloadCompletePayload {
    /** Id from the receipt. */
    requestId: string;
    /** Whether the file was written. A response with an error status such as `404` is saved and counts as a success. */
    success: boolean;
    /** HTTP status code: on success that of the response; on failure only when a redirect was refused under `redirect: "error"`, that of the redirect. */
    status?: number;
    /** Bytes written; present on success. */
    bytesWritten?: number;
    /** The file written, with path variables expanded; present on success. */
    path?: string;
    /** Why the download failed; present on failure. */
    error?: string;
    /** The error code, as in `http:response`. Present on failure. */
    code?: "INVALID_PARAMS" | "PERMISSION_DENIED" | "CANCELLED" | "OPERATION_FAILED";
    /** `true` when the download was cancelled by `http.abort`; absent otherwise. */
    cancelled?: boolean;
}

/**
 * Payload of the `http:response` event.
 * The outcome of an async `http.get`, `http.post`, `http.put`, `http.delete`, `http.patch` or `http.head`, carrying the receipt's `requestId`. On success it has the fields of the synchronous response; on failure `error` and `code`, like a failed call. Sent to the page that made the request, or to a page under the same top-level window when that one cannot be found, or else to the main window's page.
 */
export interface HttpResponsePayload {
    /** Id from the receipt. */
    requestId: string;
    /** Whether a response came back. A response with an error status such as `404` is a success. */
    success: boolean;
    /** HTTP status code: on success that of the response; on failure only when a redirect was refused under `redirect: "error"`, that of the redirect. */
    status?: number;
    /** Response headers by name; of repeated headers the last one wins. Present on success. */
    headers?: Record<string, string>;
    /** Response body, as text or Base64 according to `responseType`; always empty for `http.head`. Present on success. */
    body?: string;
    /** `text` or `base64`, the encoding of `body`. Present on success. */
    responseType?: "text" | "base64";
    /** The `Content-Length` header as a number; present on success of `http.head` when the server sends one that parses. */
    contentLength?: number;
    /** Why the request failed; present on failure. */
    error?: string;
    /** The error code: `INVALID_PARAMS` for a URL that is not `http` or `https`, `PERMISSION_DENIED` for a local or private network address the host setting does not allow, `CANCELLED` after `http.abort`, `OPERATION_FAILED` otherwise. Present on failure. */
    code?: "INVALID_PARAMS" | "PERMISSION_DENIED" | "CANCELLED" | "OPERATION_FAILED";
    /** `true` when the request was cancelled by `http.abort`; absent otherwise. */
    cancelled?: boolean;
}

/**
 * Payload of the `jitQueue:error` event.
 * A track given to `jitQueue.playNow` or `jitQueue.enqueueNext` could not be added to the shadow playlist: its location resolved to no tracks, the shadow playlist was gone, or adding it failed with an exception. The track is added after the call has already succeeded, so the failure arrives only as this event. Delivered like `jitQueue:trackChanged`.
 */
export interface JitQueueErrorPayload {
    /** Identifier the call gave the track. */
    trackId: string;
    /** What went wrong, in English. */
    error: string;
    /** The location, when it is not a local file path. Exactly one of `url` and `path` is present. */
    url?: string;
    /** The location, when it is a local file path. */
    path?: string;
}

/**
 * Payload of the `jitQueue:listExhausted` event.
 * `jitQueue.notifyEmpty` was called, even with no session active, or playback of the shadow playlist reached the end of a track while the playlist held no tracks. The session is now `Exhausted`. Delivered like `jitQueue:trackChanged`.
 */
export interface JitQueueListExhaustedPayload {
    /** Identifier of the session's current track; empty when it has none. */
    lastTrackId: string;
}

/**
 * Payload of the `jitQueue:needNext` event.
 * The host needs the next track: none is buffered and no earlier request is waiting for an answer. It checks when a track of the shadow playlist starts, dropping a request left unanswered during the previous track, and again when 30 seconds of the track remain. The page answers with `jitQueue.enqueueNext`, or with `jitQueue.notifyEmpty` when it has no more tracks. Delivered like `jitQueue:trackChanged`.
 */
export interface JitQueueNeedNextPayload {
    /** Identifier of the session's current track; empty when it has none. */
    currentTrackId: string;
    /** Always `trackChange`. */
    reason: "trackChange";
}

/**
 * Payload of the `jitQueue:preloadComplete` event.
 * `jitQueue.preloadBatch` inserted its tracks into the shadow playlist. Sent before the call returns; a call that fails sends nothing. Delivered like `jitQueue:trackChanged`: a batch that starts playback reports to the page that called it, a batch appended to a playing session reports to that session's page.
 */
export interface JitQueuePreloadCompletePayload {
    /** How many tracks were inserted; the same as `tracksAdded` of the call. */
    count: number;
    /** `startIndex` of the call. */
    startIndex: number;
    /** `replace` of the call. */
    replace: boolean;
}

/**
 * Payload of the `jitQueue:trackChanged` event.
 * A track of the shadow playlist started playing, a replay of the current track included. `trackId` and `title` are the session's current track as `jitQueue.playNow` or `jitQueue.enqueueNext` named it. Tracks inserted by `jitQueue.preloadBatch` have no identifier: playing one leaves both unchanged, and both are empty once a batch has started playback. The JIT events go to the page that started the session, with `jitQueue.playNow` or with a `jitQueue.preloadBatch` that started playback; when that page is gone, to a page under the same top-level window, and else to the main window's page. A page whose session another page started anew hears nothing more of it. Before any session has started they go to the main window's page.
 */
export interface JitQueueTrackChangedPayload {
    /** Identifier of the session's current track; empty when it has none. */
    trackId: string;
    /** Title of the session's current track; empty when it has none or none was given. */
    title: string;
}

/**
 * Payload of the `keyboard:hotkey` event.
 * A hotkey registered with `keyboard.registerHotkey` was pressed. It goes to the window that registered it. A hotkey stays registered after that window closes; its presses then go to the main window's page.
 */
export interface KeyboardHotkeyPayload {
    /** The id `keyboard.registerHotkey` returned. */
    id: number;
    /** The combination as it was registered. */
    key: string;
    /** The action name given at registration. */
    action: string;
}

/**
 * Payload of the `library:getAllResult` event.
 * The page of a `library.getAll` that answered `{ pending: true, requestId }`: the whole library in library order, built off the main thread. The list is kept for later calls only when the library did not change meanwhile. Sent to the page that made the call, or to a page under the same top-level window when that one cannot be found, or else to the main window's page.
 */
export interface LibraryGetAllResultPayload {
    /** Id from the `{ pending: true, requestId }` answer. */
    requestId: string;
    /** The whole library in library order; `index` is the position in the library. Empty when `error` is present. */
    tracks: LibraryTrack[];
    /** The same list as `tracks`. */
    items: LibraryTrack[];
    /** Tracks in the library when the call was made; `0` when `error` is present. */
    total: number;
    /** The `offset` of the call, always `0`. */
    offset: number;
    /** The `limit` of the call. */
    limit: number;
    /** Always `false`. */
    fromCache: boolean;
    /** Why the list could not be built; present only then. */
    error?: string;
}

/**
 * Payload of the `library:initialized` event.
 * foobar2000 reported that the Media Library has finished initializing. The host drops its library caches before sending it. Every window receives it; a page that loads afterwards does not.
 */
export interface LibraryInitializedPayload {
    /** When the host sent it, in milliseconds since the Unix epoch. */
    timestamp: number;
}

/**
 * Payload of the `library:itemsAdded` event.
 * Tracks were added to the Media Library. The host drops its library caches before sending it.
 */
export interface LibraryItemsAddedPayload {
    /** How many tracks. */
    count: number;
    /** When the host sent it, in milliseconds since the Unix epoch. */
    timestamp: number;
}

/**
 * Payload of the `library:itemsModified` event.
 * Tracks in the Media Library were modified, for example by a tag edit. The host drops its library caches before sending it.
 */
export interface LibraryItemsModifiedPayload {
    /** How many tracks. */
    count: number;
    /** When the host sent it, in milliseconds since the Unix epoch. */
    timestamp: number;
}

/**
 * Payload of the `library:itemsRemoved` event.
 * Tracks were removed from the Media Library. The host drops its library caches before sending it.
 */
export interface LibraryItemsRemovedPayload {
    /** How many tracks. */
    count: number;
    /** When the host sent it, in milliseconds since the Unix epoch. */
    timestamp: number;
}

/**
 * Payload of the `menu:dismiss` event.
 * A menu opened with `menu.show` closed, whether or not a row was chosen. Delivered like `menu:select`, also when another page's `menu.show` or `menu.close` closed the menu.
 */
export interface MenuDismissPayload {
    /** Id of the menu, as `menu.show` returned it. */
    menuId: string;
    /** Why the menu closed: `select` after a row was chosen, `outside` for a click outside it, `escape`, `blur` when another window took the focus, `replaced` when another menu opened, `timeout` when the menu page did not answer in time, or the `reason` given to `menu.close`. */
    reason: string;
}

/**
 * Payload of the `menu:select` event.
 * A row of a menu opened with `menu.show` was chosen; `menu:dismiss` with reason `select` follows as the menu closes. Sent to the page that called `menu.show`, or to a page under the same top-level window when that one cannot be found, or else to the main window's page.
 */
export interface MenuSelectPayload {
    /** Id of the menu, as `menu.show` returned it. */
    menuId: string;
    /** `id` of the chosen row. */
    itemId: string;
}

/**
 * Payload of the `menu:valueChanged` event.
 * A rating, slider or segmented control in a menu opened with `menu.show` changed value; the menu stays open. Delivered like `menu:select`.
 */
export interface MenuValueChangedPayload {
    /** Id of the menu, as `menu.show` returned it. */
    menuId: string;
    /** `id` of the control's row. */
    itemId: string;
    /** The control's new value. */
    value: number;
}

/**
 * Payload of the `metadata:probeComplete` event.
 * The last event of a probe, sent after a cancel and after a failed run too; only an exception while sending it, in practice running out of memory, skips it. Afterwards the `operationId` is gone and `metadata.cancelProbe` reports `cancelled: false` for it. Delivered like `metadata:probeProgress`.
 */
export interface MetadataProbeCompletePayload {
    /** Id from the `metadata.probeBatchAsync` receipt. */
    operationId: string;
    /** Paths the call accepted. */
    total: number;
    /** Paths reported with `success: true`. */
    successCount: number;
    /** Paths reported with `success: false`. */
    failureCount: number;
    /** `true` when the probe was stopped early, by `metadata.cancelProbe` or because the popup that started it closed; the two counts then fall short of `total`. */
    cancelled: boolean;
}

/**
 * Payload of the `metadata:probeProgress` event.
 * Results of a `metadata.probeBatchAsync` run, in batches: a batch goes out once 64 results are pending or 100 ms have passed since the previous one, and the last partial batch arrives before `metadata:probeComplete`. A path the run was cancelled on or never reached is not reported. Sent to the page that started the probe; when that window is gone by then, to the main window's page.
 */
export interface MetadataProbeProgressPayload {
    /** Id from the `metadata.probeBatchAsync` receipt. */
    operationId: string;
    /** Paths reported so far across all batches, failures included. */
    done: number;
    /** Paths the call accepted, its `totalCount`. */
    total: number;
    /** The results of this batch only, in request order. */
    results: MetadataProbeResultItem[];
}

/**
 * Payload of the `metadata:writeComplete` event.
 * A tag write queued by `metadata.write`, `metadata.writeBatch`, `metadata.removeTag` or `metadata.removeField` finished. Every window receives it.
 */
export interface MetadataWriteCompletePayload {
    /** `write` for `metadata.write` and `metadata.writeBatch`, `removeTag` for `metadata.removeTag` and `metadata.removeField`. */
    operation: "write" | "removeTag";
    /** The path as the call gave it, suffix included. */
    path: string;
    /** The subsong written: `cueIndex` when it is 0 or more, otherwise the one the path selects, otherwise `0`. */
    subsong: number;
    /** foobar2000's completion code: `0` success, `1` aborted, `2` errors. */
    code: number;
    /** `true` when `code` is `0`. */
    success: boolean;
    /** `success`, `aborted` or `error`, after `code`. */
    status: "success" | "aborted" | "error";
}

/**
 * Payload of the `metadb:changed` event.
 * The metadata of tracks changed: tags were written or read again, or a component such as `foo_playcount` asked foobar2000 to show its fields anew (`fromHook`). Each entry names its track by `handle`, the key the track rows of other methods carry, so a single track of a cue sheet or a multi-track file can be told from the rest of the file.
 */
export interface MetadbChangedPayload {
    /** Up to 50 of the changed tracks, in no particular order. An invalid entry is skipped, so the list can hold fewer than the smaller of `count` and 50. */
    tracks: MetadbChangedTrackItem[];
    /** How many tracks changed, at least `1`; can exceed the length of `tracks`. */
    count: number;
    /** `true` when the files themselves did not change and a component such as `foo_playcount` asked for its fields to be shown anew. */
    fromHook: boolean;
    /** When the event was sent, in milliseconds since the Unix epoch. */
    timestamp: number;
}

/**
 * Payload of the `panel:blur` event.
 * The panel lost keyboard focus. Sent only when the panel's `grabFocus` option is on.
 */
export type PanelBlurPayload = Record<string, never>;

/**
 * Payload of the `panel:configChanged` event.
 * The panel's configuration changed, through `panel.setConfig`, the panel's settings dialog, or a configuration foobar2000 handed to the element. The payload has the same fields as `config` in `panel.getConfig`.
 */
export interface PanelConfigChangedPayload {
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
 * Payload of the `panel:focus` event.
 * The panel got keyboard focus. Sent only when the panel's `grabFocus` option is on.
 */
export type PanelFocusPayload = Record<string, never>;

/**
 * Payload of the `panel:initialized` event.
 * A DUI element or CUI panel finished creating its WebView. It is sent once, as soon as the WebView is ready, which can be before the page has subscribed; a page that needs the mode reads `window.getMode` on startup instead.
 */
export interface PanelInitializedPayload {
    /** `dui` for a Default UI element, `cui` for a Columns UI panel. */
    mode: "dui" | "cui";
    /** Always `true`. */
    panelMode: boolean;
    /** Id of the panel, as `window.getMode` reports it. */
    windowId: string;
}

/**
 * Payload of the `panel:visibilityChanged` event.
 * A DUI element was shown or hidden, for instance by switching tabs in a tab container. Columns UI panels do not send it.
 */
export interface PanelVisibilityChangedPayload {
    /** Whether the element is now visible. */
    visible: boolean;
}

/**
 * Payload of the `playback:cursorFollowChanged` event.
 * foobar2000's Cursor follows playback option changed.
 */
export interface PlaybackCursorFollowChangedPayload {
    /** The new value of the option. */
    enabled: boolean;
}

/**
 * Payload of the `playback:dynamicInfo` event.
 * The decoder reported new dynamic info for the playing stream, such as the bitrate of a VBR file or the title of an internet radio stream.
 */
export interface PlaybackDynamicInfoPayload {
    /** Current bitrate in kbit/s; `0` when unknown. */
    bitrate: number;
    /** The first `TITLE` value of the dynamic info, when it has one. */
    streamTitle?: string;
}

/**
 * Payload of the `playback:dynamicInfoTrack` event.
 * The playing stream announced a new track, as internet radio does. Not sent when it carries neither an artist nor a title.
 */
export interface PlaybackDynamicInfoTrackPayload {
    /** Artists of the new track, joined with `, `; absent when there is none. */
    artist?: string;
    /** The first title of the new track; absent when there is none. */
    title?: string;
}

/**
 * Payload of the `playback:edited` event.
 * The information of the playing track changed, for example its tags were edited. The payload is the track as it reads now.
 */
export type PlaybackEditedPayload = Track;

/**
 * Payload of the `playback:followCursorChanged` event.
 * foobar2000's Playback follows cursor option changed.
 */
export interface PlaybackFollowCursorChangedPayload {
    /** The new value of the option. */
    enabled: boolean;
}

/**
 * Payload of the `playback:itemPlayed` event.
 * foobar2000 counted the playing track as played: 60 seconds of it have been played, or it reached its end after at least a third of it was played. This is when foobar2000's playback statistics record a play.
 */
export type PlaybackItemPlayedPayload = Track;

/**
 * Payload of the `playback:orderChanged` event.
 * foobar2000's playback order changed.
 */
export interface PlaybackOrderChangedPayload {
    /** Index of the new order, as `order` in the `playback.getPlaybackOrder` answer. */
    orderIndex: number;
    /** The same as `orderIndex`. */
    order: number;
}

/**
 * Payload of the `playback:paused` event.
 * Playback was paused or resumed.
 */
export interface PlaybackPausedPayload {
    /** `true` when paused, `false` when resumed. */
    paused: boolean;
}

/**
 * Payload of the `playback:queueChanged` event.
 * foobar2000's playback queue changed. A `queue.*` call that changes the queue several times sends one event at the end.
 */
export interface PlaybackQueueChangedPayload {
    /** What changed the queue: `user_added`, `user_removed`, `playback_advance` (playback took the next queued track), or `unknown`. */
    origin: "user_added" | "user_removed" | "playback_advance" | "unknown";
    /** Queue length after the change. */
    count: number;
}

/**
 * Payload of the `playback:seeked` event.
 * The playing track was seeked.
 */
export interface PlaybackSeekedPayload {
    /** The position seeked to, in seconds. */
    position: number;
    /** Host system time when foobar2000 reported the seek, Unix epoch milliseconds with a fractional part, the same clock as `Date.now()`. `position` is the seek target, not a position read at that time; the `playback:timeHighRes` sent right after can still carry the old position. */
    hostTime: number;
}

/**
 * Payload of the `playback:starting` event.
 * Playback is starting, after a play, next, previous or random command.
 */
export interface PlaybackStartingPayload {
    /** The command that started playback. */
    command: "play" | "next" | "previous" | "random" | "unknown";
    /** Whether playback starts paused. */
    paused: boolean;
}

/**
 * Payload of the `playback:stateChanged` event.
 * The playback state changed: `playing` when a track starts and on resume, `paused` on pause or when playback starts paused, `stopped` when playback stops with reason `user` or `eof` (see `playback:stopped`). `playback.next` stops the playing track with reason `starting_another` and sends no `stopped` state, but foobar2000 reports `playlist.playTrack` on another row as a `user` stop, so a `stopped` state arrives before the new track's events. When a track starts, the event sent right after `playback:trackChanged` carries that track's `canSeek`; an earlier one sent with `playback:starting`, before the track is opened, can report `false`.
 */
export interface PlaybackStateChangedPayload {
    /** The new state. */
    state: "playing" | "paused" | "stopped";
    /** Playback position in seconds, read when the event is sent, with the same latency compensation and transient behavior as `playback.getPosition`. */
    position: number;
    /** Length of the playing track in seconds, read when the event is sent. */
    duration: number;
    /** Whether the playing track can be seeked, read when the event is sent, as `canSeek` in the `playback.getState` answer; always `false` for `stopped`. */
    canSeek: boolean;
    /** Host system time when `position` was read, Unix epoch milliseconds with a fractional part, the same clock as `Date.now()`. */
    hostTime: number;
}

/**
 * Payload of the `playback:stopAfterCurrentChanged` event.
 * foobar2000's Stop after current option changed, through `playback.setStopAfterCurrent`, foobar2000's menu or a component.
 */
export interface PlaybackStopAfterCurrentChangedPayload {
    /** The new value of the option. */
    enabled: boolean;
}

/**
 * Payload of the `playback:stopped` event.
 * Playback stopped: by the user, at the end of the playlist, because another track is starting, or because foobar2000 is shutting down.
 */
export interface PlaybackStoppedPayload {
    /** Why playback stopped: `user`, `eof` (end of the playlist), `starting_another`, `shutting_down`, or `unknown`, as foobar2000 reports it. `playback.next` while playing is `starting_another`; `playlist.playTrack` on another row while playing is `user`. */
    reason: "user" | "eof" | "starting_another" | "shutting_down" | "unknown";
}

/**
 * Payload of the `playback:time` event.
 * The playback position, about once a second while playing, as foobar2000 reports it for time displays. Not sent while every window is hidden. `playback:timeHighRes` is finer.
 */
export interface PlaybackTimePayload {
    /** Coarse playback position in seconds, supplied by the host for time displays, without a sample timestamp. Use `playback:timeHighRes` or `playback.getPosition` for synchronization. */
    position: number;
}

/**
 * Payload of the `playback:timeHighRes` event.
 * The playback position, about every 100 ms while playing and not paused, and once more right after a track starts, a seek or a resume. Not sent while every window is hidden.
 */
export interface PlaybackTimeHighResPayload {
    /** Position in seconds, with its fractional part, using the same output-buffer and DSP latency compensation and stepwise reading as `playback.getPosition`. */
    position: number;
    /** Host system time when `position` was read, Unix epoch milliseconds with a fractional part, the same clock as `Date.now()`. `Date.now() - hostTime` is how late the event arrived, and `position + (Date.now() - hostTime) / 1000` estimates the current position. */
    hostTime: number;
}

/**
 * Payload of the `playback:trackChanged` event.
 * A track started playing: playback began, or moved on to another track. The payload is that track, the same row `playback.getCurrentTrack` answers with. A stream that announces a new title, as internet radio does, sends `playback:dynamicInfoTrack` instead.
 */
export type PlaybackTrackChangedPayload = Track;

/**
 * Payload of the `playback:volumeChanged` event.
 * foobar2000's volume changed, or it was muted or unmuted.
 */
export interface PlaybackVolumeChangedPayload {
    /** Volume as a linear percentage, `0` to `100`, as `playback.getVolume` reports it. */
    volume: number;
    /** Volume in dB, `0` at full volume and `-100` at the bottom. */
    volumeDb: number;
    /** Whether foobar2000 is muted. */
    muted: boolean;
    /** The same as `muted`. */
    isMuted: boolean;
}

/**
 * Payload of the `playlist:activated` event.
 * The active playlist changed.
 */
export interface PlaylistActivatedPayload {
    /** Index of the playlist active before; `-1` when none was. */
    oldIndex: number;
    /** Index of the playlist active now; `-1` when none is. */
    newIndex: number;
    /** GUID of the playlist active now; `null` when none is. No GUID is given for the playlist active before: it may be the one just removed. */
    newGuid: string | null;
}

/**
 * Payload of the `playlist:addComplete` event.
 * A `playlist.addPathsAsync` call finished adding its paths. Paths that need expanding (such as `.cue` or `.m3u`) are expanded first; when every path can be added directly, this follows the call at once. A call that was answered with an error sends nothing.
 */
export interface PlaylistAddCompletePayload {
    /** Id from the call's answer. */
    operationId: string;
    /** GUID of the playlist the call added to, as the call resolved it; still that playlist when it was moved meanwhile, and no longer anyone's when it was removed. */
    playlistGuid: string;
    /** Always `true`. */
    success: boolean;
    /** Tracks added, after expanding playlists and cue sheets. Tracks dropped because the playlist was removed or locked before the expansion finished are not counted. */
    addedCount: number;
    /** Paths the call accepted. */
    totalCount: number;
}

/**
 * Payload of the `playlist:created` event.
 * A playlist was created.
 */
export interface PlaylistCreatedPayload {
    /** Index of the new playlist. */
    index: number;
    /** The playlist's GUID, written as `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`. Pass it as `playlistGuid` to address this playlist even after the playlist list has changed. */
    guid: string;
    /** Its name. */
    name: string;
}

/**
 * Payload of the `playlist:defaultFormatChanged` event.
 * foobar2000 reported that its default playlist format changed.
 */
export type PlaylistDefaultFormatChangedPayload = Record<string, never>;

/**
 * Payload of the `playlist:focusChanged` event.
 * The focused row of a playlist moved. The JIT queue's hidden playlist sends nothing.
 */
export interface PlaylistFocusChangedPayload {
    /** Index of the playlist. */
    playlist: number;
    /** The playlist's GUID, written as `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`. Pass it as `playlistGuid` to address this playlist even after the playlist list has changed. */
    playlistGuid: string;
    /** Row focused before; `-1` when none was. */
    from: number;
    /** Row focused now; `-1` when none is. */
    to: number;
}

/**
 * Payload of the `playlist:itemsAdded` event.
 * Tracks were inserted into a playlist. The JIT queue's hidden playlist sends nothing.
 */
export interface PlaylistItemsAddedPayload {
    /** Index of the playlist. */
    playlist: number;
    /** The playlist's GUID, written as `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`. Pass it as `playlistGuid` to address this playlist even after the playlist list has changed. */
    playlistGuid: string;
    /** Row of the first inserted track. */
    start: number;
    /** Number of tracks inserted. */
    count: number;
}

/**
 * Payload of the `playlist:itemsRemoved` event.
 * Tracks were removed from a playlist. The JIT queue's hidden playlist sends nothing.
 */
export interface PlaylistItemsRemovedPayload {
    /** Index of the playlist. */
    playlist: number;
    /** The playlist's GUID, written as `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`. Pass it as `playlistGuid` to address this playlist even after the playlist list has changed. */
    playlistGuid: string;
    /** Track count before the removal. */
    oldCount: number;
    /** Track count after the removal. */
    newCount: number;
}

/**
 * Payload of the `playlist:itemsReordered` event.
 * A playlist's tracks were reordered. The JIT queue's hidden playlist sends nothing.
 */
export interface PlaylistItemsReorderedPayload {
    /** Index of the playlist. */
    playlist: number;
    /** The playlist's GUID, written as `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`. Pass it as `playlistGuid` to address this playlist even after the playlist list has changed. */
    playlistGuid: string;
    /** Track count of the playlist. */
    count: number;
}

/**
 * Payload of the `playlist:itemsReplaced` event.
 * Tracks of a playlist were replaced in place by other tracks. The JIT queue's hidden playlist sends nothing.
 */
export interface PlaylistItemsReplacedPayload {
    /** Index of the playlist. */
    playlist: number;
    /** The playlist's GUID, written as `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`. Pass it as `playlistGuid` to address this playlist even after the playlist list has changed. */
    playlistGuid: string;
    /** Number of tracks replaced. */
    count: number;
}

/**
 * Payload of the `playlist:lockChanged` event.
 * A lock was put on a playlist or taken off it.
 */
export interface PlaylistLockChangedPayload {
    /** Index of the playlist. */
    playlist: number;
    /** The playlist's GUID, written as `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`. Pass it as `playlistGuid` to address this playlist even after the playlist list has changed. */
    playlistGuid: string;
    /** Whether a lock is on it now. */
    locked: boolean;
}

/**
 * Payload of the `playlist:removed` event.
 * One or more playlists were removed.
 */
export interface PlaylistRemovedPayload {
    /** Playlist count before the removal. */
    oldCount: number;
    /** Playlist count after the removal. */
    newCount: number;
    /** Indices the removed playlists had before the removal, ascending. */
    indices: number[];
    /** GUIDs of the removed playlists, in the order of `indices`; empty when foobar2000 reported the removal without announcing it first, so that the GUIDs could no longer be read. */
    guids: string[];
}

/**
 * Payload of the `playlist:renamed` event.
 * A playlist was renamed.
 */
export interface PlaylistRenamedPayload {
    /** Index of the playlist. */
    index: number;
    /** The playlist's GUID, written as `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`. Pass it as `playlistGuid` to address this playlist even after the playlist list has changed. */
    guid: string;
    /** The new name. */
    name: string;
}

/**
 * Payload of the `playlist:reordered` event.
 * The playlists were reordered.
 */
export interface PlaylistReorderedPayload {
    /** Number of playlists. */
    count: number;
    /** GUID of every playlist, in the new order. */
    guids: string[];
}

/**
 * Payload of the `playlist:selectionChanged` event.
 * The selection in a playlist changed; read it with `playlist.getSelection`. The JIT queue's hidden playlist sends nothing.
 */
export interface PlaylistSelectionChangedPayload {
    /** Index of the playlist. */
    playlist: number;
    /** The playlist's GUID, written as `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`. Pass it as `playlistGuid` to address this playlist even after the playlist list has changed. */
    playlistGuid: string;
}

/**
 * Payload of the `plugin:registered` event.
 * An external plugin registered its namespace through the C++ `PluginRegistry`. Registering the same namespace again only updates its details and sends nothing. The plugin has no methods yet at this point, so `apis` is empty and `apiCount` is `0`; each method follows as `api:registered`.
 */
export type PluginRegisteredPayload = SystemPluginInfo;

/**
 * Payload of the `plugin:unregistered` event.
 * An external plugin was unregistered. `apis` lists the methods removed with it; they are not announced one by one with `api:unregistered`.
 */
export type PluginUnregisteredPayload = SystemPluginInfo;

/**
 * Payload of the `port:connected` event.
 * `port.connect` opened a port. Every window receives it, the opener's included.
 */
export interface PortConnectedPayload {
    /** Id of the new port, as `port.connect` returned it. */
    portId: string;
    /** The channel name. */
    name: string;
    /** Id of the window that opened the port; `main` when the caller cannot be matched to a window. */
    windowId: string;
}

/**
 * Payload of the `port:disconnected` event.
 * A port closed: `port.disconnect` closed it, or the page that opened it started a top-level navigation or went away with its WebView (popup closed, panel removed, WebView rebuilt), which closes all of that page's ports. Every window receives it; a page that closes by navigating receives its own ports' events only if they arrive before it unloads.
 */
export interface PortDisconnectedPayload {
    /** Id of the port that closed. */
    portId: string;
    /** The channel name the port was on. */
    name: string;
    /** Id of the window that opened the port. */
    windowId: string;
}

/**
 * Payload of the `port:message` event.
 * A message reached a port. Each receiving port gets its own event, sent to the window that opened it: `port.postMessage` reaches every other port on the sender's channel, the sender's own window included, and `port.postMessageTo` reaches one port.
 */
export interface PortMessagePayload {
    /** Id of the receiving port, one this window opened. */
    portId: string;
    /** Id of the sending port. */
    sourcePortId: string;
    /** Id of the sending page's window; `main` when the sender cannot be matched to a window. */
    sourceWindowId: string;
    /** The message as the sender passed it; never `null`. */
    message: JsonValue;
}

/**
 * Payload of the `selection:changed` event.
 * foobar2000's global selection changed. A change less than 50 ms after the previous event is dropped, not delayed, so the last of several quick changes may go unannounced; read `selection.get` when the current value matters.
 */
export interface SelectionChangedPayload {
    /** Number of selected tracks. */
    count: number;
    /** Where the selection comes from, as `typeName` in the `selection.getType` answer. */
    type: "now_playing" | "active_playlist_selection" | "active_playlist" | "playlist_manager" | "media_library_viewer" | "unknown";
    /** Handles of the first 100 selected tracks, in selection order: native paths with `|subsong:N` appended when the subsong is not `0`. */
    handles: string[];
    /** Whether more than 100 tracks are selected, so that `handles` lists only the first 100. */
    truncated: boolean;
    /** The selected track; present only when exactly one track is selected. */
    track?: Track;
    /** The track loaded for playback, playing or paused; absent when none is. */
    nowPlaying?: Track;
}

/**
 * Payload of the `state:changed` event.
 * `state.set` stored a value without `silent`. When the key held a value that had expired but was not swept yet, `previousValue` is that old value, and its expiry is never announced.
 */
export interface StateChangedPayload {
    /** The key. */
    key: string;
    /** The stored value; never `null`, which `state.set` treats as missing. */
    value: JsonValue;
    /** The value the key held before; `null` when the key is new. */
    previousValue: JsonValue;
    /** Id of the window that stored it; `main` when the caller cannot be matched to a window. */
    sourceWindowId: string;
    /** When the value expires, in milliseconds since the Unix epoch; present only for a value stored with a positive `ttlMs`. */
    expiresAt?: number;
}

/**
 * Payload of the `state:deleted` event.
 * A value left the shared state: `state.delete` removed it, or its lifetime ran out. Expired values are swept only when `state.get` or `state.keys` runs, so the event for an expiry comes then rather than at the expiry time; deleting a value that expired but was not swept yet reports `deleted`.
 */
export interface StateDeletedPayload {
    /** The key. */
    key: string;
    /** Id of the window that deleted it, `main` when the caller cannot be matched to a window; `""` for an expiry. */
    sourceWindowId: string;
    /** `deleted` for `state.delete`, `expired` when its lifetime ran out. */
    reason: "deleted" | "expired";
}

/**
 * Payload of the `system:themeChanged` event.
 * The colours of foobar2000's Default UI changed, dark mode included. Only pages in this plugin's Default UI panels receive it, together with `ui:coloursChanged`; Columns UI panels and the plugin's own windows do not.
 */
export interface SystemThemeChangedPayload {
    /** Whether Default UI now uses its dark mode. It follows foobar2000's own setting, which can differ from the Windows app theme that `system.getTheme` reports. */
    darkMode: boolean;
}

/**
 * Payload of the `taskbar:buttonClicked` event.
 * A thumbnail toolbar button installed with `taskbar.setThumbnailButtons` was clicked. The default playback buttons shown before a page installs its own run natively and send nothing.
 */
export interface TaskbarButtonClickedPayload {
    /** The button's `id`. */
    id: string;
}

/**
 * Payload of the `tray:beforeContextMenu` event.
 * The tray context menu is about to open on a right click. The menu does not wait for the page, so changes a handler makes to it take effect from the next right click. It is sent even when the menu has no visible row and nothing opens.
 */
export interface TrayBeforeContextMenuPayload {
    /** Screen x of the cursor, where the menu opens. */
    x: number;
    /** Screen y of the cursor, where the menu opens. */
    y: number;
}

/**
 * Payload of the `tray:click` event.
 * The tray icon was clicked with the left button, on release. A right click opens the context menu instead and sends `tray:beforeContextMenu`.
 */
export interface TrayClickPayload {
    /** Always `0`, the left button. */
    button: number;
    /** Cursor x in screen coordinates. */
    x: number;
    /** Cursor y in screen coordinates. */
    y: number;
}

/**
 * Payload of the `tray:doubleClick` event.
 * The tray icon was double-clicked with the left button.
 */
export interface TrayDoubleClickPayload {
    /** Cursor x in screen coordinates. */
    x: number;
    /** Cursor y in screen coordinates. */
    y: number;
}

/**
 * Payload of the `tray:menuItemClicked` event.
 * A row of the tray menu was chosen, or a rich control in it changed value. The built-in playback and system rows, and rows that declare `playbackAction`, run natively and send nothing. With `render: 'webview'` a rich control reports every change and the menu stays open; the default native menu closes on each pick, offers a slider as five stops and shows a segmented row as an ordinary row, which reports no `value`.
 */
export interface TrayMenuItemClickedPayload {
    /** The row's `id`; `""` for a row without one. */
    id: string;
    /** The new value of a rich control: stars `0` to `5` for `rating` (`0` clears), a value within `min` to `max` for `slider`, the index of the chosen segment for `segmented`. Absent for other rows. */
    value?: number;
}

/**
 * Payload of the `ui:coloursChanged` event.
 * The colours of foobar2000's Default UI changed. Only pages in this plugin's Default UI panels receive it, together with `system:themeChanged`; Columns UI panels and the plugin's own windows do not.
 */
export type UiColoursChangedPayload = Record<string, never>;

/**
 * Payload of the `ui:fontChanged` event.
 * The fonts of foobar2000's Default UI changed. Only pages in this plugin's Default UI panels receive it; Columns UI panels and the plugin's own windows do not.
 */
export type UiFontChangedPayload = Record<string, never>;

/**
 * Payload of the `ui:menuItemClicked` event.
 * A row of a menu opened with `ui.showCustomMenu` was chosen. The page that opened the menu gets this in addition to the method's `selectedId`; dismissing the menu sends nothing.
 */
export interface UiMenuItemClickedPayload {
    /** The row's `id`; `""` for a row without one. */
    id: string;
    /** The row's `label` as given, without the shortcut hint; `""` for a row without one. */
    label: string;
}

/**
 * Payload of the `ui:toast` event.
 * `ui.showToast` asks the calling page to show a toast. The host draws nothing; the page renders it from these fields.
 */
export interface UiToastPayload {
    /** Toast text; never empty. */
    message: string;
    /** Display time in milliseconds as given, `3000` when omitted. It is not range-checked and can be `0` or negative. */
    duration: number;
    /** Toast kind; `info` when omitted. */
    type: "info" | "success" | "warning" | "error";
    /** Screen corner as given and not checked; `bottom-right` when omitted. */
    position: string;
}

/**
 * Payload of the `webview:processFailed` event.
 * A WebView2 process of this plugin failed. It is sent to every window; when the failure could not be recovered (`recovered` is `false`), the window whose WebView failed is left out.
 */
export interface WebviewProcessFailedPayload {
    /** Which process failed; a kind this plugin does not know is reported as `unknownProcessExited`. */
    kind: "browserProcessExited" | "renderProcessExited" | "renderProcessUnresponsive" | "frameRenderProcessExited" | "utilityProcessExited" | "sandboxHelperProcessExited" | "gpuProcessExited" | "ppapiPluginProcessExited" | "ppapiBrokerProcessExited" | "unknownProcessExited";
    /** The `COREWEBVIEW2_PROCESS_FAILED_KIND` value WebView2 reported. */
    kindRaw: number;
    /** Whether the WebView is usable again: `true` after a render process was reloaded and for processes the runtime restarts by itself, `false` when the reload failed or the browser process exited. */
    recovered: boolean;
    /** What the plugin did: `reload` for a render process (see `recovered` for the outcome), `needRebuild` when the browser process exited and the WebView has to be created anew, `none` for processes the runtime restarts by itself. */
    recoveryAction: "reload" | "needRebuild" | "none";
}

/**
 * Payload of the `window:activated` event.
 * The main window was activated or deactivated. Only the main window's page gets it; popups and panels do not.
 */
export interface WindowActivatedPayload {
    /** Whether the main window is now active. */
    active: boolean;
}

/**
 * Payload of the `window:alwaysOnTopChanged` event.
 * foobar2000's own Always on top option changed, from its menu or from a component. It is not about the always-on-top state of this plugin's popups.
 */
export interface WindowAlwaysOnTopChangedPayload {
    /** The new value of the option. */
    enabled: boolean;
}

/**
 * Payload of the `window:backdropStateChanged` event.
 * The backdrop of the main window or a popup changed: it was applied with another effect, switched between its active and inactive variant, or was applied again by a forced refresh. Sent only after the backdrop was applied successfully.
 */
export interface WindowBackdropStateChangedPayload {
    /** Id of the window; `main` for the main window. */
    windowId: string;
    /** Whether the window is active. */
    active: boolean;
    /** Which variant was applied, matching `active`. */
    mode: "active" | "inactive";
    /** The effect in use, after `inherit` is resolved and an effect the window cannot show falls back; `system` when the platform draws the frame. */
    effect: "none" | "system" | "mica" | "mica-alt" | "acrylic";
}

/**
 * Payload of the `window:beforeClose` event.
 * A popup created with `beforeClose` is being closed, by its close button or `window.closePopup`. It stays open until its page calls `window.confirmClose` or `window.cancelClose`; with neither within 3 seconds, it closes anyway.
 */
export interface WindowBeforeClosePayload {
    /** Id of the popup. */
    windowId: string;
}

/**
 * Payload of the `window:behaviorChanged` event.
 * `window.setPopupBehavior` changed a popup's preset or overrides. The payload has the same fields as `window.getPopupBehavior` returns.
 */
export interface WindowBehaviorChangedPayload {
    /** Id of the popup. */
    windowId: string;
    /** The behavior preset; `legacy` when the popup was created without one. */
    profile: "legacy" | "standard" | "miniPlayer" | "desktopLyrics";
    /** The behavior overrides set on the popup. */
    behavior: Record<string, JsonValue>;
    /** The behavior in effect. */
    resolvedBehavior: WindowPopupBehaviorState;
}

/**
 * Payload of the `window:dpiChanged` event.
 * The main window's DPI changed, because it moved to another display or the display's scaling changed. Only the main window's page gets it. Sizes are physical pixels at the new DPI; the host also updates the page's titlebar CSS variables.
 */
export interface WindowDpiChangedPayload {
    /** The new DPI; 96 is 100 % scaling. */
    dpi: number;
    /** `dpi / 96`. */
    dpiScale: number;
    /** Height of the titlebar. */
    titlebarHeight: number;
    /** Width of one caption button. */
    captionButtonWidth: number;
    /** Width of the three caption buttons together, `captionButtonWidth * 3`. */
    captionButtonsWidth: number;
}

/**
 * Payload of the `window:hoverStateChanged` event.
 * The cursor entered or left a popup that lets clicks through (`window.setClickThrough`). Such a window gets no mouse messages, so the host polls the cursor while click-through is on; turning click-through off sends `hovering: false`.
 */
export interface WindowHoverStateChangedPayload {
    /** Id of the popup. */
    windowId: string;
    /** Whether the cursor is over the popup. */
    hovering: boolean;
}

/**
 * Payload of the `window:message` event.
 * A page sent a message with `window.sendMessage` or `window.broadcast`. A directed message goes only to the named window; a broadcast goes to every window except the sender's, or to every window when the sender's window cannot be found.
 */
export interface WindowMessagePayload {
    /** Id of the sending page's window: `main`, a popup id or a panel id; `main` when the sender cannot be matched to a window. */
    sourceWindowId: string;
    /** The message as the sender passed it. */
    message: JsonValue;
}

/**
 * Payload of the `window:minimizeSuppressed` event.
 * A popup whose behavior has `keepVisibleOnShowDesktop` ignored a minimize command.
 */
export interface WindowMinimizeSuppressedPayload {
    /** Id of the popup. */
    windowId: string;
    /** Why the minimize was ignored; currently always `policy.keepVisibleOnShowDesktop`. */
    reason: string;
}

/**
 * Payload of the `window:popupClosed` event.
 * A popup closed and its page was destroyed. The windows the host uses for its own menus do not send it.
 */
export interface WindowPopupClosedPayload {
    /** Id of the popup. */
    windowId: string;
}

/**
 * Payload of the `window:popupOpened` event.
 * A popup was created. The windows the host uses for its own menus do not send it.
 */
export interface WindowPopupOpenedPayload {
    /** Id of the popup. */
    windowId: string;
    /** Title the popup was created with. */
    title: string;
    /** URL the popup was created with. */
    url: string;
}

/**
 * Payload of the `window:stateChanged` event.
 * The main window was maximized, minimized, restored, activated or deactivated, or entered or left fullscreen; a popup sends it only when it enters or leaves fullscreen. It goes to every window, and `windowId` says which window changed: a page that only cares about its own window compares it with `window.getCurrentWindowId`. The main window sends it only when one of the four states differs from what it last sent.
 */
export interface WindowStateChangedPayload {
    /** Id of the window that changed: `main` for the main window, else the popup's id. The same value `window.getCurrentWindowId` gives that window's page. */
    windowId: string;
    /** Whether the window is maximized. */
    isMaximized: boolean;
    /** Whether the window is minimized. */
    isMinimized: boolean;
    /** Same as `isMaximized`. */
    maximized: boolean;
    /** Same as `isMinimized`. */
    minimized: boolean;
    /** Whether the window is active. */
    isActive: boolean;
    /** Same as `isActive`. */
    active: boolean;
    /** Whether the window is fullscreen. */
    isFullscreen: boolean;
    /** Same as `isFullscreen`. */
    fullscreen: boolean;
}
// ── Event-name union ─────────────────────────────────────────────────────
/**
 * Literal-union of every event declared under `src/api/schema/`.
 */
export type FBEventName =
    | "api:registered"
    | "api:unregistered"
    | "app:beforeQuit"
    | "audio:dspPresetChanged"
    | "audio:fullWaveformFailed"
    | "audio:fullWaveformReady"
    | "audio:outputDeviceChanged"
    | "audio:pcmFailed"
    | "audio:pcmReady"
    | "audio:replaygainModeChanged"
    | "audio:spectrum"
    | "audio:stream"
    | "cursor:hiddenChanged"
    | "dnd:capabilitiesChanged"
    | "dnd:dragEnded"
    | "dnd:drop"
    | "dnd:enter"
    | "dnd:leave"
    | "file:opComplete"
    | "file:opProgress"
    | "http:downloadComplete"
    | "http:response"
    | "jitQueue:error"
    | "jitQueue:listExhausted"
    | "jitQueue:needNext"
    | "jitQueue:preloadComplete"
    | "jitQueue:trackChanged"
    | "keyboard:hotkey"
    | "library:getAllResult"
    | "library:initialized"
    | "library:itemsAdded"
    | "library:itemsModified"
    | "library:itemsRemoved"
    | "menu:dismiss"
    | "menu:select"
    | "menu:valueChanged"
    | "metadata:probeComplete"
    | "metadata:probeProgress"
    | "metadata:writeComplete"
    | "metadb:changed"
    | "panel:blur"
    | "panel:configChanged"
    | "panel:focus"
    | "panel:initialized"
    | "panel:visibilityChanged"
    | "playback:cursorFollowChanged"
    | "playback:dynamicInfo"
    | "playback:dynamicInfoTrack"
    | "playback:edited"
    | "playback:followCursorChanged"
    | "playback:itemPlayed"
    | "playback:orderChanged"
    | "playback:paused"
    | "playback:queueChanged"
    | "playback:seeked"
    | "playback:starting"
    | "playback:stateChanged"
    | "playback:stopAfterCurrentChanged"
    | "playback:stopped"
    | "playback:time"
    | "playback:timeHighRes"
    | "playback:trackChanged"
    | "playback:volumeChanged"
    | "playlist:activated"
    | "playlist:addComplete"
    | "playlist:created"
    | "playlist:defaultFormatChanged"
    | "playlist:focusChanged"
    | "playlist:itemsAdded"
    | "playlist:itemsRemoved"
    | "playlist:itemsReordered"
    | "playlist:itemsReplaced"
    | "playlist:lockChanged"
    | "playlist:removed"
    | "playlist:renamed"
    | "playlist:reordered"
    | "playlist:selectionChanged"
    | "plugin:registered"
    | "plugin:unregistered"
    | "port:connected"
    | "port:disconnected"
    | "port:message"
    | "selection:changed"
    | "state:changed"
    | "state:deleted"
    | "system:themeChanged"
    | "taskbar:buttonClicked"
    | "tray:beforeContextMenu"
    | "tray:click"
    | "tray:doubleClick"
    | "tray:menuItemClicked"
    | "ui:coloursChanged"
    | "ui:fontChanged"
    | "ui:menuItemClicked"
    | "ui:toast"
    | "webview:processFailed"
    | "window:activated"
    | "window:alwaysOnTopChanged"
    | "window:backdropStateChanged"
    | "window:beforeClose"
    | "window:behaviorChanged"
    | "window:dpiChanged"
    | "window:hoverStateChanged"
    | "window:message"
    | "window:minimizeSuppressed"
    | "window:popupClosed"
    | "window:popupOpened"
    | "window:stateChanged";
// ── Event-name → Payload map ─────────────────────────────────────────────
/**
 * Master map from event name to its generated `*Payload` type. Consumed by
 * typed `fb.on(...)` overloads.
 */
export interface FBEventPayloadMap {
    "api:registered": ApiRegisteredPayload;
    "api:unregistered": ApiUnregisteredPayload;
    "app:beforeQuit": AppBeforeQuitPayload;
    "audio:dspPresetChanged": AudioDspPresetChangedPayload;
    "audio:fullWaveformFailed": AudioFullWaveformFailedPayload;
    "audio:fullWaveformReady": AudioFullWaveformReadyPayload;
    "audio:outputDeviceChanged": AudioOutputDeviceChangedPayload;
    "audio:pcmFailed": AudioPcmFailedPayload;
    "audio:pcmReady": AudioPcmReadyPayload;
    "audio:replaygainModeChanged": AudioReplaygainModeChangedPayload;
    "audio:spectrum": AudioSpectrumPayload;
    "audio:stream": AudioStreamPayload;
    "cursor:hiddenChanged": CursorHiddenChangedPayload;
    "dnd:capabilitiesChanged": DndCapabilitiesChangedPayload;
    "dnd:dragEnded": DndDragEndedPayload;
    "dnd:drop": DndDropPayload;
    "dnd:enter": DndEnterPayload;
    "dnd:leave": DndLeavePayload;
    "file:opComplete": FileOpCompletePayload;
    "file:opProgress": FileOpProgressPayload;
    "http:downloadComplete": HttpDownloadCompletePayload;
    "http:response": HttpResponsePayload;
    "jitQueue:error": JitQueueErrorPayload;
    "jitQueue:listExhausted": JitQueueListExhaustedPayload;
    "jitQueue:needNext": JitQueueNeedNextPayload;
    "jitQueue:preloadComplete": JitQueuePreloadCompletePayload;
    "jitQueue:trackChanged": JitQueueTrackChangedPayload;
    "keyboard:hotkey": KeyboardHotkeyPayload;
    "library:getAllResult": LibraryGetAllResultPayload;
    "library:initialized": LibraryInitializedPayload;
    "library:itemsAdded": LibraryItemsAddedPayload;
    "library:itemsModified": LibraryItemsModifiedPayload;
    "library:itemsRemoved": LibraryItemsRemovedPayload;
    "menu:dismiss": MenuDismissPayload;
    "menu:select": MenuSelectPayload;
    "menu:valueChanged": MenuValueChangedPayload;
    "metadata:probeComplete": MetadataProbeCompletePayload;
    "metadata:probeProgress": MetadataProbeProgressPayload;
    "metadata:writeComplete": MetadataWriteCompletePayload;
    "metadb:changed": MetadbChangedPayload;
    "panel:blur": PanelBlurPayload;
    "panel:configChanged": PanelConfigChangedPayload;
    "panel:focus": PanelFocusPayload;
    "panel:initialized": PanelInitializedPayload;
    "panel:visibilityChanged": PanelVisibilityChangedPayload;
    "playback:cursorFollowChanged": PlaybackCursorFollowChangedPayload;
    "playback:dynamicInfo": PlaybackDynamicInfoPayload;
    "playback:dynamicInfoTrack": PlaybackDynamicInfoTrackPayload;
    "playback:edited": PlaybackEditedPayload;
    "playback:followCursorChanged": PlaybackFollowCursorChangedPayload;
    "playback:itemPlayed": PlaybackItemPlayedPayload;
    "playback:orderChanged": PlaybackOrderChangedPayload;
    "playback:paused": PlaybackPausedPayload;
    "playback:queueChanged": PlaybackQueueChangedPayload;
    "playback:seeked": PlaybackSeekedPayload;
    "playback:starting": PlaybackStartingPayload;
    "playback:stateChanged": PlaybackStateChangedPayload;
    "playback:stopAfterCurrentChanged": PlaybackStopAfterCurrentChangedPayload;
    "playback:stopped": PlaybackStoppedPayload;
    "playback:time": PlaybackTimePayload;
    "playback:timeHighRes": PlaybackTimeHighResPayload;
    "playback:trackChanged": PlaybackTrackChangedPayload;
    "playback:volumeChanged": PlaybackVolumeChangedPayload;
    "playlist:activated": PlaylistActivatedPayload;
    "playlist:addComplete": PlaylistAddCompletePayload;
    "playlist:created": PlaylistCreatedPayload;
    "playlist:defaultFormatChanged": PlaylistDefaultFormatChangedPayload;
    "playlist:focusChanged": PlaylistFocusChangedPayload;
    "playlist:itemsAdded": PlaylistItemsAddedPayload;
    "playlist:itemsRemoved": PlaylistItemsRemovedPayload;
    "playlist:itemsReordered": PlaylistItemsReorderedPayload;
    "playlist:itemsReplaced": PlaylistItemsReplacedPayload;
    "playlist:lockChanged": PlaylistLockChangedPayload;
    "playlist:removed": PlaylistRemovedPayload;
    "playlist:renamed": PlaylistRenamedPayload;
    "playlist:reordered": PlaylistReorderedPayload;
    "playlist:selectionChanged": PlaylistSelectionChangedPayload;
    "plugin:registered": PluginRegisteredPayload;
    "plugin:unregistered": PluginUnregisteredPayload;
    "port:connected": PortConnectedPayload;
    "port:disconnected": PortDisconnectedPayload;
    "port:message": PortMessagePayload;
    "selection:changed": SelectionChangedPayload;
    "state:changed": StateChangedPayload;
    "state:deleted": StateDeletedPayload;
    "system:themeChanged": SystemThemeChangedPayload;
    "taskbar:buttonClicked": TaskbarButtonClickedPayload;
    "tray:beforeContextMenu": TrayBeforeContextMenuPayload;
    "tray:click": TrayClickPayload;
    "tray:doubleClick": TrayDoubleClickPayload;
    "tray:menuItemClicked": TrayMenuItemClickedPayload;
    "ui:coloursChanged": UiColoursChangedPayload;
    "ui:fontChanged": UiFontChangedPayload;
    "ui:menuItemClicked": UiMenuItemClickedPayload;
    "ui:toast": UiToastPayload;
    "webview:processFailed": WebviewProcessFailedPayload;
    "window:activated": WindowActivatedPayload;
    "window:alwaysOnTopChanged": WindowAlwaysOnTopChangedPayload;
    "window:backdropStateChanged": WindowBackdropStateChangedPayload;
    "window:beforeClose": WindowBeforeClosePayload;
    "window:behaviorChanged": WindowBehaviorChangedPayload;
    "window:dpiChanged": WindowDpiChangedPayload;
    "window:hoverStateChanged": WindowHoverStateChangedPayload;
    "window:message": WindowMessagePayload;
    "window:minimizeSuppressed": WindowMinimizeSuppressedPayload;
    "window:popupClosed": WindowPopupClosedPayload;
    "window:popupOpened": WindowPopupOpenedPayload;
    "window:stateChanged": WindowStateChangedPayload;
}

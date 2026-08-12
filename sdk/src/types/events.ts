/**
 * `foo-webview-sdk` - Event names, payload shapes, and the master payload map.
 *
 * Re-exports the per-event `*Payload` interfaces, and declares the
 * `FBEventName` literal-union covering every published event together with
 * the `FBEventPayloadMap` map consumed by `fb.on(name, handler)` /
 * `fb.event.subscribe(name, handler)`. Both combine the generated surface
 * with the hand-written `dnd:*` entries below.
 */

import type { ApiErrorCode, DndPathsUnavailableReason } from './responses.js';

export type {
    ApiRegisteredPayload,
    ApiUnregisteredPayload,
    AppBeforeQuitPayload,
    AudioDspPresetChangedPayload,
    AudioFullWaveformFailedPayload,
    AudioFullWaveformReadyPayload,
    AudioOutputDeviceChangedPayload,
    AudioReplaygainModeChangedPayload,
    AudioSpectrumPayload,
    AudioStreamPayload,
    CursorHiddenChangedPayload,
    HttpDownloadCompletePayload,
    HttpResponsePayload,
    JitQueueErrorPayload,
    JitQueueListExhaustedPayload,
    JitQueueNeedNextPayload,
    JitQueuePreloadCompletePayload,
    JitQueueTrackChangedPayload,
    KeyboardHotkeyPayload,
    LibraryInitializedPayload,
    LibraryItemsAddedPayload,
    LibraryItemsModifiedPayload,
    LibraryItemsRemovedPayload,
    MetadataWriteCompletePayload,
    MetadbChangedPayload,
    PanelBlurPayload,
    PanelConfigChangedPayload,
    PanelFocusPayload,
    PanelInitializedPayload,
    PanelVisibilityChangedPayload,
    PlaybackCursorFollowChangedPayload,
    PlaybackDynamicInfoPayload,
    PlaybackDynamicInfoTrackPayload,
    PlaybackEditedPayload,
    PlaybackFollowCursorChangedPayload,
    PlaybackItemPlayedPayload,
    PlaybackOrderChangedPayload,
    PlaybackPausedPayload,
    PlaybackQueueChangedPayload,
    PlaybackSeekedPayload,
    PlaybackStartingPayload,
    PlaybackStateChangedPayload,
    PlaybackStopAfterCurrentChangedPayload,
    PlaybackStoppedPayload,
    PlaybackTimeHighResPayload,
    PlaybackTimePayload,
    PlaybackTrackChangedPayload,
    PlaybackVolumeChangedPayload,
    PlaylistActivatedPayload,
    PlaylistAddCompletePayload,
    PlaylistCreatedPayload,
    PlaylistDefaultFormatChangedPayload,
    PlaylistFocusChangedPayload,
    PlaylistItemsAddedPayload,
    PlaylistItemsRemovedPayload,
    PlaylistItemsReorderedPayload,
    PlaylistItemsReplacedPayload,
    PlaylistLockChangedPayload,
    PlaylistRemovedPayload,
    PlaylistRenamedPayload,
    PlaylistReorderedPayload,
    PlaylistSelectionChangedPayload,
    PluginRegisteredPayload,
    PluginUnregisteredPayload,
    PortConnectedPayload,
    PortDisconnectedPayload,
    PortMessagePayload,
    SelectionChangedPayload,
    StateChangedPayload,
    StateDeletedPayload,
    SystemThemeChangedPayload,
    UiColoursChangedPayload,
    UiFontChangedPayload,
    UiMenuItemClickedPayload,
    UiToastPayload,
    WindowAlwaysOnTopChangedPayload,
    WindowBackdropStateChangedPayload,
    WindowBeforeClosePayload,
    WindowBehaviorChangedPayload,
    WindowHoverStateChangedPayload,
    WindowMessagePayload,
    WindowMinimizeSuppressedPayload,
    WindowPopupClosedPayload,
    WindowPopupOpenedPayload,
    WindowStateChangedPayload,
} from './generated/events.js';

export type { MetadbChangedTrackItem } from './overrides/events.js';

export type { LibraryGetAllResultPayload } from './overrides/events.js';

import type {
    AudioFullWaveformFailedPayload,
    AudioFullWaveformReadyPayload,
    AudioReplaygainModeChangedPayload,
    JitQueueTrackChangedPayload,
    LibraryItemsAddedPayload,
    PanelConfigChangedPayload,
    PanelVisibilityChangedPayload,
    PlaybackStopAfterCurrentChangedPayload,
    PluginRegisteredPayload,
    ApiRegisteredPayload,
    PortConnectedPayload,
    PortMessagePayload,
    StateChangedPayload,
    StateDeletedPayload,
    WindowAlwaysOnTopChangedPayload,
    WindowBackdropStateChangedPayload,
    WindowHoverStateChangedPayload,
    WindowPopupOpenedPayload,
    FBEventName as GeneratedFBEventName,
    FBEventPayloadMap as GeneratedFBEventPayloadMap,
} from './generated/events.js';

/** Generic event-handler signature accepted by `FBEventPayloadMap`. */
// eslint-disable-next-line @typescript-eslint/no-explicit-any
export type EventHandler<T = any> = (data: T) => void;

/** Return type of `fb.on(...)` - calling it detaches the handler. */
export type UnsubscribeFunction = () => void;

/** @deprecated Use `FBEventName`. */
export type PlaybackEventName = FBEventName;

/**
 * Minimum shape every async-failure event carries. Concrete usages include
 * `audio:fullWaveformFailed` and the failure branch of `http:response`.
 */
export interface FailureEventPayload {
    error: string;
    code: ApiErrorCode;
    /** Background-task identifier, when the failure originated from one. */
    taskId?: string;
    /** Filesystem path involved in the failure, when applicable. */
    path?: string;
    /** HTTP request identifier, when applicable. */
    requestId?: string;
}

/** Custom-event envelope produced by `fb.event.emit*`. */
// eslint-disable-next-line @typescript-eslint/no-explicit-any
export interface EventEnvelope<T = any> {
    payload: T;
    sourceWindowId: string;
}

/**
 * Fields shared by every `dnd:*` payload.
 *
 * Drag-drop events are delivered point-to-point to the window under the
 * cursor, never broadcast, because real filesystem paths are sensitive.
 */
export interface DndSessionEventPayload {
    /**
     * Identifier correlating `dnd:enter`, `dnd:leave` and `dnd:drop` for one
     * drag gesture. Unique across the whole host process, so it also
     * identifies which window the gesture belongs to.
     */
    sessionId: string;
}

/**
 * Emitted when a drag gesture enters the window.
 *
 * `paths` is an empty array when the drag carries no `CF_HDROP` file list
 * (browser links, virtual shell objects, archive entries) or when the
 * document origin is not trusted with real paths.
 */
export interface DndEnterPayload extends DndSessionEventPayload {
    /** Absolute filesystem paths, in the same order as `DataTransfer.files`. */
    paths: string[];
    /**
     * Whether the drag carries a `CF_HDROP` file list. Reported truthfully
     * even when `paths` is withheld, since it leaks nothing by itself.
     */
    hasFiles: boolean;
    /** Cursor x in client-area physical pixels; divide by `devicePixelRatio` for CSS pixels. */
    x: number;
    /** Cursor y in client-area physical pixels; divide by `devicePixelRatio` for CSS pixels. */
    y: number;
}

/** Emitted when a drag gesture leaves the window without dropping. */
export type DndLeavePayload = DndSessionEventPayload;

/**
 * Emitted when a drag gesture is dropped on the window.
 *
 * `paths` carries the final list, which the drag source may have changed
 * since `dnd:enter`. Arrival time relative to the page's own HTML5 `drop`
 * handler is not guaranteed; call `fb.dnd.getPathsAsync()` from that handler
 * when paths are needed synchronously with the drop.
 */
export interface DndDropPayload extends DndSessionEventPayload {
    /** Absolute filesystem paths, in the same order as `DataTransfer.files`. */
    paths: string[];
    /** Cursor x in client-area physical pixels. */
    x: number;
    /** Cursor y in client-area physical pixels. */
    y: number;
    /**
     * Win32 modifier / mouse-button mask at drop time (`MK_*` flags), for
     * pages that want modifier-dependent behaviour. It does not influence the
     * drop effect the host reports to the drag source, which is always copy.
     */
    keyState: number;
}

/**
 * Emitted when the resolved drag-drop capability of the window changes, for
 * instance when Chromium re-registers its own drop target and displaces the
 * host's, or when a navigation changes the document origin.
 */
export interface DndCapabilitiesChangedPayload {
    /** Page still receives standard HTML5 drag events. */
    html5: boolean;
    /** Real filesystem paths are still obtainable. */
    paths: boolean;
    /**
     * How the window hosts its WebView, which decides whether paths are
     * obtainable at all: `standard` panels cannot supply them.
     */
    hosting: 'visual' | 'standard';
    /** Present only when `paths` is `false`. */
    pathsUnavailableReason?: DndPathsUnavailableReason;
}

/**
 * Event-name to payload map for the `dnd:*` events.
 *
 * The host publishes these through a per-window event sink that passes the
 * event name as a runtime value, so they are absent from the generated map
 * and are merged into {@link FBEventPayloadMap} from here.
 */
export interface DndEventPayloadMap {
    'dnd:enter': DndEnterPayload;
    'dnd:leave': DndLeavePayload;
    'dnd:drop': DndDropPayload;
    'dnd:capabilitiesChanged': DndCapabilitiesChangedPayload;
}

/**
 * Literal-union of every published event name, accepted by the typed
 * `fb.on(name, handler)` / `fb.once(name, handler)` overloads.
 */
export type FBEventName = GeneratedFBEventName | keyof DndEventPayloadMap;

/**
 * Master map from event name to payload type. Indexing it with an
 * {@link FBEventName} yields the payload the handler receives.
 */
export interface FBEventPayloadMap
    extends GeneratedFBEventPayloadMap,
        DndEventPayloadMap {}

/** @deprecated Use `AudioFullWaveformReadyPayload`. */
export type FullWaveformReadyEvent = AudioFullWaveformReadyPayload;

/** @deprecated Use `AudioFullWaveformFailedPayload`. */
export type FullWaveformFailedEvent = AudioFullWaveformFailedPayload;

/** @deprecated Use `AudioReplaygainModeChangedPayload`. */
export type AudioReplaygainModePayload = AudioReplaygainModeChangedPayload;

/** @deprecated Use `JitQueueTrackChangedPayload`. */
export type JitQueueTrackPayload = JitQueueTrackChangedPayload;

/** @deprecated Use `PanelVisibilityChangedPayload`. */
export type PanelVisibilityPayload = PanelVisibilityChangedPayload;

/** @deprecated Use `PanelConfigChangedPayload`. */
export type PanelConfigPayload = PanelConfigChangedPayload;

/** @deprecated Use `WindowAlwaysOnTopChangedPayload`. */
export type WindowAlwaysOnTopPayload = WindowAlwaysOnTopChangedPayload;

/** @deprecated Use `WindowBackdropStateChangedPayload`. */
export type WindowBackdropStatePayload = WindowBackdropStateChangedPayload;

/** @deprecated Use `WindowHoverStateChangedPayload`. */
export type WindowHoverStatePayload = WindowHoverStateChangedPayload;

/**
 * @deprecated Use the specific `WindowPopupOpenedPayload` /
 * `WindowPopupClosedPayload`. `url` is only present on `popupOpened`.
 */
export type WindowPopupPayload = WindowPopupOpenedPayload;

/** @deprecated Use `PortMessagePayload`. */
export type PortMessage = PortMessagePayload;

/**
 * @deprecated Use the specific `PortConnectedPayload` /
 * `PortDisconnectedPayload`; both share this shape.
 */
export type PortConnectionEvent = PortConnectedPayload;

/**
 * @deprecated Use the specific `PluginRegisteredPayload` /
 * `PluginUnregisteredPayload`; both share this shape.
 */
export type PluginLifecycleEventPayload = PluginRegisteredPayload;

/**
 * @deprecated Use the specific `ApiRegisteredPayload` /
 * `ApiUnregisteredPayload`; both share this shape.
 */
export type ApiLifecycleEventPayload = ApiRegisteredPayload;

/**
 * @deprecated Use the specific `LibraryItemsAddedPayload` /
 * `LibraryItemsRemovedPayload` / `LibraryItemsModifiedPayload`; all three
 * share this shape.
 */
export type LibraryItemsPayload = LibraryItemsAddedPayload;

/**
 * @deprecated Use the specific `PlaybackStopAfterCurrentChangedPayload` /
 * `PlaybackFollowCursorChangedPayload` /
 * `PlaybackCursorFollowChangedPayload`; all share `{enabled: boolean}`.
 */
export type PlaybackBooleanPayload = PlaybackStopAfterCurrentChangedPayload;

/** @deprecated Use `StateChangedPayload`. */
// eslint-disable-next-line @typescript-eslint/no-explicit-any
export type SharedStateChange<T = any> = StateChangedPayload<T>;

/** @deprecated Use `StateDeletedPayload`. */
export type SharedStateDelete = StateDeletedPayload;

/**
 * `foo-webview-sdk` - Event names, payload shapes, and the master payload map.
 *
 * Re-exports the per-event `*Payload` interfaces, the `FBEventName`
 * literal-union covering every published event, and the
 * `FBEventPayloadMap` map consumed by `fb.on(name, handler)` /
 * `fb.event.subscribe(name, handler)`.
 */

import type { ApiErrorCode } from './responses.js';

export type {
    FBEventName,
    FBEventPayloadMap,
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
    FBEventName,
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

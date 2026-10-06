/**
 * `foo-webview-sdk` - Event names, payload shapes, and the master payload map.
 *
 * Re-exports the per-event `*Payload` interfaces, and declares the
 * `FBEventName` literal-union covering every published event together with
 * the `FBEventPayloadMap` map consumed by `fb.on(name, handler)` /
 * `fb.event.subscribe(name, handler)`.
 */

import type { ApiErrorCode } from './responses.js';

export type {
    ApiRegisteredPayload,
    ApiUnregisteredPayload,
    AppBeforeQuitPayload,
    AudioDspPresetChangedPayload,
    AudioFullWaveformFailedPayload,
    AudioFullWaveformReadyPayload,
    AudioOutputDeviceChangedPayload,
    AudioPcmFailedPayload,
    AudioPcmReadyPayload,
    AudioReplaygainModeChangedPayload,
    AudioSpectrumPayload,
    AudioStreamPayload,
    CursorHiddenChangedPayload,
    DndCapabilitiesChangedPayload,
    DndDragEndedPayload,
    DndDropPayload,
    DndEnterPayload,
    DndLeavePayload,
    FileOpCompletePayload,
    FileOpProgressPayload,
    HttpDownloadCompletePayload,
    HttpResponsePayload,
    JitQueueErrorPayload,
    JitQueueListExhaustedPayload,
    JitQueueNeedNextPayload,
    JitQueuePreloadCompletePayload,
    JitQueueTrackChangedPayload,
    KeyboardHotkeyPayload,
    LibraryGetAllResultPayload,
    LibraryInitializedPayload,
    LibraryItemsAddedPayload,
    LibraryItemsModifiedPayload,
    LibraryItemsRemovedPayload,
    MenuDismissPayload,
    MenuSelectPayload,
    MenuValueChangedPayload,
    MetadataProbeCompletePayload,
    MetadataProbeProgressPayload,
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
    StateDeletedPayload,
    SystemThemeChangedPayload,
    TaskbarButtonClickedPayload,
    TrayBeforeContextMenuPayload,
    TrayClickPayload,
    TrayDoubleClickPayload,
    TrayMenuItemClickedPayload,
    UiColoursChangedPayload,
    UiFontChangedPayload,
    UiMenuItemClickedPayload,
    UiToastPayload,
    WebviewProcessFailedPayload,
    WindowActivatedPayload,
    WindowAlwaysOnTopChangedPayload,
    WindowBackdropStateChangedPayload,
    WindowBeforeClosePayload,
    WindowBehaviorChangedPayload,
    WindowDpiChangedPayload,
    WindowHoverStateChangedPayload,
    WindowMessagePayload,
    WindowMinimizeSuppressedPayload,
    WindowPopupClosedPayload,
    WindowPopupOpenedPayload,
    WindowStateChangedPayload,
} from './generated/events.js';

export type {
    FileOpResultItem,
    MetadataProbeResultItem,
    MetadbChangedTrackItem,
} from './generated/schema-types.js';

import type {
    FileOpResultItem,
    MetadataProbeResultItem,
} from './generated/schema-types.js';
import type { FileOpProgressPayload } from './generated/events.js';

/** Why one path in a `metadata.probeBatchAsync` batch produced no info. */
export type MetadataProbeFailure = NonNullable<MetadataProbeResultItem['failure']>;

/** Where a probed track's info came from. `'none'` accompanies a failure. */
export type MetadataProbeInfoSource = MetadataProbeResultItem['infoSource'];

/** Which of the async file operations a `file:op*` event reports on. */
export type FileOpKind = FileOpProgressPayload['op'];

/**
 * Outcome class of one `FileOpResultItem`.
 *
 * `'skipped'` means the entry was deliberately not carried out (it already
 * existed, or the run was cancelled before reaching it), so it is not an
 * error; `'failed'` is.
 */
export type FileOpStatus = FileOpResultItem['status'];

/**
 * Why a `FileOpResultItem` ended the way it did.
 *
 * `'cross-volume'` is the one value that accompanies `status: 'ok'`: the
 * entry succeeded, but the move had to fall back to copy-then-delete because
 * source and destination sit on different volumes, which costs a full copy
 * instead of a rename.
 */
export type FileOpResultReason = NonNullable<FileOpResultItem['reason']>;

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
    StateChangedPayload as GeneratedStateChangedPayload,
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
 * Payload of `state:changed` with `value` and `previousValue` typed for a key
 * whose values the page knows; without a type argument they are `unknown`.
 * `FBEventPayloadMap['state:changed']` types them as `JsonValue`, which is
 * what the host sends.
 */
export interface StateChangedPayload<T = unknown>
    extends Omit<GeneratedStateChangedPayload, 'value' | 'previousValue'> {
    /** The stored value; never `null`. */
    value: T;
    /** The value the key held before; `null` when the key is new. */
    previousValue: T | null;
}

/**
 * @deprecated Use `DndLeavePayload`; `dnd:enter` and `dnd:drop` carry the same
 * `sessionId`.
 */
export interface DndSessionEventPayload {
    /**
     * Identifier correlating `dnd:enter`, `dnd:leave` and `dnd:drop` for one
     * drag gesture.
     */
    sessionId: string;
}

/**
 * @deprecated Index `FBEventPayloadMap` instead, which lists these events with
 * the rest.
 */
export type DndEventPayloadMap = Pick<
    GeneratedFBEventPayloadMap,
    'dnd:enter' | 'dnd:leave' | 'dnd:drop' | 'dnd:capabilitiesChanged'
>;

// A fresh union rather than a plain alias of the generated list, so editors and
// compiler messages show the name `FBEventName`. Both operands list the same names.
/**
 * Literal-union of every published event name, accepted by the typed
 * `fb.on(name, handler)` / `fb.once(name, handler)` overloads.
 */
export type FBEventName = GeneratedFBEventName | keyof GeneratedFBEventPayloadMap;

/**
 * Master map from event name to payload type. Indexing it with an
 * {@link FBEventName} yields the payload the handler receives.
 */
export interface FBEventPayloadMap extends GeneratedFBEventPayloadMap {}

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

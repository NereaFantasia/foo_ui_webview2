// ─────────────────────────────────────────────────────────────
// GENERATED FILE — DO NOT EDIT
// File: sdk/src/types/generated/events.ts
// Source: docs/graph/auto/code-index.json
// Emitter: scripts/gen_sdk_types.mjs --all
// schema_version: auto-1.0.0
// Regenerate: npm run gen:types (in sdk/) or `node scripts/gen_sdk_types.mjs --all`
// ─────────────────────────────────────────────────────────────

/* eslint-disable */

import type { JsonObject } from '../json.js';

// ── Hand-written overrides — see sdk/src/types/overrides/ ────────────────
import type { CursorHiddenChangedPayload } from "../overrides/cursor.js";
import type { ApiRegisteredPayload, ApiUnregisteredPayload, AppBeforeQuitPayload, AudioFullWaveformFailedPayload, AudioFullWaveformReadyPayload, AudioOutputDeviceChangedPayload, AudioReplaygainModeChangedPayload, AudioSpectrumPayload, HttpDownloadCompletePayload, HttpResponsePayload, JitQueueErrorPayload, JitQueuePreloadCompletePayload, LibraryGetAllResultPayload, LibraryItemsAddedPayload, LibraryItemsModifiedPayload, LibraryItemsRemovedPayload, MetadataWriteCompletePayload, MetadbChangedPayload, PanelBlurPayload, PanelFocusPayload, PanelInitializedPayload, PanelVisibilityChangedPayload, PlaybackCursorFollowChangedPayload, PlaybackDynamicInfoPayload, PlaybackDynamicInfoTrackPayload, PlaybackEditedPayload, PlaybackFollowCursorChangedPayload, PlaybackItemPlayedPayload, PlaybackQueueChangedPayload, PlaybackStartingPayload, PlaybackStateChangedPayload, PlaybackStopAfterCurrentChangedPayload, PlaybackStoppedPayload, PlaybackTrackChangedPayload, PluginRegisteredPayload, PluginUnregisteredPayload, StateChangedPayload, StateDeletedPayload, TrayMenuItemClickedPayload, UiToastPayload, WindowAlwaysOnTopChangedPayload } from "../overrides/events.js";
export type { ApiRegisteredPayload, ApiUnregisteredPayload, AppBeforeQuitPayload, AudioFullWaveformFailedPayload, AudioFullWaveformReadyPayload, AudioOutputDeviceChangedPayload, AudioReplaygainModeChangedPayload, AudioSpectrumPayload, CursorHiddenChangedPayload, HttpDownloadCompletePayload, HttpResponsePayload, JitQueueErrorPayload, JitQueuePreloadCompletePayload, LibraryGetAllResultPayload, LibraryItemsAddedPayload, LibraryItemsModifiedPayload, LibraryItemsRemovedPayload, MetadataWriteCompletePayload, MetadbChangedPayload, PanelBlurPayload, PanelFocusPayload, PanelInitializedPayload, PanelVisibilityChangedPayload, PlaybackCursorFollowChangedPayload, PlaybackDynamicInfoPayload, PlaybackDynamicInfoTrackPayload, PlaybackEditedPayload, PlaybackFollowCursorChangedPayload, PlaybackItemPlayedPayload, PlaybackQueueChangedPayload, PlaybackStartingPayload, PlaybackStateChangedPayload, PlaybackStopAfterCurrentChangedPayload, PlaybackStoppedPayload, PlaybackTrackChangedPayload, PluginRegisteredPayload, PluginUnregisteredPayload, StateChangedPayload, StateDeletedPayload, TrayMenuItemClickedPayload, UiToastPayload, WindowAlwaysOnTopChangedPayload };

/**
 * Payload of the `audio:dspPresetChanged` event (no fields).
 */
export type AudioDspPresetChangedPayload = Record<string, never>;

/**
 * Payload of the `audio:stream` event (no fields).
 */
export type AudioStreamPayload = Record<string, never>;

/**
 * Payload of the `jitQueue:listExhausted` event.
 */
export interface JitQueueListExhaustedPayload {
    lastTrackId: string;
}

/**
 * Payload of the `jitQueue:needNext` event.
 */
export interface JitQueueNeedNextPayload {
    currentTrackId: string;
    reason: string;
}

/**
 * Payload of the `jitQueue:trackChanged` event.
 */
export interface JitQueueTrackChangedPayload {
    trackId: string;
    title: string;
}

/**
 * Payload of the `keyboard:hotkey` event.
 */
export interface KeyboardHotkeyPayload {
    id: number;
    key: string;
    action: string;
}

/**
 * Payload of the `library:initialized` event.
 */
export interface LibraryInitializedPayload {
    /** int64 — may lose precision above 2^53 */
    timestamp: number;
}

/**
 * Payload of the `menu:dismiss` event.
 */
export interface MenuDismissPayload {
    menuId: string;
    reason: string;
}

/**
 * Payload of the `menu:select` event.
 */
export interface MenuSelectPayload {
    menuId: string;
    itemId: string;
}

/**
 * Payload of the `menu:show` event.
 */
export interface MenuShowPayload {
    menuId: string;
}

/**
 * Payload of the `panel:configChanged` event.
 */
export interface PanelConfigChangedPayload {
    panelName: string;
    templateName: string;
    edgeStyle: number;
    transparentBackground: boolean;
    grabFocus: boolean;
    enableDragDrop: boolean;
    enableDevTools: boolean;
    urlOverride: string;
}

/**
 * Payload of the `playback:orderChanged` event.
 */
export interface PlaybackOrderChangedPayload {
    /** int64 — may lose precision above 2^53 */
    orderIndex: number;
    /** int64 — may lose precision above 2^53 */
    order: number;
}

/**
 * Payload of the `playback:paused` event.
 */
export interface PlaybackPausedPayload {
    paused: boolean;
}

/**
 * Payload of the `playback:seeked` event.
 */
export interface PlaybackSeekedPayload {
    position: number;
}

/**
 * Payload of the `playback:time` event.
 */
export interface PlaybackTimePayload {
    position: number;
}

/**
 * Payload of the `playback:timeHighRes` event.
 */
export interface PlaybackTimeHighResPayload {
    position: number;
}

/**
 * Payload of the `playback:volumeChanged` event.
 */
export interface PlaybackVolumeChangedPayload {
    volume: number;
    volumeDb: number;
    muted: boolean;
    isMuted: boolean;
}

/**
 * Payload of the `playlist:activated` event.
 */
export interface PlaylistActivatedPayload {
    /** int64 — may lose precision above 2^53 */
    oldIndex: number;
    /** int64 — may lose precision above 2^53 */
    newIndex: number;
}

/**
 * Payload of the `playlist:addComplete` event.
 */
export interface PlaylistAddCompletePayload {
    operationId: string;
    success: boolean;
    /** int64 — may lose precision above 2^53 */
    addedCount: number;
    /** int64 — may lose precision above 2^53 */
    totalCount: number;
}

/**
 * Payload of the `playlist:created` event.
 */
export interface PlaylistCreatedPayload {
    /** int64 — may lose precision above 2^53 */
    index: number;
    name: string;
}

/**
 * Payload of the `playlist:defaultFormatChanged` event (no fields).
 */
export type PlaylistDefaultFormatChangedPayload = Record<string, never>;

/**
 * Payload of the `playlist:focusChanged` event.
 */
export interface PlaylistFocusChangedPayload {
    /** int64 — may lose precision above 2^53 */
    playlist: number;
    /** int64 — may lose precision above 2^53 */
    from: number;
    /** int64 — may lose precision above 2^53 */
    to: number;
}

/**
 * Payload of the `playlist:itemsAdded` event.
 */
export interface PlaylistItemsAddedPayload {
    /** int64 — may lose precision above 2^53 */
    playlist: number;
    /** int64 — may lose precision above 2^53 */
    start: number;
    count: number;
}

/**
 * Payload of the `playlist:itemsRemoved` event.
 */
export interface PlaylistItemsRemovedPayload {
    /** int64 — may lose precision above 2^53 */
    playlist: number;
    /** int64 — may lose precision above 2^53 */
    oldCount: number;
    /** int64 — may lose precision above 2^53 */
    newCount: number;
}

/**
 * Payload of the `playlist:itemsReordered` event.
 */
export interface PlaylistItemsReorderedPayload {
    /** int64 — may lose precision above 2^53 */
    playlist: number;
    /** int64 — may lose precision above 2^53 */
    count: number;
}

/**
 * Payload of the `playlist:itemsReplaced` event.
 */
export interface PlaylistItemsReplacedPayload {
    /** int64 — may lose precision above 2^53 */
    playlist: number;
    count: number;
}

/**
 * Payload of the `playlist:lockChanged` event.
 */
export interface PlaylistLockChangedPayload {
    /** int64 — may lose precision above 2^53 */
    playlist: number;
    locked: boolean;
}

/**
 * Payload of the `playlist:removed` event.
 */
export interface PlaylistRemovedPayload {
    /** int64 — may lose precision above 2^53 */
    oldCount: number;
    /** int64 — may lose precision above 2^53 */
    newCount: number;
}

/**
 * Payload of the `playlist:renamed` event.
 */
export interface PlaylistRenamedPayload {
    /** int64 — may lose precision above 2^53 */
    index: number;
    name: string;
}

/**
 * Payload of the `playlist:reordered` event.
 */
export interface PlaylistReorderedPayload {
    /** int64 — may lose precision above 2^53 */
    count: number;
}

/**
 * Payload of the `playlist:selectionChanged` event.
 */
export interface PlaylistSelectionChangedPayload {
    /** int64 — may lose precision above 2^53 */
    playlist: number;
}

/**
 * Payload of the `port:connected` event.
 */
export interface PortConnectedPayload {
    portId: string;
    name: string;
    windowId: string;
}

/**
 * Payload of the `port:disconnected` event.
 */
export interface PortDisconnectedPayload {
    portId: string;
    name: string;
    windowId: string;
}

/**
 * Payload of the `port:message` event.
 */
export interface PortMessagePayload {
    portId: unknown;
    sourcePortId: string;
    sourceWindowId: string;
    message: JsonObject;
}

/**
 * Payload of the `selection:changed` event.
 */
export interface SelectionChangedPayload {
    /** int64 — may lose precision above 2^53 */
    count: number;
    type: string;
    handles: JsonObject;
    truncated: boolean;
    track: JsonObject;
    nowPlaying: JsonObject;
}

/**
 * Payload of the `system:themeChanged` event.
 */
export interface SystemThemeChangedPayload {
    darkMode: boolean;
}

/**
 * Payload of the `taskbar:buttonClicked` event.
 */
export interface TaskbarButtonClickedPayload {
    id: string;
}

/**
 * Payload of the `tray:beforeContextMenu` event.
 */
export interface TrayBeforeContextMenuPayload {
    x: number;
    y: number;
}

/**
 * Payload of the `tray:click` event.
 */
export interface TrayClickPayload {
    button: number;
    x: number;
    y: number;
}

/**
 * Payload of the `tray:doubleClick` event.
 */
export interface TrayDoubleClickPayload {
    x: number;
    y: number;
}

/**
 * Payload of the `ui:coloursChanged` event (shape unspecified).
 */
export type UiColoursChangedPayload = JsonObject;

/**
 * Payload of the `ui:fontChanged` event (shape unspecified).
 */
export type UiFontChangedPayload = JsonObject;

/**
 * Payload of the `ui:menuItemClicked` event.
 */
export interface UiMenuItemClickedPayload {
    id: string;
    label: string;
}

/**
 * Payload of the `webview:processFailed` event.
 */
export interface WebviewProcessFailedPayload {
    kind: string;
    kindRaw: unknown;
    recovered: boolean;
    recoveryAction: unknown;
}

/**
 * Payload of the `window:backdropStateChanged` event.
 */
export interface WindowBackdropStateChangedPayload {
    windowId: string;
    active: boolean;
    mode: string;
    effect: string;
}

/**
 * Payload of the `window:beforeClose` event.
 */
export interface WindowBeforeClosePayload {
    windowId: string;
}

/**
 * Payload of the `window:behaviorChanged` event.
 */
export interface WindowBehaviorChangedPayload {
    windowId: string;
    profile: string;
    behavior: JsonObject;
    resolvedBehavior: JsonObject;
}

/**
 * Payload of the `window:hoverStateChanged` event.
 */
export interface WindowHoverStateChangedPayload {
    windowId: string;
    reason?: string;
    hovering?: boolean;
}

/**
 * Payload of the `window:message` event.
 */
export interface WindowMessagePayload {
    sourceWindowId: string;
    message: JsonObject;
}

/**
 * Payload of the `window:minimizeSuppressed` event.
 */
export interface WindowMinimizeSuppressedPayload {
    windowId: string;
    reason: string;
}

/**
 * Payload of the `window:popupClosed` event.
 */
export interface WindowPopupClosedPayload {
    windowId: string;
}

/**
 * Payload of the `window:popupOpened` event.
 */
export interface WindowPopupOpenedPayload {
    windowId: string;
    title: string;
    url: string;
}

/**
 * Payload of the `window:stateChanged` event.
 */
export interface WindowStateChangedPayload {
    isMaximized: boolean;
    isMinimized: boolean;
    maximized: boolean;
    minimized: boolean;
    isActive: boolean;
    active: boolean;
    isFullscreen: boolean;
    fullscreen: boolean;
}
// ── Event-name union ─────────────────────────────────────────────────────
/**
 * Literal-union of every C++ event name. Kept in one-to-one sync with
 * `cpp_api_event.metadata.event_name` via `gen_sdk_types.mjs`.
 */
export type FBEventName =
    | "api:registered"
    | "api:unregistered"
    | "app:beforeQuit"
    | "audio:dspPresetChanged"
    | "audio:fullWaveformFailed"
    | "audio:fullWaveformReady"
    | "audio:outputDeviceChanged"
    | "audio:replaygainModeChanged"
    | "audio:spectrum"
    | "audio:stream"
    | "cursor:hiddenChanged"
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
    | "menu:show"
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
    | "window:alwaysOnTopChanged"
    | "window:backdropStateChanged"
    | "window:beforeClose"
    | "window:behaviorChanged"
    | "window:hoverStateChanged"
    | "window:message"
    | "window:minimizeSuppressed"
    | "window:popupClosed"
    | "window:popupOpened"
    | "window:stateChanged";
// ── Event-name → Payload map ─────────────────────────────────────────────
/**
 * Master map from C++ `event_name` to the generated / override `*Payload`
 * type. Consumed by typed `fb.on(...)` overloads.
 */
export interface FBEventPayloadMap {
    "api:registered": ApiRegisteredPayload;
    "api:unregistered": ApiUnregisteredPayload;
    "app:beforeQuit": AppBeforeQuitPayload;
    "audio:dspPresetChanged": AudioDspPresetChangedPayload;
    "audio:fullWaveformFailed": AudioFullWaveformFailedPayload;
    "audio:fullWaveformReady": AudioFullWaveformReadyPayload;
    "audio:outputDeviceChanged": AudioOutputDeviceChangedPayload;
    "audio:replaygainModeChanged": AudioReplaygainModeChangedPayload;
    "audio:spectrum": AudioSpectrumPayload;
    "audio:stream": AudioStreamPayload;
    "cursor:hiddenChanged": CursorHiddenChangedPayload;
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
    "menu:show": MenuShowPayload;
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
    "window:alwaysOnTopChanged": WindowAlwaysOnTopChangedPayload;
    "window:backdropStateChanged": WindowBackdropStateChangedPayload;
    "window:beforeClose": WindowBeforeClosePayload;
    "window:behaviorChanged": WindowBehaviorChangedPayload;
    "window:hoverStateChanged": WindowHoverStateChangedPayload;
    "window:message": WindowMessagePayload;
    "window:minimizeSuppressed": WindowMinimizeSuppressedPayload;
    "window:popupClosed": WindowPopupClosedPayload;
    "window:popupOpened": WindowPopupOpenedPayload;
    "window:stateChanged": WindowStateChangedPayload;
}

# SDK event system

Event names use colon notation, such as `playback:trackChanged`. Method invocations use dot notation, such as `playback.play`.

## Subscribe

```javascript
// Subscribe and retain the returned unsubscribe function.
const unsubscribe = fb.on('playback:trackChanged', (track) => {
    console.log('Now playing:', track.title);
});

// Unsubscribe.
unsubscribe();
```

## Subscribe once

```javascript
fb.once('playback:trackChanged', (track) => {
    console.log('First track:', track.title);
});
```

## Unsubscribe

```javascript
// Preferred: call the function returned by fb.on().
const unsub = fb.on('playback:time', handler);
unsub();

// Alternatively, remove the same handler explicitly.
fb.on('playback:time', handler);
fb.off('playback:time', handler);
```

## Playback events

| Event | Emitted when | Payload |
| --- | --- | --- |
| `playback:trackChanged` | The current track changes | `PlaybackTrackChangedPayload` |
| `playback:stateChanged` | Playback state changes | `{ state, position?, duration? }` |
| `playback:paused` | Pause state changes | `{ paused }` |
| `playback:stopped` | Playback stops | `{ reason }` |
| `playback:starting` | Playback is about to start | `{ command, paused? }` |
| `playback:seeked` | Playback position is changed | `{ position }` |
| `playback:volumeChanged` | Volume or mute state changes | `{ volume, volumeDb, muted, isMuted }` |
| `playback:time` | The normal-resolution position updates | `{ position }` |
| `playback:timeHighRes` | The high-resolution position updates | `{ position }` |
| `playback:orderChanged` | Playback order changes | `{ orderIndex, order }` |
| `playback:queueChanged` | Playback queue changes | `{ origin }` |
| `playback:edited` | Current-track metadata changes | `PlaybackEditedPayload` |
| `playback:dynamicInfo` | Stream information changes | `{ bitrate, streamTitle? }` |
| `playback:dynamicInfoTrack` | Dynamic stream-track metadata changes | `{ artist?, title? }` |
| `playback:itemPlayed` | A track finishes playing | `PlaybackItemPlayedPayload` |
| `playback:stopAfterCurrentChanged` | Stop-after-current changes | `{ enabled }` |
| `playback:followCursorChanged` | Playback-follow-cursor changes | `{ enabled }` |
| `playback:cursorFollowChanged` | Cursor-follow-playback changes | `{ enabled }` |

## Playlist events

| Event | Emitted when | Payload |
| --- | --- | --- |
| `playlist:activated` | Active playlist changes | `{ oldIndex, newIndex }` |
| `playlist:created` | A playlist is created | `{ index, name }` |
| `playlist:removed` | A playlist is removed | `{ oldCount, newCount }` |
| `playlist:renamed` | A playlist is renamed | `{ index, name }` |
| `playlist:reordered` | Playlist order changes | `{ count }` |
| `playlist:lockChanged` | Playlist lock state changes | `{ playlist, locked }` |
| `playlist:itemsAdded` | Items are added | `{ playlist, start, count }` |
| `playlist:itemsRemoved` | Items are removed | `{ playlist, oldCount, newCount }` |
| `playlist:itemsReordered` | Items are reordered | `{ playlist, count }` |
| `playlist:itemsReplaced` | Items are replaced | `{ playlist, count }` |
| `playlist:selectionChanged` | Selection changes | `{ playlist }` |
| `playlist:focusChanged` | Focused item changes | `{ playlist, from, to }` |
| `playlist:defaultFormatChanged` | Default formatting changes | Empty object |
| `playlist:addComplete` | An asynchronous add completes | `{ operationId, success, addedCount, totalCount }` |

## Library, metadata, and selection events

| Event | Emitted when | Payload |
| --- | --- | --- |
| `library:itemsAdded` | Library items are added | `{ count, timestamp }` |
| `library:itemsRemoved` | Library items are removed | `{ count, timestamp }` |
| `library:itemsModified` | Library items are modified | `{ count, timestamp }` |
| `library:initialized` | Library initialization completes | `{ timestamp }` |
| `library:getAllResult` | An asynchronous `library.getAll` result is ready | `LibraryGetAllResultPayload` |
| `metadata:writeComplete` | An asynchronous metadata write completes | `MetadataWriteCompletePayload` |
| `metadb:changed` | Metadata changes | `{ tracks, count, fromHook, timestamp }` |
| `selection:changed` | Global selection changes | `{ count, type, handles, truncated, track, nowPlaying }` |

## Audio events

| Event | Emitted when | Payload |
| --- | --- | --- |
| `audio:spectrum` | Subscribed spectrum data updates | `{ spectrum, fftSize?, bands? }` |
| `audio:stream` | Subscribed audio-stream data updates | Currently typed as an empty object |
| `audio:dspPresetChanged` | DSP preset changes | Empty object |
| `audio:outputDeviceChanged` | Output device changes | Empty object |
| `audio:replaygainModeChanged` | ReplayGain mode changes | `{ mode }` |
| `audio:fullWaveformReady` | Full-waveform generation completes | `AudioFullWaveformReadyPayload` |
| `audio:fullWaveformFailed` | Full-waveform generation fails | `{ taskId, path, error, code }` |

## Window, panel, UI, and desktop events

| Event | Emitted when | Payload |
| --- | --- | --- |
| `window:alwaysOnTopChanged` | Always-on-top changes | `{ enabled }` |
| `window:stateChanged` | Window state changes | Canonical `isMaximized`, `isMinimized`, `isActive`, and `isFullscreen` fields plus compatibility aliases |
| `window:popupOpened` | A popup opens | `{ windowId, title, url }` |
| `window:popupClosed` | A popup closes | `{ windowId }` |
| `window:beforeClose` | Close confirmation is requested | `{ windowId }` |
| `window:message` | A cross-window message arrives | `{ sourceWindowId, message }` |
| `window:behaviorChanged` | Popup behavior changes | `{ windowId, profile, behavior, resolvedBehavior }` |
| `window:minimizeSuppressed` | A minimize action is suppressed | `{ windowId, reason }` |
| `window:backdropStateChanged` | A backdrop activation policy is applied | `{ windowId, active, mode, effect }` |
| `window:hoverStateChanged` | Window hover state changes | `{ windowId, reason?, hovering? }` |
| `panel:initialized` | Panel initialization completes | `PanelInitializedPayload` |
| `panel:focus` / `panel:blur` | Panel focus changes | Empty object |
| `panel:visibilityChanged` | DUI panel visibility changes | `{ visible }` |
| `panel:configChanged` | Panel configuration changes | `PanelConfigChangedPayload` |
| `ui:coloursChanged` / `ui:fontChanged` | UI colors or fonts change | Unspecified object |
| `ui:menuItemClicked` | A UI menu item is clicked | `{ id, label }` |
| `ui:toast` | A toast is requested | `{ message, duration, type, position }` |
| `system:themeChanged` | System theme changes | `{ darkMode }` |
| `cursor:hiddenChanged` | Cursor hidden state changes | `CursorHiddenChangedPayload` |
| `taskbar:buttonClicked` | A taskbar button is clicked | `{ id }` |
| `webview:processFailed` | A WebView process fails or recovers | `{ kind, kindRaw, recovered, recoveryAction }` |

## Tray events

The tray icon is application-scoped and has no source window. Its events are broadcast to all windows.

| Event | Emitted when | Payload |
| --- | --- | --- |
| `tray:click` | The tray icon is clicked | `{ button, x, y }` |
| `tray:doubleClick` | The tray icon is double-clicked | `{ x, y }` |
| `tray:beforeContextMenu` | Before the context menu opens | `{ x, y }` |
| `tray:menuItemClicked` | An ordinary tray item is selected or a rich value changes | `{ id, value? }` |

Ordinary user items and now-playing cards report `{ id }` and close the menu. Rich value controls report `{ id, value }` and keep it open: ratings use `0..5`, sliders use an integer in `[min, max]`, and segmented controls use the selected zero-based index. Built-in `showPlaybackControls` / `showSystemItems` injections and items declaring `playbackAction` execute natively and do **not** fire this event.

## Application, keyboard, and menu events

| Event | Emitted when | Payload |
| --- | --- | --- |
| `app:beforeQuit` | foobar2000 is about to quit | Empty object |
| `keyboard:hotkey` | A registered hotkey fires | `{ id, key, action }` |
| `menu:show` | A custom menu opens | `{ menuId }` |
| `menu:select` | A custom-menu item is selected | `{ menuId, itemId }` |
| `menu:valueChanged` | A rating, slider, or segmented control in a custom menu changes value; the menu stays open | `{ menuId, itemId, value }` |
| `menu:dismiss` | A custom menu closes | `{ menuId, reason }` |

## Cross-window events

| Event | Emitted when | Payload |
| --- | --- | --- |
| `port:connected` | A named port connects | `{ portId, name, windowId }` |
| `port:disconnected` | A named port disconnects | `{ portId, name, windowId }` |
| `port:message` | A port message arrives | `{ portId, sourcePortId, sourceWindowId, message }` |
| `state:changed` | Shared state changes | `{ key, value, previousValue, sourceWindowId, expiresAt? }` |
| `state:deleted` | Shared state is removed or expires | `{ key, sourceWindowId, reason }` |

## Plugin, HTTP, and JIT queue events

| Event | Emitted when | Payload |
| --- | --- | --- |
| `plugin:registered` / `plugin:unregistered` | An external plugin is registered or removed | `PluginRegisteredPayload` / `PluginUnregisteredPayload` |
| `api:registered` / `api:unregistered` | An external API is registered or removed | `ApiRegisteredPayload` / `ApiUnregisteredPayload` |
| `http:response` | An asynchronous HTTP request completes | `HttpResponsePayload` |
| `http:downloadComplete` | A download completes | `HttpDownloadCompletePayload` |
| `jitQueue:needNext` | The host requests the next item | `{ currentTrackId, reason }` |
| `jitQueue:trackChanged` | The JIT queue's current track changes | `{ trackId, title }` |
| `jitQueue:listExhausted` | The JIT queue buffer is exhausted | `{ lastTrackId }` |
| `jitQueue:preloadComplete` | JIT preloading completes | `JitQueuePreloadCompletePayload` |
| `jitQueue:error` | A JIT queue item fails | `{ trackId, error, url? or path? }` |

Host events are also dispatched as `fb2k:*` DOM `CustomEvent` instances.

See the [event-system reference](/reference/events) for the complete payload reference. SMP event-name mappings are documented in the [SMP compatibility layer](/reference/smp-compat#smp-events).

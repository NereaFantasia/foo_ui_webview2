# Events API

Runtime event payload reference for foo_ui_webview2. Event names use **colon** format (`namespace:eventName`); method invocations use **dot** format (`namespace.method`).

Every payload key set is backed by a precise C++ emit-source rule. Internal `:__` events and historical entries without runtime emit evidence are excluded.

## app events

| Event | Payload keys |
| --- | --- |
| app:beforeQuit | `{}` |

## audio events

| Event | Payload keys |
| --- | --- |
| audio:dspPresetChanged | `{}` |
| audio:fullWaveformFailed | `{ code, error, path, taskId }` |
| audio:fullWaveformReady | `{ cached, channels, duration, method, path, resolution, sampleRate, scale, signed, taskId, waveform }` |
| audio:outputDeviceChanged | `{}` |
| audio:replaygainModeChanged | `{ mode }` |
| audio:spectrum | `{ spectrum }` |
| audio:stream | `{}` |

`audio:stream` is currently only a reserved token returned by the unimplemented stream-capture subscription stub; the runtime does not emit it.

## cursor events

| Event | Payload keys |
| --- | --- |
| cursor:hiddenChanged | `{ hidden }` |

## http events

| Event | Payload keys |
| --- | --- |
| http:downloadComplete | `{ bytesWritten, cancelled, code, error, path, requestId, status, success }` |
| http:response | `{ body, cancelled, code, contentLength, error, headers, requestId, responseType, status, success }` |

## jitQueue events

| Event | Payload keys |
| --- | --- |
| jitQueue:error | `{ error, path, trackId, url }` |
| jitQueue:listExhausted | `{ lastTrackId }` |
| jitQueue:needNext | `{ currentTrackId, reason }` |
| jitQueue:preloadComplete | `{ count, replace, startIndex }` |
| jitQueue:trackChanged | `{ title, trackId }` |

For `jitQueue:error`, local-file failures include `path`, while remote-source failures include `url`; exactly one of those two keys is present in each payload.

## keyboard events

| Event | Payload keys |
| --- | --- |
| keyboard:hotkey | `{ action, id, key }` |

## library events

| Event | Payload keys |
| --- | --- |
| library:getAllResult | `{ error, fromCache, items, limit, offset, requestId, total, tracks }` |
| library:initialized | `{ timestamp }` |
| library:itemsAdded | `{ count, timestamp }` |
| library:itemsModified | `{ count, timestamp }` |
| library:itemsRemoved | `{ count, timestamp }` |

## menu events

| Event | Payload keys |
| --- | --- |
| menu:dismiss | `{ menuId, reason }` |
| menu:select | `{ itemId, menuId }` |
| menu:show | `{ menuId }` |
| menu:valueChanged | `{ itemId, menuId, value }` |

`menu:valueChanged` fires when a rating, slider, or segmented control in a self-drawn menu changes value, and the menu stays open; an ordinary row reports through `menu:select` and closes the menu.

## metadata events

| Event | Payload keys |
| --- | --- |
| metadata:writeComplete | `{ code, operation, path, status, subsong, success }` |

## metadb events

| Event | Payload keys |
| --- | --- |
| metadb:changed | `{ count, fromHook, timestamp, tracks }` |

## panel events

| Event | Payload keys |
| --- | --- |
| panel:blur | `{}` |
| panel:configChanged | `{ edgeStyle, enableDevTools, enableDragDrop, grabFocus, panelName, templateName, transparentBackground, urlOverride }` |
| panel:focus | `{}` |
| panel:initialized | `{ mode, panelMode, windowId }` |
| panel:visibilityChanged | `{ visible }` |

## playback events

| Event | Payload keys |
| --- | --- |
| playback:cursorFollowChanged | `{ enabled }` |
| playback:dynamicInfo | `{ bitrate, streamTitle }` |
| playback:dynamicInfoTrack | `{ artist, title }` |
| playback:edited | `{ absolutePath, album, albumArtist, artist, bitrate, channels, codec, date, discNumber, duration, fileSize, fullPath, genre, id, path, sampleRate, subsong, title, trackNumber }` |
| playback:followCursorChanged | `{ enabled }` |
| playback:itemPlayed | `{ absolutePath, album, albumArtist, artist, bitrate, channels, codec, date, discNumber, duration, fileSize, fullPath, genre, id, path, sampleRate, subsong, title, trackNumber }` |
| playback:orderChanged | `{ order, orderIndex }` |
| playback:paused | `{ paused }` |
| playback:queueChanged | `{ origin }` |
| playback:seeked | `{ position }` |
| playback:starting | `{ command, paused }` |
| playback:stateChanged | `{ duration, position, state }` |
| playback:stopAfterCurrentChanged | `{ enabled }` |
| playback:stopped | `{ reason }` |
| playback:time | `{ position }` |
| playback:timeHighRes | `{ position }` |
| playback:trackChanged | `{ absolutePath, album, albumArtist, artist, bitrate, channels, codec, date, discNumber, duration, fileSize, fullPath, genre, id, path, sampleRate, subsong, title, trackNumber }` |
| playback:volumeChanged | `{ isMuted, muted, volume, volumeDb }` |

## playlist events

| Event | Payload keys |
| --- | --- |
| playlist:activated | `{ oldIndex, newIndex }` |
| playlist:addComplete | `{ addedCount, operationId, success, totalCount }` |
| playlist:created | `{ index, name }` |
| playlist:defaultFormatChanged | `{}` |
| playlist:focusChanged | `{ playlist, from, to }` |
| playlist:itemsAdded | `{ playlist, start, count }` |
| playlist:itemsRemoved | `{ playlist, oldCount, newCount }` |
| playlist:itemsReordered | `{ playlist, count }` |
| playlist:itemsReplaced | `{ playlist, count }` |
| playlist:lockChanged | `{ playlist, locked }` |
| playlist:removed | `{ oldCount, newCount }` |
| playlist:renamed | `{ index, name }` |
| playlist:reordered | `{ count }` |
| playlist:selectionChanged | `{ playlist }` |

## port events

| Event | Payload keys |
| --- | --- |
| port:connected | `{ name, portId, windowId }` |
| port:disconnected | `{ name, portId, windowId }` |
| port:message | `{ message, portId, sourcePortId, sourceWindowId }` |

## selection events

| Event | Payload keys |
| --- | --- |
| selection:changed | `{ absolutePath, album, albumArtist, artist, bitrate, channels, codec, count, date, discNumber, duration, fileSize, fullPath, genre, handles, id, nowPlaying, path, sampleRate, subsong, title, track, trackNumber, truncated, type }` |

## state events

| Event | Payload keys |
| --- | --- |
| state:changed | `{ expiresAt, key, previousValue, sourceWindowId, value }` |
| state:deleted | `{ key, reason, sourceWindowId }` |

## system events

| Event | Payload keys |
| --- | --- |
| system:themeChanged | `{ darkMode }` |

## taskbar events

| Event | Payload keys |
| --- | --- |
| taskbar:buttonClicked | `{ id }` |

## tray events

| Event | Payload keys |
| --- | --- |
| tray:beforeContextMenu | `{ x, y }` |
| tray:click | `{ button, x, y }` |
| tray:doubleClick | `{ x, y }` |
| tray:menuItemClicked | `{ id, value }` |

## ui events

| Event | Payload keys |
| --- | --- |
| ui:coloursChanged | `{}` |
| ui:fontChanged | `{}` |
| ui:menuItemClicked | `{ id, label }` |
| ui:toast | `{ duration, message, position, type }` |

## webview events

| Event | Payload keys |
| --- | --- |
| webview:processFailed | `{ kind, kindRaw, recovered, recoveryAction }` |

## window events

| Event | Payload keys |
| --- | --- |
| window:alwaysOnTopChanged | `{ enabled }` |
| window:backdropStateChanged | `{ active, effect, mode, windowId }` |
| window:beforeClose | `{ windowId }` |
| window:behaviorChanged | `{ behavior, profile, resolvedBehavior, windowId }` |
| window:hoverStateChanged | `{ hovering, windowId }` |
| window:message | `{ message, sourceWindowId }` |
| window:minimizeSuppressed | `{ reason, windowId }` |
| window:popupClosed | `{ windowId }` |
| window:popupOpened | `{ title, url, windowId }` |
| window:stateChanged | `{ active, fullscreen, isActive, isFullscreen, isMaximized, isMinimized, maximized, minimized }` |

## Notes

- `playback:stopAfterCurrentChanged` uses `{ enabled }`, the same field as its API.
- Playlist lifecycle payload fields may gain entries in future versions; treat unknown keys as additive.
- The exact emit source and field set for every row follow the C++ `EmitEvent` / `BroadcastEvent` call sites in the component source.

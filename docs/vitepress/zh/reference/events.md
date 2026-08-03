# 事件参考

foo_ui_webview2 的运行时事件 payload 参考。事件名使用 **冒号格式**（`namespace:eventName`），方法调用使用 **点格式**（`namespace.method`）。

每个 payload key set 均来自对应 C++ emit 源的精确规则；内部 `:__` 事件和没有运行时 emit 证据的历史条目不在此参考中。

## app 事件

| 事件 | Payload keys |
| --- | --- |
| app:beforeQuit | `{}` |

## audio 事件

| 事件 | Payload keys |
| --- | --- |
| audio:dspPresetChanged | `{}` |
| audio:fullWaveformFailed | `{ code, error, path, taskId }` |
| audio:fullWaveformReady | `{ cached, channels, duration, method, path, resolution, sampleRate, scale, signed, taskId, waveform }` |
| audio:outputDeviceChanged | `{}` |
| audio:replaygainModeChanged | `{ mode }` |
| audio:spectrum | `{ spectrum }` |
| audio:stream | `{}` |

`audio:stream` 当前只是尚未实现的流捕获订阅 stub 返回的保留 token；运行时不会发射该事件。

## cursor 事件

| 事件 | Payload keys |
| --- | --- |
| cursor:hiddenChanged | `{ hidden }` |

## http 事件

| 事件 | Payload keys |
| --- | --- |
| http:downloadComplete | `{ bytesWritten, cancelled, code, error, path, requestId, status, success }` |
| http:response | `{ body, cancelled, code, contentLength, error, headers, requestId, responseType, status, success }` |

## jitQueue 事件

| 事件 | Payload keys |
| --- | --- |
| jitQueue:error | `{ error, path, trackId, url }` |
| jitQueue:listExhausted | `{ lastTrackId }` |
| jitQueue:needNext | `{ currentTrackId, reason }` |
| jitQueue:preloadComplete | `{ count, replace, startIndex }` |
| jitQueue:trackChanged | `{ title, trackId }` |

对于 `jitQueue:error`，本地文件失败包含 `path`，远程源失败包含 `url`；每个 payload 中这两个 key 恰有其一。

## keyboard 事件

| 事件 | Payload keys |
| --- | --- |
| keyboard:hotkey | `{ action, id, key }` |

## library 事件

| 事件 | Payload keys |
| --- | --- |
| library:getAllResult | `{ error, fromCache, items, limit, offset, requestId, total, tracks }` |
| library:initialized | `{ timestamp }` |
| library:itemsAdded | `{ count, timestamp }` |
| library:itemsModified | `{ count, timestamp }` |
| library:itemsRemoved | `{ count, timestamp }` |

## menu 事件

| 事件 | Payload keys |
| --- | --- |
| menu:dismiss | `{ menuId, reason }` |
| menu:select | `{ itemId, menuId }` |
| menu:show | `{ menuId }` |

## metadata 事件

| 事件 | Payload keys |
| --- | --- |
| metadata:writeComplete | `{ code, operation, path, status, subsong, success }` |

## metadb 事件

| 事件 | Payload keys |
| --- | --- |
| metadb:changed | `{ count, fromHook, timestamp, tracks }` |

## panel 事件

| 事件 | Payload keys |
| --- | --- |
| panel:blur | `{}` |
| panel:configChanged | `{ edgeStyle, enableDevTools, enableDragDrop, grabFocus, panelName, templateName, transparentBackground, urlOverride }` |
| panel:focus | `{}` |
| panel:initialized | `{ mode, panelMode, windowId }` |
| panel:visibilityChanged | `{ visible }` |

## playback 事件

| 事件 | Payload keys |
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

## playlist 事件

| 事件 | Payload keys |
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

## port 事件

| 事件 | Payload keys |
| --- | --- |
| port:connected | `{ name, portId, windowId }` |
| port:disconnected | `{ name, portId, windowId }` |
| port:message | `{ message, portId, sourcePortId, sourceWindowId }` |

## selection 事件

| 事件 | Payload keys |
| --- | --- |
| selection:changed | `{ absolutePath, album, albumArtist, artist, bitrate, channels, codec, count, date, discNumber, duration, fileSize, fullPath, genre, handles, id, nowPlaying, path, sampleRate, subsong, title, track, trackNumber, truncated, type }` |

## state 事件

| 事件 | Payload keys |
| --- | --- |
| state:changed | `{ expiresAt, key, previousValue, sourceWindowId, value }` |
| state:deleted | `{ key, reason, sourceWindowId }` |

## system 事件

| 事件 | Payload keys |
| --- | --- |
| system:themeChanged | `{ darkMode }` |

## taskbar 事件

| 事件 | Payload keys |
| --- | --- |
| taskbar:buttonClicked | `{ id }` |

## tray 事件

| 事件 | Payload keys |
| --- | --- |
| tray:beforeContextMenu | `{ x, y }` |
| tray:click | `{ button, x, y }` |
| tray:doubleClick | `{ x, y }` |
| tray:menuItemClicked | `{ id, value }` |

## ui 事件

| 事件 | Payload keys |
| --- | --- |
| ui:coloursChanged | `{}` |
| ui:fontChanged | `{}` |
| ui:menuItemClicked | `{ id, label }` |
| ui:toast | `{ duration, message, position, type }` |

## webview 事件

| 事件 | Payload keys |
| --- | --- |
| webview:processFailed | `{ kind, kindRaw, recovered, recoveryAction }` |

## window 事件

| 事件 | Payload keys |
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

## 说明

- `playback:stopAfterCurrentChanged` 的 payload 为 `{ enabled }`，与对应 API 字段相同。
- 播放列表生命周期 payload 的字段在后续版本中可能增加；请把未知键视为增量而非异常。
- 每一行的精确 emit source 和字段集以组件源码中的 C++ `EmitEvent` / `BroadcastEvent` 调用点为准。
